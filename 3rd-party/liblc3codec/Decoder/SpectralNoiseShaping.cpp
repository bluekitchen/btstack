/*
 * SpectralNoiseShaping.cpp
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

#include "SpectralNoiseShaping.hpp"
#include "SnsQuantizationTables.hpp"
#include "BandIndexTables.hpp"
#include "MPVQ.hpp"

#include <cmath>

namespace Lc3Dec
{

SpectralNoiseShaping::SpectralNoiseShaping(const Lc3Config& lc3Config_)
    :
    lc3Config(lc3Config_),
    I_fs(nullptr)
{
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
                break;
            case 1:
                I_fs = &I_16000_7p5ms[0];
                break;
            case 2:
                I_fs = &I_24000_7p5ms[0];
                break;
            case 3:
                I_fs = &I_32000_7p5ms[0];
                break;
            case 4:
                I_fs = &I_48000_7p5ms[0];
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
                break;
            case 1:
                I_fs = &I_16000[0];
                break;
            case 2:
                I_fs = &I_24000[0];
                break;
            case 3:
                I_fs = &I_32000[0];
                break;
            case 4:
                I_fs = &I_48000[0];
                break;
        }
    }

}

SpectralNoiseShaping::~SpectralNoiseShaping()
{
}

void SpectralNoiseShaping::run(
            const double* const X_s_tns,
            double* const X_hat_ss,
            int16_t ind_LF,
            int16_t ind_HF,
            int16_t submodeMSB,
            int16_t submodeLSB,
            int16_t Gind,
            int16_t LS_indA,
            int16_t LS_indB,
            int32_t idxA,
            int16_t idxB
        )
{
    if (!lc3Config.isValid())
    {
        return;
    }

    //3.4.7 SNS decoder (d09r02_F2F)
    // 3.4.7.2 SNS scale factor decoding  (d09r02_F2F)
    // 3.4.7.2.1 Stage 1 SNS VQ decoding  (d09r02_F2F)
    // already done earlier (see SideInformation)

    //The first stage vector is composed as:
    //𝑠𝑡1(𝑛) = 𝐿𝐹𝐶𝐵𝑖𝑛𝑑_𝐿𝐹 (𝑛), 𝑓𝑜𝑟 𝑛 = [0 … 7], # (33)
    //𝑠𝑡1(𝑛 + 8) = 𝐻𝐹𝐶𝐵𝑖𝑛𝑑_𝐻𝐹(𝑛), 𝑓𝑜𝑟 𝑛 = [0 … 7], # (34)
    double st1[16];
    for (uint8_t n = 0; n<8; n++)
    {
        st1[n]   = LFCB[ind_LF][n];
        st1[n+8] = HFCB[ind_HF][n];
    }

    // 3.4.7.2.2 Stage 2 SNS VQ decoding   (d09r02_F2F)
    // already done earlier -> submodeMSB, Gind, LS_indA, LS_indB, idxA, idxB
    int16_t shape_j = (submodeMSB<<1) + submodeLSB;
    int16_t gain_i = Gind;

    int16_t y[16];
    int16_t z[16];

    switch (shape_j)
    {
        case 0:
            MPVQdeenum(10, 10, LS_indA, idxA, y);
            MPVQdeenum( 6,  1, LS_indB, idxB, z);
            for (uint8_t n=10; n <=15; n++)
            {
                y[n] = z[n-10];
            }
            break;
        case 1:
            MPVQdeenum(10, 10, LS_indA, idxA, y);
            for (uint8_t n=10; n <=15; n++)
            {
                y[n] = 0;
            }
            break;
        case 2:
            MPVQdeenum(16, 8, LS_indA, idxA, y);
            break;
        case 3:
            MPVQdeenum(16, 6, LS_indA, idxA, y);
            break;
    }

    double yNorm = 0;
    for (uint8_t n=0; n < 16; n++)
    {
        //yNorm += y[n]*(y[n]*1.0);
        yNorm += y[n]*y[n];
    }
    yNorm = std::sqrt(yNorm);
    // Note: we skipped intermediate signal xq_shape_j and applied yNorm
    //  directly together with G_gain_i_shape_j

    double G_gain_i_shape_j = sns_vq_far_adj_gains[gain_i]; // default initialization to avoid warnings
    switch (shape_j)
    {
        case 0:
            G_gain_i_shape_j = sns_vq_reg_adj_gains[gain_i];
            break;
        case 1:
            G_gain_i_shape_j = sns_vq_reg_lf_adj_gains[gain_i];
            break;
        case 2:
            G_gain_i_shape_j = sns_vq_near_adj_gains[gain_i];
            break;
        case 3:
            G_gain_i_shape_j = sns_vq_far_adj_gains[gain_i];
            break;
    }
    if (0.0 != yNorm) // do we have to make this even more robust???
    {
        G_gain_i_shape_j /= yNorm;
    }

    // Synthesis of the Quantized SNS scale factor vector
    double scfQ[16];
    for (uint8_t n = 0; n < 16; n++)
    {
        double factor=0;
        for (uint8_t col=0; col < 16; col++)
        {
            factor += y[col] * D[n][col];
        }
        scfQ[n] = st1[n] + G_gain_i_shape_j * factor;
    }

    // 3.4.7.3 SNS scale factors interpolation  (d09r02_F2F)
    double scfQint[64];
    scfQint[0] = scfQ[0];
    scfQint[1] = scfQ[0];
    for (uint8_t n=0; n <= 14; n++)
    {
        scfQint[4*n+2] = scfQ[n] + (1.0/8.0 * (scfQ[n+1] - scfQ[n]));
        scfQint[4*n+3] = scfQ[n] + (3.0/8.0 * (scfQ[n+1] - scfQ[n]));
        scfQint[4*n+4] = scfQ[n] + (5.0/8.0 * (scfQ[n+1] - scfQ[n]));
        scfQint[4*n+5] = scfQ[n] + (7.0/8.0 * (scfQ[n+1] - scfQ[n]));
    }
    scfQint[62] = scfQ[15] + 1/8.0 * (scfQ[15] - scfQ[14]);
    scfQint[63] = scfQ[15] + 3/8.0 * (scfQ[15] - scfQ[14]);

    // add special handling for lc3Config.N_b=60 (happens for 7.5ms and fs=8kHz)
    // see section 3.4.7.3 SNS scale factors interpolation (d1.0r03 including Errata 15036)
    const uint8_t n2 = 64 - lc3Config.N_b;
    if ( n2 != 0 )
    {

        for (uint8_t i=0; i < n2; i++)
        {
            scfQint[i] = (scfQint[2*i]+scfQint[2*i+1])/2;
        }
        for (uint8_t i=n2; i < lc3Config.N_b; i++)
        {
            scfQint[i] = scfQint[i+n2];
        }
    }

    double g_SNS[64];
    for (uint8_t b = 0; b < lc3Config.N_b; b++)
    {
        g_SNS[b] = exp2(scfQint[b]);
    }

    // 3.4.7.4 Spectral Shaping   (d09r02_F2F)
    //for (b=0; b<𝑁𝑏; b++)
    for (uint8_t b=0; b<lc3Config.N_b; b++)
    {
        //for (k=𝐼𝑓𝑠 (𝑏); k< 𝐼𝑓𝑠 (𝑏 + 1); k++)
        for (uint16_t k=I_fs[b]; k < I_fs[b+1] ; k++)
        {
            //𝑋 ̂(𝑘) = 𝑋𝑆 ̂(𝑘) ∙ 𝑔𝑆𝑁𝑆 (𝑏)
            X_hat_ss[k] = X_s_tns[k] * g_SNS[b];
        }
    }

}

void SpectralNoiseShaping::registerDatapoints(DatapointContainer* datapoints)
{
}

}//namespace Lc3Dec
