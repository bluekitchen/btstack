#!/usr/bin/env python3
#
# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import numpy as np
import scipy.signal as signal
import scipy.io.wavfile as wavfile
import struct
import argparse

import build.lc3 as lc3
import tables as T, appendix_c as C

import mdct, energy, bwdet, sns, tns, spec, ltpf
import bitstream

### ------------------------------------------------------------------------ ###

class Decoder:

    def __init__(self, dt_ms, sr_hz):

        dt = { 7.5: T.DT_7M5, 10: T.DT_10M }[dt_ms]

        sr = {  8000: T.SRATE_8K , 16000: T.SRATE_16K, 24000: T.SRATE_24K,
               32000: T.SRATE_32K, 48000: T.SRATE_48K }[sr_hz]

        self.sr = sr
        self.ne = T.NE[dt][sr]
        self.ns = T.NS[dt][sr]

        self.mdct = mdct.MdctInverse(dt, sr)

        self.bwdet = bwdet.BandwidthDetector(dt, sr)
        self.spec = spec.SpectrumSynthesis(dt, sr)
        self.tns = tns.TnsSynthesis(dt)
        self.sns = sns.SnsSynthesis(dt, sr)
        self.ltpf = ltpf.LtpfSynthesis(dt, sr)

    def decode(self, data):

        b = bitstream.BitstreamReader(data)

        bw = self.bwdet.get_bw(b)
        if bw > self.sr:
            raise ValueError('Invalid bandwidth indication')

        self.spec.load(b)

        self.tns.load(b, bw, len(data))

        pitch = b.read_bit()

        self.sns.load(b)

        if pitch:
            self.ltpf.load(b)
        else:
            self.ltpf.disable()

        x = self.spec.decode(b, bw, len(data))

        return (x, bw, pitch)

    def synthesize(self, x, bw, pitch, nbytes):

        x = self.tns.run(x, bw)

        x = self.sns.run(x)

        x = np.append(x, np.zeros(self.ns - self.ne))
        x = self.mdct.run(x)

        x = self.ltpf.run(x, len(data))

        return x

    def run(self, data):

        (x, bw, pitch) = self.decode(data)

        x = self.synthesize(x, bw, pitch, len(data))

        return x

### ------------------------------------------------------------------------ ###

def check_appendix_c(dt):

    ok = True

    dec_c = lc3.setup_decoder(int(T.DT_MS[dt] * 1000), 16000)

    for i in range(len(C.BYTES_AC[dt])):

        pcm = lc3.decode(dec_c, bytes(C.BYTES_AC[dt][i]))
        ok = ok and np.max(np.abs(pcm - C.X_HAT_CLIP[dt][i])) < 1

    return ok

def check():

    ok = True

    for dt in range(T.NUM_DT):
        ok = ok and check_appendix_c(dt)

    return ok

### ------------------------------------------------------------------------ ###

if __name__ == "__main__":

    parser = argparse.ArgumentParser(description='LC3 Decoder Test Framework')
    parser.add_argument('lc3_file',
        help='Input bitstream file', type=argparse.FileType('r'))
    parser.add_argument('--pyout',
        help='Python output file', type=argparse.FileType('w'))
    parser.add_argument('--cout',
        help='C output file', type=argparse.FileType('w'))
    args = parser.parse_args()

    ### File Header ###

    f_lc3 = open(args.lc3_file.name, 'rb')

    header = struct.unpack('=HHHHHHHI', f_lc3.read(18))

    if header[0] != 0xcc1c:
        raise ValueError('Invalid bitstream file')

    if header[4] != 1:
        raise ValueError('Unsupported number of channels')

    sr_hz = header[2] * 100
    bitrate = header[3] * 100
    nchannels = header[4]
    dt_ms = header[5] / 100

    f_lc3.seek(header[1])

    ### Setup ###

    dec = Decoder(dt_ms, sr_hz)
    dec_c = lc3.setup_decoder(int(dt_ms * 1000), sr_hz)

    pcm_c  = np.empty(0).astype(np.int16)
    pcm_py = np.empty(0).astype(np.int16)

    ### Decoding loop ###

    nframes = 0

    while True:

        data = f_lc3.read(2)
        if len(data) != 2:
            break

        if nframes >= 1000:
            break

        (frame_nbytes,) = struct.unpack('=H', data)

        print('Decoding frame %d' % nframes, end='\r')

        data = f_lc3.read(frame_nbytes)

        x = dec.run(data)
        pcm_py = np.append(pcm_py,
            np.clip(np.round(x), -32768, 32767).astype(np.int16))

        x_c = lc3.decode(dec_c, data)
        pcm_c = np.append(pcm_c, x_c)

        nframes += 1

    print('done ! %16s' % '')

    ### Terminate ###

    if args.pyout:
        wavfile.write(args.pyout.name, sr_hz, pcm_py)
    if args.cout:
        wavfile.write(args.cout.name, sr_hz, pcm_c)

### ------------------------------------------------------------------------ ###
