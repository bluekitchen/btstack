/*
 * MdctDec.cpp
 *
 * Copyright 2019 HIMSA II K/S - www.himsa.com. Represented by EHIMA - www.ehima.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "MdctDec.hpp"
#include "MdctWindows.hpp"
#include <cmath>

namespace Lc3Dec
{

MdctDec::MdctDec(const Lc3Config& lc3Config_) :
    lc3Config(lc3Config_),
    x_hat_mdct(nullptr),
    dctIVDbl(lc3Config.NF),
    mem_ola_add(nullptr),
    t_hat_mdct(nullptr),
    wN(nullptr)
{
    mem_ola_add  = new double[lc3Config.NF - lc3Config.Z];
    t_hat_mdct   = new double[2*lc3Config.NF];
    x_hat_mdct   = new double[lc3Config.NF];
    for (uint16_t n=0; n<(lc3Config.NF - lc3Config.Z); n++)
    {
        mem_ola_add[n] = 0;
    }

    // Note: we do not add additional configuration error checking at this level.
    //   We assume that there will be nor processing with invalid configuration,
    //   thus nonsense results for invalid lc3Config.N_ms and/or lc3Config.Fs_ind
    //   are accepted here.
    wN = w_N80; // default initialization to avoid warnings
    if (lc3Config.N_ms == Lc3Config::FrameDuration::d7p5ms)
    {
        switch(lc3Config.NF)
        {
            case 60:
                wN = w_N60_7p5ms;
                break;
            case 120:
                wN = w_N120_7p5ms;
                break;
            case 180:
                wN = w_N180_7p5ms;
                break;
            case 240:
                wN = w_N240_7p5ms;
                break;
            case 360:
                wN = w_N360_7p5ms;
                break;
        }
    }
    else
    {
        // Lc3Config::FrameDuration::d10ms (and other as fallback)
        switch(lc3Config.NF)
        {
            case 80:
                wN = w_N80;
                break;
            case 160:
                wN = w_N160;
                break;
            case 240:
                wN = w_N240;
                break;
            case 320:
                wN = w_N320;
                break;
            case 480:
                wN = w_N480;
                break;
        }
    }

}

MdctDec::~MdctDec()
{
    if (nullptr != mem_ola_add)
    {
        delete[] mem_ola_add;
    }
    if (nullptr != t_hat_mdct)
    {
        delete[] t_hat_mdct;
    }
    if (nullptr != x_hat_mdct)
    {
        delete[] x_hat_mdct;
    }
}

void MdctDec::MdctInvFastDbl()
{

    dctIVDbl.run();

    for (uint16_t n=0; n < lc3Config.NF; n++)
    {
        t_hat_mdct[n] = dctIVDbl.out[n];
    }
    for (uint16_t n=lc3Config.NF; n < 2*lc3Config.NF; n++)
    {
        t_hat_mdct[n] = -dctIVDbl.out[2*lc3Config.NF-1-n];
    }

    // TODO try to optimize out the mem-buffer
    double mem[lc3Config.NF/2];
    for (uint16_t n=0; n < lc3Config.NF/2; n++)
    {
        mem[n] = t_hat_mdct[n];
    }
    for (uint16_t n=0; n < 3*lc3Config.NF/2; n++)
    {
        t_hat_mdct[n] = t_hat_mdct[n+lc3Config.NF/2];
    }
    for (uint16_t n=3*lc3Config.NF/2; n < 2*lc3Config.NF; n++)
    {
        t_hat_mdct[n] = -mem[n-3*lc3Config.NF/2];
    }

    double gain = 1.0 / sqrt(2.0 * lc3Config.NF);
    for (uint16_t n=0; n < 2*lc3Config.NF; n++)
    {
        t_hat_mdct[n] *= gain;
    }
}


void MdctDec::run(const double* const X_hat)
{
    if (!lc3Config.isValid())
    {
        return;
    }

    for (uint16_t k=0; k<lc3Config.NE; k++)
    {
        dctIVDbl.in[k] = X_hat[k];
    }
    for (uint16_t k=lc3Config.NE; k<lc3Config.NF; k++)
    {
        dctIVDbl.in[k] = 0;
    }

    // 1. Generation of time domain aliasing buffer
    MdctInvFastDbl();

    // 2. Windowing of time-aliased buffer
    for (uint16_t n=0; n < 2*lc3Config.NF; n++)
    {
        t_hat_mdct[n] *= wN[2*lc3Config.NF-1-n];
    }

    // 3. Conduct overlapp-add operation
    for (uint16_t n=0; n < lc3Config.NF-lc3Config.Z; n++)
    {
        x_hat_mdct[n]  = mem_ola_add[n] + t_hat_mdct[lc3Config.Z+n];
        mem_ola_add[n] = t_hat_mdct[lc3Config.NF+lc3Config.Z+n];
    }
    for (uint16_t n=lc3Config.NF-lc3Config.Z; n < lc3Config.NF; n++)
    {
        x_hat_mdct[n] = t_hat_mdct[lc3Config.Z+n];
    }

}


void MdctDec::registerDatapoints(DatapointContainer* datapoints)
{
    if (nullptr != datapoints)
    {
        datapoints->addDatapoint( "X_hat_mdct", dctIVDbl.in, sizeof(double)*lc3Config.NF );
        datapoints->addDatapoint( "t_hat_mdct", t_hat_mdct, sizeof(double)*2*lc3Config.NF );
        datapoints->addDatapoint( "mem_ola_add", mem_ola_add, sizeof(double)*(lc3Config.NF-lc3Config.Z) );
        datapoints->addDatapoint( "x_hat_mdct", x_hat_mdct, sizeof(double)*lc3Config.NF );
    }
}

}//namespace Lc3Dec
