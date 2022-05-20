/*
 * MdctEnc.cpp
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

#include "MdctEnc.hpp"
#include "MdctWindows.hpp"
#include "BandIndexTables.hpp"
#include <cmath>

namespace Lc3Enc
{


MdctEnc::MdctEnc(const Lc3Config& lc3Config_) :
    lc3Config(lc3Config_),
    X(nullptr),
    E_B(nullptr),
    near_nyquist_flag(0),
    dctIVDbl(lc3Config.NF),
    skipMdct(0),
    t(nullptr),
    wN(nullptr),
    I_fs(nullptr)
{
    X = dctIVDbl.out;

    E_B = new double[lc3Config.N_b];
    // initialization of E_B skipped since this will be fully re-computed on any run anyway

    t = new int16_t[2*lc3Config.NF];
    for (uint16_t n=0; n < 2*lc3Config.NF; n++)
    {
        t[n]=0;
    }

    // Note: we do not add additional configuration error checking at this level.
    //   We assume that there will be nor processing with invalid configuration,
    //   thus nonsense results for invalid lc3Config.N_ms and/or lc3Config.Fs_ind
    //   are accepted here.
    I_fs = &I_8000[0]; // default initialization to avoid warnings
    wN = w_N80;        // default initialization to avoid warnings

    // Note: we do not add additional configuration error checking at this level.
    //   We assume that there will be nor processing with invalid configuration,
    //   thus nonsense results for invalid lc3Config.N_ms and/or lc3Config.Fs_ind
    //   are accepted here.
    if (lc3Config.N_ms == Lc3Config::FrameDuration::d7p5ms)
    {
        switch(lc3Config.Fs_ind)
        {
            case 0:
                I_fs = &I_8000_7p5ms[0];
                wN = w_N60_7p5ms;
                break;
            case 1:
                I_fs = &I_16000_7p5ms[0];
                wN = w_N120_7p5ms;
                break;
            case 2:
                I_fs = &I_24000_7p5ms[0];
                wN = w_N180_7p5ms;
                break;
            case 3:
                I_fs = &I_32000_7p5ms[0];
                wN = w_N240_7p5ms;
                break;
            case 4:
                I_fs = &I_48000_7p5ms[0];
                wN = w_N360_7p5ms;
                break;
        }
    }
    else
    {
        // Lc3Config::FrameDuration::d10ms (and other as fallback)
        switch(lc3Config.Fs_ind)
        {
            case 0:
                I_fs = &I_8000[0];
                wN = w_N80;
                break;
            case 1:
                I_fs = &I_16000[0];
                wN = w_N160;
                break;
            case 2:
                I_fs = &I_24000[0];
                wN = w_N240;
                break;
            case 3:
                I_fs = &I_32000[0];
                wN = w_N320;
                break;
            case 4:
                I_fs = &I_48000[0];
                wN = w_N480;
                break;
        }
    }

}

MdctEnc::~MdctEnc()
{
    delete[] t;
}

const int* MdctEnc::get_I_fs() const
{
    return I_fs;
}


void MdctEnc::MdctFastDbl(const double* const tw)
{

    for (uint16_t n=0; n < lc3Config.NF/2; n++)
    {
        dctIVDbl.in[n] = -tw[3*lc3Config.NF/2-1-n] - tw[3*lc3Config.NF/2+n] ;
    }
    for (uint16_t n=lc3Config.NF/2; n < lc3Config.NF; n++)
    {
        dctIVDbl.in[n] = tw[n-lc3Config.NF/2] - tw[3*lc3Config.NF/2-1-n] ;
    }

    dctIVDbl.run();

    double gain = 1.0 / sqrt(2.0 * lc3Config.NF);
    for (uint16_t k=0; k < lc3Config.NF; k++)
    {
        dctIVDbl.out[k] *= gain;
    }
}


void MdctEnc::run(const int16_t* const x_s)
{
    if (skipMdct)
    {
        return;
    }

    // 3.3.4.2 Update time buffer (LC3 Specification d09r02_F2F)
    // Note: specification has strange loop indices
    //  -> corrected start index appropriately
    for (uint16_t n=0; n<(lc3Config.NF-lc3Config.Z); n++)
    {
        t[n] = t[lc3Config.NF+n];
    }
    for (uint16_t n=lc3Config.NF-lc3Config.Z; n<(2*lc3Config.NF-lc3Config.Z); n++)
    {
        t[n] = x_s[lc3Config.Z-lc3Config.NF+n];
    }

    // 3.3.4.3 Time-Frequency Transformation (LC3 Specification d09r02_F2F)
    double tw[2*lc3Config.NF];
    for (uint16_t n=0; n<2*lc3Config.NF; n++)
    {
        tw[n] = wN[n] * t[n];
    }
    MdctFastDbl(tw);

    //3.3.4.4 Energy estimation per band   (d09r02_F2F)
    for (uint8_t b = 0; b < lc3Config.N_b; b++)
    {
        E_B[b] = 0.0;
        uint16_t width =  I_fs[b+1]-I_fs[b];
        for (uint16_t k = I_fs[b]; k < I_fs[b+1]; k++)
        {
            E_B[b] += (X[k]*X[k]) / width;
        }
    }

    //3.3.4.5 Near Nyquist Detector (LC3 Specification d1.0r03; Errata 15041)
    if (lc3Config.Fs <= 32000)
    {
        const uint16_t nn_idx = (lc3Config.N_ms == Lc3Config::FrameDuration::d7p5ms) ? lc3Config.N_b-4 : lc3Config.N_b-2;
        double upperBandsEnergy = 0;
        double lowerBandsEnergy = 0;
        for (uint8_t n=0; n < lc3Config.N_b; n++)
        {
            if (n < nn_idx)
            {
                lowerBandsEnergy += E_B[n];
            }
            else
            {
                upperBandsEnergy += E_B[n];
            }
        }
        const double NN_thresh = 30;
        near_nyquist_flag = ( upperBandsEnergy > NN_thresh * lowerBandsEnergy) ? 1 : 0;
    }
    else
    {
        near_nyquist_flag = 0;
    }
}


void MdctEnc::registerDatapoints(DatapointContainer* datapoints)
{
    if (nullptr != datapoints)
    {
        datapoints->addDatapoint( "skipMdct", &skipMdct, sizeof(skipMdct) );
        datapoints->addDatapoint( "X", &X[0], sizeof(double)*lc3Config.NF );
        datapoints->addDatapoint( "E_B", &E_B[0], sizeof(double)*lc3Config.N_b);
        datapoints->addDatapoint( "near_nyquist_flag", &near_nyquist_flag, sizeof(near_nyquist_flag));
    }
}

}//namespace Lc3Enc
