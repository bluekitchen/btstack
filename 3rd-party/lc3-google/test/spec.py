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

import lc3
import tables as T, appendix_c as C

import bwdet as m_bwdet
import ltpf as m_ltpf
import sns as m_sns
import tns as m_tns

### ------------------------------------------------------------------------ ###

class SpectrumQuantization:

    def __init__(self, dt, sr):

        self.dt = dt
        self.sr = sr

    def get_gain_offset(self, nbytes):

        sr_ind = self.sr if self.sr < T.SRATE_48K_HR \
            else 4 + (self.sr - T.SRATE_48K_HR)

        g_off = (nbytes * 8) // (10 * (1 + sr_ind))
        g_off = -min(115, g_off) - (105 + 5*(1 + sr_ind))
        if self.sr >= T.SRATE_48K_HR:
            g_off = max(g_off, -181)

        return g_off

    def get_noise_indices(self, bw, xq, lastnz):

        nf_start = [  6, 12, 18, 24 ][self.dt]
        nf_width = [  1,  1,  2,  3 ][self.dt]

        bw_stop = T.I[self.dt][min(bw, T.SRATE_48K)][-1]

        xq = np.append(xq[:lastnz], np.zeros(len(xq) - lastnz))
        xq[:nf_start-nf_width] = 1

        return [ np.all(xq[max(k-nf_width, 0):min(k+nf_width+1, bw_stop)] == 0)
                    for k in range(bw_stop) ]

class SpectrumAnalysis(SpectrumQuantization):

    def __init__(self, dt, sr):

        super().__init__(dt, sr)

        self.reset_off  = 0
        self.nbits_off  = 0
        self.nbits_spec = 0
        self.nbits_est  = 0

        (self.g_idx, self.noise_factor, self.xq, self.lastnz,
                self.nbits_residual_max, self.xg) = \
            (None, None, None, None, None, None)

    def estimate_gain(self, x, nbytes, nbits_spec, nbits_off, g_off):

        nbits = int(nbits_spec + nbits_off + 0.5)

        ### Energy (dB) by 4 MDCT coefficients

        hr = (self.sr >= T.SRATE_48K_HR)
        nf = 0

        if hr:
            dt = self.dt
            sr = self.sr

            dt_ms = T.DT_MS[dt]
            bitrate = (8 * nbytes / (dt_ms * 1e-3)).astype(int)

            C = [ [ -6, 0, None, 2 ], [ -6, 0, None, 5 ] ]

            reg_bits = np.clip(
                bitrate // 12500 + C[sr - T.SRATE_48K_HR][dt], 6, 23)

            M0 = np.sum(np.abs(x)) + 1e-5
            M1 = np.sum(np.arange(len(x)) * np.abs(x)) + 1e-5

            low_bits = (4 / dt_ms) * (2*dt_ms - min(M0/M1, 2*dt_ms))

            nf = np.max(np.abs(x)) * np.exp2(-reg_bits - low_bits)

        e = [ np.sum(x[4*k:4*(k+1)] ** 2) for k in range(len(x) // 4) ]
        e = 10 * np.log10(2**-31 + np.array(e) + nf)

        ### Compute gain index

        g_idx = 255

        for i in range(8):
            factor = 1 << (7 - i)
            g_idx -= factor
            tmp = 0
            iszero = 1

            for ei in e[-1::-1]:

                if ei * 28/20 < g_idx + g_off:
                    if iszero == 0:
                        tmp += 2.7*28/20
                else:
                    if g_idx + g_off < (ei - 43) * 28/20:
                        tmp += 2*ei*28/20 - 2*(g_idx + g_off) - 36*28/20
                    else:
                        tmp += ei*28/20 - (g_idx + g_off) + 7*28/20
                    iszero = 0

            if tmp > nbits * 1.4 * 28/20 and iszero == 0:
                g_idx += factor

        ### Limit gain index

        x_max = np.amax(np.abs(x))
        if x_max > 0:
            x_lim = [ 2**15 - 0.375,  2**23 ][hr]
            g_min = 28 * np.log10(x_max / x_lim)
            g_min = np.ceil(g_min).astype(int) - g_off
            reset_off = g_idx < g_min
        else:
            g_min = 0
            reset_off = True

        if reset_off:
            g_idx = g_min

        return (g_min, g_idx + g_off, reset_off)

    def quantize(self, g_int, x):

        xg = x / 10 ** (g_int / 28)

        hr = (self.sr >= T.SRATE_48K_HR)
        offset = [ 0.375, 0.5 ][hr]
        xq_min = [ -(2**15)  , -(2**23)   ][hr]
        xq_max = [  (2**15)-1,  (2**23)-1 ][hr]

        xq = np.where(xg < 0, np.ceil(xg - offset), np.floor(xg + offset))
        xq = xq.astype(np.int32)
        xq = np.fmin(np.fmax(xq, xq_min), xq_max)

        nz_pairs = np.any([ xq[::2] != 0, xq[1::2] != 0 ], axis=0)
        lastnz = len(xq) - 2 * np.argmax(nz_pairs[-1::-1])
        if not np.any(nz_pairs):
            lastnz = 0

        return (xg, xq, lastnz)

    def compute_nbits(self, nbytes, x, lastnz, nbits_spec):

        mode = [ 0,   1 ][int(self.sr < T.SRATE_96K_HR and \
                              nbytes >= 20 * (3 + min(self.sr, T.SRATE_48K)))]
        rate = [ 0, 512 ][int(self.sr < T.SRATE_96K_HR and \
                              nbytes >  20 * (1 + min(self.sr, T.SRATE_48K)))]

        nbits_est = 0
        nbits_trunc = 0
        nbits_lsb = 0
        lastnz_trunc = 2
        c = 0

        for n in range(0, lastnz, 2):
            t = c + rate
            if n > len(x) // 2:
                t += 256

            a = abs(x[n  ])
            b = abs(x[n+1])
            lev = 0
            while max(a, b) >= 4:
                nbits_est += \
                    T.AC_SPEC_BITS[T.AC_SPEC_LOOKUP[t + lev*1024]][16];
                if lev == 0 and mode == 1:
                    nbits_lsb += 2
                else:
                    nbits_est += 2 * 2048

                a >>= 1
                b >>= 1
                lev = min(lev + 1, 3)

            nbits_est += \
                T.AC_SPEC_BITS[T.AC_SPEC_LOOKUP[t + lev*1024]][a + 4*b]

            a_lsb = abs(x[n  ])
            b_lsb = abs(x[n+1])
            nbits_est += (min(a_lsb, 1) + min(b_lsb, 1)) * 2048
            if lev > 0 and mode == 1:
                a_lsb >>= 1;
                b_lsb >>= 1;
                nbits_lsb += int(a_lsb == 0 and x[n  ] != 0)
                nbits_lsb += int(b_lsb == 0 and x[n+1] != 0)

            if (x[n] != 0 or x[n+1] != 0) and \
                    (nbits_est <= nbits_spec * 2048):
                lastnz_trunc = n + 2;
                nbits_trunc = nbits_est

            t = 1 + (a + b) * (lev + 1) if lev <= 1 else 12 + lev;
            c = (c & 15) * 16 + t;

        nbits_est = (nbits_est + 2047) // 2048 + nbits_lsb;
        nbits_trunc = (nbits_trunc + 2047) // 2048

        self.rate = rate
        self.lsb_mode = mode == 1 and nbits_est > nbits_spec

        return (nbits_est, nbits_trunc, lastnz_trunc, self.lsb_mode)

    def adjust_gain(self, g_idx, nbits, nbits_spec):

        T1 = [  80,  230,  380,  530,  680,  680,  830 ]
        T2 = [ 500, 1025, 1550, 2075, 2600, 2600, 3125 ]
        T3 = [ 850, 1700, 2550, 3400, 4250, 4250, 5100 ]

        dt = self.dt
        sr = self.sr

        if nbits < T1[sr]:
            delta = (nbits + 48) / 16

        elif nbits < T2[sr]:
            a = T1[sr] / 16 + 3
            b = T2[sr] / 48
            delta = a + (nbits - T1[sr]) * (b - a) / (T2[sr] - T1[sr])

        elif nbits < T3[sr]:
            delta = nbits / 48

        else:
            delta = T3[sr] / 48;

        delta = np.fix(delta + 0.5).astype(int)

        if self.sr >= T.SRATE_48K_HR and \
            (g_idx < 255 and nbits > nbits_spec):

            factor = [ 3 + (nbits >= 520), 2, 0, 1 ][dt]
            g_incr = int(factor * (1 + (nbits - nbits_spec) / delta))
            return min(g_idx + g_incr, 255) - g_idx;

        elif self.sr < T.SRATE_48K_HR and \
            ( (g_idx < 255 and nbits > nbits_spec) or \
              (g_idx >   0 and nbits < nbits_spec - (delta + 2)) ):

            if nbits < nbits_spec - (delta + 2):
                return -1

            if g_idx == 254 or nbits < nbits_spec + delta:
                return 1

            else:
                return 2

        return 0

    def estimate_noise(self, bw, xq, lastnz, x):

        i_nf = self.get_noise_indices(bw, xq, lastnz)
        l_nf = sum(abs(x[:len(i_nf)] * i_nf)) / sum(i_nf) \
            if sum(i_nf) > 0 else 0

        return min(max(np.rint(8 - 16 * l_nf).astype(int), 0), 7)

    def run(self, bw, nbytes, nbits_bw, nbits_ltpf, nbits_sns, nbits_tns, x):

        sr = self.sr

        ### Bit budget

        hr = self.sr >= T.SRATE_48K_HR

        nbits_gain = 8
        nbits_nf   = 3

        nbits_ari  = np.ceil(np.log2(len(x) / 2)).astype(int)
        nbits_ari += 3 + int(hr) + min((8*nbytes - 1) // 1280, 2)

        nbits_spec = 8*nbytes - \
            nbits_bw - nbits_ltpf - nbits_sns - nbits_tns - \
            nbits_gain - nbits_nf - nbits_ari

        ### Global gain estimation

        nbits_off = self.nbits_off + self.nbits_spec - self.nbits_est
        nbits_off = min(40, max(-40, nbits_off))

        nbits_off = 0 if self.reset_off else \
                    0.8 * self.nbits_off + 0.2 * nbits_off

        g_off = self.get_gain_offset(nbytes)

        (g_min, g_int, self.reset_off) = \
            self.estimate_gain(x, nbytes, nbits_spec, nbits_off, g_off)
        self.nbits_off = nbits_off
        self.nbits_spec = nbits_spec

        ### Quantization

        (xg, xq, lastnz) = self.quantize(g_int, x)

        (nbits_est, nbits_trunc, lastnz_trunc, _) = \
            self.compute_nbits(nbytes, xq, lastnz, nbits_spec)

        self.nbits_est = nbits_est

        ### Adjust gain and requantize

        g_adj = self.adjust_gain(g_int - g_off, nbits_est, nbits_spec)
        g_adj = max(g_int + g_adj, g_min + g_off) - g_int

        (xg, xq, lastnz) = self.quantize(g_adj, xg)

        (nbits_est, nbits_trunc, lastnz_trunc, lsb_mode) = \
            self.compute_nbits(nbytes, xq, lastnz, nbits_spec)

        self.g_idx = g_int + g_adj - g_off
        self.xq = xq
        self.lastnz = lastnz_trunc

        self.nbits_residual_max = nbits_spec - nbits_trunc + 4
        self.xg = xg

        ### Noise factor

        self.noise_factor = self.estimate_noise(bw, xq, lastnz, x)

        return (self.xq, self.lastnz, self.xg)

    def store(self, b):

        ne = T.I[self.dt][self.sr][-1]
        nbits_lastnz = np.ceil(np.log2(ne/2)).astype(int)

        b.write_uint((self.lastnz >> 1) - 1, nbits_lastnz)
        b.write_uint(self.lsb_mode, 1)
        b.write_uint(self.g_idx, 8)

    def encode(self, bits):

        ### Noise factor

        bits.write_uint(self.noise_factor, 3)

        ### Quantized data

        lsbs = []

        x = self.xq
        c = 0

        for n in range(0, self.lastnz, 2):
            t = c + self.rate
            if n > len(x) // 2:
                t += 256

            a = abs(x[n  ])
            b = abs(x[n+1])
            lev = 0
            while max(a, b) >= 4:

                bits.ac_encode(
                    T.AC_SPEC_CUMFREQ[T.AC_SPEC_LOOKUP[t + lev*1024]][16],
                    T.AC_SPEC_FREQ[T.AC_SPEC_LOOKUP[t + lev*1024]][16])

                if lev == 0 and self.lsb_mode:
                    lsb_0 = a & 1
                    lsb_1 = b & 1
                else:
                    bits.write_bit(a & 1)
                    bits.write_bit(b & 1)

                a >>= 1
                b >>= 1
                lev = min(lev + 1, 3)

            bits.ac_encode(
                T.AC_SPEC_CUMFREQ[T.AC_SPEC_LOOKUP[t + lev*1024]][a + 4*b],
                T.AC_SPEC_FREQ[T.AC_SPEC_LOOKUP[t + lev*1024]][a + 4*b])

            a_lsb = abs(x[n  ])
            b_lsb = abs(x[n+1])
            if lev > 0 and self.lsb_mode:
                a_lsb >>= 1
                b_lsb >>= 1

                lsbs.append(lsb_0)
                if a_lsb == 0 and x[n+0] != 0:
                    lsbs.append(int(x[n+0] < 0))

                lsbs.append(lsb_1)
                if b_lsb == 0 and x[n+1] != 0:
                    lsbs.append(int(x[n+1] < 0))

            if a_lsb > 0:
                bits.write_bit(int(x[n+0] < 0))

            if b_lsb > 0:
                bits.write_bit(int(x[n+1] < 0))

            t = 1 + (a + b) * (lev + 1) if lev <= 1 else 12 + lev;
            c = (c & 15) * 16 + t;

        ### Residual data

        if self.lsb_mode == 0:
            nbits_residual = min(bits.get_bits_left(), self.nbits_residual_max)

            for i in range(len(self.xg)):

                if self.xq[i] == 0:
                    continue

                bits.write_bit(self.xg[i] >= self.xq[i])
                nbits_residual -= 1
                if nbits_residual <= 0:
                    break

        else:
            nbits_residual = min(bits.get_bits_left(), len(lsbs))
            for lsb in lsbs[:nbits_residual]:
                bits.write_bit(lsb)


class SpectrumSynthesis(SpectrumQuantization):

    def __init__(self, dt, sr):

        super().__init__(dt, sr)

        (self.lastnz, self.lsb_mode, self.g_idx) = \
            (None, None, None)

    def fill_noise(self, bw, x, lastnz, f_nf, nf_seed):

        i_nf = self.get_noise_indices(bw, x, lastnz)

        k_nf = np.argwhere(i_nf)
        l_nf = (8 - f_nf)/16

        for k in k_nf:
            nf_seed = (13849 + nf_seed * 31821) & 0xffff
            x[k] = [ -l_nf, l_nf ][nf_seed < 0x8000]

        return x

    def load(self, b):

        ne = T.I[self.dt][self.sr][-1]
        nbits_lastnz = np.ceil(np.log2(ne/2)).astype(int)

        self.lastnz = (b.read_uint(nbits_lastnz) + 1) << 1
        self.lsb_mode = b.read_uint(1)
        self.g_idx = b.read_uint(8)

        if self.lastnz > ne:
            raise ValueError('Invalid count of coded samples')

    def decode(self, bits, bw, nbytes):

        ### Noise factor

        f_nf = bits.read_uint(3)

        ### Quantized data

        ne = T.I[self.dt][self.sr][-1]
        x  = np.zeros(ne)
        rate = [ 0, 512 ][int(self.sr < T.SRATE_96K_HR and \
                              nbytes >  20 * (1 + min(self.sr, T.SRATE_48K)))]

        levs = np.zeros(len(x), dtype=np.intc)
        c = 0

        for n in range(0, self.lastnz, 2):
            t = c + rate
            if n > len(x) // 2:
                t += 256

            for lev in range(14):

                s = t + min(lev, 3) * 1024

                sym = bits.ac_decode(
                    T.AC_SPEC_CUMFREQ[T.AC_SPEC_LOOKUP[s]],
                    T.AC_SPEC_FREQ[T.AC_SPEC_LOOKUP[s]])

                if sym < 16:
                    break

                if self.lsb_mode == 0 or lev > 0:
                    x[n  ] += bits.read_bit() << lev
                    x[n+1] += bits.read_bit() << lev

            if lev >= 14:
                raise ValueError('Out of range value')

            a = sym %  4
            b = sym // 4

            levs[n  ] = lev
            levs[n+1] = lev

            x[n  ] += a << lev
            x[n+1] += b << lev

            if x[n] and bits.read_bit():
                x[n] = -x[n]

            if x[n+1] and bits.read_bit():
                x[n+1] = -x[n+1]

            lev = min(lev, 3)
            t = 1 + (a + b) * (lev + 1) if lev <= 1 else 12 + lev;
            c = (c & 15) * 16 + t;

        ### Residual data

        nbits_residual = bits.get_bits_left()
        if nbits_residual < 0:
            raise ValueError('Out of bitstream')

        if self.lsb_mode == 0:

            xr = np.zeros(len(x), dtype=np.bool)

            for i in range(len(x)):

                if nbits_residual <= 0:
                    xr.resize(i)
                    break

                if x[i] == 0:
                    continue

                xr[i] = bits.read_bit()
                nbits_residual -= 1

        else:

            for i in range(len(levs)):

                if nbits_residual <= 0:
                    break

                if levs[i] <= 0:
                    continue

                lsb = bits.read_bit()
                nbits_residual -= 1
                if not lsb:
                    continue

                sign = int(x[i] < 0)

                if x[i] == 0:

                    if nbits_residual <= 0:
                        break

                    sign = bits.read_bit()
                    nbits_residual -= 1

                x[i] += [ 1, -1 ][sign]

        ### Set residual and noise

        nf_seed = sum(abs(x.astype(np.intc)) * range(len(x)))

        zero_frame = (self.lastnz <= 2 and x[0] == 0 and x[1] == 0
                      and self.g_idx <= 0 and f_nf >= 7)

        if self.lsb_mode == 0:

            for i in range(len(xr)):

                if x[i] and xr[i] == 0:
                    x[i] += [ -0.1875, -0.3125 ][x[i] < 0]
                elif x[i]:
                    x[i] += [  0.1875,  0.3125 ][x[i] > 0]

        if not zero_frame:
            x = self.fill_noise(bw, x, self.lastnz, f_nf, nf_seed)

        ### Rescale coefficients

        g_int = self.get_gain_offset(nbytes) + self.g_idx
        x *= 10 ** (g_int / 28)

        return x


def initial_state():
    return { 'nbits_off' : 0.0, 'nbits_spare' : 0 }


### ------------------------------------------------------------------------ ###

def check_estimate_gain(rng, dt, sr):

    ok = True

    analysis = SpectrumAnalysis(dt, sr)

    mismatch_count = 0
    for i in range(10):
        ne = T.I[dt][sr][-1]
        x  = rng.random(ne) * i * 1e2

        nbytes = 20 + int(rng.random() * 100)
        nbits_budget = 8 * nbytes - int(rng.random() * 100)
        nbits_off = rng.random() * 10
        g_off = 10 - int(rng.random() * 20)

        (_, g_int, reset_off) = \
            analysis.estimate_gain(x, nbytes, nbits_budget, nbits_off, g_off)

        (g_int_c, reset_off_c, _) = lc3.spec_estimate_gain(
            dt, sr, x, nbytes, nbits_budget, nbits_off, -g_off)

        if g_int_c != g_int:
            mismatch_count += 1

        ok = ok and (g_int_c == g_int or mismatch_count <= 1)
        ok = ok and (reset_off_c == reset_off or mismatch_count <= 1)

    return ok

def check_quantization(rng, dt, sr):

    ok = True

    analysis = SpectrumAnalysis(dt, sr)

    for g_int in range(-128, 128):

        ne = T.I[dt][sr][-1]
        x  = rng.random(ne) * 1e2
        nbytes = 20 + int(rng.random() * 30)

        (xg, xq, nq) = analysis.quantize(g_int, x)
        (xg_c, nq_c) = lc3.spec_quantize(dt, sr, g_int, x)

        ok = ok and np.amax(np.abs(1 - xg_c/xg)) < 1e-6
        ok = ok and nq_c == nq

    return ok

def check_compute_nbits(rng, dt, sr):

    ok = True

    analysis = SpectrumAnalysis(dt, sr)

    for nbytes in range(20, 150):

        nbits_budget = nbytes * 8 - int(rng.random() * 100)
        ne = T.I[dt][sr][-1]
        xq = (rng.random(ne) * 8).astype(int)
        nq = ne // 2 + int(rng.random() * ne // 2)

        nq = nq - nq % 2
        if xq[nq-2] == 0 and xq[nq-1] == 0:
            xq[nq-2] = 1

        (nbits, nbits_trunc, nq_trunc, lsb_mode) = \
            analysis.compute_nbits(nbytes, xq, nq, nbits_budget)

        (nbits_c, nq_c, _) = \
            lc3.spec_compute_nbits(dt, sr, nbytes, xq, nq, 0)

        (nbits_trunc_c, nq_trunc_c, lsb_mode_c) = \
            lc3.spec_compute_nbits(dt, sr, nbytes, xq, nq, nbits_budget)

        ok = ok and nbits_c == nbits
        ok = ok and nbits_trunc_c == nbits_trunc
        ok = ok and nq_trunc_c == nq_trunc
        ok = ok and lsb_mode_c == lsb_mode

    return ok

def check_adjust_gain(rng, dt, sr):

    ok = True

    analysis = SpectrumAnalysis(dt, sr)

    for g_idx in (0, 128, 254, 255):
        for nbits in range(50, 5000, 5):
            nbits_budget = int(nbits * (0.95 + (rng.random() * 0.1)))

            g_adj = analysis.adjust_gain(g_idx, nbits, nbits_budget)

            g_adj_c = lc3.spec_adjust_gain(
                dt, sr, g_idx, nbits, nbits_budget, 0)

            ok = ok and g_adj_c == g_adj

    return ok

def check_unit(rng, dt, sr):

    ok = True

    state_c = initial_state()

    bwdet = m_bwdet.BandwidthDetector(dt, sr)
    ltpf = m_ltpf.LtpfAnalysis(dt, sr)
    tns = m_tns.TnsAnalysis(dt)
    sns = m_sns.SnsAnalysis(dt, sr)
    analysis = SpectrumAnalysis(dt, sr)

    nbytes = 100

    for i in range(10):
        ns = T.NS[dt][sr]
        ne = T.I[dt][sr][-1]

        x = rng.random(ns) * 1e4
        e = rng.random(min(len(x), 64)) * 1e10

        if sr < T.SRATE_48K_HR:
            bwdet.run(e)
        pitch_present = ltpf.run(x)
        tns.run(x[:ne], sr, False, nbytes)
        sns.run(e, False, 0, x)

        (xq, nq, xg) = analysis.run(sr, nbytes,
            0 if sr >= T.SRATE_48K_HR else bwdet.get_nbits(),
            ltpf.get_nbits(), sns.get_nbits(), tns.get_nbits(), x[:ne])

        (xg_c, side_c) = lc3.spec_analyze(dt, sr,
            nbytes, pitch_present, tns.get_data(), state_c, x[:ne])

        ok = ok and side_c['g_idx'] == analysis.g_idx
        ok = ok and side_c['nq'] == nq
        ok = ok and np.amax(np.abs(1 - xg_c/xg)) < 1e-6

    return ok

def check_noise(rng, dt, bw, hrmode = False):

    ok = True

    analysis = SpectrumAnalysis(dt, bw)

    xq_off = [ 0.375, 0.5 ][hrmode]

    for i in range(10):
        ne = T.I[dt][bw][-1]
        xq = ((rng.random(ne) - 0.5) * 10 ** (0.5)).astype(int)
        nq = ne - int(rng.random() * 5)
        x  = xq - np.select([xq < 0, xq > 0], np.array([ xq_off, -xq_off ]))

        nf = analysis.estimate_noise(bw, xq, nq, x)
        nf_c = lc3.spec_estimate_noise(dt, bw, hrmode, x, nq)

        ok = ok and nf_c == nf

    return ok

def check_appendix_c(dt):

    i0 = dt - T.DT_7M5
    sr = T.SRATE_16K

    ok = True

    state_c = initial_state()

    for i in range(len(C.X_F[i0])):

        ne = T.I[dt][sr][-1]

        g_int = lc3.spec_estimate_gain(dt, sr, C.X_F[i0][i],
            0, C.NBITS_SPEC[i0][i], C.NBITS_OFFSET[i0][i], -C.GG_OFF[i0][i])[0]
        ok = ok and g_int == C.GG_IND[i0][i] + C.GG_OFF[i0][i]

        (x, nq) = lc3.spec_quantize(dt, sr,
            C.GG_IND[i0][i] + C.GG_OFF[i0][i], C.X_F[i0][i])
        x += np.select([x < 0, x > 0], np.array([ 0.375, -0.375 ]))
        ok = ok and np.any((np.trunc(x) - C.X_Q[i0][i]) == 0)
        ok = ok and nq == C.LASTNZ[i0][i]
        nbits = lc3.spec_compute_nbits(dt, sr,
            C.NBYTES[i0], C.X_Q[i0][i], C.LASTNZ[i0][i], 0)[0]
        ok = ok and nbits == C.NBITS_EST[i0][i]

        g_adj = lc3.spec_adjust_gain(dt, sr,
            C.GG_IND[i0][i], C.NBITS_EST[i0][i], C.NBITS_SPEC[i0][i], 0)
        ok = ok and g_adj == C.GG_IND_ADJ[i0][i] - C.GG_IND[i0][i]

        if C.GG_IND_ADJ[i0][i] != C.GG_IND[i0][i]:

            (x, nq) = lc3.spec_quantize(dt, sr,
                C.GG_IND_ADJ[i0][i] + C.GG_OFF[i0][i], C.X_F[i0][i])
            lastnz = C.LASTNZ_REQ[i0][i]
            x += np.select([x < 0, x > 0], np.array([ 0.375, -0.375 ]))
            ok = ok and np.any(((np.trunc(x) - C.X_Q_REQ[i0][i])[:lastnz]) == 0)

        tns_data = {
            'nfilters' : C.NUM_TNS_FILTERS[i0][i],
            'lpc_weighting' : [ True, True ],
            'rc_order' : [ C.RC_ORDER[i0][i][0], 0 ],
            'rc' : [ C.RC_I_1[i0][i] - 8, np.zeros(8, dtype = np.intc) ]
        }

        (x, side) = lc3.spec_analyze(dt, sr, C.NBYTES[i0],
            C.PITCH_PRESENT[i0][i], tns_data, state_c, C.X_F[i0][i])

        xq = x + np.select([x < 0, x > 0], np.array([ 0.375, -0.375 ]))
        xq = np.trunc(xq)

        ok = ok and np.abs(state_c['nbits_off'] - C.NBITS_OFFSET[i0][i]) < 1e-5
        if C.GG_IND_ADJ[i0][i] != C.GG_IND[i0][i]:
            xq = C.X_Q_REQ[i0][i]
            nq = C.LASTNZ_REQ[i0][i]
            ok = ok and side['g_idx'] == C.GG_IND_ADJ[i0][i]
            ok = ok and side['nq'] == nq
            ok = ok and np.any(((xq[:nq] - xq[:nq])) == 0)
        else:
            xq = C.X_Q[i0][i]
            nq = C.LASTNZ[i0][i]
            ok = ok and side['g_idx'] == C.GG_IND[i0][i]
            ok = ok and side['nq'] == nq
            ok = ok and np.any((xq[:nq] - C.X_Q[i0][i][:nq]) == 0)
        ok = ok and side['lsb_mode'] == C.LSB_MODE[i0][i]

        gg = C.GG[i0][i] if C.GG_IND_ADJ[i0][i] == C.GG_IND[i0][i] \
                else C.GG_ADJ[i0][i]

        nf = lc3.spec_estimate_noise(
                dt, C.P_BW[i0][i], False, C.X_F[i0][i] / gg, nq)
        ok = ok and nf == C.F_NF[i0][i]

    return ok

def check():

    rng = np.random.default_rng(1234)
    ok = True

    for dt in range(T.NUM_DT):
        for sr in range(T.SRATE_8K, T.SRATE_48K + 1):
            ok = ok and check_estimate_gain(rng, dt, sr)
            ok = ok and check_quantization(rng, dt, sr)
            ok = ok and check_compute_nbits(rng, dt, sr)
            ok = ok and check_adjust_gain(rng, dt, sr)
            ok = ok and check_unit(rng, dt, sr)
            ok = ok and check_noise(rng, dt, sr)

    for dt in ( T.DT_2M5, T.DT_5M, T.DT_10M ):
        for sr in ( T.SRATE_48K_HR, T.SRATE_96K_HR ):
            ok = ok and check_estimate_gain(rng, dt, sr)
            ok = ok and check_quantization(rng, dt, sr)
            ok = ok and check_compute_nbits(rng, dt, sr)
            ok = ok and check_adjust_gain(rng, dt, sr)
            ok = ok and check_unit(rng, dt, sr)
            ok = ok and check_noise(rng, dt, sr, True)

    for dt in ( T.DT_7M5, T.DT_10M ):
        ok = ok and check_appendix_c(dt)

    return ok

### ------------------------------------------------------------------------ ###
