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

import lc3
import tables as T, appendix_c as C

import mdct, energy, bwdet, sns, tns, spec, ltpf
import bitstream

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

        bw = self.bwdet.get(b)
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

        x = self.ltpf.run(x)

        return x

    def run(self, data):

        (x, bw, pitch) = self.decode(data)

        x = self.synthesize(x, bw, pitch, len(data))

        return x

def check_appendix_c(dt):

    i0 = dt - T.DT_7M5

    dec_c = lc3.setup_decoder(int(T.DT_MS[dt] * 1000), 16000)
    ok = True

    for i in range(len(C.BYTES_AC[i0])):

        pcm = lc3.decode(dec_c, bytes(C.BYTES_AC[i0][i]))
        ok = ok and np.max(np.abs(pcm - C.X_HAT_CLIP[i0][i])) < 1

    return ok

def check():

    ok = True

    for dt in range(T.DT_7M5, T.NUM_DT):
        ok = ok and check_appendix_c(dt)

    return ok
