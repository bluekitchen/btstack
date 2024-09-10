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

class AttackDetector:

    def __init__(self, dt, sr):

        self.dt = dt
        self.sr = sr
        self.ms = T.DT_MS[dt]

        self.xn1 = 0
        self.xn2 = 0
        self.en1 = 0
        self.an1 = 0
        self.p_att = 0

    def is_enabled(self, nbytes):

        c1 = self.dt == T.DT_10M and \
             self.sr == T.SRATE_32K and nbytes > 80

        c2 = self.dt == T.DT_10M and \
             self.sr >= T.SRATE_48K and nbytes >= 100

        c3 = self.dt == T.DT_7M5 and \
             self.sr == T.SRATE_32K and nbytes >= 61 and nbytes < 150

        c4 = self.dt == T.DT_7M5 and \
             self.sr >= T.SRATE_48K and nbytes >= 75 and nbytes < 150

        return c1 or c2 or c3 or c4

    def run(self, nbytes, x):

        ### Downsampling and filtering input

        mf = int(16 * self.ms)

        r = len(x) // mf
        x_att = np.array([ np.sum(x[i*r:(i+1)*r]) for i in range(mf) ])

        x_hp = np.empty(mf)
        x_hp[0 ] = 0.375 * x_att[0 ] - 0.5 * self.xn1    + 0.125 * self.xn2
        x_hp[1 ] = 0.375 * x_att[1 ] - 0.5 * x_att[0   ] + 0.125 * self.xn1
        x_hp[2:] = 0.375 * x_att[2:] - 0.5 * x_att[1:-1] + 0.125 * x_att[0:-2]
        self.xn2 = x_att[-2]
        self.xn1 = x_att[-1]

        ### Energy calculation

        nb = int(self.ms / 2.5)

        e_att = np.array([ np.sum(np.square(x_hp[40*i:40*(i+1)]))
                           for i in range(nb) ])

        a_att = np.empty(nb)
        a_att[0] = np.maximum(0.25 * self.an1, self.en1)
        for i in range(1,nb):
            a_att[i] = np.maximum(0.25 * a_att[i-1], e_att[i-1])
        self.en1 = e_att[-1]
        self.an1 = a_att[-1]

        ### Attack Detection

        p_att = -1
        flags = [ (e_att[i] > 8.5 * a_att[i]) for i in range(nb) ]

        for (i, f) in enumerate(flags):
            if f: p_att = i

        f_att = p_att >= 0 or self.p_att - 1 >= nb // 2
        self.p_att = 1 + p_att

        return self.is_enabled(nbytes) and f_att


def initial_state():
    return { 'en1': 0.0, 'an1': 0.0, 'p_att': 0 }

### ------------------------------------------------------------------------ ###

def check_enabling(rng, dt):

    ok = True

    for sr in range(T.SRATE_8K, T.SRATE_48K + 1):

        attdet = AttackDetector(dt, sr)

        for nbytes in [ 61, 61-1, 75-1, 75, 80, 80+1, 100-1, 100, 150-1, 150 ]:

            f_att = lc3.attdet_run(dt, sr, nbytes,
                initial_state(), 2 * rng.random(T.NS[dt][sr]+6) - 1)

            ok = ok and f_att == attdet.is_enabled(nbytes)

    return ok

def check_unit(rng, dt, sr):

    ns = T.NS[dt][sr]
    ok = True

    attdet = AttackDetector(dt, sr)

    state_c = initial_state()
    x_c = np.zeros(ns+6)

    for run in range(100):

        ### Generate noise, and an attack at random point

        x = ((2 * rng.random(ns)) - 1) * (2 ** 8 - 1)
        x[(ns * rng.random()).astype(int)] *= 2 ** 7

        ### Check Implementation

        f_att = attdet.run(100, x)

        x_c = np.append(x_c[-6:], x)
        f_att_c = lc3.attdet_run(dt, sr, 100, state_c, x_c)

        ok = ok and f_att_c == f_att
        ok = ok and np.amax(np.abs(1 - state_c['en1']/attdet.en1)) < 2
        ok = ok and np.amax(np.abs(1 - state_c['an1']/attdet.an1)) < 2
        ok = ok and state_c['p_att'] == attdet.p_att

    return ok

def check_appendix_c(dt):

    i0 = dt - T.DT_7M5
    sr = T.SRATE_48K

    ok = True
    state = initial_state()

    x = np.append(np.zeros(6), C.X_PCM_ATT[i0][0])
    f_att = lc3.attdet_run(dt, sr, C.NBYTES_ATT[i0], state, x)
    ok = ok and f_att == C.F_ATT[i0][0]

    x = np.append(x[-6:], C.X_PCM_ATT[i0][1])
    f_att = lc3.attdet_run(dt, sr, C.NBYTES_ATT[i0], state, x)
    ok = ok and f_att == C.F_ATT[i0][1]

    return ok

def check():

    rng = np.random.default_rng(1234)
    ok = True

    for dt in range(T.NUM_DT):
        ok and check_enabling(rng, dt)

    for dt in ( T.DT_7M5, T.DT_10M ):
        for sr in ( T.SRATE_32K, T.SRATE_48K ):
            ok = ok and check_unit(rng, dt, sr)

    for dt in ( T.DT_7M5, T.DT_10M ):
        ok = ok and check_appendix_c(dt)

    return ok

### ------------------------------------------------------------------------ ###
