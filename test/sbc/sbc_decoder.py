#!/usr/bin/env python
import numpy as np
import wave
import struct
import sys

# channel mode
MONO = 0
DUAL_CHANNEL = 1
STEREO = 2
JOINT_STEREO = 3

# allocation method
LOUDNESS = 0
SNR = 1

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

nr_blocks = [4, 8, 12, 16]
nr_subbands = [4, 8]
sampling_frequency =[16000, 32000, 44100, 48000]

ibuffer = None
ibuffer_count = 0

sb_sample = np.ndarray(shape=(16, 2, 8), dtype = np.int32)
V = np.zeros(shape = (2, 10*2*8))

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
    # pro subband - 1
    join = np.zeros(8, dtype = np.uint8)
    scale_factor =  np.zeros(shape=(2, 8), dtype = np.int32)
    scalefactor =  np.zeros(shape=(2, 8), dtype = np.int32)
    audio_sample = np.ndarray(shape = (16,2,8), dtype = np.uint16)
    X = np.zeros(8, dtype = np.int16)
    pcm = np.array([], dtype = np.int16)

    def __init__(self):
        return

    def __str__(self):
        res =  "SBCFrameHeader:"
        res = res + "\n - syncword %d" % self.syncword
        res = res + "\n - sample frequency %d" % self.sampling_frequency
        res = res + "\n - nr blocks %d" % self.nr_blocks

        if self.channel_mode == MONO:
            res = res + "\n - channel mode MONO"
        elif self.channel_mode == DUAL_CHANNEL:
            res = res + "\n - channel mode DUAL CHANNEL"
        elif self.channel_mode == STEREO:
            res = res + "\n - channel mode STEREO"
        elif self.channel_mode == JOINT_STEREO:
            res = res + "\n - channel mode JOINT STEREO"
        else:
            res = res + "\n - channel mode %d" % self.channel_mode
        
        res = res + "\n - nr channels %d" % self.nr_channels
        
        if self.allocation_method == 1:
            res = res + "\n - allocation method SNR"
        elif self.allocation_method == 0:
            res = res + "\n - allocation method LOUNDNESS"
        else:
            res = res + "\n - allocation method %d" % self.allocation_method

        res = res + "\n - nr subbands %d" % self.nr_subbands
        res = res + "\n - bitpool %d" % self.bitpool
        res = res + "\n - crc check %d" % self.crc_check

        return res


def get_bit(fin):
    global ibuffer, ibuffer_count
    if ibuffer_count == 0:
        ibuffer = ord(fin.read(1))
        ibuffer_count = 8
        # print "new byte ", hex(ibuffer)

    bit = (ibuffer >> 7) & 1
    ibuffer = ibuffer << 1
    ibuffer_count = ibuffer_count - 1
    # print "bit: ", bit
    return bit

def drop_remaining_bits():
    global ibuffer_count
    ibuffer_count = 0

def get_bits(fin, bit_count):
    bits = 0
    for i in range(bit_count):
        bits = (bits << 1) | get_bit(fin)
    # print "collected bits", hex(bits)
    return bits

def get_frame_sample_frequences(fin, bit_count):
    for i in range(bit_count):
        get_bit(fin)

def sbc_bit_allocation_stereo_joint(fin, frame, ch):
    bitneed = np.zeros(shape=(frame.nr_channels, frame.nr_subbands))
    bits    = np.zeros(shape=(frame.nr_channels, frame.nr_subbands))
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
                        loudness = scale_factor[ch][sb] - offset4[frame.sampling_frequency][sb]

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

    # # print "max_bitneed: ", max_bitneed
        
    # calculate how many bitslices fit into the bitpool
    bitcount = 0
    slicecount = 0
    bitslice = max_bitneed + 1 #/* init just above the largest sf */

    while True:
        bitslice = bitslice - 1
        bitcount = bitcount + slicecount
        slicecount = 0
        for ch in range(frame.nr_channels):
            for sb in range(frame.nr_subbands):
                if (bitneed[ch][sb] > bitslice+1) and (bitneed[ch][sb] < bitslice+16):
                    slicecount = slicecount + 1
                elif bitneed[ch][sb] == bitslice + 1:
                    slicecount = slicecount + 2
        if bitcount + slicecount >= frame.bitpool:
            break 

    # print "bitcount %d, slicecount %d" % (bitcount, slicecount)

    if bitcount + slicecount == frame.bitpool:
        bitcount = bitcount + slicecount
        bitslice = bitslice - 1
    
    # bits are distributed until the last bitslice is reached
    for ch in range(frame.nr_channels):
        for sb in range(frame.nr_subbands):
            if bitneed[ch][sb] < bitslice+2 :
               bits[ch][sb]=0;
            else:
                bits[ch][sb] = min(bitneed[ch][sb]-bitslice,16)


    ch = 0
    sb = 0
    while bitcount < frame.bitpool and sb < frame.nr_subbands:
        if bits[ch][sb] >= 2 and bits[ch][sb] < 16:
               bits[ch][sb] = bits[ch][sb] + 1
               bitcount = bitcount + 1
            
        elif (bitneed[ch][sb] == bitslice+1) and (frame.bitpool > bitcount+1):
            bits[ch][sb] = 2
            bitcount += 2
        
        if ch == 1:
            ch = 0
            sb = sb + 1
        else:
            ch = 1

    ch = 0
    sb = 0
    while bitcount < frame.bitpool and sb < frame.nr_subbands:
        if bits[ch][sb] < 16:
            bits[ch][sb] = bits[ch][sb] + 1
            bitcount = bitcount + 1
        if ch == 1:
            ch = 0
            sb = sb + 1
        else:
            ch = 1 

    return bits


def sbc_bit_allocation_mono_dual(fin, frame):
    #print "Bit allocation for mono/dual channel" 
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

        #print "max_bitneed: ", max_bitneed
        
        # calculate how many bitslices fit into the bitpool
        bitcount = 0
        slicecount = 0
        bitslice = max_bitneed + 1 #/* init just above the largest sf */

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

        #print "bitcount %d, slicecount %d" % (bitcount, slicecount)

        if bitcount + slicecount == frame.bitpool:
            bitcount = bitcount + slicecount
            bitslice = bitslice - 1
        
        # bits are distributed until the last bitslice is reached
        for sb in range(frame.nr_subbands):
            if bitneed[ch][sb] < bitslice+2 :
               bits[ch][sb]=0;
            else:
                bits[ch][sb] = min(bitneed[ch][sb]-bitslice,16)

        # The remaining bits are allocated starting at subband 0.
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

def sbc_process_frame(fin, frame):
    global sb_sample

    frame.syncword = get_bits(fin,8)
    if frame.syncword != 156:
        print "incorrect syncword ", frame.syncword
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
    
    frame.bitpool = get_bits(fin,8)
    frame.crc_check = get_bits(fin,8)

    frame.join = np.zeros(frame.nr_subbands, dtype = np.uint8)

    if frame.channel_mode == JOINT_STEREO:
        frame.join = np.zeros(frame.nr_subbands-1)
        for sb in range(frame.nr_subbands-1):
            frame.join[sb] = get_bits(fin,1)
        get_bits(fin,1) # RFA

    # print frame

    frame.scale_factor = np.zeros(shape=(frame.nr_channels, frame.nr_subbands), dtype = np.int32)
    frame.audio_sample = np.ndarray(shape=(frame.nr_blocks, frame.nr_channels, frame.nr_subbands), dtype = np.uint16)
    
    # print frame.audio_sample
    
    for ch in range(frame.nr_channels):
        for sb in range(frame.nr_subbands):
            frame.scale_factor[ch][sb] = get_bits(fin, 4)

    for ch in range(frame.nr_channels):
        for sb in range(frame.nr_subbands):
            frame.scalefactor[ch][sb] = 1 << (frame.scale_factor[ch][sb] + 1)

    bits = sbc_bit_allocation_mono_dual(fin, frame)
    # print "bits: ", bits
    #print "Nr blocks ", frame.nr_blocks, frame.nr_channels, frame.nr_subbands

    for blk in range(frame.nr_blocks):
        for ch in range(frame.nr_channels):
            for sb in range(frame.nr_subbands):
                frame.audio_sample[blk][ch][sb] = get_bits(fin, bits[ch][sb])
        # print "block %2d - audio sample: %s" % (blk, frame.audio_sample[blk][0])
        
    # add padding        
    drop_remaining_bits()
    
    # Reconstruct the Subband Samples

    levels = np.zeros(shape=(frame.nr_channels, frame.nr_subbands), dtype = np.int32)
    sb_sample = np.ndarray(shape=(frame.nr_blocks, frame.nr_channels, frame.nr_subbands))
    
    for ch in range(frame.nr_channels):
        for sb in range(frame.nr_subbands):
            levels[ch][sb] = pow(2.0, bits[ch][sb]) - 1
    # print "Levels: ", levels

    for blk in range(frame.nr_blocks):
        for ch in range(frame.nr_channels):
            for sb in range(frame.nr_subbands):
                if levels[ch][sb] > 0:
                    sb_sample[blk][ch][sb] = frame.scalefactor[ch][sb] * ((frame.audio_sample[blk][ch][sb]*2.0+1.0) / levels[ch][sb] -1.0 )
                    # tmpa = (((frame.audio_sample[blk][ch][sb] << 16) | 0x8000) / levels[ch][sb] ) - 0x8000
                    # tmpb = tmpa >> 3
                    # sb_sample[blk][ch][sb] = tmpb * frame.scalefactor[ch][sb]
                else:
                    sb_sample[blk][ch][sb] = 0 

    # sythesis filter
    if frame.channel_mode == JOINT_STEREO:
        for blk in range(frame.nr_blocks):
            for sb in range(frame.nr_subbands):
                if frame.join[sb]==1:
                    sb_sample[blk][0][sb] = sb_sample[blk][0][sb] + sb_sample[blk][1][sb]
                    sb_sample[blk][1][sb] = sb_sample[blk][0][sb] - 2 * sb_sample[blk][1][sb]
    
    # print "Scale factors ", frame.scale_factor[0]

    # print "\nReconstructed subband samples: "
    # for blk in range(frame.nr_blocks):
    #     print "block %2d - recon. sample: %s" % (blk, sb_sample[blk][0])
    # print 
    return 0



def sbc_synthesis(frame, ch, blk, proto_table):
    global V, sb_sample
    M = frame.nr_subbands
    L = 10 * M
    M2 = 2*M
    L2 = 2*L

    S = np.zeros(M)
    U = np.zeros(L)
    W = np.zeros(L)
    frame.X = np.zeros(M)
    
    for i in range(M):
        S[i] = sb_sample[blk][ch][i]

    for i in range(L2-1, M2-1,-1):
        V[ch][i] = V[ch][i-M2] 

    for k in range(M2):
        V[ch][k] = 0
        for i in range(M):
            N = np.cos((i+0.5)*(k+2)*np.pi/M)
            V[ch][k] += N * S[i]

    for i in range(5):
        for j in range(M):
            U[i*M2+j] = V[ch][i*2*M2+j]
            U[(i*2+1)*M+j] = V[ch][(i*2+1)*2*M+j]

    for i in range(L):
        D = proto_table[i] * (-M)
        W[i] = U[i]*D

    
    for j in range(M):
        for i in range(10):
            frame.X[j] += W[j+M*i]
    
    frame.pcm = np.concatenate([frame.pcm, frame.X])


def sbc_decode(frame):
    if frame.nr_subbands == 4:
        proto_table = Proto_4_40
    elif frame.nr_subbands == 8:
        proto_table = Proto_8_80
    else:
        return -1
     
    for ch in range(frame.nr_channels):
        for blk in range(frame.nr_blocks):
            sbc_synthesis(frame, ch, blk, proto_table)
       
    return frame.nr_blocks * frame.nr_subbands


def write_wav_file(fout, sample):
    values = []
    for i in range(len(sample)):
        packed_value = struct.pack('h', sample[i])
        values.append(packed_value)

    value_str = ''.join(values)
    fout.writeframes(value_str)
    

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

    with open (infile, 'rb') as fin:
        try:
            frame_count = 0
            while True:
                sbc_frame = SBCFrame()
                if frame_count % 200 == 0:
                    print "== Frame %d ==" % (frame_count)
                err = sbc_process_frame(fin, sbc_frame)
                if err:
                    print "error, frame_count: ", frame_count
                    break
                
                sbc_decode(sbc_frame)
                frame_count += 1
                # print sbc_frame.pcm
                
                if frame_count == 1:
                    fout = wave.open(wavfile, 'w')
                    fout.setnchannels(sbc_frame.nr_channels)
                    fout.setsampwidth(2)
                    fout.setframerate(sampling_frequency[sbc_frame.sampling_frequency])
                    fout.setnframes(0)
                    fout.setcomptype = 'NONE'
                
                write_wav_file(fout, sbc_frame.pcm)

                # if frame_count == 8:
                #     fout.close()
                #     break

        except TypeError:
            fout.close()
            print "DONE, SBC file %s decoded into WAV file %s ", (infile, wavfile)
            exit(0) 

except IOError as e:
    print(usage)
    sys.exit(1)





