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

### ------------------------------------------------------------------------ ###

class EnergyBand:

    def __init__(self, dt, sr):

        self.dt = dt
        self.I = T.I[dt][sr]

    def compute(self, x):

        e = [ np.mean(np.square(x[self.I[i]:self.I[i+1]]))
               for i in range(len(self.I)-1) ]

        e_lo = np.sum(e[:len(e) - [4, 2][self.dt]])
        e_hi = np.sum(e[len(e) - [4, 2][self.dt]:])

        return np.append(e, np.zeros(64-len(e))), (e_hi > 30*e_lo)

### ------------------------------------------------------------------------ ###

def check_unit(rng, dt, sr):

    ns = T.NS[dt][sr]
    ok = True

    nrg = EnergyBand(dt, sr)

    x = (2 * rng.random(T.NS[dt][sr])) - 1

    (e  , nn  ) = nrg.compute(x)
    (e_c, nn_c) = lc3.energy_compute(dt, sr, x)
    ok = ok and np.amax(np.abs(e_c - e)) < 1e-5 and nn_c == nn

    x[15*ns//16:] *= 1e2;

    (e  , nn  ) = nrg.compute(x)
    (e_c, nn_c) = lc3.energy_compute(dt, sr, x)
    ok = ok and np.amax(np.abs(e_c - e)) < 1e-3 and nn_c == nn

    return ok

def check_appendix_c(dt):

    sr = T.SRATE_16K
    ok = True

    e  = lc3.energy_compute(dt, sr, C.X[dt][0])[0]
    ok = ok and np.amax(np.abs(1 - e/C.E_B[dt][0])) < 1e-6

    e  = lc3.energy_compute(dt, sr, C.X[dt][1])[0]
    ok = ok and np.amax(np.abs(1 - e/C.E_B[dt][1])) < 1e-6

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
