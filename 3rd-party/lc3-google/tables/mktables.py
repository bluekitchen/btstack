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

import numpy as np

LTPF_H12K8 = np.array([
    -2.04305583e-05, -4.46345894e-05, -7.16366399e-05, -1.00101113e-04,
    -1.28372848e-04, -1.54543830e-04, -1.76544567e-04, -1.92256960e-04,
    -1.99643819e-04, -1.96888686e-04, -1.82538332e-04, -1.55639427e-04,
    -1.15860365e-04, -6.35893034e-05,  2.81006480e-19,  7.29218021e-05,
     1.52397076e-04,  2.34920777e-04,  3.16378650e-04,  3.92211738e-04,
     4.57623849e-04,  5.07824294e-04,  5.38295523e-04,  5.45072918e-04,
     5.25022155e-04,  4.76098424e-04,  3.97571380e-04,  2.90200217e-04,
     1.56344667e-04, -5.81880142e-19, -1.73252713e-04, -3.56385965e-04,
    -5.41155231e-04, -7.18414023e-04, -8.78505232e-04, -1.01171451e-03,
    -1.10876706e-03, -1.16134522e-03, -1.16260169e-03, -1.10764097e-03,
    -9.93941563e-04, -8.21692190e-04, -5.94017766e-04, -3.17074654e-04,
     9.74695082e-19,  3.45293760e-04,  7.04480871e-04,  1.06133447e-03,
     1.39837473e-03,  1.69763080e-03,  1.94148675e-03,  2.11357591e-03,
     2.19968245e-03,  2.18860625e-03,  2.07294546e-03,  1.84975249e-03,
     1.52102188e-03,  1.09397426e-03,  5.81108062e-04, -1.42248266e-18,
    -6.27153730e-04, -1.27425140e-03, -1.91223839e-03, -2.51026925e-03,
    -3.03703830e-03, -3.46222687e-03, -3.75800672e-03, -3.90053247e-03,
    -3.87135231e-03, -3.65866558e-03, -3.25835851e-03, -2.67475555e-03,
    -1.92103305e-03, -1.01925433e-03,  1.86962369e-18,  1.09841545e-03,
     2.23113197e-03,  3.34830927e-03,  4.39702277e-03,  5.32342672e-03,
     6.07510531e-03,  6.60352025e-03,  6.86645399e-03,  6.83034270e-03,
     6.47239234e-03,  5.78237521e-03,  4.76401273e-03,  3.43586351e-03,
     1.83165284e-03, -2.25189837e-18, -1.99647619e-03, -4.08266886e-03,
    -6.17308037e-03, -8.17444895e-03, -9.98882386e-03, -1.15169871e-02,
    -1.26621006e-02, -1.33334458e-02, -1.34501120e-02, -1.29444881e-02,
    -1.17654154e-02, -9.88086732e-03, -7.28003640e-03, -3.97473021e-03,
     2.50961778e-18,  4.58604422e-03,  9.70324900e-03,  1.52512477e-02,
     2.11120585e-02,  2.71533724e-02,  3.32324245e-02,  3.92003203e-02,
     4.49066644e-02,  5.02043309e-02,  5.49542017e-02,  5.90297032e-02,
     6.23209727e-02,  6.47385023e-02,  6.62161245e-02,  6.67132287e-02,
     6.62161245e-02,  6.47385023e-02,  6.23209727e-02,  5.90297032e-02,
     5.49542017e-02,  5.02043309e-02,  4.49066644e-02,  3.92003203e-02,
     3.32324245e-02,  2.71533724e-02,  2.11120585e-02,  1.52512477e-02,
     9.70324900e-03,  4.58604422e-03,  2.50961778e-18, -3.97473021e-03,
    -7.28003640e-03, -9.88086732e-03, -1.17654154e-02, -1.29444881e-02,
    -1.34501120e-02, -1.33334458e-02, -1.26621006e-02, -1.15169871e-02,
    -9.98882386e-03, -8.17444895e-03, -6.17308037e-03, -4.08266886e-03,
    -1.99647619e-03, -2.25189837e-18,  1.83165284e-03,  3.43586351e-03,
     4.76401273e-03,  5.78237521e-03,  6.47239234e-03,  6.83034270e-03,
     6.86645399e-03,  6.60352025e-03,  6.07510531e-03,  5.32342672e-03,
     4.39702277e-03,  3.34830927e-03,  2.23113197e-03,  1.09841545e-03,
     1.86962369e-18, -1.01925433e-03, -1.92103305e-03, -2.67475555e-03,
    -3.25835851e-03, -3.65866558e-03, -3.87135231e-03, -3.90053247e-03,
    -3.75800672e-03, -3.46222687e-03, -3.03703830e-03, -2.51026925e-03,
    -1.91223839e-03, -1.27425140e-03, -6.27153730e-04, -1.42248266e-18,
     5.81108062e-04,  1.09397426e-03,  1.52102188e-03,  1.84975249e-03,
     2.07294546e-03,  2.18860625e-03,  2.19968245e-03,  2.11357591e-03,
     1.94148675e-03,  1.69763080e-03,  1.39837473e-03,  1.06133447e-03,
     7.04480871e-04,  3.45293760e-04,  9.74695082e-19, -3.17074654e-04,
    -5.94017766e-04, -8.21692190e-04, -9.93941563e-04, -1.10764097e-03,
    -1.16260169e-03, -1.16134522e-03, -1.10876706e-03, -1.01171451e-03,
    -8.78505232e-04, -7.18414023e-04, -5.41155231e-04, -3.56385965e-04,
    -1.73252713e-04, -5.81880142e-19,  1.56344667e-04,  2.90200217e-04,
     3.97571380e-04,  4.76098424e-04,  5.25022155e-04,  5.45072918e-04,
     5.38295523e-04,  5.07824294e-04,  4.57623849e-04,  3.92211738e-04,
     3.16378650e-04,  2.34920777e-04,  1.52397076e-04,  7.29218021e-05,
     2.81006480e-19, -6.35893034e-05, -1.15860365e-04, -1.55639427e-04,
    -1.82538332e-04, -1.96888686e-04, -1.99643819e-04, -1.92256960e-04,
    -1.76544567e-04, -1.54543830e-04, -1.28372848e-04, -1.00101113e-04,
    -7.16366399e-05, -4.46345894e-05, -2.04305583e-05
])

LTPF_HI = np.array([
     6.69885837e-03,  3.96711478e-02,  1.06999186e-01,  2.09880463e-01,
     3.35690625e-01,  4.59220930e-01,  5.50075002e-01,  5.83527575e-01,
     5.50075002e-01,  4.59220930e-01,  3.35690625e-01,  2.09880463e-01,
     1.06999186e-01,  3.96711478e-02,  6.69885837e-03
])

def print_table(t, m=4):

    for (i, v) in enumerate(t):
        print('{:14.8e},'.format(v), end = '\n' if i%m == m-1 else ' ')

    if len(t) % 4:
        print('')


def mdct_fft_twiddles():

    for n in (10, 20, 30, 40, 60, 80, 90, 120, 160, 180, 240, 480):

        print('\n--- fft bf2 twiddles {:3d} ---'.format(n))

        kv = -2 * np.pi * np.arange(n // 2) / n
        for (i, k) in enumerate(kv):
            print('{{ {:14.7e}, {:14.7e} }},'.format(np.cos(k), np.sin(k)),
                  end = '\n' if i%2 == 1 else ' ')

    for n in (15, 45):

        print('\n--- fft bf3 twiddles {:3d} ---'.format(n))

        kv = -2 * np.pi * np.arange(n) / n
        for k in kv:
            print(('{{ {{ {:14.7e}, {:14.7e} }},' +
                     ' {{ {:14.7e}, {:14.7e} }} }},').format(
                np.cos(k), np.sin(k), np.cos(2*k), np.sin(2*k)))


def mdct_rot_twiddles():

    for n in (40, 80, 120, 160, 240, 320, 360, 480, 640, 720, 960, 1920):

        print('\n--- mdct rot twiddles {:3d} ---'.format(n))

        kv = 2 * np.pi * (np.arange(n // 4) + 1/8) / n
        scale = np.sqrt( np.sqrt( 4 / n ) )
        for (i, k) in enumerate(kv):
            print('{{ {:14.7e}, {:14.7e} }},'.format(
                np.cos(k) * scale, np.sin(k) * scale),
                  end = '\n' if i%2 == 1 else ' ')


def tns_lag_window():

    print('\n--- tns lag window ---')
    print_table(np.exp(-0.5 * (0.02 * np.pi * np.arange(9)) ** 2))


def tns_quantization_table():

    print('\n--- tns quantization table ---')

    xe = np.sin((np.arange(8) + 0.5) * (np.pi / 17))
    xe = np.rint(xe * 2**15).astype(np.int16)

    xd = np.sin(np.arange(9) * (np.pi / 17))
    xd = np.rint(xd * 2**15).astype(np.int16)

    for x in (xe, xd):
        print()
        for (i, xi) in enumerate(x):
            print('0x{:04x}p-15,'.format(xi), end = '\n' if i%4 == 4-1 else ' ')
        if len(x) % 4:
            print()

def quant_iq_table():

    print('\n--- quantization iq table ---')
    print_table(10 ** (np.arange(65) / 28))


def sns_ge_table():

    g_tilt_table = [ 14, 18, 22, 26, 30, 34 ]

    for (sr, g_tilt) in enumerate(g_tilt_table):
        print('\n--- sns ge table, sr:{} ---'.format(sr))
        print_table(10 ** ((np.arange(64) * g_tilt) / 630))


def inv_table():

    print('\n--- inv table ---')
    print_table(np.append(np.zeros(1), 1 / np.arange(1, 28)))

def ltpf_resampler_table():

    for sr in [ 8, 16, 32, 24, 48, 96 ]:

        r = 192 // sr
        k = 64 if r & (r-1) else 192

        p = (192 // k) * (k // sr)
        q = p * (0.5 if sr == 8 else 1)

        print('\n--- LTPF resampler {:d} KHz to 12.8 KHz ---'.format(sr))

        h = np.rint(np.append(LTPF_H12K8, 0.) * q * 2**15).astype(int)
        h = h.reshape((len(h) // p, p)).T
        h = np.flip(h, axis=0)
        print('... Gain:', np.max(np.sum(np.abs(h), axis=1)) / 32768.)

        for i in range(0, len(h), 192 // k):
            for j in range(0, len(h[i]), 10):
                print('{:5d}, {:5d}, {:5d}, {:5d}, {:5d}, '
                      '{:5d}, {:5d}, {:5d}, {:5d}, {:5d},'.format(
                    h[i][j+0], h[i][j+1], h[i][j+2], h[i][j+3], h[i][j+4],
                    h[i][j+5], h[i][j+6], h[i][j+7], h[i][j+8], h[i][j+9]))


def ltpf_interpolate_table():

    print('\n--- LTPF interpolation ---')

    h = np.rint(np.append(LTPF_HI, 0.) * 2**15).astype(int)

    h = h.reshape(len(h) // 4, 4).T
    h = np.flip(h, axis=0)
    print('... Gain:', np.max(np.sum(np.abs(h), axis=1)) / 32768.)

    for i in range(len(h)):
        print('{:5d}, {:5d}, {:5d}, {:5d}'.format(
            h[i][0], h[i][1], h[i][2], h[i][3]))


if __name__ == '__main__':

    mdct_fft_twiddles()
    mdct_rot_twiddles()

    inv_table()
    sns_ge_table()
    tns_lag_window()
    tns_quantization_table()
    quant_iq_table()

    ltpf_resampler_table()
    ltpf_interpolate_table()

    print('')
