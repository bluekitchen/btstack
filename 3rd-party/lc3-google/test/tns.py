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

### ------------------------------------------------------------------------ ###

class Tns:

    SUB_LIM_10M_NB   = [ [  12,  34,  57,  80 ] ]
    SUB_LIM_10M_WB   = [ [  12,  61, 110, 160 ] ]
    SUB_LIM_10M_SSWB = [ [  12,  88, 164, 240 ] ]
    SUB_LIM_10M_SWB  = [ [  12,  61, 110, 160 ], [ 160, 213, 266, 320 ] ]
    SUB_LIM_10M_FB   = [ [  12,  74, 137, 200 ], [ 200, 266, 333, 400 ] ]

    SUB_LIM_10M = [ SUB_LIM_10M_NB, SUB_LIM_10M_WB,
        SUB_LIM_10M_SSWB, SUB_LIM_10M_SWB, SUB_LIM_10M_FB ]

    SUB_LIM_7M5_NB   = [ [   9,  26,  43,  60 ] ]
    SUB_LIM_7M5_WB   = [ [   9,  46,  83, 120 ] ]
    SUB_LIM_7M5_SSWB = [ [   9,  66, 123, 180 ] ]
    SUB_LIM_7M5_SWB  = [ [   9,  46,  82, 120 ], [ 120, 159, 200, 240 ] ]
    SUB_LIM_7M5_FB   = [ [   9,  56, 103, 150 ], [ 150, 200, 250, 300 ] ]

    SUB_LIM_7M5 = [ SUB_LIM_7M5_NB, SUB_LIM_7M5_WB,
        SUB_LIM_7M5_SSWB, SUB_LIM_7M5_SWB, SUB_LIM_7M5_FB ]

    SUB_LIM = [ SUB_LIM_7M5, SUB_LIM_10M ]

    FREQ_LIM_10M_NB   = [  12,  80 ]
    FREQ_LIM_10M_WB   = [  12, 160 ]
    FREQ_LIM_10M_SSWB = [  12, 240 ]
    FREQ_LIM_10M_SWB  = [  12, 160, 320 ]
    FREQ_LIM_10M_FB   = [  12, 200, 400 ]

    FREQ_LIM_10M = [ FREQ_LIM_10M_NB, FREQ_LIM_10M_WB,
        FREQ_LIM_10M_SSWB, FREQ_LIM_10M_SWB, FREQ_LIM_10M_FB ]

    FREQ_LIM_7M5_NB   = [   9,  60 ]
    FREQ_LIM_7M5_WB   = [   9, 120 ]
    FREQ_LIM_7M5_SSWB = [   9, 180 ]
    FREQ_LIM_7M5_SWB  = [   9, 120, 240 ]
    FREQ_LIM_7M5_FB   = [   9, 150, 300 ]

    FREQ_LIM_7M5 = [ FREQ_LIM_7M5_NB, FREQ_LIM_7M5_WB,
        FREQ_LIM_7M5_SSWB, FREQ_LIM_7M5_SWB, FREQ_LIM_7M5_FB ]

    FREQ_LIM = [ FREQ_LIM_7M5, FREQ_LIM_10M ]

    def __init__(self, dt):

        self.dt = dt

        (self.nfilters, self.lpc_weighting, self.rc_order, self.rc) = \
            (0, False, np.array([ 0, 0 ]), np.array([ 0, 0 ]))

    def get_data(self):

        return { 'nfilters' : self.nfilters,
                 'lpc_weighting' : self.lpc_weighting,
                 'rc_order' : self.rc_order, 'rc' : self.rc - 8 }

    def get_nbits(self):

        lpc_weighting = self.lpc_weighting
        nbits = 0

        for f in range(self.nfilters):
            rc_order = self.rc_order[f]
            rc = self.rc[f]

            nbits_order = T.TNS_ORDER_BITS[int(lpc_weighting)][rc_order]
            nbits_coef = sum([ T.TNS_COEF_BITS[k][rc[k]]
                                  for k in range(rc_order) ])

            nbits += ((2048 + nbits_order + nbits_coef) + 2047) >> 11

        return nbits


class TnsAnalysis(Tns):

    def __init__(self, dt):

        super().__init__(dt)

    def compute_lpc_coeffs(self, bw, f, x):

        ### Normalized autocorrelation function

        S = Tns.SUB_LIM[self.dt][bw][f]

        r = np.append([ 3 ], np.zeros(8))
        e = [ sum(x[S[s]:S[s+1]] ** 2) for s in range(3) ]

        for k in range(len(r) if sum(e) > 0 else 0):
            c = [ np.dot(x[S[s]:S[s+1]-k], x[S[s]+k:S[s+1]])
                      for s in range(3) ]

            r[k] = np.sum( np.array(c) / np.array(e) )

        r *= np.exp(-0.5 * (0.02 * np.pi * np.arange(9)) ** 2)

        ### Levinson-Durbin recursion

        err = r[0]
        a = np.ones(len(r))

        for k in range(1, len(a)):

            rc = -sum(a[:k] * r[k:0:-1]) / err

            a[1:k] += rc * a[k-1:0:-1]
            a[k] = rc

            err *= 1 - rc ** 2

        return (r[0] / err, a)

    def lpc_weight(self, pred_gain, a):

        gamma = 1 - (1 - 0.85) * (2 - pred_gain) / (2 - 1.5)
        return a * np.power(gamma, np.arange(len(a)))

    def coeffs_reflexion(self, a):

        rc = np.zeros(8)
        b  = a.copy()

        for k in range(8, 0, -1):
            rc[k-1] = b[k]
            e = 1 - rc[k-1] ** 2
            b[1:k] = (b[1:k] - rc[k-1] * b[k-1:0:-1]) / e

        return rc

    def quantization(self, rc, lpc_weighting):

        delta = np.pi / 17
        rc_i = np.rint(np.arcsin(rc) / delta).astype(int) + 8
        rc_q = np.sin(delta * (rc_i - 8))

        rc_order = len(rc_i) - np.argmin(rc_i[::-1] == 8)

        return (rc_order, rc_q, rc_i)

    def filtering(self, st, x, rc_order, rc):

        y = np.empty(len(x))

        for i in range(len(x)):

            xi = x[i]
            s1 = xi

            for k in range(rc_order):
                s0 = st[k]
                st[k] = s1

                s1  = rc[k] * xi + s0
                xi += rc[k] * s0

            y[i] = xi

        return y

    def run(self, x, bw, nn_flag, nbytes):

        fstate = np.zeros(8)
        y = x.copy()

        self.nfilters = len(Tns.SUB_LIM[self.dt][bw])
        self.lpc_weighting = nbytes * 8 < 48 * T.DT_MS[self.dt]
        self.rc_order = np.zeros(2, dtype=np.int)
        self.rc = np.zeros((2, 8), dtype=np.int)

        for f in range(self.nfilters):

            (pred_gain, a) = self.compute_lpc_coeffs(bw, f, x)

            tns_off = pred_gain <= 1.5 or nn_flag
            if tns_off:
                continue

            if self.lpc_weighting and pred_gain < 2:
                a = self.lpc_weight(pred_gain, a)

            rc = self.coeffs_reflexion(a)

            (rc_order, rc_q, rc_i) = \
                self.quantization(rc, self.lpc_weighting)

            self.rc_order[f] = rc_order
            self.rc[f] = rc_i

            if rc_order > 0:
                i0 = Tns.FREQ_LIM[self.dt][bw][f]
                i1 = Tns.FREQ_LIM[self.dt][bw][f+1]

                y[i0:i1] = self.filtering(
                    fstate, x[i0:i1], rc_order, rc_q)

        return y

    def store(self, b):

        for f in range(self.nfilters):
            lpc_weighting = self.lpc_weighting
            rc_order = self.rc_order[f]
            rc = self.rc[f]

            b.write_bit(min(rc_order, 1))

            if rc_order > 0:
                b.ac_encode(
                    T.TNS_ORDER_CUMFREQ[int(lpc_weighting)][rc_order-1],
                    T.TNS_ORDER_FREQ[int(lpc_weighting)][rc_order-1]    )

            for k in range(rc_order):
                b.ac_encode(T.TNS_COEF_CUMFREQ[k][rc[k]],
                            T.TNS_COEF_FREQ[k][rc[k]]    )


class TnsSynthesis(Tns):

    def filtering(self, st, x, rc_order, rc):

        y = x.copy()

        for i in range(len(x)):

            xi = x[i] - rc[rc_order-1] * st[rc_order-1]
            for k in range(rc_order-2, -1, -1):
                xi -= rc[k] * st[k]
                st[k+1] = xi * rc[k] + st[k];
            st[0] = xi;

            y[i] = xi

        return y

    def load(self, b, bw, nbytes):

        self.nfilters = len(Tns.SUB_LIM[self.dt][bw])
        self.lpc_weighting = nbytes * 8 < 48 * T.DT_MS[self.dt]
        self.rc_order = np.zeros(2, dtype=np.int)
        self.rc = 8 * np.ones((2, 8), dtype=np.int)

        for f in range(self.nfilters):

            if not b.read_bit():
                continue

            rc_order = 1 + b.ac_decode(
                T.TNS_ORDER_CUMFREQ[int(self.lpc_weighting)],
                T.TNS_ORDER_FREQ[int(self.lpc_weighting)])

            self.rc_order[f] = rc_order

            for k in range(rc_order):
                rc = b.ac_decode(T.TNS_COEF_CUMFREQ[k], T.TNS_COEF_FREQ[k])
                self.rc[f][k] = rc

    def run(self, x, bw):

        fstate = np.zeros(8)
        y = x.copy()

        for f in range(self.nfilters):

            rc_order = self.rc_order[f]
            rc = np.sin((np.pi / 17) * (self.rc[f] - 8))

            if rc_order > 0:
                i0 = Tns.FREQ_LIM[self.dt][bw][f]
                i1 = Tns.FREQ_LIM[self.dt][bw][f+1]

                y[i0:i1] = self.filtering(
                    fstate, x[i0:i1], rc_order, rc)

        return y


### ------------------------------------------------------------------------ ###

def check_analysis(rng, dt, bw):

    ok = True

    analysis = TnsAnalysis(dt)
    nbytes_lim = int((48 * T.DT_MS[dt]) // 8)

    for i in range(10):
        x = rng.random(T.NE[dt][bw]) * 1e2
        x = pow(x, .5 + i/5)

        for nn_flag in (True, False):
            for nbytes in (nbytes_lim, nbytes_lim + 1):

                y = analysis.run(x, bw, nn_flag, nbytes)
                (y_c, data_c) = lc3.tns_analyze(dt, bw, nn_flag, nbytes, x)

                ok = ok and data_c['nfilters'] == analysis.nfilters
                ok = ok and data_c['lpc_weighting'] == analysis.lpc_weighting
                for f in range(analysis.nfilters):
                    rc_order = analysis.rc_order[f]
                    rc_order_c = data_c['rc_order'][f]
                    rc_c = 8 + data_c['rc'][f]
                    ok = ok and rc_order_c == rc_order
                    ok = ok and not np.any((rc_c - analysis.rc[f])[:rc_order])

                ok = ok and lc3.tns_get_nbits(data_c) == analysis.get_nbits()
                ok = ok and np.amax(np.abs(y_c - y)) < 1e-2

    return ok

def check_synthesis(rng, dt, bw):

    ok = True
    synthesis = TnsSynthesis(dt)

    for i in range(100):

        x = rng.random(T.NE[dt][bw]) * 1e2

        synthesis.nfilters = 1 + int(bw >= T.SRATE_32K)
        synthesis.rc_order = rng.integers(0, 9, 2)
        synthesis.rc = rng.integers(0, 17, 16).reshape(2, 8)

        y = synthesis.run(x, bw)
        y_c = lc3.tns_synthesize(dt, bw, synthesis.get_data(), x)

        ok = ok and np.amax(np.abs(y_c - y) < 1e-6)

    return ok

def check_analysis_appendix_c(dt):

    sr = T.SRATE_16K
    ok = True

    fs = Tns.FREQ_LIM[dt][sr][0]
    fe = Tns.FREQ_LIM[dt][sr][1]
    st = np.zeros(8)

    for i in range(len(C.X_S[dt])):

        (_, a) = lc3.tns_compute_lpc_coeffs(dt, sr, C.X_S[dt][i])
        ok = ok and np.amax(np.abs(a[0] - C.TNS_LEV_A[dt][i])) < 1e-5

        rc = lc3.tns_lpc_reflection(a[0])
        ok = ok and np.amax(np.abs(rc - C.TNS_LEV_RC[dt][i])) < 1e-5

        (rc_order, rc_i) = lc3.tns_quantize_rc(C.TNS_LEV_RC[dt][i])
        ok = ok and rc_order == C.RC_ORDER[dt][i][0]
        ok = ok and np.any((rc_i + 8) - C.RC_I_1[dt][i] == 0)

        rc_q = lc3.tns_unquantize_rc(rc_i, rc_order)
        ok = ok and np.amax(np.abs(rc_q - C.RC_Q_1[dt][i])) < 1e-6

        (x, side) = lc3.tns_analyze(dt, sr, False, C.NBYTES[dt], C.X_S[dt][i])
        ok = ok and side['nfilters'] == 1
        ok = ok and side['rc_order'][0] == C.RC_ORDER[dt][i][0]
        ok = ok and not np.any((side['rc'][0] + 8) - C.RC_I_1[dt][i])
        ok = ok and lc3.tns_get_nbits(side) == C.NBITS_TNS[dt][i]
        ok = ok and np.amax(np.abs(x - C.X_F[dt][i])) < 1e-3

    return ok

def check_synthesis_appendix_c(dt):

    sr = T.SRATE_16K
    ok = True

    for i in range(len(C.X_HAT_Q[dt])):

        side = {
            'nfilters' : 1,
            'lpc_weighting' : C.NBYTES[dt] * 8 < 48 * T.DT_MS[dt],
            'rc_order': C.RC_ORDER[dt][i],
            'rc': [ C.RC_I_1[dt][i] - 8, C.RC_I_2[dt][i] - 8 ]
        }

        g_int = C.GG_IND_ADJ[dt][i] + C.GG_OFF[dt][i]
        x = C.X_HAT_Q[dt][i] * (10 ** (g_int / 28))

        x = lc3.tns_synthesize(dt, sr, side, x)
        ok = ok and np.amax(np.abs(x - C.X_HAT_TNS[dt][i])) < 1e-3

    if dt != T.DT_10M:
        return ok

    sr = T.SRATE_48K

    side = {
        'nfilters' : 2,
        'lpc_weighting' : False,
        'rc_order': C.RC_ORDER_48K_10M,
        'rc': [ C.RC_I_1_48K_10M - 8, C.RC_I_2_48K_10M - 8 ]
    }

    x = C.X_HAT_F_48K_10M
    x = lc3.tns_synthesize(dt, sr, side, x)
    ok = ok and np.amax(np.abs(x - C.X_HAT_TNS_48K_10M)) < 1e-3

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
