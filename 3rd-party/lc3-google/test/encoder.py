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

import attdet, ltpf
import mdct, energy, bwdet, sns, tns, spec
import bitstream

class Encoder:

    def __init__(self, dt_ms, sr_hz):

        dt = { 7.5: T.DT_7M5, 10: T.DT_10M }[dt_ms]

        sr = {  8000: T.SRATE_8K , 16000: T.SRATE_16K, 24000: T.SRATE_24K,
               32000: T.SRATE_32K, 48000: T.SRATE_48K }[sr_hz]

        self.ne = T.NE[dt][sr]

        self.attdet = attdet.AttackDetector(dt, sr)
        self.ltpf = ltpf.LtpfAnalysis(dt, sr)

        self.mdct = mdct.MdctForward(dt, sr)
        self.energy = energy.EnergyBand(dt, sr)
        self.bwdet = bwdet.BandwidthDetector(dt, sr)
        self.sns = sns.SnsAnalysis(dt, sr)
        self.tns = tns.TnsAnalysis(dt)
        self.spec = spec.SpectrumAnalysis(dt, sr)

    def analyse(self, x, nbytes):

        att = self.attdet.run(nbytes, x)

        pitch_present = self.ltpf.run(x)

        x = self.mdct.run(x)[:self.ne]

        (e, nn_flag) = self.energy.compute(x)
        if nn_flag:
            self.ltpf.disable()

        bw = self.bwdet.run(e)

        x = self.sns.run(e, att, x)

        x = self.tns.run(x, bw, nn_flag, nbytes)

        (xq, lastnz, x) = self.spec.run(bw, nbytes,
            self.bwdet.get_nbits(), self.ltpf.get_nbits(),
            self.sns.get_nbits(), self.tns.get_nbits(), x)

        return pitch_present

    def encode(self, pitch_present, nbytes):

        b = bitstream.BitstreamWriter(nbytes)

        self.bwdet.store(b)

        self.spec.store(b)

        self.tns.store(b)

        b.write_bit(pitch_present)

        self.sns.store(b)

        if pitch_present:
            self.ltpf.store(b)

        self.spec.encode(b)

        return b.terminate()

    def run(self, x, nbytes):

        pitch_present = self.analyse(x, nbytes)

        data = self.encode(pitch_present, nbytes)

        return data

def check_appendix_c(dt):

    i0 = dt - T.DT_7M5

    enc_c = lc3.setup_encoder(int(T.DT_MS[dt] * 1000), 16000)
    ok = True

    for i in range(len(C.X_PCM[i0])):

        data = lc3.encode(enc_c, C.X_PCM[i0][i], C.NBYTES[i0])
        ok = ok and data == C.BYTES_AC[i0][i]

    return ok

def check():

    ok = True

    for dt in ( T.DT_7M5, T.DT_10M ):
        ok = ok and check_appendix_c(dt)

    return ok
