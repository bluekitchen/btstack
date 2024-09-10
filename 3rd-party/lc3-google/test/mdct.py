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
import scipy.fft

import lc3
import tables as T, appendix_c as C

### ------------------------------------------------------------------------ ###

class Mdct:

    W = [ [ T.W_2M5_8K , T.W_2M5_16K, T.W_2M5_24K,
            T.W_2M5_32K, T.W_2M5_48K, T.W_2M5_48K_HR, T.W_2M5_96K_HR ],

          [ T.W_5M_8K  , T.W_5M_16K , T.W_5M_24K ,
            T.W_5M_32K , T.W_5M_48K , T.W_5M_48K_HR , T.W_5M_96K_HR  ],

          [ T.W_7M5_8K , T.W_7M5_16K, T.W_7M5_24K,
            T.W_7M5_32K, T.W_7M5_48K, None, None ],

          [ T.W_10M_8K , T.W_10M_16K, T.W_10M_24K,
            T.W_10M_32K, T.W_10M_48K, T.W_10M_48K_HR, T.W_10M_96K_HR ] ]

    def __init__(self, dt, sr):

        self.ns = T.NS[dt][sr]
        self.nd = T.ND[dt][sr]

        self.t = np.zeros(2*self.ns)
        self.w = Mdct.W[dt][sr]


class MdctForward(Mdct):

    def __init__(self, dt, sr):

        super().__init__(dt, sr)

    def run(self, x):

        ns = self.ns
        nd = self.nd

        self.t[nd:nd+ns] = x
        t = self.t * self.w
        self.t[0:nd] = x[ns-nd:]

        n  = len(t)
        n2 = n // 2

        z = t * np.exp(-2j * np.pi * np.arange(n) / (2*n))
        z = scipy.fft.fft(z)[:n2]
        z = z * np.exp(-2j * np.pi *
                (n2/2 + 0.5) * (np.arange(n2) + 0.5) / (2 * n2))
        return np.real(z) * np.sqrt(2/n2)


class MdctInverse(Mdct):

    def __init__(self, dt, sr):

        super().__init__(dt, sr)

    def run(self, x):

        ns = self.ns
        nd = self.nd

        n = len(x)

        x = np.append(x, -x[::-1])
        z = x * np.exp(2j * np.pi * (n/2 + 0.5) * np.arange(2*n) / (2*n))
        z = scipy.fft.ifft(z) * n
        z = z * np.exp(2j * np.pi * (np.arange(2*n) + (n/2 + 0.5)) / (4*n))
        t = np.real(z) * np.sqrt(2/n)

        t = t * self.w[::-1]

        y = np.empty(ns)
        y[:nd] = t[ns-nd:ns] + self.t[2*ns-nd:]
        y[nd:] = t[ns:2*ns-nd]
        self.t = t

        return y

### ------------------------------------------------------------------------ ###

def check_forward_unit(rng, dt, sr):

    ns = T.NS[dt][sr]
    nd = T.ND[dt][sr]
    ok = True

    x = (2 * rng.random(ns)) - 1

    y   = [ None ] * 2
    y_c = [ None ] * 2

    mdct = MdctForward(dt, sr)
    y[0] = mdct.run(x)
    y[1] = mdct.run(x)

    (y_c[0], d_c) = lc3.mdct_forward(dt, sr, x, np.zeros(nd))
    y_c[1] = lc3.mdct_forward(dt, sr, x, d_c)[0]

    ok = ok and np.amax(np.abs(y[0] - y_c[0])) < 1e-5
    ok = ok and np.amax(np.abs(y[1] - y_c[1])) < 1e-5

    return ok


def check_forward_appendix_c(dt):

    i0 = dt - T.DT_7M5
    sr = T.SRATE_16K

    ok = True

    ns = T.NS[dt][sr]
    nd = T.ND[dt][sr]

    (y, d) = lc3.mdct_forward(dt, sr, C.X_PCM[i0][0], np.zeros(nd))
    ok = ok and np.amax(np.abs(y - C.X[i0][0])) < 1e-1

    (y, d) = lc3.mdct_forward(dt, sr, C.X_PCM[i0][1], d)
    ok = ok and np.amax(np.abs(y - C.X[i0][1])) < 1e-1

    return ok


def check_inverse_unit(rng, dt, sr):

    ns = T.NS[dt][sr]
    nd = T.ND[dt][sr]
    ok = True

    x  = (2 * rng.random(ns)) - 1

    y   = [ None ] * 2
    y_c = [ None ] * 2

    mdct = MdctInverse(dt, sr)
    y[0] = mdct.run(x)
    y[1] = mdct.run(x)

    (y_c[0], d_c) = lc3.mdct_inverse(dt, sr, x, np.zeros(nd))
    y_c[1] = lc3.mdct_inverse(dt, sr, x, d_c)[0]

    ok = ok and np.amax(np.abs(y[0] - y_c[0])) < 1e-5
    ok = ok and np.amax(np.abs(y[1] - y_c[1])) < 1e-5

    return ok


def check_inverse_appendix_c(dt):

    i0 = dt - T.DT_7M5
    sr = T.SRATE_16K

    ok = True

    ns = T.NS[dt][sr]
    nd = T.ND[dt][sr]

    (y, d0) = lc3.mdct_inverse(dt, sr, C.X_HAT_SNS[i0][0], np.zeros(nd))
    yr = C.T_HAT_MDCT[i0][0][ns-nd:2*ns-nd]
    dr = C.T_HAT_MDCT[i0][0][2*ns-nd:]

    ok = ok and np.amax(np.abs(yr - y )) < 1e-1
    ok = ok and np.amax(np.abs(dr - d0)) < 1e-1

    (y, d1) = lc3.mdct_inverse(dt, sr, C.X_HAT_SNS[i0][1], d0)
    yr[  :nd] = C.T_HAT_MDCT[i0][1][ns-nd:ns] + d0
    yr[nd:ns] = C.T_HAT_MDCT[i0][1][ns:2*ns-nd]
    dr        = C.T_HAT_MDCT[i0][1][2*ns-nd:]

    ok = ok and np.amax(np.abs(yr - y )) < 1e-1
    ok = ok and np.amax(np.abs(dr - d1)) < 1e-1

    return ok


def check():

    rng = np.random.default_rng(1234)

    ok  = True

    for dt in range(T.NUM_DT):
        for sr in range(T.SRATE_8K, T.SRATE_48K + 1):
            ok = ok and check_forward_unit(rng, dt, sr)
            ok = ok and check_inverse_unit(rng, dt, sr)

    for dt in ( T.DT_2M5, T.DT_5M, T.DT_10M ):
        for sr in ( T.SRATE_48K_HR, T.SRATE_96K_HR ):
            ok = ok and check_forward_unit(rng, dt, sr)
            ok = ok and check_inverse_unit(rng, dt, sr)

    for dt in ( T.DT_7M5, T.DT_10M ):
        ok = ok and check_forward_appendix_c(dt)
        ok = ok and check_inverse_appendix_c(dt)

    return ok
