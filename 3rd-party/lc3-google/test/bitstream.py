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

import math

class Bitstream:

    def __init__(self, data):

        self.bytes = data

        self.bp_bw = len(data) - 1
        self.mask_bw = 1

        self.bp = 0
        self.low = 0
        self.range = 0xffffff

    def dump(self):

        b = self.bytes

        for i in range(0, len(b), 20):
            print(''.join('{:02x} '.format(x)
                for x in b[i:min(i+20, len(b))] ))

class BitstreamReader(Bitstream):

    def __init__(self, data):

        super().__init__(data)

        self.low = ( (self.bytes[0] << 16) |
                     (self.bytes[1] <<  8) |
                     (self.bytes[2]      ) )
        self.bp = 3

    def read_bit(self):

        bit = bool(self.bytes[self.bp_bw] & self.mask_bw)

        self.mask_bw <<= 1
        if self.mask_bw == 0x100:
            self.mask_bw = 1
            self.bp_bw -= 1

        return bit

    def read_uint(self, nbits):

        val = 0
        for k in range(nbits):
            val |= self.read_bit() << k

        return val

    def ac_decode(self, cum_freqs, sym_freqs):

        r = self.range >> 10
        if self.low >= r << 10:
            raise ValueError('Invalid ac bitstream')

        val = len(cum_freqs) - 1
        while self.low < r * cum_freqs[val]:
            val -= 1

        self.low -= r * cum_freqs[val]
        self.range = r * sym_freqs[val]
        while self.range < 0x10000:
            self.range <<= 8

            self.low <<= 8
            self.low &= 0xffffff
            self.low += self.bytes[self.bp]
            self.bp += 1

        return val

    def get_bits_left(self):

        nbits = 8 * len(self.bytes)

        nbits_bw = nbits - \
            (8*self.bp_bw + 8 - int(math.log2(self.mask_bw)))

        nbits_ac = 8 * (self.bp - 3) + \
            (25 - int(math.floor(math.log2(self.range))))

        return nbits - (nbits_bw + nbits_ac)

class BitstreamWriter(Bitstream):

    def __init__(self, nbytes):

        super().__init__(bytearray(nbytes))

        self.cache = -1
        self.carry = 0
        self.carry_count = 0

    def write_bit(self, bit):

        mask = self.mask_bw
        bp = self.bp_bw

        if bit == 0:
            self.bytes[bp] &= ~mask
        else:
            self.bytes[bp] |= mask

        self.mask_bw <<= 1
        if self.mask_bw == 0x100:
            self.mask_bw = 1
            self.bp_bw -= 1

    def write_uint(self, val, nbits):

        for k in range(nbits):
            self.write_bit(val & 1)
            val >>= 1

    def ac_shift(self):

        if self.low < 0xff0000 or self.carry == 1:

            if self.cache >= 0:
                self.bytes[self.bp] = self.cache + self.carry
                self.bp += 1

            while self.carry_count > 0:
                self.bytes[self.bp] = (self.carry + 0xff) & 0xff
                self.bp += 1
                self.carry_count -= 1

            self.cache = self.low >> 16
            self.carry = 0

        else:
            self.carry_count += 1

        self.low <<= 8
        self.low &= 0xffffff

    def ac_encode(self, cum_freq, sym_freq):

        r = self.range >> 10
        self.low += r * cum_freq
        if (self.low >> 24) != 0:
            self.carry = 1

        self.low &= 0xffffff
        self.range = r * sym_freq
        while self.range < 0x10000:
            self.range <<= 8
            self.ac_shift()

    def get_bits_left(self):

        nbits = 8 * len(self.bytes)

        nbits_bw = nbits - \
            (8*self.bp_bw + 8 - int(math.log2(self.mask_bw)))

        nbits_ac = 8 * self.bp + (25 - int(math.floor(math.log2(self.range))))
        if self.cache >= 0:
            nbits_ac += 8
        if self.carry_count > 0:
            nbits_ac += 8 * self.carry_count

        return nbits - (nbits_bw + nbits_ac)

    def terminate(self):

        bits = 1
        while self.range >> (24 - bits) == 0:
            bits += 1

        mask = 0xffffff >> bits
        val = self.low + mask

        over1 = val >> 24
        val &= 0x00ffffff
        high = self.low + self.range
        over2 = high >> 24
        high &= 0x00ffffff
        val = val & ~mask

        if over1 == over2:

            if val + mask >= high:
                bits += 1
                mask >>= 1
                val = ((self.low + mask) & 0x00ffffff) & ~mask

            if val < self.low:
                self.carry = 1

        self.low = val
        while bits > 0:
            self.ac_shift()
            bits -= 8
        bits += 8

        val = self.cache

        if self.carry_count > 0:
            self.bytes[self.bp] = self.cache
            self.bp += 1

            while self.carry_count > 1:
                self.bytes[self.bp] = 0xff
                self.bp += 1
                self.carry_count -= 1

            val = 0xff >> (8 - bits)

        mask = 0x80
        for k in range(bits):

            if val & mask == 0:
                self.bytes[self.bp] &= ~mask
            else:
                self.bytes[self.bp] |= mask

            mask >>= 1

        return self.bytes
