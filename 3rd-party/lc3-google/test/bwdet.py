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

import build.lc3 as lc3
import tables as T, appendix_c as C


BW_START = [
    [ [], [ 51 ], [ 45, 58 ], [ 42, 53, 60 ], [ 40, 51, 57, 61 ] ],
    [ [], [ 53 ], [ 47, 59 ], [ 44, 54, 60 ], [ 41, 51, 57, 61 ] ]
]

BW_STOP = [
    [ [], [ 63 ], [ 55, 63 ], [ 51, 58, 63 ], [ 48, 55, 60, 63 ] ],
    [ [], [ 63 ], [ 56, 63 ], [ 52, 59, 63 ], [ 49, 55, 60, 63 ] ]
]

TQ = [ 20, 10, 10, 10 ]

TC = [ 15, 23, 20, 20 ]
L  = [ [ 4, 4, 3, 2 ], [ 4, 4, 3, 1 ] ]


### ------------------------------------------------------------------------ ###

class BandwidthDetector:

    def __init__(self, dt, sr):

        self.dt = dt
        self.sr = sr

    def run(self, e):

        dt = self.dt
        sr = self.sr

        ### Stage 1, determine bw0 candidate

        bw0 = 0

        for bw in range(sr):
            i0 = BW_START[dt][sr][bw]
            i1 = BW_STOP[dt][sr][bw]
            if np.mean(e[i0:i1+1]) >= TQ[bw]:
                bw0 = bw + 1

        ### Stage 2, Cut-off random coefficients at each steps

        bw = bw0

        if bw0 < sr:
            l  = L[dt][bw0]
            i0 = BW_START[dt][sr][bw0] - l
            i1 = BW_START[dt][sr][bw0]

            c = 10 * np.log10(1e-31 + e[i0-l+1:i1-l+2] / e[i0+1:i1+2])
            if np.amax(c) <= TC[bw0]:
                bw = sr

        self.bw = bw
        return self.bw

    def get_nbits(self):

        return 0 if self.sr == 0 else \
               1 + np.log2(self.sr).astype(int)

    def store_bw(self, b):

        b.write_uint(self.bw, self.get_nbits())

    def get_bw(self, b):

        return b.read_uint(self.get_nbits())

### ------------------------------------------------------------------------ ###

def check_unit(rng, dt, sr):

    ok = True

    bwdet = BandwidthDetector(dt, sr)

    for bw0 in range(sr+1):
        for drop in range(10):

            ### Generate random 'high' energy and
            ### scale relevant bands to select 'bw0'

            e = 20 + 100 * rng.random(64)

            for i in range(sr):
                if i+1 != bw0:
                    i0 = BW_START[dt][sr][i]
                    i1 = BW_STOP[dt][sr][i]
                    e[i0:i1+1] /= (np.mean(e[i0:i1+1]) / TQ[i] + 1e-3)

            ### Stage 2 Condition,
            ### cut-off random coefficients at each steps

            if bw0 < sr:
                l  = L[dt][bw0]
                i0 = BW_START[dt][sr][bw0] - l
                i1 = BW_START[dt][sr][bw0]

                e[i0-l+1:i1+2] /= np.power(10, np.arange(2*l+1) / (1 + drop))

            ### Check with implementation

            bw_c = lc3.bwdet_run(dt, sr, e)

            ok = ok and bw_c == bwdet.run(e)

    return ok

def check_appendix_c(dt):

    sr = T.SRATE_16K
    ok = True

    E_B  = C.E_B[dt]
    P_BW = C.P_BW[dt]

    bw = lc3.bwdet_run(dt, sr, E_B[0])
    ok = ok and bw == P_BW[0]

    bw = lc3.bwdet_run(dt, sr, E_B[1])
    ok = ok and bw == P_BW[1]

    return ok

def check():

    rng = np.random.default_rng(1234)

    ok = True
    for dt in range(T.NUM_DT):
        for sr in range(T.NUM_SRATE):
            ok = ok and check_unit(rng, dt, sr)

    for dt in range(T.NUM_DT):
        ok = ok and check_appendix_c(dt)

    return ok

### ------------------------------------------------------------------------ ###
