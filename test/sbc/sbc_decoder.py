#!/usr/bin/env python
import numpy as np
import wave
import struct
import sys
from sbc import *

V = np.zeros(shape = (2, 10*2*8))

def sbc_unpack_frame(fin, available_bytes, frame):
    if available_bytes == 0:
        print "no available_bytes"
        raise TypeError

    frame.syncword = get_bits(fin,8)
    if frame.syncword != 156:
        print ("out of sync %02x" % frame.syncword)
        return -1
    frame.sampling_frequency = get_bits(fin,2)
    frame.nr_blocks = nr_blocks[get_bits(fin,2)]

    frame.channel_mode = get_bits(fin,2)

    if frame.channel_mode == MONO:
        frame.nr_channels = 1
    else:
        frame.nr_channels = 2
    
    frame.allocation_method = get_bits(fin,1)
    frame.nr_subbands = nr_subbands[get_bits(fin,1)] 
    frame.init(frame.nr_blocks, frame.nr_subbands, frame.nr_channels)

    frame.bitpool = get_bits(fin,8)
    frame.crc_check = get_bits(fin,8)

    # frame.join = np.zeros(frame.nr_subbands, dtype = np.uint8)

    if frame.channel_mode == JOINT_STEREO:
        for sb in range(frame.nr_subbands-1):
            frame.join[sb] = get_bits(fin,1)
        get_bits(fin,1) # RFA
    
    frame.scale_factor = np.zeros(shape=(frame.nr_channels, frame.nr_subbands), dtype = np.int32)
    
    # print frame.audio_sample
    for ch in range(frame.nr_channels):
        for sb in range(frame.nr_subbands):
            frame.scale_factor[ch][sb] = get_bits(fin, 4)
    
    crc = calculate_crc(frame)
    if crc != frame.crc_check:
        print frame
        print "error, crc not equal: ", crc, frame.crc_check
        exit(1)
    
    frame.scalefactor = np.zeros(shape=(frame.nr_channels, frame.nr_subbands), dtype = np.int32)
    for ch in range(frame.nr_channels):
        for sb in range(frame.nr_subbands):
            frame.scalefactor[ch][sb] = 1 << (frame.scale_factor[ch][sb] + 1)

    
    frame.bits = sbc_bit_allocation(frame)
    
    frame.audio_sample = np.ndarray(shape=(frame.nr_blocks, frame.nr_channels, frame.nr_subbands), dtype = np.uint16)
    for blk in range(frame.nr_blocks):
        for ch in range(frame.nr_channels):
            for sb in range(frame.nr_subbands):
                frame.audio_sample[blk][ch][sb] = get_bits(fin, frame.bits[ch][sb])
        #print "block %2d - audio sample: %s" % (blk, frame.audio_sample[blk][0])
     
    drop_remaining_bits()
    return 0

def sbc_reconstruct_subband_samples(frame):
    frame.levels = np.zeros(shape=(frame.nr_channels, frame.nr_subbands), dtype = np.int32)
    frame.sb_sample = np.zeros(shape=(frame.nr_blocks, frame.nr_channels, frame.nr_subbands))
    
    for ch in range(frame.nr_channels):
        for sb in range(frame.nr_subbands):
            frame.levels[ch][sb] = pow(2.0, frame.bits[ch][sb]) - 1

    for blk in range(frame.nr_blocks):
        for ch in range(frame.nr_channels):
            for sb in range(frame.nr_subbands):
                if frame.levels[ch][sb] > 0:
                    AS = frame.audio_sample[blk][ch][sb]
                    L  = frame.levels[ch][sb]
                    SF = frame.scalefactor[ch][sb]
                    frame.sb_sample[blk][ch][sb] = SF * ((AS*2.0+1.0) / L -1.0 )
                else:
                    frame.sb_sample[blk][ch][sb] = 0

    # sythesis filter
    if frame.channel_mode == JOINT_STEREO:
        for blk in range(frame.nr_blocks):
            for sb in range(frame.nr_subbands):
                if frame.join[sb]==1:
                    ch_a = frame.sb_sample[blk][0][sb] + frame.sb_sample[blk][1][sb]
                    ch_b = frame.sb_sample[blk][0][sb] - frame.sb_sample[blk][1][sb]
                    frame.sb_sample[blk][0][sb] = ch_a/2
                    frame.sb_sample[blk][1][sb] = ch_b/2

    return 0


def sbc_frame_synthesis(frame, ch, blk, proto_table):
    global V
    M = frame.nr_subbands
    L = 10 * M
    M2 = 2*M
    L2 = 2*L

    S = np.zeros(M)
    U = np.zeros(L)
    W = np.zeros(L)
    frame.X = np.zeros(M)
    
    for i in range(M):
        S[i] = frame.sb_sample[blk][ch][i]

    for i in range(L2-1, M2-1,-1):
        V[ch][i] = V[ch][i-M2] 

    for k in range(M2):
        V[ch][k] = 0
        for i in range(M):
            N = np.cos((i+0.5)*(k+M/2)*np.pi/M)
            V[ch][k] += N * S[i]

    for i in range(5):
        for j in range(M):
            U[i*M2+j] = V[ch][i*2*M2+j]
            U[(i*2+1)*M+j] = V[ch][(i*4+3)*M+j]

    for i in range(L):
        D = proto_table[i] * (-M)
        W[i] = U[i]*D

    
    offset = blk*M
    for j in range(M):
        for i in range(10):
            frame.X[j] += W[j+M*i]
        frame.pcm[ch][offset + j] = np.int16(frame.X[j])


def sbc_synthesis(frame):
    if frame.nr_subbands == 4:
        proto_table = Proto_4_40
    elif frame.nr_subbands == 8:
        proto_table = Proto_8_80
    else:
        return -1
    for ch in range(frame.nr_channels):
        for blk in range(frame.nr_blocks):
            sbc_frame_synthesis(frame, ch, blk, proto_table)
       
    return frame.nr_blocks * frame.nr_subbands

def sbc_decode(frame):
    err = sbc_reconstruct_subband_samples(frame)
    if err >= 0:
        err = sbc_synthesis(frame)
    return err


def write_wav_file(fout, frame):
    values = []

    for i in range(frame.nr_subbands * frame.nr_blocks):
        for ch in range(frame.nr_channels):
            try:
                packed_value = struct.pack('h', frame.pcm[ch][i])
                values.append(packed_value)
            except struct.error:
                print frame
                print i, frame.pcm[ch][i], frame.pcm[ch]
                exit(1)

    value_str = ''.join(values)
    fout.writeframes(value_str)
    

if __name__ == "__main__":
    usage = '''
    Usage: ./sbc_decoder.py input.sbc
    '''

    if (len(sys.argv) < 2):
        print(usage)
        sys.exit(1)
    try:
        infile = sys.argv[1]
        if not infile.endswith('.sbc'):
            print(usage)
            sys.exit(1)

        wavfile = infile.replace('.sbc', '-decoded.wav')
        fout = False

        with open (infile, 'rb') as fin:
            try:
                fin.seek(0, 2)
                file_size = fin.tell()
                fin.seek(0, 0)

                frame_count = 0
                while True:
                    sbc_decoder_frame = SBCFrame()
                    if frame_count % 200 == 0:
                        print "== Frame %d == %d" % (frame_count, fin.tell())


                    err = sbc_unpack_frame(fin, file_size - fin.tell(), sbc_decoder_frame)
                    if frame_count == 0:
                        print sbc_decoder_frame

                    if err:
                        print "error, frame_count: ", frame_count
                        break
                    

                    sbc_decode(sbc_decoder_frame)
                    
                    if frame_count == 0:
                        fout = wave.open(wavfile, 'w')
                        fout.setnchannels(sbc_decoder_frame.nr_channels)
                        fout.setsampwidth(2)
                        fout.setframerate(sampling_frequencies[sbc_decoder_frame.sampling_frequency])
                        fout.setnframes(0)
                        fout.setcomptype = 'NONE'
                    
                    write_wav_file(fout, sbc_decoder_frame)
                    frame_count += 1

            except TypeError as err:
                if not fout:
                    print err
                else:
                    fout.close()
                    print ("DONE, SBC file %s decoded into WAV file %s " % (infile, wavfile))
                exit(0) 

    except IOError as e:
        print(usage)
        sys.exit(1)





