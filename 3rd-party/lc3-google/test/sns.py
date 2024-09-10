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
        self.I = T.I[dt][sr]

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

        ## Scale factors interpolation

        scf_i = np.empty(4*len(scf))
        scf_i[0     ] = scf[0]
        scf_i[1     ] = scf[0]
        scf_i[2:62:4] = scf[:15] + 1/8 * (scf[1:] - scf[:15])
        scf_i[3:63:4] = scf[:15] + 3/8 * (scf[1:] - scf[:15])
        scf_i[4:64:4] = scf[:15] + 5/8 * (scf[1:] - scf[:15])
        scf_i[5:64:4] = scf[:15] + 7/8 * (scf[1:] - scf[:15])
        scf_i[62    ] = scf[15 ] + 1/8 * (scf[15] - scf[14 ])
        scf_i[63    ] = scf[15 ] + 3/8 * (scf[15] - scf[14 ])

        nb = len(self.I) - 1

        if nb < 32:
            n4 = round(abs(1-32/nb)*nb)
            n2 = nb - n4

            for i in range(n4):
                scf_i[i] = np.mean(scf_i[4*i:4*i+4])

            for i in range(n4, n4+n2):
                scf_i[i] = np.mean(scf_i[2*n4+2*i:2*n4+2*i+2])

            scf_i = scf_i[:n4+n2]

        elif nb < 64:
            n2 = 64 - nb

            for i in range(n2):
                scf_i[i] = np.mean(scf_i[2*i:2*i+2])
            scf_i = np.append(scf_i[:n2], scf_i[2*n2:])

        g_sns = np.power(2, [ -scf_i, scf_i ][inv])

        ## Spectral shaping

        y = np.empty(len(x))
        I = self.I

        for b in range(nb):
            y[I[b]:I[b+1]] = x[I[b]:I[b+1]] * g_sns[b]

        return y


class SnsAnalysis(Sns):

    def __init__(self, dt, sr):

        super().__init__(dt, sr)

    def compute_scale_factors(self, e, att, nbytes):

        dt = self.dt
        sr = self.sr
        hr = self.sr >= T.SRATE_48K_HR

        ## Padding

        if len(e) < 32:
            n4 = round(abs(1-32/len(e))*len(e))
            n2 = len(e) - n4

            e = np.append(np.zeros(3*n4+n2), e)
            for i in range(n4):
                e[4*i+0] = e[4*i+1] = \
                e[4*i+2] = e[4*i+3] = e[3*n4+n2+i]

            for i in range(2*n4, 2*n4+n2):
                e[2*i+0] = e[2*i+1] = e[2*n4+n2+i]

        elif len(e) < 64:
            n2 = 64 - len(e)

            e = np.append(np.empty(n2), e)
            for i in range(n2):
                e[2*i+0] = e[2*i+1] = e[n2+i]

        ## Smoothing

        e_s = np.zeros(len(e))
        e_s[0   ] = 0.75 * e[0   ] + 0.25 * e[1   ]
        e_s[1:63] = 0.25 * e[0:62] + 0.5  * e[1:63] + 0.25 * e[2:64]
        e_s[  63] = 0.25 * e[  62] + 0.75 * e[  63]

        ## Pre-emphasis

        g_tilt = [ 14, 18, 22, 26, 30, 30, 34 ][self.sr]
        e_p = e_s * (10 ** ((np.arange(64) * g_tilt) / 630))

        ## Noise floor

        noise_floor = max(np.average(e_p) * (10 ** (-40/10)), 2 ** -32)
        e_p = np.fmax(e_p, noise_floor * np.ones(len(e)))

        ## Logarithm

        e_l = np.log2(10 ** -31 + e_p) / 2

        ## Band energy grouping

        w = [ 1/12, 2/12, 3/12, 3/12, 2/12, 1/12 ]

        e_4 = np.zeros(len(e_l) // 4)
        e_4[0   ] = w[0] * e_l[0] + np.sum(w[1:] * e_l[:5])
        e_4[1:15] = [ np.sum(w * e_l[4*i-1:4*i+5]) for i in range(1, 15) ]
        e_4[  15] = np.sum(w[:5] * e_l[59:64]) + w[5] * e_l[63]

        ## Mean removal and scaling, attack handling

        cf = [ 0.85, 0.6 ][hr]
        if hr and nbytes * 8 > [ 1150, 2300, 0, 4400 ][self.dt]:
            cf *= [ 0.25, 0.35 ][ self.dt == T.DT_10M ]

        scf = cf * (e_4 - np.average(e_4))

        scf_a = np.zeros(len(scf))
        scf_a[0   ] = np.mean(scf[:3])
        scf_a[1   ] = np.mean(scf[:4])
        scf_a[2:14] = [ np.mean(scf[i:i+5]) for i in range(12) ]
        scf_a[  14] = np.mean(scf[12:])
        scf_a[  15] = np.mean(scf[13:])

        scf_a = (0.5 if self.dt != T.DT_7M5 else 0.3) * \
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

        ## Stage 1

        dmse_lf = [ np.sum((scf[:8] - T.SNS_LFCB[i]) ** 2) for i in range(32) ]
        dmse_hf = [ np.sum((scf[8:] - T.SNS_HFCB[i]) ** 2) for i in range(32) ]

        self.ind_lf = np.argmin(dmse_lf)
        self.ind_hf = np.argmin(dmse_hf)

        st1 = np.append(T.SNS_LFCB[self.ind_lf], T.SNS_HFCB[self.ind_hf])
        r1 = scf - st1

        ## Stage 2

        t2_rot = fftpack.dct(r1, norm = 'ortho')
        x = np.abs(t2_rot)

        ## Stage 2 Shape search, step 1

        K = 6

        proj_fac = (K - 1) / sum(np.abs(t2_rot))
        y3 = np.floor(x * proj_fac).astype(int)

        ## Stage 2 Shape search, step 2

        corr_xy = np.sum(y3 * x)
        energy_y = np.sum(y3 * y3)

        k0 = sum(y3)
        for k in range(k0, K):
            q_pvq = ((corr_xy + x) ** 2) / (energy_y + 2*y3 + 1)
            n_best = np.argmax(q_pvq)

            corr_xy += x[n_best]
            energy_y += 2*y3[n_best] + 1
            y3[n_best] += 1

        ## Stage 2 Shape search, step 3

        K = 8

        y2 = y3.copy()

        for k in range(sum(y2), K):
            q_pvq = ((corr_xy + x) ** 2) / (energy_y + 2*y2 + 1)
            n_best = np.argmax(q_pvq)

            corr_xy += x[n_best]
            energy_y += 2*y2[n_best] + 1
            y2[n_best] += 1


        ## Stage 2 Shape search, step 4

        y1 = np.append(y2[:10], [0] * 6)

        ## Stage 2 Shape search, step 5

        corr_xy -= sum(y2[10:] * x[10:])
        energy_y -= sum(y2[10:] * y2[10:])

        ## Stage 2 Shape search, step 6

        K = 10

        for k in range(sum(y1), K):
            q_pvq = ((corr_xy + x[:10]) ** 2) / (energy_y + 2*y1[:10] + 1)
            n_best = np.argmax(q_pvq)

            corr_xy += x[n_best]
            energy_y += 2*y1[n_best] + 1
            y1[n_best] += 1

        ## Stage 2 Shape search, step 7

        y0 = np.append(y1[:10], [ 0 ] * 6)

        q_pvq = ((corr_xy + x[10:]) ** 2) / (energy_y + 2*y0[10:] + 1)
        n_best = 10 + np.argmax(q_pvq)

        y0[n_best] += 1

        ## Stage 2 Shape search, step 8

        y0 *= np.sign(t2_rot).astype(int)
        y1 *= np.sign(t2_rot).astype(int)
        y2 *= np.sign(t2_rot).astype(int)
        y3 *= np.sign(t2_rot).astype(int)

        ## Stage 2 Shape search, step 9

        xq = [ y / np.sqrt(sum(y ** 2)) for y in (y0, y1, y2, y3) ]

        ## Shape and gain combination determination

        G = [ T.SNS_VQ_REG_ADJ_GAINS, T.SNS_VQ_REG_LF_ADJ_GAINS,
              T.SNS_VQ_NEAR_ADJ_GAINS, T.SNS_VQ_FAR_ADJ_GAINS ]

        dMSE = [ [ sum((t2_rot - G[j][i] * xq[j]) ** 2)
                   for i in range(len(G[j])) ] for j in range(4) ]

        self.shape = np.argmin([ np.min(dMSE[j]) for j in range(4) ])
        self.gain = np.argmin(dMSE[self.shape])

        gain = G[self.shape][self.gain]

        ## Enumeration of the selected PVQ pulse configurations

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

        ## Synthesis of the Quantized scale factor

        scf_q = st1 + gain * fftpack.idct(xq[self.shape], norm = 'ortho')

        return scf_q

    def run(self, eb, att, nbytes, x):

        scf = self.compute_scale_factors(eb, att, nbytes)
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

        y = np.zeros(n, dtype=np.intc)
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

        ## SNS VQ Decoding

        y = np.empty(16, dtype=np.intc)

        if self.shape == 0:
            y[:10] = self.deenum_mpvq(self.idx_a, self.ls_a, 10, 10)
            y[10:] = self.deenum_mpvq(self.idx_b, self.ls_b,  1,  6)
        elif self.shape == 1:
            y[:10] = self.deenum_mpvq(self.idx_a, self.ls_a, 10, 10)
            y[10:] = np.zeros(6, dtype=np.intc)
        elif self.shape == 2:
            y = self.deenum_mpvq(self.idx_a, self.ls_a, 8, 16)
        elif self.shape == 3:
            y = self.deenum_mpvq(self.idx_a, self.ls_a, 6, 16)

        ## Unit energy normalization

        y = y / np.sqrt(sum(y ** 2))

        ## Reconstruction of the quantized scale factors

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
        ne = T.I[dt][sr][-1]
        x  = rng.random(ne) * 1e4
        e  = rng.random(len(T.I[dt][sr]) - 1) * 1e10

        if sr >= T.SRATE_48K_HR:
            for nbits in (1144, 1152, 2296, 2304, 4400, 4408):
                y = analysis.run(e, False, nbits // 8, x)
                data = analysis.get_data()

                (y_c, data_c) = lc3.sns_analyze(
                    dt, sr, nbits // 8, e, False, x)

                for k in data.keys():
                    ok = ok and data_c[k] == data[k]

                ok = ok and lc3.sns_get_nbits() == analysis.get_nbits()
                ok = ok and np.amax(np.abs(y - y_c)) < 1e-1

        else:
            for att in (0, 1):
                y = analysis.run(e, att, 0, x)
                data = analysis.get_data()

                (y_c, data_c) = lc3.sns_analyze(dt, sr, 0, e, att, x)

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

        ne = T.I[dt][sr][-1]
        x  = rng.random(ne) * 1e4

        y = synthesis.run(x)
        y_c = lc3.sns_synthesize(dt, sr, synthesis.get_data(), x)
        ok = ok and np.amax(np.abs(1 - y/y_c)) < 1e-5

    return ok

def check_analysis_appendix_c(dt):

    i0 = dt - T.DT_7M5
    sr = T.SRATE_16K

    ok = True

    for i in range(len(C.E_B[i0])):

        scf = lc3.sns_compute_scale_factors(dt, sr, 0, C.E_B[i0][i], False)
        ok = ok and np.amax(np.abs(scf - C.SCF[i0][i])) < 1e-4

        (lf, hf) = lc3.sns_resolve_codebooks(scf)
        ok = ok and lf == C.IND_LF[i0][i] and hf == C.IND_HF[i0][i]

        (y, yn, shape, gain) = lc3.sns_quantize(scf, lf, hf)
        ok = ok and np.any(y[0][:16] - C.SNS_Y0[i0][i] == 0)
        ok = ok and np.any(y[1][:10] - C.SNS_Y1[i0][i] == 0)
        ok = ok and np.any(y[2][:16] - C.SNS_Y2[i0][i] == 0)
        ok = ok and np.any(y[3][:16] - C.SNS_Y3[i0][i] == 0)
        ok = ok and shape == 2*C.SUBMODE_MSB[i0][i] + C.SUBMODE_LSB[i0][i]
        ok = ok and gain == C.G_IND[i0][i]

        scf_q = lc3.sns_unquantize(lf, hf, yn[shape], shape, gain)
        ok = ok and np.amax(np.abs(scf_q - C.SCF_Q[i0][i])) < 1e-5

        x = lc3.sns_spectral_shaping(dt, sr, C.SCF_Q[i0][i], False, C.X[i0][i])
        ok = ok and np.amax(np.abs(1 - x/C.X_S[i0][i])) < 1e-5

        (x, data) = lc3.sns_analyze(dt, sr, 0, C.E_B[i0][i], False, C.X[i0][i])
        ok = ok and data['lfcb'] == C.IND_LF[i0][i]
        ok = ok and data['hfcb'] == C.IND_HF[i0][i]
        ok = ok and data['shape'] == 2*C.SUBMODE_MSB[i0][i] + \
                                       C.SUBMODE_LSB[i0][i]
        ok = ok and data['gain'] == C.G_IND[i0][i]
        ok = ok and data['idx_a'] == C.IDX_A[i0][i]
        ok = ok and data['ls_a'] == C.LS_IND_A[i0][i]
        ok = ok and (C.IDX_B[i0][i] is None or
            data['idx_b'] == C.IDX_B[i0][i])
        ok = ok and (C.LS_IND_B[i0][i] is None or
            data['ls_b'] == C.LS_IND_B[i0][i])
        ok = ok and np.amax(np.abs(1 - x/C.X_S[i0][i])) < 1e-5

    return ok

def check_synthesis_appendix_c(dt):

    i0 = dt - T.DT_7M5
    sr = T.SRATE_16K

    ok = True

    for i in range(len(C.X_HAT_TNS[i0])):

        data = {
            'lfcb'  : C.IND_LF[i0][i], 'hfcb' : C.IND_HF[i0][i],
            'shape' : 2*C.SUBMODE_MSB[i0][i] + C.SUBMODE_LSB[i0][i],
            'gain'  : C.G_IND[i0][i],
            'idx_a' : C.IDX_A[i0][i],
            'ls_a'  : C.LS_IND_A[i0][i],
            'idx_b' : C.IDX_B[i0][i] if C.IDX_B[i0][i] is not None else 0,
            'ls_b'  : C.LS_IND_B[i0][i] if C.LS_IND_B[i0][i] is not None else 0,
        }

        x = lc3.sns_synthesize(dt, sr, data, C.X_HAT_TNS[i0][i])
        ok = ok and np.amax(np.abs(x - C.X_HAT_SNS[i0][i])) < 1e0

    return ok

def check():

    rng = np.random.default_rng(1234)
    ok = True

    for dt in range(T.NUM_DT):
        for sr in range(T.SRATE_8K, T.SRATE_48K + 1):
            ok = ok and check_analysis(rng, dt, sr)
            ok = ok and check_synthesis(rng, dt, sr)

    for dt in ( T.DT_2M5, T.DT_5M, T.DT_10M ):
        for sr in ( T.SRATE_48K_HR, T.SRATE_96K_HR ):
            ok = ok and check_analysis(rng, dt, sr)
            ok = ok and check_synthesis(rng, dt, sr)

    for dt in ( T.DT_7M5, T.DT_10M ):
        check_analysis_appendix_c(dt)
        check_synthesis_appendix_c(dt)

    return ok

### ------------------------------------------------------------------------ ###
