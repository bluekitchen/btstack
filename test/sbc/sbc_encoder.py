#!/usr/bin/env python3
import numpy as np
import wave
import struct
import sys
from sbc import *

X = np.zeros(shape=(2,80), dtype = np.int16)
implementation = "SIG"
msbc_enabled = 0
total_time_ms = 0

def fetch_samples_for_next_sbc_frame(fin, frame):
    raw_data = fin.readframes(frame.nr_blocks * frame.nr_subbands) 
    fmt = "%ih" % (len(raw_data) / 2)
    data = struct.unpack(fmt, raw_data) 

    if frame.nr_channels == 2:
        for i in range(len(data)/2):
            frame.pcm[0][i] = data[2*i]
            frame.pcm[1][i] = data[2*i+1]
    else:
        for i in range(len(data)):
            frame.pcm[0][i] = data[i]

    
def sbc_frame_analysis_sig(frame, ch, blk, C):
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
        X[ch][i] = X[ch][i-M]
    for i in range(M-1, -1, -1):
        X[ch][i] = frame.EX[M-1-i]
    for i in range(L):
        Z[i] = X[ch][i] * C[i]
    
    for i in range(M2):
        for k in range(5):
            Y[i] += Z[i+k*M2]

    for i in range(M):
        for k in range(M2):
            W[i][k] = np.cos((i+0.5)*(k-M/2)*np.pi/M)
            S[i] += W[i][k] * Y[k]

    for sb in range(M):
        frame.sb_sample[blk][ch][sb] = S[sb]


def sbc_frame_analysis(frame, ch, blk, proto_table):
    global total_time_ms, implementation

    t1 = time_ms()
    if implementation == "SIG":
         sbc_frame_analysis_sig(frame, ch, blk, proto_table)
    else:
        print ("Analysis %s not implemented" % implementation)
        exit(1)

    t2 = time_ms()
    total_time_ms += t2-t1


def sbc_analysis(frame):
    if frame.nr_subbands == 4:
        C = Proto_4_40
    elif frame.nr_subbands == 8:
        C = Proto_8_80
    else:
        return -1

    frame.sb_sample = np.ndarray(shape=(frame.nr_blocks, frame.nr_channels, frame.nr_subbands))
    for ch in range(frame.nr_channels):
        index = 0
        for blk in range(frame.nr_blocks):
            for sb in range(frame.nr_subbands):
                frame.EX[sb] = frame.pcm[ch][index]
                index+=1
            sbc_frame_analysis(frame, ch, blk, C)
    return 0

def sbc_encode(frame, force_channel_mode):
    err = sbc_analysis(frame)
    if err >= 0:
        err = sbc_quantization(frame, force_channel_mode)
    return err

def sbc_quantization(frame, force_channel_mode):
    global msbc_enabled
    calculate_channel_mode_and_scale_factors(frame, force_channel_mode)
    frame.bits = sbc_bit_allocation(frame)
    
    # Reconstruct the Audio Samples
    frame.levels = np.zeros(shape=(frame.nr_channels, frame.nr_subbands), dtype = np.int32)
    for ch in range(frame.nr_channels):
        for sb in range(frame.nr_subbands):
            frame.levels[ch][sb] = (1 << frame.bits[ch][sb]) - 1 #pow(2.0, frame.bits[ch][sb]) - 1

    if msbc_enabled:
        frame.syncword = 0xad
    else:
        frame.syncword = 0x9c
        
    frame.crc_check = calculate_crc(frame)

    
    for blk in range(frame.nr_blocks):
        for ch in range(frame.nr_channels):
            for sb in range(frame.nr_subbands):
                if frame.levels[ch][sb] > 0:
                    SB = frame.sb_sample[blk][ch][sb]
                    L  = frame.levels[ch][sb]
                    SF = frame.scalefactor[ch][sb]    
                    frame.audio_sample[blk][ch][sb] = np.uint16(((SB * L / SF + L) - 1.0)/2.0)
                else:
                    frame.audio_sample[blk][ch][sb] = 0 

    return 0

def sbc_write_frame(fout, sbc_encoder_frame):
    stream = frame_to_bitstream(sbc_encoder_frame)
    barray = bytearray(stream)
    fout.write(barray)

if __name__ == "__main__":
    usage = '''
    Usage:      ./sbc_encoder.py input.wav blocks subbands bitpool allocation_method[0-LOUDNESS,1-SNR] force_channel_mode[2-STEREO,3-JOINT_STEREO] [0-sbc|1-msbc]
    Example:    ./sbc_encoder.py fanfare.wav 16 4 31 0 0
    '''
    nr_blocks = 0
    nr_subbands = 0
    
    if (len(sys.argv) < 7):
        print(usage)
        sys.exit(1)
    try:
        infile = sys.argv[1]
        if not infile.endswith('.wav'):
            print(usage)
            sys.exit(1)
        
        msbc_enabled = int(sys.argv[7]) 
        print("msbc_enabled %d"%msbc_enabled)
        if msbc_enabled:
            sbcfile = infile.replace('.wav', '-encoded.msbc')
        else:
            sbcfile = infile.replace('.wav', '-encoded.sbc')
            
        nr_blocks = int(sys.argv[2])
        nr_subbands = int(sys.argv[3])
        bitpool = int(sys.argv[4])
        allocation_method = int(sys.argv[5]) 
        
        force_channel_mode = int(sys.argv[6]) 
        print("force_channel_mode %d"%force_channel_mode)


        fin = wave.open(infile, 'rb')
        nr_channels = fin.getnchannels()
        if msbc_enabled:
            sampling_frequency = 16000
            nr_channels = 1
            bitpool = 26
            nr_subbands = 8
            allocation_method = 0
            force_channel_mode = 0
        else:
            sampling_frequency = fin.getframerate()
            nr_channels = fin.getnchannels()
        nr_audio_frames = fin.getnframes()

        subband_frame_count = 0
        audio_frame_count = 0
        nr_samples = nr_blocks * nr_subbands
        fout = open(sbcfile, 'wb')
        while audio_frame_count < nr_audio_frames:
            if subband_frame_count % 200 == 0:
                print("== Frame %d == "  % (subband_frame_count))

            sbc_encoder_frame = SBCFrame(nr_blocks, nr_subbands, nr_channels, bitpool, sampling_frequency, allocation_method, force_channel_mode)
            
            if subband_frame_count == 0:
                print (sbc_encoder_frame)
            fetch_samples_for_next_sbc_frame(fin, sbc_encoder_frame)
            
            sbc_encode(sbc_encoder_frame, force_channel_mode)
            sbc_write_frame(fout, sbc_encoder_frame)

            audio_frame_count += nr_samples
            subband_frame_count += 1

        fin.close()
        fout.close()
        print("DONE, WAV file %s encoded into SBC file %s " % (infile, sbcfile))
        if subband_frame_count > 0:
            print ("Average analysis time per frame: %d ms/frame" % (total_time_ms/subband_frame_count))
        else:
            print ("No frame found")

        
    except IOError as e:
        print(usage)
        sys.exit(1)





