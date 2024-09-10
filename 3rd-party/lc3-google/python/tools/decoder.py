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
import struct
import sys
import wave

parser = argparse.ArgumentParser(description='LC3 Decoder')

parser.add_argument(
    'lc3_file', nargs='?',
    help='Input bitstream file, default is stdin',
    type=argparse.FileType('rb'), default=sys.stdin.buffer)

parser.add_argument(
    'wav_file', nargs='?',
    help='Output wave file, default is stdout',
    type=argparse.FileType('wb'), default=sys.stdout.buffer)

parser.add_argument(
    '--bitdepth',
    help='Output bitdepth, default is 16 bits',
    type=int, choices=[16, 24], default=16)

parser.add_argument(
    '--libpath', help='LC3 Library path')

args = parser.parse_args()

# --- LC3 File input ---

f_lc3 = args.lc3_file

header = struct.unpack('=HHHHHHHI', f_lc3.read(18))
if header[0] != 0xcc1c:
    raise ValueError('Invalid bitstream file')

samplerate = header[2] * 100
nchannels = header[4]
frame_duration = header[5] / 100
stream_length = header[7]

# --- Setup output ---

bitdepth = args.bitdepth
pcm_size = nchannels * (bitdepth // 8)

f_wav = args.wav_file
wavfile = wave.open(f_wav)
wavfile.setnchannels(nchannels)
wavfile.setsampwidth(bitdepth // 8)
wavfile.setframerate(samplerate)
wavfile.setnframes(stream_length)

# --- Setup decoder ---

dec = lc3.Decoder(
    frame_duration, samplerate, nchannels, libpath=args.libpath)
frame_length = dec.get_frame_samples()
encoded_length = stream_length + dec.get_delay_samples()

# --- Decoding loop ---

for i in range(0, encoded_length, frame_length):

    lc3_frame_size = struct.unpack('=H', f_lc3.read(2))[0]
    pcm = dec.decode(f_lc3.read(lc3_frame_size), bitdepth=bitdepth)

    pcm = pcm[max(encoded_length - stream_length - i, 0) * pcm_size:
              min(encoded_length - i, frame_length) * pcm_size]

    wavfile.writeframesraw(pcm)

# --- Cleanup ---

wavfile.close()

for f in (f_lc3, f_wav):
    if f is not sys.stdout.buffer:
        f.close()
