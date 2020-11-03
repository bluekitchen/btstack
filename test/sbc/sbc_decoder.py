#!/usr/bin/env python3
import numpy as np
import wave
import struct
import sys
from sbc import *
from sbc_synthesis_v1 import *

V = np.zeros(shape = (2, 10*2*8))
N = np.zeros(shape = (16,8))
total_time_ms = 0
mSBC_enabled = 1
H2_first_byte = 0
H2_second_byte = 0

def find_syncword(h2_first_byte, h2_second_byte):
    if h2_first_byte != 1:
        return -1

    hn = h2_second_byte >> 4
    ln = h2_second_byte & 0x0F
    if ln == 8:
        sn0 = hn & 3 
        sn1 = hn >> 2

        if sn0 != sn1:
            return -1

        if sn0 not in [0,3]:
            return -1

    return sn0

def sbc_unpack_frame(fin, available_bytes, frame):
    global H2_first_byte, H2_second_byte
    if available_bytes == 0:
        print ("no available_bytes")
        raise TypeError

    frame.syncword = get_bits(fin,8)
    if mSBC_enabled:
        if frame.syncword != 173:
            #print ("out of sync %02x" % frame.syncword)
            H2_first_byte = H2_second_byte
            H2_second_byte = frame.syncword
            return -1
    else:
        if frame.syncword != 156:
            #print ("out of sync %02x" % frame.syncword)
            return -1

    if mSBC_enabled:
        frame.sampling_frequency = 0    # == 16 kHz
        frame.nr_blocks = 15
        frame.channel_mode = MONO
        frame.allocation_method = LOUDNESS
        frame.nr_subbands = 8
        frame.bitpool = 26
        frame.reserved_for_future_use = get_bits(fin,16)
    else:
        frame.sampling_frequency = get_bits(fin,2)
        frame.nr_blocks = nr_blocks[get_bits(fin,2)]
        frame.channel_mode = get_bits(fin,2)
        frame.allocation_method = get_bits(fin,1)
        frame.nr_subbands = nr_subbands[get_bits(fin,1)] 
        frame.bitpool = get_bits(fin,8)

    if frame.channel_mode == MONO:
        frame.nr_channels = 1
    else:
        frame.nr_channels = 2

    frame.crc_check = get_bits(fin,8)

    frame.init(frame.nr_blocks, frame.nr_subbands, frame.nr_channels)

    # read joint stereo flags
    if frame.channel_mode == JOINT_STEREO:
        for sb in range(frame.nr_subbands-1):
            frame.join[sb] = get_bits(fin,1)
        get_bits(fin,1) # RFA
    
    frame.scale_factor = np.zeros(shape=(frame.nr_channels, frame.nr_subbands), dtype = np.int32)
        
    # read scale factors
    for ch in range(frame.nr_channels):
        for sb in range(frame.nr_subbands):
            frame.scale_factor[ch][sb] = get_bits(fin, 4)

    if mSBC_enabled:
        #print ("syncword: ", find_syncword(H2_first_byte, H2_second_byte))
        crc = calculate_crc_mSBC(frame)
    else:
        crc = calculate_crc(frame)
    
    if crc != frame.crc_check:
        print ("CRC mismatch: calculated %d, expected %d" % (crc, frame.crc_check))
        return -1

    
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
        #print ("block %2d - audio sample: %s" % (blk, frame.audio_sample[blk][0]))
     
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
                    frame.sb_sample[blk][0][sb] = ch_a
                    frame.sb_sample[blk][1][sb] = ch_b

    return 0


def sbc_frame_synthesis_sig(frame, ch, blk, proto_table):
    global V, N
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
            V[ch][k] += N[k][i] * S[i]
    
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


def sbc_frame_synthesis_v1(frame, ch, blk, proto_table):
    global V
    N = matrix_N()
    
    M = frame.nr_subbands
    L = 10 * M
    M2 = 2*M
    L2 = 2*L

    S = np.zeros(M)
    W = np.zeros(L)
    frame.X = np.zeros(M)
    
    for i in range(M):
        S[i] = frame.sb_sample[blk][ch][i]

    for i in range(L2-1, M2-1,-1):
        V[ch][i] = V[ch][i-M2] 

    
    for k in range(M2):
        V[ch][k] = 0
        for i in range(M):
            V[ch][k] += N[k][i] * S[i]

    for i in range(L):
        D = proto_table[i] * (-M)
        W[i] = D * VSGN(i,M2) * V[ch][remap_V(i)]
    
    offset = blk*M
    for j in range(M):
        for i in range(10):
            frame.X[j] += W[j+M*i]
        frame.pcm[ch][offset + j] = np.int16(frame.X[j])


def sbc_frame_synthesis(frame, ch, blk, proto_table, implementation = "SIG"):
    global total_time_ms
        
    t1 = time_ms()
    if implementation == "SIG":
         sbc_frame_synthesis_sig(frame, ch, blk, proto_table)
    elif implementation == "V1":
        sbc_frame_synthesis_v1(frame, ch, blk, proto_table)
    else:
        print ("synthesis %s not implemented" % implementation)
        exit(1)

    t2 = time_ms()
    total_time_ms += t2-t1


def sbc_init_synthesis_sig(M):
    global N
    M2 = M << 1
    
    N = np.zeros(shape = (M2,M))
    for k in range(M2):
        for i in range(M):
            N[k][i] = np.cos((i+0.5)*(k+M/2)*np.pi/M)


    
def sbc_init_sythesis(nr_subbands, implementation = "SIG"):
    if implementation == "SIG":
         sbc_init_synthesis_sig(nr_subbands)
    elif implementation == "V1":
        sbc_init_synthesis_v1(nr_subbands)
    else:
        print ("synthesis %s not implemented" % implementation)
        exit(1)


def sbc_synthesis(frame, implementation = "SIG"):
    if frame.nr_subbands == 4:
        proto_table = Proto_4_40
    elif frame.nr_subbands == 8:
        proto_table = Proto_8_80
    else:
        return -1
    for ch in range(frame.nr_channels):
        for blk in range(frame.nr_blocks):
            sbc_frame_synthesis(frame, ch, blk, proto_table, implementation)
       
    return frame.nr_blocks * frame.nr_subbands

def sbc_decode(frame, implementation = "SIG"):
    err = sbc_reconstruct_subband_samples(frame)
    if err >= 0:
        err = sbc_synthesis(frame, implementation)
    return err


def write_wav_file(fout, frame):
    values = []

    for i in range(frame.nr_subbands * frame.nr_blocks):
        for ch in range(frame.nr_channels):
            try:
                packed_value = struct.pack('h', frame.pcm[ch][i])
                values.append(packed_value)
            except struct.error:
                print (frame)
                print (i, frame.pcm[ch][i], frame.pcm[ch])
                exit(1)

    value_str = ''.join(values)
    fout.writeframes(value_str)



if __name__ == "__main__":
    usage = '''
    Usage: ./sbc_decoder.py input.(msbc|sbc) implementation[default=SIG, V1]
    '''

    if (len(sys.argv) < 2):
        print(usage)
        sys.exit(1)
    try:
        mSBC_enabled = 0
        infile = sys.argv[1]
        if not infile.endswith('.sbc'):
            if infile.endswith('.msbc'):
                wavfile = infile.replace('.msbc', '-decoded.wav')
                mSBC_enabled = 1
            else:
                print(usage)
                sys.exit(1)
        else:
            wavfile = infile.replace('.sbc', '-decoded-py.wav')
        
        print ("input file: ", infile)
        print ("output file: ", wavfile)
        print ("mSBC enabled: ", mSBC_enabled)

        fout = False

        implementation = "SIG"
        if len(sys.argv) == 3:
            implementation = sys.argv[2]
            if implementation != "V1":
                print ("synthesis %s not implemented" % implementation)
                exit(1)

        print ("\nSynthesis implementation: %s\n" % implementation)

        with open (infile, 'rb') as fin:
            try:
                fin.seek(0, 2)
                file_size = fin.tell()
                fin.seek(0, 0)

                frame_count = 0
                while True:
                    frame = SBCFrame()
                    if frame_count % 200 == 0:
                        print ("== Frame %d == offset %d" % (frame_count, fin.tell()))

                    err = sbc_unpack_frame(fin, file_size - fin.tell(), frame)
                    if err:
                        #print ("error, frame_count: ", frame_count)
                        continue

                    if frame_count == 0:
                        sbc_init_sythesis(frame.nr_subbands, implementation)
                        print (frame                    )

                    sbc_decode(frame, implementation)
                        
                    if frame_count == 0:
                        fout = wave.open(wavfile, 'w')
                        fout.setnchannels(frame.nr_channels)
                        fout.setsampwidth(2)
                        fout.setframerate(sampling_frequencies[frame.sampling_frequency])
                        fout.setnframes(0)
                        fout.setcomptype = 'NONE'
                        
                        print (frame.pcm)


                    write_wav_file(fout, frame)
                    frame_count += 1
                    
                    # if frame_count == 1:
                    #     break

            except TypeError as err:
                if not fout:
                    print (err)
                else:
                    fout.close()
                    if frame_count > 0:
                        print ("DONE, SBC file %s decoded into WAV file %s " % (infile, wavfile))
                        print ("Average sythesis time per frame: %d ms/frame" % (total_time_ms/frame_count))
                    else:
                        print ("No frame found")
                exit(0) 

        fout.close()
        if frame_count > 0:
            print ("DONE: SBC file %s decoded into WAV file %s " % (infile, wavfile))
            print ("Average sythesis time per frame: %d ms/frame" % (total_time_ms/frame_count))
        else:
            print ("No frame found")
        
    except IOError as e:
        print(usage)
        sys.exit(1)





