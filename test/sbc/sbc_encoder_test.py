#!/usr/bin/env python3
import numpy as np
import wave
import struct
import sys
from sbc import *
from sbc_encoder import *
from sbc_decoder import *

error = 0.99
max_error = -1


def sbc_compare_subband_samples(frame_count, actual_frame, expected_frame):
    global error, max_error
    for blk in range(actual_frame.nr_blocks):
        for ch in range(actual_frame.nr_channels):
            M = mse(actual_frame.sb_sample[blk][ch], expected_frame.sb_sample[blk][ch])
            if M > max_error:
                max_error = M

            if max_error > error:
                print ("Frame %d: sb_sample error %f (ch %d, blk %d)" % (frame_count, max_error, ch, blk))
                print (actual_frame.sb_sample[blk])
                print (expected_frame.sb_sample[blk])
                return -1
    return 0 

def sbc_compare_audio_frames(frame_count, actual_frame, expected_frame):
    global error, max_error

    for blk in range(actual_frame.nr_blocks):
        for ch in range(actual_frame.nr_channels):
            M = mse(actual_frame.audio_sample[blk][ch], expected_frame.audio_sample[blk][ch])
            if M > max_error:
                max_error = M

        if max_error > error:
            print ("audio_sample error (%d, %f ) " % (frame_count, max_error))
            print (actual_frame.audio_sample[blk])
            print (expected_frame.audio_sample[blk])

            return -1
    return 0 

def sbc_compare_headers(frame_count, actual_frame, expected_frame):
    if actual_frame.syncword != expected_frame.syncword:
        print ("syncword wrong ", actual_frame.syncword)
        return -1

    if actual_frame.sampling_frequency != expected_frame.sampling_frequency:
        print ("sampling_frequency wrong ", actual_frame.sampling_frequency)
        return -1

    if actual_frame.nr_blocks != expected_frame.nr_blocks:
        print ("nr_blocks wrong ", actual_frame.nr_blocks)
        return -1

    if actual_frame.channel_mode != expected_frame.channel_mode:
        print ("channel_mode wrong ", actual_frame.channel_mode)
        return -1

    if actual_frame.nr_channels != expected_frame.nr_channels:
        print ("nr_channels wrong ", actual_frame.nr_channels)
        return -1

    if actual_frame.allocation_method != expected_frame.allocation_method:
        print ("allocation_method wrong ", actual_frame.allocation_method)
        return -1

    if actual_frame.nr_subbands != expected_frame.nr_subbands:
        print ("nr_subbands wrong ", actual_frame.nr_subbands)
        return -1

    if actual_frame.bitpool != expected_frame.bitpool:
        print ("bitpool wrong (E: %d, D: %d)" % (actual_frame.bitpool, expected_frame.bitpool))
        return -1
    
    if  mse(actual_frame.join, expected_frame.join) > 0:
        print ("join error \nE:\n %s \nD:\n %s" % (actual_frame.join, expected_frame.join))
        return -1
    
    
    if  mse(actual_frame.scale_factor, expected_frame.scale_factor) > 0:
        print ("scale_factor error %d \nE:\n %s \nD:\n %s" % (frame_count, actual_frame.scale_factor, expected_frame.scale_factor))
        return -1

    if  mse(actual_frame.scalefactor, expected_frame.scalefactor) > 0:
        print ("scalefactor error %d \nE:\n %s \nD:\n %s" % (frame_count, actual_frame.scalefactor, expected_frame.scalefactor))
        return -1
    
    if  mse(actual_frame.bits, expected_frame.bits) > 0:
        print ("bits error %d \nE:\n %s \nD:\n %s" % (frame_count, actual_frame.bits, expected_frame.bits))
        return -1

    if actual_frame.crc_check != expected_frame.crc_check:
        print ("crc_check wrong (E: %d, D: %d)" % (actual_frame.crc_check, expected_frame.crc_check))
        return -1

    return 0


def get_actual_frame(fin, nr_blocks, nr_subbands, nr_channels, sampling_frequency, bitpool, allocation_method, force_channel_mode):
    actual_frame = SBCFrame(nr_blocks, nr_subbands, nr_channels, sampling_frequency, bitpool, allocation_method)
    fetch_samples_for_next_sbc_frame(fin, actual_frame)
    sbc_encode(actual_frame, force_channel_mode)
    return actual_frame

file_size = 0
def get_expected_frame(fin_expected):
    global file_size
    expected_frame = SBCFrame()
    sbc_unpack_frame(fin_expected, file_size - fin_expected.tell(), expected_frame)
    sbc_reconstruct_subband_samples(expected_frame)
    return expected_frame

usage = '''
Usage:      ./sbc_encoder_test.py encoder_input.wav blocks subbands bitpool allocation_method force_channel_mode encoder_expected_output.sbc
Example:    ./sbc_encoder_test.py fanfare.wav 16 4 31 0 2 fanfare-4sb.sbc
'''

if (len(sys.argv) < 8):
    print(usage)
    sys.exit(1)
try:
    encoder_input_wav = sys.argv[1]
    nr_blocks = int(sys.argv[2])
    nr_subbands = int(sys.argv[3])
    bitpool = int(sys.argv[4])
    allocation_method = int(sys.argv[5])
    encoder_expected_sbc = sys.argv[6]
    force_channel_mode = int(sys.argv[7])
    sampling_frequency = 44100

    if not encoder_input_wav.endswith('.wav'):
        print(usage)
        sys.exit(1)

    if not encoder_expected_sbc.endswith('.sbc'):
        print(usage)
        sys.exit(1)

    fin = wave.open(encoder_input_wav, 'rb')
    nr_channels = fin.getnchannels()
    sampling_frequency = fin.getframerate()
    nr_audio_frames = fin.getnframes()

    fin_expected = open(encoder_expected_sbc, 'rb')
    fin_expected.seek(0,2)
    file_size = fin_expected.tell()
    fin_expected.seek(0,0)
    
    subband_frame_count = 0
    audio_frame_count = 0
    nr_samples = nr_blocks * nr_subbands
    
    while audio_frame_count < nr_audio_frames:
        if subband_frame_count % 200 == 0:
            print("== Frame %d ==" % (subband_frame_count))

        actual_frame = get_actual_frame(fin, nr_blocks, nr_subbands, nr_channels, bitpool, sampling_frequency, allocation_method, force_channel_mode)    
        expected_frame = get_expected_frame(fin_expected)

        err = sbc_compare_headers(subband_frame_count, actual_frame, expected_frame)
        if err < 0:
            exit(1)
        
        err = sbc_compare_audio_frames(subband_frame_count, actual_frame, expected_frame)
        if err < 0:
            exit(1)
        
        audio_frame_count += nr_samples
        subband_frame_count += 1

    print ("Max MSE audio sample error %f" % max_error)
    fin.close()
    fin_expected.close()

except TypeError:
    print ("Max MSE audio sample error %f" % max_error)
    fin.close()
    fin_expected.close()

except IOError:
    print(usage)
    sys.exit(1)





