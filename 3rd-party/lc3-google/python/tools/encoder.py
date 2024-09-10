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

parser = argparse.ArgumentParser(description='LC3 Encoder')

parser.add_argument(
    'wav_file', nargs='?',
    help='Input wave file, default is stdin',
    type=argparse.FileType('rb'), default=sys.stdin.buffer)

parser.add_argument(
    'lc3_file', nargs='?',
    help='Output bitstream file, default is stdout',
    type=argparse.FileType('wb'), default=sys.stdout.buffer)

parser.add_argument(
    '--bitrate', help='Bitrate in bps', type=int, required=True)

parser.add_argument(
    '--frame_duration', help='Frame duration in ms', type=float, default=10)

parser.add_argument(
    '--libpath', help='LC3 Library path')

args = parser.parse_args()

# --- WAV File input ---

f_wav = args.wav_file
wavfile = wave.open(f_wav, 'rb')

samplerate = wavfile.getframerate()
nchannels = wavfile.getnchannels()
bitdepth = wavfile.getsampwidth() * 8
stream_length = wavfile.getnframes()

# --- Setup encoder ---

enc = lc3.Encoder(
    args.frame_duration, samplerate, nchannels, libpath=args.libpath)
frame_size = enc.get_frame_bytes(args.bitrate)
frame_length = enc.get_frame_samples()
bitrate = enc.resolve_bitrate(frame_size)

# --- Setup output ---

f_lc3 = args.lc3_file
f_lc3.write(struct.pack(
    '=HHHHHHHI', 0xcc1c, 18,
    samplerate // 100, bitrate // 100, nchannels,
    int(args.frame_duration * 100), 0, stream_length))

# --- Encoding loop ---

for i in range(0, stream_length, frame_length):

    f_lc3.write(struct.pack('=H', frame_size))

    pcm = wavfile.readframes(frame_length)
    f_lc3.write(enc.encode(pcm, frame_size, bitdepth=bitdepth))

# --- Cleanup ---

wavfile.close()

for f in (f_wav, f_lc3):
    if f is not sys.stdout.buffer:
        f.close()
