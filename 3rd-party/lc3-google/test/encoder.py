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

import attdet, ltpf
import mdct, energy, bwdet, sns, tns, spec
import bitstream

### ------------------------------------------------------------------------ ###

class Encoder:

    def __init__(self, dt_ms, sr_hz):

        dt = { 7.5: T.DT_7M5, 10: T.DT_10M }[dt_ms]

        sr = {  8000: T.SRATE_8K , 16000: T.SRATE_16K, 24000: T.SRATE_24K,
               32000: T.SRATE_32K, 48000: T.SRATE_48K }[sr_hz]

        self.ne = T.NE[dt][sr]

        self.attdet = attdet.AttackDetector(dt, sr)
        self.ltpf = ltpf.Ltpf(dt, sr)

        self.mdct = mdct.Mdct(dt, sr)
        self.energy = e_energy.EnergyBand(dt, sr)
        self.bwdet = bwdet.BandwidthDetector(dt, sr)
        self.sns = sns.SnsAnalysis(dt, sr)
        self.tns = tns.TnsAnalysis(dt)
        self.spec = spec.SpectrumEncoder(dt, sr)

    def analyse(self, x, nbytes):

        att = self.attdet.run(nbytes, x)

        pitch_present = self.ltpf.run(x)

        x = self.mdct.forward(x)[:self.ne]

        (e, nn_flag) = self.energy.compute(x)
        if nn_flag:
            self.ltpf.disable()

        bw = self.bwdet.run(e)

        x = self.sns.run(e, att, x)

        x = self.tns.run(x, bw, nn_flag, nbytes)

        (xq, lastnz, x) = self.spec.quantize(bw, nbytes,
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
            self.ltpf.store_data(b)

        self.spec.encode(b)

        return b.terminate()

    def run(self, x, nbytes):

        pitch_present = self.analyse(x, nbytes)

        data = self.encode(pitch_present, nbytes)

        return data

### ------------------------------------------------------------------------ ###

def check_appendix_c(dt):

    ok = True

    enc_c = lc3.setup_encoder(int(T.DT_MS[dt] * 1000), 16000)

    for i in range(len(C.X_PCM[dt])):

        data = lc3.encode(enc_c, C.X_PCM[dt][i], C.NBYTES[dt])
        ok = ok and data == C.BYTES_AC[dt][i]
        if not ok:
            dump(data)
            dump(C.BYTES_AC[dt][i])

    return ok

def check():

    ok = True

    for dt in range(T.NUM_DT):
        ok = ok and check_appendix_c(dt)

    return ok

### ------------------------------------------------------------------------ ###

def dump(data):
    for i in range(0, len(data), 20):
        print(''.join('{:02x} '.format(x)
            for x in data[i:min(i+20, len(data))] ))

if __name__ == "__main__":

    parser = argparse.ArgumentParser(description='LC3 Encoder Test Framework')
    parser.add_argument('wav_file',
        help='Input wave file', type=argparse.FileType('r'))
    parser.add_argument('--bitrate',
        help='Bitrate in bps', type=int, required=True)
    parser.add_argument('--dt',
        help='Frame duration in ms', type=float, default=10)
    parser.add_argument('--pyout',
        help='Python output file', type=argparse.FileType('w'))
    parser.add_argument('--cout',
        help='C output file', type=argparse.FileType('w'))
    args = parser.parse_args()

    if args.bitrate < 16000 or args.bitrate > 320000:
        raise ValueError('Invalid bitate %d bps' % args.bitrate)

    if args.dt not in (7.5, 10):
        raise ValueError('Invalid frame duration %.1f ms' % args.dt)

    (sr_hz, pcm) = wavfile.read(args.wav_file.name)
    if sr_hz not in (8000, 16000, 24000, 320000, 48000):
        raise ValueError('Unsupported input samplerate: %d' % sr_hz)

    ### Setup ###

    enc = Encoder(args.dt, sr_hz)
    enc_c = lc3.setup_encoder(int(args.dt * 1000), sr_hz)

    frame_samples = int((args.dt * sr_hz) / 1000)
    frame_nbytes = int((args.bitrate * args.dt) / (1000 * 8))

    ### File Header ###

    f_py = open(args.pyout.name, 'wb') if args.pyout else None
    f_c  = open(args.cout.name , 'wb') if args.cout  else None

    header = struct.pack('=HHHHHHHI', 0xcc1c, 18,
        sr_hz // 100, args.bitrate // 100, 1, int(args.dt * 100), 0, len(pcm))

    for f in (f_py, f_c):
        if f: f.write(header)

    ### Encoding loop ###

    if len(pcm) % frame_samples > 0:
        pcm = np.append(pcm, np.zeros(frame_samples - (len(pcm) % frame_samples)))

    for i in range(0, len(pcm), frame_samples):

        print('Encoding frame %d' % (i // frame_samples), end='\r')

        frame_pcm = pcm[i:i+frame_samples]

        data = enc.run(frame_pcm, frame_nbytes)
        data_c = lc3.encode(enc_c, frame_pcm, frame_nbytes)

        for f in (f_py, f_c):
            if f: f.write(struct.pack('=H', frame_nbytes))

        if f_py: f_py.write(data)
        if f_c: f_c.write(data_c)

    print('done ! %16s' % '')

    ### Terminate ###

    for f in (f_py, f_c):
        if f: f.close()


### ------------------------------------------------------------------------ ###
