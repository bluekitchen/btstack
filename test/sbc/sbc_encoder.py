#!/usr/bin/env python
import numpy as np
import wave
import struct
import sys
from sbc import *

X = np.zeros(80)

def fetch_samples_for_next_sbc_frame(fin, nr_audio_frames, frame):
    raw_data = fin.readframes(nr_audio_frames) # Returns byte data
    
    total_samples = nr_audio_frames * frame.nr_channels
    fmt = "%ih" % total_samples # read signed 2 byte shorts

    frame.pcm =  np.array(struct.unpack(fmt, raw_data))
    del raw_data

    
def sbc_analyse(frame, ch, blk, C, debug):
    global X
    M = frame.nr_subbands
    L = 10 * M
    M2 = 2*M
    L2 = 2*L

    Z = np.zeros(L)
    Y = np.zeros(M2)
    W = np.zeros(shape=(M, M2))
    S = np.zeros(M)

    for i in range(L-1, M-1, -1):
        X[i] = X[i-M]
    for i in range(M-1, -1, -1):
        X[i] = frame.EX[M-1-i]
    
    for i in range(L):
        Z[i] = X[i] * C[i]
    
    for i in range(M2):
        for k in range(5):
            Y[i] += Z[i+k*8]

    for i in range(M):
        for k in range(M2):
            W[i][k] = np.cos((i+0.5)*(k-2)*np.pi/M)
            S[i] += W[i][k] * Y[k]

    if debug:
        #print "EX:", frame.EX
        print "X:", X
        # print "Z:"
        # print "Y:", Y
        # print "W:", W
        print "S:", S

    for sb in range(M):
        frame.sb_sample[blk][ch][sb] = S[sb]


def sbc_encode(frame,debug):
    if frame.nr_subbands == 4:
        proto_table = Proto_4_40
    elif frame.nr_subbands == 8:
        proto_table = Proto_8_80
    else:
        return -1
     
    frame.sb_sample = np.ndarray(shape=(frame.nr_blocks, frame.nr_channels, frame.nr_subbands))
    index = 0
    for ch in range(frame.nr_channels):
        for blk in range(frame.nr_blocks):
            for sb in range(frame.nr_subbands):
                frame.EX[sb] = frame.pcm[index] 
                index+=1
            sbc_analyse(frame, ch, blk, proto_table,debug)
    sbc_quantization(frame)


def should_use_joint_coding(frame):
    # TODO: implement this
    return False

def calculate_scalefactor(max_subbandsample):
    x = 0
    while True:
        y = 1 << x + 1
        if y > max_subbandsample:
            break
        x += 1
    return (x,y)


def frame_to_bitstream(frame):
    global bitstream, bitstream_bits_available
    init_bitstream()

    add_bits(frame.syncword, 8)
    add_bits(frame.sampling_frequency, 2)
    add_bits(frame.nr_blocks/4-1, 2)
    add_bits(frame.channel_mode, 2)
    add_bits(frame.allocation_method, 1)
    add_bits(frame.nr_subbands/4-1, 1)
    add_bits(frame.bitpool, 8)
    add_bits(frame.crc_check, 8)

    for sb in range(frame.nr_subbands):
        add_bits(frame.join[sb],1)
    
    for ch in range(frame.nr_channels):
        for sb in range(frame.nr_subbands):
            add_bits(frame.scale_factor[ch][sb], 4)

    for blk in range(frame.nr_blocks):
        for ch in range(frame.nr_channels):
            for sb in range(frame.nr_subbands):
                add_bits(frame.audio_sample[blk][ch][sb], frame.bits[ch][sb])

    return bitstream

def sbc_quantization(frame):
    frame.join = np.zeros(frame.nr_subbands, dtype = np.uint8)
    if should_use_joint_coding(frame):
        return

    max_subbandsample = np.zeros(shape = (frame.nr_channels, frame.nr_subbands))

    for blk in range(frame.nr_blocks):
        for ch in range(frame.nr_channels):
            for sb in range(frame.nr_subbands):
                m = abs(frame.sb_sample[blk][ch][sb])
                if max_subbandsample[ch][sb] < m:
                    max_subbandsample[ch][sb] = m


    for ch in range(frame.nr_channels):
        for sb in range(frame.nr_subbands):
            (frame.scale_factor[ch][sb], frame.scalefactor[ch][sb]) = calculate_scalefactor(max_subbandsample[ch][sb])
        
    frame.bits = sbc_bit_allocation(frame)
    
    # Reconstruct the Audio Samples
    frame.levels = np.zeros(shape=(frame.nr_channels, frame.nr_subbands), dtype = np.int32)
    for ch in range(frame.nr_channels):
        for sb in range(frame.nr_subbands):
            frame.levels[ch][sb] = pow(2.0, frame.bits[ch][sb]) - 1

    frame.syncword = 156
    frame.crc_check = calculate_crc(frame)
    
    for blk in range(frame.nr_blocks):
        for ch in range(frame.nr_channels):
            for sb in range(frame.nr_subbands):
                if frame.levels[ch][sb] > 0:
                    SB = frame.sb_sample[blk][ch][sb]
                    SF = frame.scalefactor[ch][sb]
                    L  = frame.levels[ch][sb] 
                
                    frame.audio_sample[blk][ch][sb] = np.uint16(((SB * L / SF    + L) - 1.0)/2.0)
                else:
                    frame.audio_sample[blk][ch][sb] = 0 


    return 0

if __name__ == "__main__":
    usage = '''
    Usage: ./sbc_encoder.py input.wav block_size nr_subbands bitpool
    '''
    nr_blocks = 0
    nr_subbands = 0

    if (len(sys.argv) < 5):
        print(usage)
        sys.exit(1)
    try:
        infile = sys.argv[1]
        if not infile.endswith('.wav'):
            print(usage)
            sys.exit(1)
        nr_blocks = int(sys.argv[2])
        nr_subbands = int(sys.argv[3])
        bitpool = int(sys.argv[4])      
        sbcfile = infile.replace('.wav', '-encoded.sbc')

        fin = wave.open(infile, 'rb')
        
        wav_nr_channels = fin.getnchannels()
        wav_sample_rate = fin.getframerate()
        wav_nr_frames = fin.getnframes()
        sbc_sampling_frequency = sbc_sampling_frequency_index(wav_sample_rate)

        sbc_frame_count = 0
        audio_frame_count = 0

        while audio_frame_count < wav_nr_frames:
            # if sbc_frame_count % 200 == 0:
            print "== Frame %d ==" % (sbc_frame_count)

            sbc_encoder_frame = SBCFrame(nr_blocks, nr_subbands, wav_nr_channels, sbc_sampling_frequency, bitpool)
            
            wav_nr_audio_frames = sbc_encoder_frame.nr_blocks * sbc_encoder_frame.nr_subbands
            fetch_samples_for_next_sbc_frame(fin, wav_nr_audio_frames, sbc_encoder_frame)
            sbc_encode(sbc_encoder_frame, 1)
            
            # stream = frame_to_bitstream(frame)
            audio_frame_count += wav_nr_audio_frames
            sbc_frame_count += 1

            if sbc_frame_count == 87:
                break;

        # except TypeError:
        #     fin.close()
        #     print "DONE, WAV file %s encoded into SBC file %s ", (infile, sbcfile)

        #channels, num_audio_frames, wav_nr_channels, wav_sample_rate = read_waw_file(wavfile)
        
        
    except IOError as e:
        print(usage)
        sys.exit(1)





