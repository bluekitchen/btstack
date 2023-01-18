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
import scipy.fftpack as fftpack

import lc3
import tables as T, appendix_c as C

### ------------------------------------------------------------------------ ###

class Sns:

    def __init__(self, dt, sr):

        self.dt = dt
        self.sr = sr

        (self.ind_lf, self.ind_hf, self.shape, self.gain) = \
            (None, None, None, None)

        (self.idx_a, self.ls_a, self.idx_b, self.ls_b) = \
            (None, None, None, None)

    def get_data(self):

        data = { 'lfcb' : self.ind_lf, 'hfcb' : self.ind_hf,
                 'shape' : self.shape, 'gain' : self.gain,
                 'idx_a' : self.idx_a, 'ls_a' : self.ls_a }

        if self.idx_b is not None:
            data.update({ 'idx_b' : self.idx_b, 'ls_b' : self.ls_b })

        return data

    def get_nbits(self):

        return 38

    def spectral_shaping(self, scf, inv, x):

        ## 3.3.7.4 Scale factors interpolation

        scf_i = np.empty(4*len(scf))
        scf_i[0     ] = scf[0]
        scf_i[1     ] = scf[0]
        scf_i[2:62:4] = scf[:15] + 1/8 * (scf[1:] - scf[:15])
        scf_i[3:63:4] = scf[:15] + 3/8 * (scf[1:] - scf[:15])
        scf_i[4:64:4] = scf[:15] + 5/8 * (scf[1:] - scf[:15])
        scf_i[5:64:4] = scf[:15] + 7/8 * (scf[1:] - scf[:15])
        scf_i[62    ] = scf[15 ] + 1/8 * (scf[15] - scf[14 ])
        scf_i[63    ] = scf[15 ] + 3/8 * (scf[15] - scf[14 ])

        n2 = 64 - min(len(x), 64)

        for i in range(n2):
            scf_i[i] = 0.5 * (scf_i[2*i] + scf_i[2*i+1])
        scf_i = np.append(scf_i[:n2], scf_i[2*n2:])

        g_sns = np.power(2, [ -scf_i, scf_i ][inv])

        ## 3.3.7.4 Spectral shaping

        y = np.empty(len(x))
        I = T.I[self.dt][self.sr]

        for b in range(len(g_sns)):
            y[I[b]:I[b+1]] = x[I[b]:I[b+1]] * g_sns[b]

        return y


class SnsAnalysis(Sns):

    def __init__(self, dt, sr):

        super().__init__(dt, sr)

    def compute_scale_factors(self, e, att):

        dt = self.dt

        ## 3.3.7.2.1 Padding

        n2 = 64 - len(e)

        e = np.append(np.empty(n2), e)
        for i in range(n2):
            e[2*i+0] = e[2*i+1] = e[n2+i]

        ## 3.3.7.2.2 Smoothing

        e_s = np.zeros(len(e))
        e_s[0   ] = 0.75 * e[0   ] + 0.25 * e[1   ]
        e_s[1:63] = 0.25 * e[0:62] + 0.5  * e[1:63] + 0.25 * e[2:64]
        e_s[  63] = 0.25 * e[  62] + 0.75 * e[  63]

        ## 3.3.7.2.3 Pre-emphasis

        g_tilt = [ 14, 18, 22, 26, 30 ][self.sr]
        e_p = e_s * (10 ** ((np.arange(64) * g_tilt) / 630))

        ## 3.3.7.2.4 Noise floor

        noise_floor = max(np.average(e_p) * (10 ** (-40/10)), 2 ** -32)
        e_p = np.fmax(e_p, noise_floor * np.ones(len(e)))

        ## 3.3.7.2.5 Logarithm

        e_l = np.log2(10 ** -31 + e_p) / 2

        ## 3.3.7.2.6 Band energy grouping

        w = [ 1/12, 2/12, 3/12, 3/12, 2/12, 1/12 ]

        e_4 = np.zeros(len(e_l) // 4)
        e_4[0   ] = w[0] * e_l[0] + np.sum(w[1:] * e_l[:5])
        e_4[1:15] = [ np.sum(w * e_l[4*i-1:4*i+5]) for i in range(1, 15) ]
        e_4[  15] = np.sum(w[:5] * e_l[59:64]) + w[5] * e_l[63]

        ## 3.3.7.2.7 Mean removal and scaling, attack handling

        scf = 0.85 * (e_4 - np.average(e_4))

        scf_a = np.zeros(len(scf))
        scf_a[0   ] = np.average(scf[:3])
        scf_a[1   ] = np.average(scf[:4])
        scf_a[2:14] = [ np.average(scf[i:i+5]) for i in range(12) ]
        scf_a[  14] = np.average(scf[12:])
        scf_a[  15] = np.average(scf[13:])

        scf_a = (0.5 if self.dt == T.DT_10M else 0.3) * \
                (scf_a - np.average(scf_a))

        return scf_a if att else scf

    def enum_mpvq(self, v):

        sign = None
        index = 0
        x = 0

        for (n, vn) in enumerate(v[::-1]):

            if sign is not None and vn != 0:
                index = 2*index + sign
            if vn != 0:
                sign = 1 if vn < 0 else 0

            index += T.SNS_MPVQ_OFFSETS[n][x]
            x += abs(vn)

        return (index, bool(sign))

    def quantize(self, scf):

        ## 3.3.7.3.2 Stage 1

        dmse_lf = [ np.sum((scf[:8] - T.SNS_LFCB[i]) ** 2) for i in range(32) ]
        dmse_hf = [ np.sum((scf[8:] - T.SNS_HFCB[i]) ** 2) for i in range(32) ]

        self.ind_lf = np.argmin(dmse_lf)
        self.ind_hf = np.argmin(dmse_hf)

        st1 = np.append(T.SNS_LFCB[self.ind_lf], T.SNS_HFCB[self.ind_hf])
        r1 = scf - st1

        ## 3.3.7.3.3 Stage 2

        t2_rot = fftpack.dct(r1, norm = 'ortho')
        x = np.abs(t2_rot)

        ## 3.3.7.3.3 Stage 2 Shape search, step 1

        K = 6

        proj_fac = (K - 1) / sum(np.abs(t2_rot))
        y3 = np.floor(x * proj_fac).astype(int)

        ## 3.3.7.3.3 Stage 2 Shape search, step 2

        corr_xy = np.sum(y3 * x)
        energy_y = np.sum(y3 * y3)

        k0 = sum(y3)
        for k in range(k0, K):
            q_pvq = ((corr_xy + x) ** 2) / (energy_y + 2*y3 + 1)
            n_best = np.argmax(q_pvq)

            corr_xy += x[n_best]
            energy_y += 2*y3[n_best] + 1
            y3[n_best] += 1

        ## 3.3.7.3.3 Stage 2 Shape search, step 3

        K = 8

        y2 = y3.copy()

        for k in range(sum(y2), K):
            q_pvq = ((corr_xy + x) ** 2) / (energy_y + 2*y2 + 1)
            n_best = np.argmax(q_pvq)

            corr_xy += x[n_best]
            energy_y += 2*y2[n_best] + 1
            y2[n_best] += 1


        ## 3.3.7.3.3 Stage 2 Shape search, step 4

        y1 = np.append(y2[:10], [0] * 6)

        ## 3.3.7.3.3 Stage 2 Shape search, step 5

        corr_xy -= sum(y2[10:] * x[10:])
        energy_y -= sum(y2[10:] * y2[10:])

        ## 3.3.7.3.3 Stage 2 Shape search, step 6

        K = 10

        for k in range(sum(y1), K):
            q_pvq = ((corr_xy + x[:10]) ** 2) / (energy_y + 2*y1[:10] + 1)
            n_best = np.argmax(q_pvq)

            corr_xy += x[n_best]
            energy_y += 2*y1[n_best] + 1
            y1[n_best] += 1

        ## 3.3.7.3.3 Stage 2 Shape search, step 7

        y0 = np.append(y1[:10], [ 0 ] * 6)

        q_pvq = ((corr_xy + x[10:]) ** 2) / (energy_y + 2*y0[10:] + 1)
        n_best = 10 + np.argmax(q_pvq)

        y0[n_best] += 1

        ## 3.3.7.3.3 Stage 2 Shape search, step 8

        y0 *= np.sign(t2_rot).astype(int)
        y1 *= np.sign(t2_rot).astype(int)
        y2 *= np.sign(t2_rot).astype(int)
        y3 *= np.sign(t2_rot).astype(int)

        ## 3.3.7.3.3 Stage 2 Shape search, step 9

        xq = [ y / np.sqrt(sum(y ** 2)) for y in (y0, y1, y2, y3) ]

        ## 3.3.7.3.3 Shape and gain combination determination

        G = [ T.SNS_VQ_REG_ADJ_GAINS, T.SNS_VQ_REG_LF_ADJ_GAINS,
              T.SNS_VQ_NEAR_ADJ_GAINS, T.SNS_VQ_FAR_ADJ_GAINS ]

        dMSE = [ [ sum((t2_rot - G[j][i] * xq[j]) ** 2)
                   for i in range(len(G[j])) ] for j in range(4) ]

        self.shape = np.argmin([ np.min(dMSE[j]) for j in range(4) ])
        self.gain = np.argmin(dMSE[self.shape])

        gain = G[self.shape][self.gain]

        ## 3.3.7.3.3 Enumeration of the selected PVQ pulse configurations

        if self.shape == 0:
            (self.idx_a, self.ls_a) = self.enum_mpvq(y0[:10])
            (self.idx_b, self.ls_b) = self.enum_mpvq(y0[10:])
        elif self.shape == 1:
            (self.idx_a, self.ls_a) = self.enum_mpvq(y1[:10])
            (self.idx_b, self.ls_b) = (None, None)
        elif self.shape == 2:
            (self.idx_a, self.ls_a) = self.enum_mpvq(y2)
            (self.idx_b, self.ls_b) = (None, None)
        elif self.shape == 3:
            (self.idx_a, self.ls_a) = self.enum_mpvq(y3)
            (self.idx_b, self.ls_b) = (None, None)

        ## 3.3.7.3.4 Synthesis of the Quantized scale factor

        scf_q = st1 + gain * fftpack.idct(xq[self.shape], norm = 'ortho')

        return scf_q

    def run(self, eb, att, x):

        scf = self.compute_scale_factors(eb, att)
        scf_q = self.quantize(scf)
        y = self.spectral_shaping(scf_q, False, x)

        return y

    def store(self, b):

        shape = self.shape
        gain_msb_bits = np.array([ 1, 1, 2, 2 ])[shape]
        gain_lsb_bits = np.array([ 0, 1, 0, 1 ])[shape]

        b.write_uint(self.ind_lf, 5)
        b.write_uint(self.ind_hf, 5)

        b.write_bit(shape >> 1)

        b.write_uint(self.gain >> gain_lsb_bits, gain_msb_bits)

        b.write_bit(self.ls_a)

        if self.shape == 0:
            sz_shape_a = 2390004
            index_joint = self.idx_a + \
                (2 * self.idx_b + self.ls_b + 2) * sz_shape_a

        elif self.shape == 1:
            sz_shape_a = 2390004
            index_joint = self.idx_a + (self.gain & 1) * sz_shape_a

        elif self.shape == 2:
            index_joint = self.idx_a

        elif self.shape == 3:
            sz_shape_a = 15158272
            index_joint = sz_shape_a + (self.gain & 1) + 2 * self.idx_a

        b.write_uint(index_joint, 14 - gain_msb_bits)
        b.write_uint(index_joint >> (14 - gain_msb_bits), 12)


class SnsSynthesis(Sns):

    def __init__(self, dt, sr):

        super().__init__(dt, sr)

    def deenum_mpvq(self, index, ls, npulses, n):

        y = np.zeros(n, dtype=np.int)
        pos = 0

        for i in range(len(y)-1, -1, -1):

            if index > 0:
                yi = 0
                while index < T.SNS_MPVQ_OFFSETS[i][npulses - yi]: yi += 1
                index -= T.SNS_MPVQ_OFFSETS[i][npulses - yi]
            else:
                yi = npulses

            y[pos] = [ yi, -yi ][int(ls)]
            pos += 1

            npulses -= yi
            if npulses <= 0:
                break

            if yi > 0:
                ls = index & 1
                index >>= 1

        return y

    def unquantize(self):

        ## 3.7.4.2.1-2  SNS VQ Decoding

        y = np.empty(16, dtype=np.int)

        if self.shape == 0:
            y[:10] = self.deenum_mpvq(self.idx_a, self.ls_a, 10, 10)
            y[10:] = self.deenum_mpvq(self.idx_b, self.ls_b,  1,  6)
        elif self.shape == 1:
            y[:10] = self.deenum_mpvq(self.idx_a, self.ls_a, 10, 10)
            y[10:] = np.zeros(6, dtype=np.int)
        elif self.shape == 2:
            y = self.deenum_mpvq(self.idx_a, self.ls_a, 8, 16)
        elif self.shape == 3:
            y = self.deenum_mpvq(self.idx_a, self.ls_a, 6, 16)

        ## 3.7.4.2.3  Unit energy normalization

        y = y / np.sqrt(sum(y ** 2))

        ## 3.7.4.2.4  Reconstruction of the quantized scale factors

        G = [ T.SNS_VQ_REG_ADJ_GAINS, T.SNS_VQ_REG_LF_ADJ_GAINS,
              T.SNS_VQ_NEAR_ADJ_GAINS, T.SNS_VQ_FAR_ADJ_GAINS ]

        gain = G[self.shape][self.gain]

        scf = np.append(T.SNS_LFCB[self.ind_lf], T.SNS_HFCB[self.ind_hf]) \
                + gain * fftpack.idct(y, norm = 'ortho')

        return scf

    def load(self, b):

        self.ind_lf = b.read_uint(5)
        self.ind_hf = b.read_uint(5)

        shape_msb = b.read_bit()

        gain_msb_bits = 1 + shape_msb
        self.gain = b.read_uint(gain_msb_bits)

        self.ls_a = b.read_bit()

        index_joint  = b.read_uint(14 - gain_msb_bits)
        index_joint |= b.read_uint(12) << (14 - gain_msb_bits)

        if shape_msb == 0:
            sz_shape_a = 2390004

            if index_joint >= sz_shape_a * 14:
                raise ValueError('Invalide SNS joint index')

            self.idx_a = index_joint % sz_shape_a
            index_joint = index_joint // sz_shape_a
            if index_joint >= 2:
                self.shape = 0
                self.idx_b = (index_joint - 2) // 2
                self.ls_b =  (index_joint - 2)  % 2
            else:
                self.shape = 1
                self.gain = (self.gain << 1) + (index_joint & 1)

        else:
            sz_shape_a = 15158272
            if index_joint >= sz_shape_a + 1549824:
                raise ValueError('Invalide SNS joint index')

            if index_joint < sz_shape_a:
                self.shape = 2
                self.idx_a = index_joint
            else:
                self.shape = 3
                index_joint -= sz_shape_a
                self.gain = (self.gain << 1) + (index_joint % 2)
                self.idx_a = index_joint // 2

    def run(self, x):

        scf = self.unquantize()
        y = self.spectral_shaping(scf, True, x)

        return y

### ------------------------------------------------------------------------ ###

def check_analysis(rng, dt, sr):

    ok = True

    analysis = SnsAnalysis(dt, sr)

    for i in range(10):
        x = rng.random(T.NE[dt][sr]) * 1e4
        e = rng.random(min(len(x), 64)) * 1e10

        for att in (0, 1):
            y = analysis.run(e, att, x)
            data = analysis.get_data()

            (y_c, data_c) = lc3.sns_analyze(dt, sr, e, att, x)

            for k in data.keys():
                ok = ok and data_c[k] == data[k]

            ok = ok and lc3.sns_get_nbits() == analysis.get_nbits()
            ok = ok and np.amax(np.abs(y - y_c)) < 1e-1

    return ok

def check_synthesis(rng, dt, sr):

    ok = True

    synthesis = SnsSynthesis(dt, sr)

    for i in range(100):

        synthesis.ind_lf = rng.integers(0, 32)
        synthesis.ind_hf = rng.integers(0, 32)

        shape = rng.integers(0, 4)
        sz_shape_a = [ 2390004, 2390004, 15158272, 774912 ][shape]
        sz_shape_b = [ 6, 1, 0, 0 ][shape]
        synthesis.shape = shape
        synthesis.gain = rng.integers(0, [ 2, 4, 4, 8 ][shape])
        synthesis.idx_a = rng.integers(0, sz_shape_a, endpoint=True)
        synthesis.ls_a = bool(rng.integers(0, 1, endpoint=True))
        synthesis.idx_b = rng.integers(0, sz_shape_b, endpoint=True)
        synthesis.ls_b = bool(rng.integers(0, 1, endpoint=True))

        x = rng.random(T.NE[dt][sr]) * 1e4

        y = synthesis.run(x)
        y_c = lc3.sns_synthesize(dt, sr, synthesis.get_data(), x)
        ok = ok and np.amax(np.abs(y - y_c)) < 2e0

    return ok

def check_analysis_appendix_c(dt):

    sr = T.SRATE_16K
    ok = True

    for i in range(len(C.E_B[dt])):

        scf = lc3.sns_compute_scale_factors(dt, sr, C.E_B[dt][i], False)
        ok = ok and np.amax(np.abs(scf - C.SCF[dt][i])) < 1e-4

        (lf, hf) = lc3.sns_resolve_codebooks(scf)
        ok = ok and lf == C.IND_LF[dt][i] and hf == C.IND_HF[dt][i]

        (y, yn, shape, gain) = lc3.sns_quantize(scf, lf, hf)
        ok = ok and np.any(y[0][:16] - C.SNS_Y0[dt][i] == 0)
        ok = ok and np.any(y[1][:10] - C.SNS_Y1[dt][i] == 0)
        ok = ok and np.any(y[2][:16] - C.SNS_Y2[dt][i] == 0)
        ok = ok and np.any(y[3][:16] - C.SNS_Y3[dt][i] == 0)
        ok = ok and shape == 2*C.SUBMODE_MSB[dt][i] + C.SUBMODE_LSB[dt][i]
        ok = ok and gain == C.G_IND[dt][i]

        scf_q = lc3.sns_unquantize(lf, hf, yn[shape], shape, gain)
        ok = ok and np.amax(np.abs(scf_q - C.SCF_Q[dt][i])) < 1e-5

        x = lc3.sns_spectral_shaping(dt, sr, C.SCF_Q[dt][i], False, C.X[dt][i])
        ok = ok and np.amax(np.abs(1 - x/C.X_S[dt][i])) < 1e-5

        (x, data) = lc3.sns_analyze(dt, sr, C.E_B[dt][i], False, C.X[dt][i])
        ok = ok and data['lfcb'] == C.IND_LF[dt][i]
        ok = ok and data['hfcb'] == C.IND_HF[dt][i]
        ok = ok and data['shape'] == \
            2*C.SUBMODE_MSB[dt][i] + C.SUBMODE_LSB[dt][i]
        ok = ok and data['gain'] == C.G_IND[dt][i]
        ok = ok and data['idx_a'] == C.IDX_A[dt][i]
        ok = ok and data['ls_a'] == C.LS_IND_A[dt][i]
        ok = ok and (C.IDX_B[dt][i] is None or
            data['idx_b'] == C.IDX_B[dt][i])
        ok = ok and (C.LS_IND_B[dt][i] is None or
            data['ls_b'] == C.LS_IND_B[dt][i])
        ok = ok and np.amax(np.abs(1 - x/C.X_S[dt][i])) < 1e-5

    return ok

def check_synthesis_appendix_c(dt):

    sr = T.SRATE_16K
    ok = True

    for i in range(len(C.X_HAT_TNS[dt])):

        data = {
            'lfcb'  : C.IND_LF[dt][i], 'hfcb' : C.IND_HF[dt][i],
            'shape' : 2*C.SUBMODE_MSB[dt][i] + C.SUBMODE_LSB[dt][i],
            'gain'  : C.G_IND[dt][i],
            'idx_a' : C.IDX_A[dt][i],
            'ls_a'  : C.LS_IND_A[dt][i],
            'idx_b' : C.IDX_B[dt][i] if C.IDX_B[dt][i] is not None else 0,
            'ls_b'  : C.LS_IND_B[dt][i] if C.LS_IND_B[dt][i] is not None else 0,
        }

        x = lc3.sns_synthesize(dt, sr, data, C.X_HAT_TNS[dt][i])
        ok = ok and np.amax(np.abs(x - C.X_HAT_SNS[dt][i])) < 1e0

    return ok

def check():

    rng = np.random.default_rng(1234)
    ok = True

    for dt in range(T.NUM_DT):
        for sr in range(T.NUM_SRATE):
            ok = ok and check_analysis(rng, dt, sr)
            ok = ok and check_synthesis(rng, dt, sr)

    for dt in range(T.NUM_DT):
        ok = ok and check_analysis_appendix_c(dt)
        ok = ok and check_synthesis_appendix_c(dt)

    return ok

### ------------------------------------------------------------------------ ###
