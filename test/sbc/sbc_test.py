#!/usr/bin/env python
import numpy as np
import wave
import struct
import sys
from sbc import *
from sbc_encoder import *
from sbc_decoder import *

error = 100
max_error = -1

def dump_samples(a,b,nr_blocks):
    for blk in range(nr_blocks):
        print "D%02d %s" %(blk, b[blk][0])
        print "E%02d %s" %(blk, a[blk][0])
        print "F%02d %s" %(blk, a[blk][0] - b[blk][0])
        print 

def mse(a,b):
    count = 1
    for i in a.shape:
        count *= i
    delta = a - b
    sqr = delta ** 2
    # print "count", count
    # print "sqr", sqr
    # print "sum", sqr.sum()
    res = sqr.sum()*1.0/count
    # print "mse", res
    return res
    
def check_equal(encoder, decoder, frame_count):
    global max_error
    M = mse(encoder.sb_sample, decoder.sb_sample)
    # print "sb_sample error", M
    # dump_samples(encoder.sb_sample, decoder.sb_sample, encoder.nr_blocks)
    if M > max_error:
        max_error = M
    print "sb_sample error ", frame_count, max_error, M
    if M > error:
        #print "sb_sample error ", frame_count, M
        #dump_samples(encoder.sb_sample, decoder.sb_sample, encoder.nr_blocks)
        return -1
    
    return

    if mse(encoder.pcm, decoder.pcm) > error:
        print "pcm wrong ", encoder.pcm, decoder.pcm
        return -1

    if encoder.syncword != decoder.syncword:
        print "syncword wrong ", encoder.syncword
        return -1

    if encoder.sampling_frequency != decoder.sampling_frequency:
        print "sampling_frequency wrong ", encoder.sampling_frequency
        return -1

    if encoder.nr_blocks != decoder.nr_blocks:
        print "nr_blocks wrong ", encoder.nr_blocks
        return -1

    if encoder.channel_mode != decoder.channel_mode:
        print "channel_mode wrong ", encoder.channel_mode
        return -1

    if encoder.nr_channels != decoder.nr_channels:
        print "nr_channels wrong ", encoder.nr_channels
        return -1

    if encoder.allocation_method != decoder.allocation_method:
        print "allocation_method wrong ", encoder.allocation_method
        return -1

    if encoder.nr_subbands != decoder.nr_subbands:
        print "nr_subbands wrong ", encoder.nr_subbands
        return -1

    if encoder.bitpool != decoder.bitpool:
        print "bitpool wrong (E: %d, D: %d)" % (encoder.bitpool, decoder.bitpool)
        return -1

    
    if  mse(encoder.join, decoder.join) > 0:
        print "join error \nE:\n %s \nD:\n %s" % (encoder.join, decoder.join)
        return -1
    
    if  mse(encoder.scale_factor, decoder.scale_factor) > 0:
        print "scale_factor error \nE:\n %s \nD:\n %s" % (encoder.scale_factor, decoder.scale_factor)
        return -1

    if  mse(encoder.scalefactor, decoder.scalefactor) > 0:
        print "scalefactor error \nE:\n %s \nD:\n %s" % (encoder.scalefactor, decoder.scalefactor)
        return -1
    
    if  mse(encoder.bits, decoder.bits) > 0:
        print "bits error \nE:\n %s \nD:\n %s" % (encoder.bits, decoder.bits)
        return -1

    if  mse(encoder.levels, decoder.levels) > 0:
        print "levels error \nE:\n %s \nD:\n %s" % (encoder.levels, decoder.levels)
        return -1
    if  mse(encoder.audio_sample, decoder.audio_sample) > error:
        #print "audio_sample diff %s" % (encoder.audio_sample-decoder.audio_sample)
        #print "audio_sample error \nE:\n %s \nD:\n %s" % (encoder.audio_sample, decoder.audio_sample)
        print "audio_sample error"
        dump_samples(encoder.audio_sample, decoder.audio_sample, encoder.nr_blocks)
        return -1
    
    if encoder.crc_check != decoder.crc_check:
        print "crc_check wrong (E: %d, D: %d)" % (encoder.crc_check, decoder.crc_check)
        return -1

    return


usage = '''
Usage: ./sbc_test.py input.sbc
'''

if (len(sys.argv) < 2):
    print(usage)
    sys.exit(1)
try:
    infile = sys.argv[1]
    if not infile.endswith('.sbc'):
        print(usage)
        sys.exit(1)

    with open (infile, 'rb') as fin:
        try:
            frame_count = 0
            while True:
                sbc_decoder_frame = SBCFrame(0,0,0,0,0)
                if frame_count % 200 == 0:
                    print "== Frame %d ==" % (frame_count)

                err = sbc_unpack_frame(fin, sbc_decoder_frame)
                if err:
                    print "error, frame_count: ", frame_count
                    break
                
                sbc_decode(sbc_decoder_frame) # => pcm
                

                sbc_encoder_frame = SBCFrame(   sbc_decoder_frame.nr_blocks, 
                                                sbc_decoder_frame.nr_subbands, 
                                                sbc_decoder_frame.nr_channels, 
                                                sbc_decoder_frame.sampling_frequency,
                                                sbc_decoder_frame.bitpool)
                
                sbc_encoder_frame.pcm = np.array(sbc_decoder_frame.pcm)
                # TODO: join field 
                # TODO: clear memory

                # sbc_encoder_frame.sb_sample = np.array(sbc_decoder_frame.sb_sample)
                # sbc_quantization(sbc_encoder_frame)

                # print "encoder pcm ", sbc_encoder_frame.pcm
                sbc_encode(sbc_encoder_frame,frame_count >86)
                
                # test
                err = check_equal(sbc_encoder_frame, sbc_decoder_frame, frame_count)
                if err:
                    print "error, frames not equal, frame_count: ", frame_count
                    break
                
                # if frame_count == 0:
                #    break
                frame_count += 1
                
        except TypeError:
            print "DONE"
            exit(0) 

except IOError as e:
    print(usage)
    sys.exit(1)





