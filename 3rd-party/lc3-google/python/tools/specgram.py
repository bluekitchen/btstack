#!/usr/bin/env python3
#
# Copyright 2024 Google LLC
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

import argparse
import lc3
import matplotlib
import matplotlib.pyplot as plt
import numpy as np
import scipy.signal as signal

matplotlib.use('QtAgg')

parser = argparse.ArgumentParser(description='LC3 Encoder')

parser.add_argument(
    '--frame_duration', help='Frame duration (ms)', type=float, default=10)

parser.add_argument(
    '--sample_rate', help='Sampling frequency (Hz)', type=float, default=48000)

parser.add_argument(
    '--hrmode', help='Enable high-resolution mode', action='store_true')

parser.add_argument(
    '--bitrate', help='Bitrate (bps)', type=int, default=96000)

parser.add_argument(
    '--libpath', help='LC3 Library path')

args = parser.parse_args()

# --- Setup encoder + decoder ---

fs = args.sample_rate

enc = lc3.Encoder(
    args.frame_duration, fs, hrmode=args.hrmode, libpath=args.libpath)
dec = lc3.Decoder(
    args.frame_duration, fs, hrmode=args.hrmode, libpath=args.libpath)

frame_len = enc.get_frame_samples()
delay_len = enc.get_delay_samples()

# --- Generate 10 seconds chirp ---

n = 10 * int(fs)
t = np.arange(n - (n % frame_len)) / fs
s = signal.chirp(t, f0=10, f1=fs/2, t1=t[-1], phi=-90, method='linear')

# --- Encoding / decoding loop ---

frame_size = enc.get_frame_bytes(args.bitrate)
bitrate = enc.resolve_bitrate(frame_size)

y = np.empty(len(s) + frame_len)

for i in range(0, len(s), frame_len):
    y[i:i+frame_len] = dec.decode(enc.encode(s[i:i+frame_len], frame_size))

y[len(s):] = dec.decode(enc.encode(np.zeros(frame_len), frame_size))
y = y[delay_len:len(s)+delay_len]

# --- Plot spectrograms ---

fig, (ax1, ax2) = plt.subplots(nrows=2, sharex=True)

NFFT = 512
for (ax, s) in [(ax1, s), (ax2, y)]:
    ax.specgram(s, Fs=fs, NFFT=NFFT, pad_to=4*NFFT, noverlap=NFFT//2,
                vmin=-160, vmax=0)

ax1.set_title('Input signal')
ax1.set_ylabel('Frequency (Hz)')

ax2.set_title(('Processed signal (%.1f kbps)' % (bitrate/1000)))
ax2.set_ylabel('Frequency (Hz)')

plt.show()
