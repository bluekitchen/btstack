#!/usr/bin/env python3
import numpy as np
import wave
import struct
import sys
import time


# channel mode
MONO = 0
DUAL_CHANNEL = 1
STEREO = 2
JOINT_STEREO = 3
channel_modes = ["MONO", "DUAL CHANNEL", "STEREO", "JOINT STEREO"]

# allocation method
LOUDNESS = 0
SNR = 1
allocation_methods = ["LOUDNESS", "SNR"]

sampling_frequencies = [16000, 32000, 44100, 48000]
nr_blocks = [4, 8, 12, 16]
nr_subbands = [4, 8]

time_ms = lambda: int(round(time.time() * 1000))

def allocation_method_to_str(allocation_method):
    global allocation_methods
    return allocation_methods[allocation_method]

def channel_mode_to_str(channel_mode):
    global channel_modes
    return channel_modes[channel_mode]

def sampling_frequency_to_str(sampling_frequency):
    global sampling_frequencies
    return sampling_frequencies[sampling_frequency]

def sampling_frequency_index(sampling_frequency):
    global sampling_frequencies
    for index, value in enumerate(sampling_frequencies):
        if value == sampling_frequency:
            return index
    return -1

Proto_4_40 = [
    0.00000000E+00, 5.36548976E-04, 1.49188357E-03, 2.73370904E-03,
    3.83720193E-03, 3.89205149E-03, 1.86581691E-03, -3.06012286E-03,
    1.09137620E-02, 2.04385087E-02, 2.88757392E-02, 3.21939290E-02,
    2.58767811E-02, 6.13245186E-03, -2.88217274E-02, -7.76463494E-02,
    1.35593274E-01, 1.94987841E-01, 2.46636662E-01, 2.81828203E-01,
    2.94315332E-01, 2.81828203E-01, 2.46636662E-01, 1.94987841E-01,
    -1.35593274E-01, -7.76463494E-02, -2.88217274E-02, 6.13245186E-03,
    2.58767811E-02, 3.21939290E-02, 2.88757392E-02, 2.04385087E-02,
    -1.09137620E-02, -3.06012286E-03, 1.86581691E-03, 3.89205149E-03,
    3.83720193E-03, 2.73370904E-03, 1.49188357E-03, 5.36548976E-04
]

Proto_8_80 = [
    0.00000000E+00, 1.56575398E-04, 3.43256425E-04, 5.54620202E-04,
    8.23919506E-04, 1.13992507E-03, 1.47640169E-03, 1.78371725E-03,
    2.01182542E-03, 2.10371989E-03, 1.99454554E-03, 1.61656283E-03,
    9.02154502E-04, -1.78805361E-04, -1.64973098E-03, -3.49717454E-03,
    5.65949473E-03, 8.02941163E-03, 1.04584443E-02, 1.27472335E-02,
    1.46525263E-02, 1.59045603E-02, 1.62208471E-02, 1.53184106E-02,
    1.29371806E-02, 8.85757540E-03, 2.92408442E-03, -4.91578024E-03,
    -1.46404076E-02, -2.61098752E-02, -3.90751381E-02, -5.31873032E-02,
    6.79989431E-02, 8.29847578E-02, 9.75753918E-02, 1.11196689E-01,
    1.23264548E-01, 1.33264415E-01, 1.40753505E-01, 1.45389847E-01,
    1.46955068E-01, 1.45389847E-01, 1.40753505E-01, 1.33264415E-01,
    1.23264548E-01, 1.11196689E-01, 9.75753918E-02, 8.29847578E-02,
    -6.79989431E-02, -5.31873032E-02, -3.90751381E-02, -2.61098752E-02,
    -1.46404076E-02, -4.91578024E-03, 2.92408442E-03, 8.85757540E-03,
    1.29371806E-02, 1.53184106E-02, 1.62208471E-02, 1.59045603E-02,
    1.46525263E-02, 1.27472335E-02, 1.04584443E-02, 8.02941163E-03, 
    -5.65949473E-03, -3.49717454E-03, -1.64973098E-03, -1.78805361E-04,
    9.02154502E-04, 1.61656283E-03, 1.99454554E-03, 2.10371989E-03,
    2.01182542E-03, 1.78371725E-03, 1.47640169E-03, 1.13992507E-03,
    8.23919506E-04, 5.54620202E-04, 3.43256425E-04, 1.56575398E-04
]

crc_table = [
    0x00, 0x1D, 0x3A, 0x27, 0x74, 0x69, 0x4E, 0x53,
    0xE8, 0xF5, 0xD2, 0xCF, 0x9C, 0x81, 0xA6, 0xBB,
    0xCD, 0xD0, 0xF7, 0xEA, 0xB9, 0xA4, 0x83, 0x9E,
    0x25, 0x38, 0x1F, 0x02, 0x51, 0x4C, 0x6B, 0x76,
    0x87, 0x9A, 0xBD, 0xA0, 0xF3, 0xEE, 0xC9, 0xD4,
    0x6F, 0x72, 0x55, 0x48, 0x1B, 0x06, 0x21, 0x3C,
    0x4A, 0x57, 0x70, 0x6D, 0x3E, 0x23, 0x04, 0x19,
    0xA2, 0xBF, 0x98, 0x85, 0xD6, 0xCB, 0xEC, 0xF1,
    0x13, 0x0E, 0x29, 0x34, 0x67, 0x7A, 0x5D, 0x40,
    0xFB, 0xE6, 0xC1, 0xDC, 0x8F, 0x92, 0xB5, 0xA8,
    0xDE, 0xC3, 0xE4, 0xF9, 0xAA, 0xB7, 0x90, 0x8D,
    0x36, 0x2B, 0x0C, 0x11, 0x42, 0x5F, 0x78, 0x65,
    0x94, 0x89, 0xAE, 0xB3, 0xE0, 0xFD, 0xDA, 0xC7,
    0x7C, 0x61, 0x46, 0x5B, 0x08, 0x15, 0x32, 0x2F,
    0x59, 0x44, 0x63, 0x7E, 0x2D, 0x30, 0x17, 0x0A,
    0xB1, 0xAC, 0x8B, 0x96, 0xC5, 0xD8, 0xFF, 0xE2,
    0x26, 0x3B, 0x1C, 0x01, 0x52, 0x4F, 0x68, 0x75,
    0xCE, 0xD3, 0xF4, 0xE9, 0xBA, 0xA7, 0x80, 0x9D,
    0xEB, 0xF6, 0xD1, 0xCC, 0x9F, 0x82, 0xA5, 0xB8,
    0x03, 0x1E, 0x39, 0x24, 0x77, 0x6A, 0x4D, 0x50,
    0xA1, 0xBC, 0x9B, 0x86, 0xD5, 0xC8, 0xEF, 0xF2,
    0x49, 0x54, 0x73, 0x6E, 0x3D, 0x20, 0x07, 0x1A,
    0x6C, 0x71, 0x56, 0x4B, 0x18, 0x05, 0x22, 0x3F,
    0x84, 0x99, 0xBE, 0xA3, 0xF0, 0xED, 0xCA, 0xD7,
    0x35, 0x28, 0x0F, 0x12, 0x41, 0x5C, 0x7B, 0x66,
    0xDD, 0xC0, 0xE7, 0xFA, 0xA9, 0xB4, 0x93, 0x8E,
    0xF8, 0xE5, 0xC2, 0xDF, 0x8C, 0x91, 0xB6, 0xAB,
    0x10, 0x0D, 0x2A, 0x37, 0x64, 0x79, 0x5E, 0x43,
    0xB2, 0xAF, 0x88, 0x95, 0xC6, 0xDB, 0xFC, 0xE1,
    0x5A, 0x47, 0x60, 0x7D, 0x2E, 0x33, 0x14, 0x09,
    0x7F, 0x62, 0x45, 0x58, 0x0B, 0x16, 0x31, 0x2C,
    0x97, 0x8A, 0xAD, 0xB0, 0xE3, 0xFE, 0xD9, 0xC4
]
# offset table for 4-subbands
offset4 = np.array([[ -1, 0, 0, 0 ],
                    [ -2, 0, 0, 1 ],
                    [ -2, 0, 0, 1 ],
                    [ -2, 0, 0, 1 ]
                    ])

# offset tables for 8-subbands
offset8 = np.array([[ -2, 0, 0, 0, 0, 0, 0, 1 ],
                    [ -3, 0, 0, 0, 0, 0, 1, 2 ],
                    [ -4, 0, 0, 0, 0, 0, 1, 2 ],
                    [ -4, 0, 0, 0, 0, 0, 1, 2 ]
                    ])

def calculate_scalefactor(max_subbandsample):
    x = 0
    while True:
        y = 1 << (x + 1)
        if y > max_subbandsample:
            break
        x += 1
    return (x,y)


def calculate_max_subbandsample(nr_blocks, nr_channels, nr_subbands, sb_sample):
    max_subbandsample = np.zeros(shape = (nr_channels, nr_subbands))

    for blk in range(nr_blocks):
        for ch in range(nr_channels):
            for sb in range(nr_subbands):
                m = abs(sb_sample[blk][ch][sb])
                if max_subbandsample[ch][sb] < m:
                    max_subbandsample[ch][sb] = m
    return max_subbandsample

def calculate_scalefactors(nr_blocks, nr_channels, nr_subbands, sb_sample):
    scale_factor =  np.zeros(shape=(nr_channels, nr_subbands), dtype = np.int32)
    scalefactor =  np.zeros(shape=(nr_channels, nr_subbands), dtype = np.int32)    

    max_subbandsample = calculate_max_subbandsample(nr_blocks, nr_channels, nr_subbands, sb_sample)
    for ch in range(nr_channels):
        for sb in range(nr_subbands):
            (scale_factor[ch][sb], scalefactor[ch][sb]) = calculate_scalefactor(max_subbandsample[ch][sb])  
    return scale_factor, scalefactor

def calculate_channel_mode_and_scale_factors(frame, force_channel_mode):
    frame.scale_factor, frame.scalefactor = calculate_scalefactors(frame.nr_blocks, frame.nr_channels, frame.nr_subbands, frame.sb_sample)

    if frame.nr_channels == 1:
        frame.channel_mode = MONO
        return

    frame.join = np.zeros(frame.nr_subbands, dtype = np.uint8)
    
    if force_channel_mode == STEREO:
        frame.channel_mode = STEREO
        return

    sb_sample = np.zeros(shape = (frame.nr_blocks,2,frame.nr_subbands), dtype = np.int32)
    for blk in range(frame.nr_blocks):
        for sb in range(frame.nr_subbands):
            sb_sample[blk][0][sb] = (frame.sb_sample[blk][0][sb] + frame.sb_sample[blk][1][sb])/2
            sb_sample[blk][1][sb] = (frame.sb_sample[blk][0][sb] - frame.sb_sample[blk][1][sb])/2

    scale_factor, scalefactor = calculate_scalefactors(frame.nr_blocks, frame.nr_channels, frame.nr_subbands, sb_sample)

    for sb in range(frame.nr_subbands-1):
        suma = frame.scale_factor[0][sb] + frame.scale_factor[1][sb]
        sumb = scale_factor[0][sb] + scale_factor[1][sb]
    
        if suma > sumb or force_channel_mode == JOINT_STEREO:
            frame.channel_mode = JOINT_STEREO
            frame.join[sb] = 1
            
            frame.scale_factor[0][sb] = scale_factor[0][sb]
            frame.scale_factor[1][sb] = scale_factor[1][sb]
            frame.scalefactor[0][sb]  = scalefactor[0][sb]
            frame.scalefactor[1][sb]  = scalefactor[1][sb]

            for blk in range(frame.nr_blocks):
                frame.sb_sample[blk][0][sb] = sb_sample[blk][0][sb]
                frame.sb_sample[blk][1][sb] = sb_sample[blk][1][sb]
                

class SBCFrame:
    syncword = 0
    sampling_frequency = 0
    nr_blocks = 0
    channel_mode = 0
    nr_channels = 0
    allocation_method = 0
    nr_subbands = 0
    bitpool = 0
    crc_check = 0
    reserved_for_future_use = 0
    # pro subband - 1
    join = np.zeros(8, dtype = np.uint8)
    scale_factor =  np.zeros(shape=(2, 8), dtype = np.int32)
    scalefactor =  np.zeros(shape=(2, 8), dtype = np.int32)
    audio_sample = np.zeros(shape = (16,2,8), dtype = np.uint16)
    sb_sample = np.zeros(shape = (16,2,8), dtype = np.int32)
    X = np.zeros(8, dtype = np.int16)
    EX = np.zeros(8)
    pcm = np.zeros(shape=(2, 8*16), dtype = np.int16)
    bits    = np.zeros(shape=(2, 8))
    levels = np.zeros(shape=(2, 8), dtype = np.int32)


    def __init__(self, nr_blocks=16, nr_subbands=4, nr_channels=1, bitpool=31, sampling_frequency=44100, allocation_method = 0, force_channel_mode = 0):
        self.nr_blocks = nr_blocks
        self.nr_subbands = nr_subbands
        self.nr_channels = nr_channels
        self.sampling_frequency = sampling_frequency_index(sampling_frequency)
        self.bitpool = bitpool
        self.allocation_method = allocation_method
        self.init(nr_blocks, nr_subbands, nr_channels)
        self.channel_mode = force_channel_mode
        return
    
    def init(self, nr_blocks, nr_subbands, nr_channels):
        self.scale_factor = np.zeros(shape=(nr_channels, nr_subbands), dtype = np.int32)
        self.scalefactor = np.zeros(shape=(nr_channels, nr_subbands), dtype = np.int32)
        self.audio_sample = np.zeros(shape=(nr_blocks, nr_channels, nr_subbands), dtype = np.uint16)
        self.sb_sample = np.zeros(shape=(nr_blocks, nr_channels, nr_subbands), dtype = np.int32)
        self.levels = np.zeros(shape=(nr_channels, nr_subbands), dtype = np.int32)
        self.pcm = np.zeros(shape=(nr_channels, nr_subbands*nr_blocks), dtype = np.int16)
        self.join = np.zeros(nr_subbands, dtype = np.uint8)
        self.X = np.zeros(nr_subbands, dtype = np.int16)
        self.EX = np.zeros(nr_subbands)

    def dump_audio_samples(self, blk, ch):
        print(self.audio_sample[blk][ch])

    def dump_subband_samples(self, blk, ch):
        print(self.sb_sample[blk][ch])

    def dump_state(self):
        res =  "SBCFrameHeader state:"
        res += "\n - nr channels %d" % self.nr_channels
        res += "\n - nr blocks %d" % self.nr_blocks
        res += "\n - nr subbands %d" % self.nr_subbands
        res += "\n - scale factors: %s" % self.scale_factor
        res += "\n - levels: %s" % self.levels
        res += "\n - join: %s" % self.join
        res += "\n - bits: %s" % self.bits
        print(res)

    def __str__(self):
        res =  "SBCFrameHeader:"
        res += "\n - syncword %x" % self.syncword
        res += "\n - sampling frequency %d Hz" % sampling_frequency_to_str(self.sampling_frequency)
        
        res += "\n - nr channels %d" % self.nr_channels
        res += "\n - nr blocks %d" % self.nr_blocks
        res += "\n - nr subbands %d" % self.nr_subbands
        
        res += "\n - channel mode %s" % channel_mode_to_str(self.channel_mode)
        res += "\n - allocation method %s" % allocation_method_to_str(self.allocation_method)

        res += "\n - bitpool %d" % self.bitpool
        res += "\n - crc check %x" % self.crc_check
        return res


def sbc_bit_allocation_stereo_joint(frame):
    bitneed = np.zeros(shape=(frame.nr_channels, frame.nr_subbands), dtype = np.int32)
    bits    = np.zeros(shape=(frame.nr_channels, frame.nr_subbands), dtype = np.int32)

    loudness = 0

    if frame.allocation_method == SNR:
        for ch in range(frame.nr_channels):
            for sb in range(frame.nr_subbands):
                bitneed[ch][sb] = frame.scale_factor[ch][sb]
    else:
        for ch in range(frame.nr_channels):
            for sb in range(frame.nr_subbands):
                if frame.scale_factor[ch][sb] == 0:
                    bitneed[ch][sb] = -5
                else:
                    if frame.nr_subbands == 4:
                        loudness = frame.scale_factor[ch][sb] - offset4[frame.sampling_frequency][sb]
                    else:
                        loudness = frame.scale_factor[ch][sb] - offset8[frame.sampling_frequency][sb]
                        
                    if loudness > 0:
                        bitneed[ch][sb] = loudness/2
                    else:
                        bitneed[ch][sb] = loudness

    # search the maximum bitneed index
    max_bitneed = 0
    for ch in range(frame.nr_channels):
        for sb in range(frame.nr_subbands):
            if bitneed[ch][sb] > max_bitneed:
                max_bitneed = bitneed[ch][sb]
        
    
    # calculate how many bitslices fit into the bitpool
    bitcount = 0
    slicecount = 0
    bitslice = max_bitneed + 1 #/* init just above the largest sf */

    while True:
        bitslice -= 1
        bitcount += slicecount
        slicecount = 0
        for ch in range(frame.nr_channels):
            for sb in range(frame.nr_subbands):
                if (bitneed[ch][sb] > bitslice+1) and (bitneed[ch][sb] < bitslice+16):
                    slicecount += 1
                elif bitneed[ch][sb] == bitslice + 1:
                    slicecount += 2
        if bitcount + slicecount >= frame.bitpool:
            break 

    if bitcount + slicecount == frame.bitpool:
        bitcount += slicecount
        bitslice -= 1
    
    # bits are distributed until the last bitslice is reached
    for ch in range(frame.nr_channels):
        for sb in range(frame.nr_subbands):
            if bitneed[ch][sb] < bitslice+2:
                bits[ch][sb]=0
            else:
                bits[ch][sb] = min(bitneed[ch][sb]-bitslice,16)

    ch = 0
    sb = 0
    while bitcount < frame.bitpool and sb < frame.nr_subbands:
        if bits[ch][sb] >= 2 and bits[ch][sb] < 16:
            bits[ch][sb] += 1
            bitcount += 1            
        elif (bitneed[ch][sb] == bitslice+1) and (frame.bitpool > bitcount+1):
            bits[ch][sb] = 2
            bitcount += 2     
        if ch == 1:
            ch = 0
            sb += 1
        else:
            ch = 1

        
    ch = 0  
    sb = 0
    while bitcount < frame.bitpool and sb < frame.nr_subbands:
        if bits[ch][sb] < 16:
            bits[ch][sb]+=1
            bitcount+=1
        if ch == 1:
            ch = 0
            sb += 1
        else:
            ch = 1

    
    if bits.sum() != frame.bitpool:
        print("bit allocation failed, bitpool %d, allocated %d" % (bits.sum() , frame.bitpool))
        exit(1)
    return bits


def sbc_bit_allocation_mono_dual(frame):
    #print("Bit allocation for mono/dual channel" )
    bitneed = np.zeros(shape=(frame.nr_channels, frame.nr_subbands), dtype = np.int32)
    bits    = np.zeros(shape=(frame.nr_channels, frame.nr_subbands), dtype = np.int32)
    loudness = 0

    for ch in range(frame.nr_channels):
        # bitneed values are derived from the scale factors
        if frame.allocation_method == SNR:
            for sb in range(frame.nr_subbands):
                bitneed[ch][sb] = frame.scale_factor[ch][sb]
        else: 
            for sb in range(frame.nr_subbands):
                if frame.scale_factor[ch][sb] == 0:
                    bitneed[ch][sb] = -5
                else:
                    if frame.nr_subbands == 4:
                        loudness = frame.scale_factor[ch][sb] - offset4[frame.sampling_frequency][sb]
                    else:
                        loudness = frame.scale_factor[ch][sb] - offset8[frame.sampling_frequency][sb]
                    if loudness > 0:
                        bitneed[ch][sb] = loudness/2
                    else:
                        bitneed[ch][sb] = loudness

        # search the maximum bitneed index
        max_bitneed = 0
        for sb in range(frame.nr_subbands):
            if bitneed[ch][sb] > max_bitneed:
                max_bitneed = bitneed[ch][sb]
        
        # calculate how many bitslices fit into the bitpool
        bitcount = 0
        slicecount = 0
        bitslice = max_bitneed + 1

        while True:
            bitslice = bitslice - 1
            bitcount = bitcount + slicecount
            slicecount = 0
            for sb in range(frame.nr_subbands):
                if (bitneed[ch][sb] > bitslice+1) and (bitneed[ch][sb] < bitslice+16):
                    slicecount = slicecount + 1
                elif bitneed[ch][sb] == bitslice + 1:
                    slicecount = slicecount + 2
            if bitcount + slicecount >= frame.bitpool:
                break 

        if bitcount + slicecount == frame.bitpool:
            bitcount = bitcount + slicecount
            bitslice = bitslice - 1
        
        for sb in range(frame.nr_subbands):
            if bitneed[ch][sb] < bitslice+2 :
               bits[ch][sb]=0;
            else:
                bits[ch][sb] = min(bitneed[ch][sb]-bitslice,16)

        sb = 0
        while bitcount < frame.bitpool and sb < frame.nr_subbands:
            if bits[ch][sb] >= 2 and bits[ch][sb] < 16:
                   bits[ch][sb] = bits[ch][sb] + 1
                   bitcount = bitcount + 1
                
            elif (bitneed[ch][sb] == bitslice+1) and (frame.bitpool > bitcount+1):
                bits[ch][sb] = 2
                bitcount += 2
            
            sb = sb + 1

        sb = 0
        while bitcount < frame.bitpool and sb < frame.nr_subbands:
            if bits[ch][sb] < 16:
                bits[ch][sb] = bits[ch][sb] + 1
                bitcount = bitcount + 1
            sb = sb + 1 

    return bits

def sbc_bit_allocation(frame):
    if frame.channel_mode == MONO or frame.channel_mode == DUAL_CHANNEL:
        return sbc_bit_allocation_mono_dual(frame)
    elif frame.channel_mode == STEREO or frame.channel_mode == JOINT_STEREO:
        return sbc_bit_allocation_stereo_joint(frame)
    else:
        print("Wrong channel mode ", frame.channel_mode)
        return -1

def sbc_sampling_frequency_index(sample_rate):
    sbc_sampling_frequency_index = 0
    for i in range(len(sampling_frequency)):
        if sample_rate == sampling_frequency[i]:
            sbc_sampling_frequency_index = i
            break 
    return sbc_sampling_frequency_index


def sbc_crc8(data, data_len):
    crc = 0x0f
    j = 0
    for i in range(data_len / 8):
        crc = crc_table[crc ^ data[i]]
        j = i + 1
        
    bits_left = data_len%8
    if bits_left:
        octet = data[j]
        for i in range(data_len%8):
            bit = ((octet ^ crc) & 0x80) >> 7
            if bit:
                bit = 0x1d
            crc = ((crc & 0x7f) << 1) ^ bit
            octet = octet << 1 
    return crc


bitstream = None
bitstream_index = -1
bitstream_bits_available = 0

def init_bitstream():
    global bitstream, bitstream_bits_available, bitstream_index
    bitstream = []
    bitstream_index = -1
    bitstream_bits_available = 0

def add_bit(bit):
    global bitstream, bitstream_bits_available, bitstream_index
    if bitstream_bits_available == 0:
        bitstream.append(0)
        bitstream_bits_available = 8
        bitstream_index += 1

    bitstream[bitstream_index] |= bit << (bitstream_bits_available - 1)
    bitstream_bits_available -= 1


def add_bits(bits, len):
    global bitstream, bitstream_bits_available
    for i in range(len):
        add_bit((bits >> (len-1-i)) & 1)

ibuffer = None
ibuffer_count = 0

def get_bit(fin):
    global ibuffer, ibuffer_count
    if ibuffer_count == 0:
        ibuffer = ord(fin.read(1))
        ibuffer_count = 8
        
    bit = (ibuffer >> 7) & 1
    ibuffer = ibuffer << 1
    ibuffer_count = ibuffer_count - 1
    return bit

def drop_remaining_bits():
    global ibuffer_count
    #print("dropping %d bits" % ibuffer_count)
    ibuffer_count = 0

def get_bits(fin, bit_count):
    bits = 0
    for i in range(bit_count):
        bits = (bits << 1) | get_bit(fin)
    # print("get bits: %d -> %02x" %(bit_count, bits))
    return bits


def calculate_crc(frame):
    global bitstream, bitstream_bits_available, bitstream_index
    init_bitstream()
    
    add_bits(frame.sampling_frequency, 2)
    add_bits(frame.nr_blocks/4-1, 2)
    add_bits(frame.channel_mode, 2)
    add_bits(frame.allocation_method, 1)
    add_bits(frame.nr_subbands/4-1, 1)
    add_bits(frame.bitpool, 8)
    if frame.channel_mode == JOINT_STEREO:
        for sb in range(frame.nr_subbands):
            add_bits(frame.join[sb],1)

    for ch in range(frame.nr_channels):
        for sb in range(frame.nr_subbands):
            add_bits(frame.scale_factor[ch][sb], 4)
    
    bitstream_len = (bitstream_index + 1) * 8
    if bitstream_bits_available:
        bitstream_len -= bitstream_bits_available
    
    return sbc_crc8(bitstream, bitstream_len)

def calculate_crc_mSBC(frame):
    init_bitstream()
    add_bits(frame.reserved_for_future_use, 16)
    for sb in range(8):
        add_bits(frame.scale_factor[0][sb], 4)
    bitstream_len = (bitstream_index + 1) * 8
    if bitstream_bits_available:
        bitstream_len -= bitstream_bits_available
    return sbc_crc8(bitstream, bitstream_len)

def frame_to_bitstream(frame):
    global bitstream, bitstream_bits_available, bitstream_index
    init_bitstream()

    add_bits(frame.syncword, 8)
    add_bits(frame.sampling_frequency, 2)
    add_bits(frame.nr_blocks/4-1, 2)
    add_bits(frame.channel_mode, 2)
    add_bits(frame.allocation_method, 1)
    add_bits(frame.nr_subbands/4-1, 1)
    add_bits(frame.bitpool, 8)
    add_bits(frame.crc_check, 8)

    if frame.channel_mode == JOINT_STEREO:
        for sb in range(frame.nr_subbands-1):
            add_bits(frame.join[sb],1)
        add_bits(0,1)

    for ch in range(frame.nr_channels):
        for sb in range(frame.nr_subbands):
            add_bits(frame.scale_factor[ch][sb], 4)
    
    for blk in range(frame.nr_blocks):
        for ch in range(frame.nr_channels):
            for sb in range(frame.nr_subbands):
                add_bits(frame.audio_sample[blk][ch][sb], frame.bits[ch][sb])

    bitstream_bits_available = 0
    return bitstream

def mse(a,b):
    count = 1
    for i in a.shape:
        count *= i
    delta = a - b
    sqr = delta ** 2
    res = sqr.sum()*1.0/count
    # res = ((a - b) ** 2).mean()
    return res
