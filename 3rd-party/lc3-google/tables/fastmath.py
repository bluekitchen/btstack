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
import matplotlib.pyplot as plt


def fast_exp2(x, t, p):

    p = p.astype(np.float32)
    x = x.astype(np.float32)

    m = ((x + 0.5/8) % (1/8)) - (0.5/8)
    e = int((x - m) * 8)

    y = ((((p[0]*m) + p[1])*m + p[2])*m + p[3])*m + p[4]
    y = y * 2**(e // 8) * t[e % 8]

    return y.astype(np.float32)

def approx_exp2():

    x = np.arange(0, 1/8, step=1e-6)
    p = np.polyfit(x, 2 ** x, 4)
    t = [ 2**(i/8) for i in range(8) ]

    x =  np.arange(-10, 10, step=1e-3)
    y = [ fast_exp2(x[i], t, p) for i in range(len(x)) ]

    e = np.abs(y - 2**x) / (2 ** x)

    print('{{ {:14.8e}, {:14.8e}, {:14.8e}, {:14.8e}, \n'
           '  {:14.8e}, {:14.8e}, {:14.8e}, {:14.8e}, '.format(*t))
    print('{{ {:14.8e}, {:14.8e}, {:14.8e}, {:14.8e}, {:14.8e} }}'.format(*p))
    print('Max relative error: ', np.max(e))
    print('Max RMS error: ', np.sqrt(np.mean(e ** 2)))

    if False:
        fig, (ax1, ax2) = plt.subplots(2)

        ax1.plot(x, 2**x, label='Reference')
        ax1.plot(x, y, label='Approximation')
        ax1.legend()

        ax2.plot(x, e, label='Relative Error')
        ax2.legend()

        plt.show()


def fast_log2(x, p):

    p = p.astype(np.float32)
    x = x.astype(np.float32)

    (x, e) = np.frexp(x)

    y = ((((p[0]*x) + p[1])*x + p[2])*x + p[3])*x + p[4]

    return (e ) + y.astype(np.float32)

def approx_log2():

    x = np.logspace(-1, 0, base=2, num=100)
    p = np.polyfit(x, np.log2(x), 4)

    x = np.logspace(-2, 5, num=10000)
    y = [ fast_log2(x[i], p) for i in range(len(x)) ]
    e = np.abs(y - np.log2(x))

    print('{{ {:14.8e}, {:14.8e}, {:14.8e}, {:14.8e}, {:14.8e} }}'
                .format(p[0], p[1], p[2], p[3], p[4]))
    print('Max absolute error: ', np.max(e))
    print('Max RMS error: ', np.sqrt(np.mean(e ** 2)))

    if False:
        fig, (ax1, ax2) = plt.subplots(2)

        ax1.plot(x, np.log2(x),  label='Reference')
        ax1.plot(x, y, label='Approximation')
        ax1.legend()

        ax2.plot(x, e, label = 'Absolute error')
        ax2.legend()

        plt.show()


def table_db_q16():

    k = 10 * np.log10(2)

    for i in range(32):
        a = k * np.log2(np.ldexp(32 + i  , -5)) - (i // 16) * (k/2)
        b = k * np.log2(np.ldexp(32 + i+1, -5)) - (i // 16) * (k/2)

        an = np.ldexp(a, 15) + 0.5
        bn = np.ldexp(b - a, 15) + 0.5
        print('{{ {:5d}, {:4d} }},'
            .format(int(np.ldexp(a, 15) + 0.5),
                    int(np.ldexp(b - a, 15) + 0.5)),
            end = ' ' if i % 4 < 3 else '\n')


if __name__ == '__main__':

    print('\n--- Approximation of 2^n ---')
    approx_exp2()

    print('\n--- Approximation of log2(n) ---')
    approx_log2()

    print('\n--- Table of fixed Q16 dB ---')
    table_db_q16()

    print('')
