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

import lc3
import tables as T, appendix_c as C

### ------------------------------------------------------------------------ ###

class Resampler_12k8:

    def __init__(self, dt, sr, history = 0):

        self.sr = sr
        self.p = 192 // T.SRATE_KHZ[sr]
        self.w = 240 // self.p

        self.n = ((T.DT_MS[dt] * 128) / 10).astype(int)
        self.d = [ 44, 24 ][dt]

        self.x = np.zeros(self.w + T.NS[dt][sr])
        self.u = np.zeros(self.n + 2)
        self.y = np.zeros(self.n + self.d + history)

    def resample(self, x):

        p = self.p
        w = self.w
        d = self.d
        n = self.n

        ### Sliding window

        self.x[:w] = self.x[-w:]
        self.x[w:] = x
        self.u[:2] = self.u[-2:]

        if len(self.y) > 2*n + d:
            self.y[n+d:-n] = self.y[d+2*n:]
        if len(self.y) > n + d:
            self.y[-n:] = self.y[:n]
        self.y[:d] = self.y[n:d+n]

        x = self.x
        u = self.u

        ### 3.3.9.3 Resampling

        h = np.zeros(240 + p)
        h[-119:] = T.LTPF_H12K8[:119]
        h[ :120] = T.LTPF_H12K8[119:]

        for i in range(n):
            e = (15 * i) // p
            f = (15 * i)  % p
            k = np.arange(-120, 120 + p, p) - f
            u[2+i] = p * np.dot( x[e:e+w+1], np.take(h, k) )

        if self.sr == T.SRATE_8K:
            u = 0.5 * u

        ### 3.3.9.4 High-pass filtering

        b = [ 0.9827947082978771, -1.9655894165957540, 0.9827947082978771 ]
        a = [ 1                 , -1.9652933726226904, 0.9658854605688177 ]

        self.y[d:d+n] = b[0] * u[2:] + b[1] * u[1:-1] + b[2] * u[:-2]
        for i in range(n):
            self.y[d+i] -= a[1] * self.y[d+i-1] + a[2] * self.y[d+i-2]

        return self.y


class Resampler_6k4:

    def __init__(self, n, history = 0):

        self.x = np.zeros(n + 5)
        self.n = n // 2

        self.y = np.zeros(self.n + history)

    def resample(self, x):

        n = self.n

        ### Sliding window

        self.x[:3] = self.x[-5:-2]
        self.x[3:] = x[:2*n+2]
        x = self.x

        if len(self.y) > 2*n:
            self.y[n:-n] = self.y[2*n:]
        if len(self.y) > n:
            self.y[-n:] = self.y[:n]

        ### 3.3.9.5 Downsampling to 6.4 KHz

        h = [ 0.1236796411180537, 0.2353512128364889, 0.2819382920909148,
              0.2353512128364889, 0.1236796411180537 ]

        self.y[:n] = [ np.dot(x[2*i:2*i+5], h) for i in range(self.n) ]
        return self.y


def initial_hp50_state():
    return { 's1': 0, 's2': 0 }

### ------------------------------------------------------------------------ ###

class Ltpf:

    def __init__(self, dt, sr):

        self.dt = dt
        self.sr = sr

        (self.pitch_present, self.pitch_index) = (None, None)


class LtpfAnalysis(Ltpf):

    def __init__(self, dt, sr):

        super().__init__(dt, sr)

        self.resampler_12k8 = Resampler_12k8(
                dt, sr, history = 232)

        self.resampler_6k4 = Resampler_6k4(
                self.resampler_12k8.n, history = 114)

        self.active = False
        self.tc = 0
        self.pitch = 0
        self.nc = np.zeros(2)

    def get_data(self):

        return { 'active' : self.active,
                 'pitch_index' : self.pitch_index }

    def get_nbits(self):

        return 1 + 10 * int(self.pitch_present)

    def correlate(self, x, n, k0, k1):

        return [ np.dot(x[:n], np.take(x, np.arange(n) - k)) \
                    for k in range(k0, 1+k1) ]

    def norm_corr(self, x, n, k):

        u  = x[:n]
        v  = np.take(x, np.arange(n) - k)
        uv = np.dot(u, v)
        return uv / np.sqrt(np.dot(u, u) * np.dot(v, v)) if uv > 0 else 0

    def run(self, x):

        ### 3.3.9.3-4 Resampling

        x_12k8 = self.resampler_12k8.resample(x)

        ### 3.3.9.5-6 Pitch detection algorithm

        x = self.resampler_6k4.resample(x_12k8)
        n = self.resampler_6k4.n

        r  = self.correlate(x, n, 17, 114)
        rw = r * (1 - 0.5 * np.arange(len(r)) / (len(r) - 1))

        tc = self.tc
        k0 = max(0, tc-4)
        k1 = min(len(r)-1, tc+4)
        t  = [ 17 + np.argmax(rw), 17 + k0 + np.argmax(r[k0:1+k1]) ]

        nc = [ self.norm_corr(x, n, t[i]) for i in range(2) ]
        ti = int(nc[1] > 0.85 * nc[0])
        self.tc = t[ti] - 17

        self.pitch_present = bool(nc[ti] > 0.6)

        ### 3.3.9.7 Pitch-lag parameter

        if self.pitch_present:
            tc = self.tc + 17

            x = x_12k8
            n = self.resampler_12k8.n

            k0 = max( 32, 2*tc-4)
            k1 = min(228, 2*tc+4)
            r  = self.correlate(x, n, k0-4, k1+4)
            e  = k0 + np.argmax(r[4:-4])

            h = np.zeros(42)
            h[-15:] = T.LTPF_H4[:15]
            h[ :16] = T.LTPF_H4[15:]

            m = np.arange(-4, 5)
            s = [ np.dot( np.take(r, e-k0+4 + m), np.take(h, 4*m-d) ) \
                      for d in range(-3, 4) ]

            f = np.argmax(s[3:])            if e <=  32 else \
                -3 + np.argmax(s)           if e <  127 else \
                -2 + 2*np.argmax(s[1:-1:2]) if e <  157 else 0

            e -=   (f < 0)
            f += 4*(f < 0)

            self.pitch_index = 4*e + f    - 128 if e < 127 else \
                               2*e + f//2 + 126 if e < 157 else e + 283

        else:
            e = f = 0
            self.pitch_index = 0

        ### 3.3.9.8 Activation bit

        h = np.zeros(24)
        h[-7:] = T.LTPF_HI[:7]
        h[ :8] = T.LTPF_HI[7:]

        k = np.arange(-2, 3)
        u = [ np.dot( np.take(x, i-k), np.take(h, 4*k) ) \
                  for i in range(n) ]
        v = [ np.dot( np.take(x, i-k), np.take(h, 4*k-f) ) \
                  for i in range(-e, n-e) ]

        nc = max(0, np.dot(u, v)) / np.sqrt(np.dot(u, u) * np.dot(v, v)) \
                if self.pitch_present else 0

        pitch = e + f/4

        if not self.active:
            active = (self.dt == T.DT_10M or self.nc[1] > 0.94) \
                     and self.nc[0] > 0.94 and nc > 0.94

        else:
            dp = abs(pitch - self.pitch)
            dc = nc - self.nc[0]
            active = nc > 0.9 or (dp < 2 and dc > -0.1 and nc > 0.84)

        if not self.pitch_present:
            active = False
            pitch = 0
            nc = 0

        self.active = active
        self.pitch = pitch
        self.nc[1] = self.nc[0]
        self.nc[0] = nc

        return self.pitch_present

    def disable(self):

        self.active = False

    def store(self, b):

        b.write_uint(self.active, 1)
        b.write_uint(self.pitch_index, 9)


class LtpfSynthesis(Ltpf):

    C_N = [ T.LTPF_N_8K , T.LTPF_N_16K,
            T.LTPF_N_24K, T.LTPF_N_32K, T.LTPF_N_48K ]

    C_D = [ T.LTPF_D_8K , T.LTPF_D_16K,
            T.LTPF_D_24K, T.LTPF_D_32K, T.LTPF_D_48K ]

    def __init__(self, dt, sr):

        super().__init__(dt, sr)

        self.C_N = LtpfSynthesis.C_N[sr]
        self.C_D = LtpfSynthesis.C_D[sr]

        ns = T.NS[dt][sr]

        self.active = [ False, False ]
        self.pitch_index = 0

        max_pitch_12k8 = 228
        max_pitch = max_pitch_12k8 * T.SRATE_KHZ[self.sr] / 12.8
        max_pitch = np.ceil(max_pitch).astype(int)

        self.x = np.zeros(ns)
        self.y = np.zeros(max_pitch + len(self.C_D[0]))

        self.p_e = [ 0, 0 ]
        self.p_f = [ 0, 0 ]
        self.c_n = [ None, None ]
        self.c_d = [ None, None ]

    def load(self, b):

        self.active[0] = bool(b.read_uint(1))
        self.pitch_index = b.read_uint(9)

    def disable(self):

        self.active[0] = False
        self.pitch_index = 0

    def run(self, x, nbytes):

        sr = self.sr
        dt = self.dt

        ### 3.4.9.4 Filter parameters

        pitch_index = self.pitch_index

        if pitch_index >= 440:
            p_e = pitch_index - 283
            p_f = 0
        elif pitch_index >= 380:
            p_e = pitch_index // 2 - 63
            p_f = 2*(pitch_index - 2*(p_e + 63))
        else:
            p_e = pitch_index // 4 + 32
            p_f = pitch_index - 4*(p_e - 32)

        p = (p_e + p_f / 4) * T.SRATE_KHZ[self.sr] / 12.8

        self.p_e[0] = int(p * 4 + 0.5) // 4
        self.p_f[0] = int(p * 4 + 0.5) - 4*self.p_e[0]

        nbits = round(nbytes*80 / T.DT_MS[dt])
        g_idx = max(nbits // 80, 3+sr) - (3+sr)

        g = [ 0.4, 0.35, 0.3, 0.25 ][g_idx] if g_idx < 4 else 0
        g_idx = min(g_idx, 3)

        self.c_n[0] = 0.85 * g * LtpfSynthesis.C_N[sr][g_idx]
        self.c_d[0] = g * LtpfSynthesis.C_D[sr][self.p_f[0]]

        ### 3.4.9.2 Transition handling

        n0 = (T.SRATE_KHZ[sr] * 1000) // 400
        ns = T.NS[dt][sr]

        x  = np.append(x, self.x)
        y  = np.append(np.zeros(ns), self.y)
        yc = y.copy()

        c_n = self.c_n
        c_d = self.c_d

        l_n = len(c_n[0])
        l_d = len(c_d[0])

        d = [ self.p_e[0] - (l_d - 1) // 2,
              self.p_e[1] - (l_d - 1) // 2 ]

        for k in range(n0):

            if not self.active[0] and not self.active[1]:
                y[k] = x[k]

            elif self.active[0] and not self.active[1]:
                u = np.dot(c_n[0], np.take(x, k - np.arange(l_n))) - \
                    np.dot(c_d[0], np.take(y, k - d[0] - np.arange(l_d)))
                y[k] = x[k] - (k/n0) * u

            elif not self.active[0] and self.active[1]:
                u = np.dot(c_n[1], np.take(x, k - np.arange(l_n))) - \
                    np.dot(c_d[1], np.take(y, k - d[1] - np.arange(l_d)))
                y[k] = x[k] - (1 - k/n0) * u

            elif self.p_e[0] == self.p_e[1] and self.p_f[0] == self.p_f[1]:
                u = np.dot(c_n[0], np.take(x, k - np.arange(l_n))) - \
                    np.dot(c_d[0], np.take(y, k - d[0] - np.arange(l_d)))
                y[k] = x[k] - u

            else:
                u = np.dot(c_n[1], np.take(x, k - np.arange(l_n))) - \
                    np.dot(c_d[1], np.take(y, k - d[1] - np.arange(l_d)))
                yc[k] = x[k] - (1 - k/n0) * u

                u = np.dot(c_n[0], np.take(yc, k - np.arange(l_n))) - \
                    np.dot(c_d[0], np.take(y , k - d[0] - np.arange(l_d)))
                y[k] = yc[k] - (k/n0) * u


        ### 3.4.9.3 Remainder of the frame

        for k in range(n0, ns):

            if not self.active[0]:
                y[k] = x[k]

            else:
                u = np.dot(c_n[0], np.take(x, k - np.arange(l_n))) - \
                    np.dot(c_d[0], np.take(y, k - d[0] - np.arange(l_d)))
                y[k] = x[k] - u

        ### Sliding window

        self.active[1] = self.active[0]
        self.p_e[1] = self.p_e[0]
        self.p_f[1] = self.p_f[0]
        self.c_n[1] = self.c_n[0]
        self.c_d[1] = self.c_d[0]

        self.x = x[:ns]
        self.y = np.append(self.y[ns:], y[:ns])

        return y[:ns]

def initial_state():
    return { 'active' : False, 'pitch': 0, 'nc':  np.zeros(2),
             'hp50' : initial_hp50_state(),
             'x_12k8' : np.zeros(384), 'x_6k4' : np.zeros(178), 'tc' : 0 }

def initial_sstate():
    return { 'active': False, 'pitch': 0,
             'c': np.zeros(2*12), 'x': np.zeros(12) }

### ------------------------------------------------------------------------ ###

def check_resampler(rng, dt, sr):

    ns = T.NS[dt][sr]
    nt = (5 * T.SRATE_KHZ[sr]) // 4
    ok = True

    r = Resampler_12k8(dt, sr)

    hp50_c = initial_hp50_state()
    x_c = np.zeros(nt)
    y_c = np.zeros(384)

    for run in range(10):

        x = ((2 * rng.random(ns)) - 1) * (2 ** 15 - 1)
        y = r.resample(x)

        x_c = np.append(x_c[-nt:], x.astype(np.int16))
        y_c[:-r.n] = y_c[r.n:]
        y_c = lc3.ltpf_resample(dt, sr, hp50_c, x_c, y_c)

        ok = ok and np.amax(np.abs(y_c[-r.d-r.n:] - y[:r.d+r.n]/2)) < 4

    return ok

def check_resampler_appendix_c(dt):

    sr = T.SRATE_16K
    ok = True

    nt = (5 * T.SRATE_KHZ[sr]) // 4
    n  = [ 96, 128 ][dt]
    k  = [ 44,  24 ][dt] + n

    state = initial_hp50_state()

    x = np.append(np.zeros(nt), C.X_PCM[dt][0])
    y = np.zeros(384)
    y = lc3.ltpf_resample(dt, sr, state, x, y)
    u = y[-k:len(C.X_TILDE_12K8D[dt][0])-k]

    ok = ok and np.amax(np.abs(u - C.X_TILDE_12K8D[dt][0]/2)) < 2

    x = np.append(x[-nt:], C.X_PCM[dt][1])
    y[:-n] = y[n:]
    y = lc3.ltpf_resample(dt, sr, state, x, y)
    u = y[-k:len(C.X_TILDE_12K8D[dt][1])-k]

    ok = ok and np.amax(np.abs(u - C.X_TILDE_12K8D[dt][1]/2)) < 2

    return ok

def check_analysis(rng, dt, sr):

    ns = T.NS[dt][sr]
    nt = (5 * T.SRATE_KHZ[sr]) // 4
    ok = True

    state_c = initial_state()
    x_c = np.zeros(ns+nt)

    ltpf = LtpfAnalysis(dt, sr)

    t = np.arange(100 * ns) / (T.SRATE_KHZ[sr] * 1000)
    s = signal.chirp(t, f0=10, f1=3e3, t1=t[-1], method='logarithmic')

    for i in range(20):

        x = s[i*ns:(i+1)*ns] * (2 ** 15 - 1)

        pitch_present = ltpf.run(x)
        data = ltpf.get_data()

        x_c = np.append(x_c[-nt:], x.astype(np.int16))
        (pitch_present_c, data_c) = lc3.ltpf_analyse(dt, sr, state_c, x_c)

        ok = ok and (not pitch_present or state_c['tc'] == ltpf.tc)
        ok = ok and np.amax(np.abs(state_c['nc'][0] - ltpf.nc[0])) < 1e-2
        ok = ok and pitch_present_c == pitch_present
        ok = ok and data_c['active'] == data['active']
        ok = ok and data_c['pitch_index'] == data['pitch_index']
        ok = ok and lc3.ltpf_get_nbits(pitch_present) == ltpf.get_nbits()

    return ok

def check_synthesis(rng, dt, sr):

    ok = True

    ns = T.NS[dt][sr]
    nd = 18 * T.SRATE_KHZ[sr]

    synthesis = LtpfSynthesis(dt, sr)

    state_c = initial_sstate()
    x_c = np.zeros(nd+ns)

    for i in range(50):
        pitch_present = bool(rng.integers(0, 10) >= 1)
        if not pitch_present:
            synthesis.disable()
        else:
            synthesis.active[0] = bool(rng.integers(0, 5) >= 1)
            synthesis.pitch_index = rng.integers(0, 512)

        data_c = None if not pitch_present else \
            { 'active' : synthesis.active[0],
              'pitch_index' : synthesis.pitch_index }

        x = rng.random(ns) * 1e4
        nbytes = rng.integers(10*(2+sr), 10*(6+sr))

        x_c[:nd] = x_c[ns:]
        x_c[nd:] = x

        y = synthesis.run(x, nbytes)
        x_c = lc3.ltpf_synthesize(dt, sr, nbytes, state_c, data_c, x_c)

        ok = ok and np.amax(np.abs(x_c[nd:] - y)) < 1e-2

    return ok

def check_analysis_appendix_c(dt):

    sr = T.SRATE_16K
    nt = (5 * T.SRATE_KHZ[sr]) // 4
    ok = True

    state = initial_state()

    x = np.append(np.zeros(nt), C.X_PCM[dt][0])
    (pitch_present, data) = lc3.ltpf_analyse(dt, sr, state, x)

    ok = ok and C.T_CURR[dt][0] - state['tc'] == 17
    ok = ok and np.amax(np.abs(state['nc'][0] - C.NC_LTPF[dt][0])) < 1e-5
    ok = ok and pitch_present == C.PITCH_PRESENT[dt][0]
    ok = ok and data['pitch_index'] == C.PITCH_INDEX[dt][0]
    ok = ok and data['active'] == C.LTPF_ACTIVE[dt][0]

    x = np.append(x[-nt:], C.X_PCM[dt][1])
    (pitch_present, data) = lc3.ltpf_analyse(dt, sr, state, x)

    ok = ok and C.T_CURR[dt][1] - state['tc'] == 17
    ok = ok and np.amax(np.abs(state['nc'][0] - C.NC_LTPF[dt][1])) < 1e-5
    ok = ok and pitch_present == C.PITCH_PRESENT[dt][1]
    ok = ok and data['pitch_index'] == C.PITCH_INDEX[dt][1]
    ok = ok and data['active'] == C.LTPF_ACTIVE[dt][1]

    return ok

def check_synthesis_appendix_c(dt):

    sr = T.SRATE_16K
    ok = True

    if dt != T.DT_10M:
        return ok

    ns = T.NS[dt][sr]
    nd = 18 * T.SRATE_KHZ[sr]

    NBYTES = [ C.LTPF_C2_NBITS // 8, C.LTPF_C3_NBITS // 8,
               C.LTPF_C4_NBITS // 8, C.LTPF_C5_NBITS // 8 ]

    ACTIVE = [ C.LTPF_C2_ACTIVE, C.LTPF_C3_ACTIVE,
               C.LTPF_C4_ACTIVE, C.LTPF_C5_ACTIVE ]

    PITCH_INDEX = [ C.LTPF_C2_PITCH_INDEX, C.LTPF_C3_PITCH_INDEX,
                    C.LTPF_C4_PITCH_INDEX, C.LTPF_C5_PITCH_INDEX ]

    X = [ C.LTPF_C2_X, C.LTPF_C3_X,
          C.LTPF_C4_X, C.LTPF_C5_X ]

    PREV = [ C.LTPF_C2_PREV, C.LTPF_C3_PREV,
             C.LTPF_C4_PREV, C.LTPF_C5_PREV  ]

    TRANS = [ C.LTPF_C2_TRANS, C.LTPF_C3_TRANS,
              C.LTPF_C4_TRANS, C.LTPF_C5_TRANS ]

    for i in range(4):

        state = initial_sstate()
        nbytes = NBYTES[i]

        data = { 'active' : ACTIVE[i][0], 'pitch_index' : PITCH_INDEX[i][0] }
        x = np.append(np.zeros(nd), X[i][0])

        lc3.ltpf_synthesize(dt, sr, nbytes, state, data, x)

        data = { 'active' : ACTIVE[i][1], 'pitch_index' : PITCH_INDEX[i][1] }
        x[  :nd-ns] = PREV[i][0][-nd+ns:]
        x[nd-ns:nd] = PREV[i][1]
        x[nd:nd+ns] = X[i][1]

        y = lc3.ltpf_synthesize(dt, sr, nbytes, state, data, x)[nd:]

        ok = ok and np.amax(np.abs(y - TRANS[i])) < 1e-3

    return ok

def check():

    rng = np.random.default_rng(1234)
    ok = True

    for dt in range(T.NUM_DT):
        for sr in range(T.NUM_SRATE):
            ok = ok and check_resampler(rng, dt, sr)
            ok = ok and check_analysis(rng, dt, sr)
            ok = ok and check_synthesis(rng, dt, sr)

    for dt in range(T.NUM_DT):
        ok = ok and check_resampler_appendix_c(dt)
        ok = ok and check_analysis_appendix_c(dt)
        ok = ok and check_synthesis_appendix_c(dt)

    return ok

### ------------------------------------------------------------------------ ###
