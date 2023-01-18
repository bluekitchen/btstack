#!/usr/bin/env python3
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

import mdct, energy, bwdet, attdet
import ltpf, sns, tns, spec, encoder, decoder

ok = True

for m in [ ( mdct    , "MDCT"                   ),
           ( energy  , "Energy Band"            ),
           ( bwdet   , "Bandwidth Detector"     ),
           ( attdet  , "Attack Detector"        ),
           ( ltpf    , "Long Term Postfilter"   ),
           ( sns     , "Spectral Noise Shaping" ),
           ( tns     , "Temporal Noise Shaping" ),
           ( spec    , "Spectral Quantization"  ),
           ( encoder , "Encoder"                ),
           ( decoder , "Decoder"                ) ]:

    print('[{:^6}] {:}'.format('...', m[1]), end='\r', flush=True)
    ret = m[0].check()
    print('[{:^6}] {:}'.format('OK' if ret else 'FAILED', m[1]))

    ok = ok and ret

exit(0 if ok else 1)
