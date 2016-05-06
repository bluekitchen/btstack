#!/usr/bin/env python
import numpy as np
import wave
import struct
import sys
from sbc import *

X = np.zeros(80, dtype = np.int16)


def fetch_samples_for_next_sbc_frame(fin, frame):
    nr_audio_frames = frame.nr_blocks * frame.nr_subbands
    raw_data = fin.readframes(nr_audio_frames) # Returns byte data
    
    total_samples = nr_audio_frames * frame.nr_channels
    len_raw_data =  len(raw_data) / 2

    padding = np.zeros(total_samples - len_raw_data, dtype=np.int16)

    fmt = "%ih" % len_raw_data # read signed 2 byte shorts

    frame.pcm =  np.concatenate([np.array(struct.unpack(fmt, raw_data)), padding]) 
    del raw_data

    
def sbc_frame_analysis(frame, ch, blk, C):
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

    for sb in range(M):
        frame.sb_sample[blk][ch][sb] = S[sb]

def sbc_analysis(frame):
    if frame.nr_subbands == 4:
        C = Proto_4_40
    elif frame.nr_subbands == 8:
        C = Proto_8_80
    else:
        return -1

    frame.sb_sample = np.ndarray(shape=(frame.nr_blocks, frame.nr_channels, frame.nr_subbands))
    index = 0
    for ch in range(frame.nr_channels):
        for blk in range(frame.nr_blocks):
            for sb in range(frame.nr_subbands):
                frame.EX[sb] = np.int16(frame.pcm[index]) 
                index+=1
            sbc_frame_analysis(frame, ch, blk, C)
    return 0

def sbc_encode(frame):
    err = sbc_analysis(frame)
    if err >= 0:
        err = sbc_quantization(frame)
    return err

def calculate_joint_stereo_signal(frame):
    sb_sample = np.zeros(shape = (frame.nr_blocks,frame.nr_channels,frame.nr_subbands), dtype = np.uint32)
    scale_factor = np.zeros(shape=(frame.nr_channels, frame.nr_subbands), dtype = np.int32)
    scalefactor = np.zeros(shape=(frame.nr_channels, frame.nr_subbands), dtype = np.int32)
    
    for sb in range(frame.nr_subbands-1):
        for blk in range(frame.nr_blocks):
             sb_sample[blk][0][sb] = (frame.sb_sample_f[blk][0][sb] +  frame.sb_sample_f[blk][1][sb]) >> 1
             sb_sample[blk][1][sb] = (frame.sb_sample_f[blk][0][sb] -  frame.sb_sample_f[blk][1][sb]) >> 1

    for ch in range(frame.nr_channels):
        for sb in range(frame.nr_subbands-1):
            frame.scale_factor[ch][sb] = 0
            frame.scalefactor[ch][sb] = 2
            for blk in range(frame.nr_blocks):
                while frame.scalefactor[ch][sb] < abs(frame.sb_sample[blk][ch][sb]):
                    frame.scale_factor[ch][sb]+=1
                    frame.scalefactor[ch][sb] *= 2

    for sb in range(frame.nr_subbands-1):
        if (frame.scalefactor[0][sb] + frame.scalefactor[1][sb]) > (scalefactor[0][sb] + scalefactor[1][sb]):
            frame.join[sb] = 1
            frame.scale_factor[0][sb] = scale_factor[0][sb]
            frame.scale_factor[1][sb] = scale_factor[1][sb]
            frame.scalefactor[0][sb]  = scalefactor[0][sb]
            frame.scalefactor[1][sb]  = scalefactor[1][sb]
            for blk in range(frame.nr_blocks):
                frame.sb_sample[blk][0][sb] = sb_sample[blk][0][sb]
                frame.sb_sample[blk][1][sb] = sb_sample[blk][1][sb]

def calculate_scalefactor(max_subbandsample):
    x = 0
    while True:
        y = 1 << x + 1
        if y > max_subbandsample:
            break
        x += 1
    return (x,y)


def sbc_quantization(frame):
    max_subbandsample = np.zeros(shape = (frame.nr_channels, frame.nr_subbands))

    for blk in range(frame.nr_blocks):
        for ch in range(frame.nr_channels):
            for sb in range(frame.nr_subbands):
                m = abs(frame.sb_sample[blk][ch][sb])
                if max_subbandsample[ch][sb] < m:
                    max_subbandsample[ch][sb] = m


    for ch in range(frame.nr_channels):
        for sb in range(frame.nr_subbands):
            frame.scale_factor[ch][sb] = 0
            frame.scalefactor[ch][sb] = 2
            for blk in range(frame.nr_blocks):
                while frame.scalefactor[ch][sb] < abs(frame.sb_sample[blk][ch][sb]):
                    frame.scale_factor[ch][sb]+=1
                    frame.scalefactor[ch][sb] *= 2

            #(frame.scale_factor[ch][sb], frame.scalefactor[ch][sb]) = calculate_scalefactor(max_subbandsample[ch][sb])
        
    frame.bits = sbc_bit_allocation(frame)
    
    # Reconstruct the Audio Samples
    frame.levels = np.zeros(shape=(frame.nr_channels, frame.nr_subbands), dtype = np.int32)
    for ch in range(frame.nr_channels):
        for sb in range(frame.nr_subbands):
            frame.levels[ch][sb] = (1 << frame.bits[ch][sb]) - 1 #pow(2.0, frame.bits[ch][sb]) - 1

    frame.syncword = 156
    frame.crc_check = calculate_crc(frame)
    
    frame.join = np.zeros(frame.nr_subbands, dtype = np.uint8)
    if frame.channel_mode == JOINT_STEREO:
        calculate_joint_stereo_signal(frame)

    for blk in range(frame.nr_blocks):
        for ch in range(frame.nr_channels):
            for sb in range(frame.nr_subbands):
                if frame.levels[ch][sb] > 0:
                    SB = frame.sb_sample[blk][ch][sb]
                    L  = frame.levels[ch][sb]             
                    SF = frame.scalefactor[ch][sb]
                    frame.audio_sample[blk][ch][sb] = np.uint16(((SB * L / SF    + L) - 1.0)/2.0)
                else:
                    frame.audio_sample[blk][ch][sb] = 0 

    return 0

def sbc_write_frame(fout, sbc_encoder_frame):
    stream = frame_to_bitstream(sbc_encoder_frame)
    barray = bytearray(stream)
    fout.write(barray)

if __name__ == "__main__":
    usage = '''
    Usage:      ./sbc_encoder.py input.wav blocks subbands bitpool
    Example:    ./sbc_encoder.py fanfare.wav 16 4 31
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
        sbcfile = infile.replace('.wav', '-encoded.sbc')

        nr_blocks = int(sys.argv[2])
        nr_subbands = int(sys.argv[3])
        bitpool = int(sys.argv[4])      

        fin = wave.open(infile, 'rb')
        nr_channels = fin.getnchannels()
        sampling_frequency = fin.getframerate()
        nr_audio_frames = fin.getnframes()

        subband_frame_count = 0
        audio_frame_count = 0
        nr_samples = nr_blocks * nr_subbands
        fout = open(sbcfile, 'wb')
        while audio_frame_count < nr_audio_frames:
            if subband_frame_count % 200 == 0:
                print("== Frame %d ==" % (subband_frame_count))

            sbc_encoder_frame = SBCFrame(nr_blocks, nr_subbands, nr_channels, bitpool, sampling_frequency)
            fetch_samples_for_next_sbc_frame(fin, sbc_encoder_frame)
            
            sbc_encode(sbc_encoder_frame)
            sbc_write_frame(fout, sbc_encoder_frame)

            audio_frame_count += nr_samples
            subband_frame_count += 1

        fin.close()
        fout.close()
        print("DONE, WAV file %s encoded into SBC file %s " % (infile, sbcfile))
        
        
    except IOError as e:
        print(usage)
        sys.exit(1)





