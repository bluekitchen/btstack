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
#include "BandIndexTables.hpp"
#include <cmath>

namespace Lc3Enc
{

static const uint8_t g_tilt_table[5] = {
    14, //fs= 8000
    18, //fs=16000
    22, //fs=24000
    26, //fs=32000
    30  //fs=44100, 48000
};

static const double w[6] = {
    1.0/12,
    2.0/12,
    3.0/12,
    3.0/12,
    2.0/12,
    1.0/12
};

SpectralNoiseShaping::SpectralNoiseShaping(const Lc3Config& lc3Config_, const int* const I_fs_) :
    lc3Config(lc3Config_),
    g_tilt(g_tilt_table[lc3Config.Fs_ind]),
    X_S(nullptr),
    I_fs(I_fs_),
    snsQuantization(),
    datapoints(nullptr)
{
    X_S = new double[lc3Config.NE];
}

SpectralNoiseShaping::~SpectralNoiseShaping()
{
    if (nullptr != X_S)
    {
        delete[] X_S;
    }
}

uint8_t SpectralNoiseShaping::get_ind_LF()
{
    return snsQuantization.ind_LF;
}

uint8_t SpectralNoiseShaping::get_ind_HF()
{
    return snsQuantization.ind_HF;
}

uint8_t SpectralNoiseShaping::get_shape_j()
{
    return snsQuantization.shape_j;
}

uint8_t SpectralNoiseShaping::get_Gind()
{
    return snsQuantization.Gind;
}

int32_t SpectralNoiseShaping::get_LS_indA()
{
    return snsQuantization.LS_indA;
}

int32_t SpectralNoiseShaping::get_LS_indB()
{
    return snsQuantization.LS_indB;
}

uint32_t SpectralNoiseShaping::get_index_joint_j()
{
    return snsQuantization.index_joint_j;
}


void SpectralNoiseShaping::run(const double* const X, const double* E_B, bool F_att)
{
    // 3.3.7.2 SNS analysis  (d09r06_FhG)
    // 3.3.7.2.1 Padding  (d09r06_FhG)
    const uint8_t n2 = N_b - lc3Config.N_b;
    double E_B_patched[N_b]; // TODO: can we optimize out this buffer that is needed for a rare case only?

    for (uint8_t i=0; i < n2; i++)
    {
        // Note: at the inverse operation (see end of this function) there is a factor 0.5.
        //       Is this definition consistent?
        E_B_patched[i*2+0] = E_B[i];
        E_B_patched[i*2+1] = E_B[i];
    }
    for (uint8_t i=0; i < lc3Config.N_b; i++)
    {
        E_B_patched[2*n2+i] = E_B[n2+i];
    }


    // 3.3.7.2.1 Smoothing  (d09r02_F2F)
    double E_local[N_b];  // Note: memory will be re-used step-by-step
    double* E_S = E_local;
    E_S[0]  = 0.75*E_B_patched[0] + 0.25*E_B_patched[1];
    for (uint8_t b=1; b < N_b-1; b++)
    {
        E_S[b] = 0.25*E_B_patched[b-1] + 0.5*E_B_patched[b] + 0.25*E_B_patched[b+1];
    }
    E_S[N_b-1] = 0.25*E_B_patched[N_b-2] + 0.75*E_B_patched[N_b-1];

    // 3.3.7.2.2 Pre-emphasis (d09r02_F2F)
    double* E_P = E_local;
    const double fix_exponent_part = g_tilt / static_cast<double>(10*63);
    for (uint8_t b=0; b < N_b; b++)
    {
        E_P[b] = E_S[b] * pow( 10.0, b*fix_exponent_part);
    }

    // 3.3.7.2.3 Noise floor (d09r02_F2F)
    double E_total = 0;
    for (uint8_t b=0; b < N_b; b++)
    {
        E_total +=E_P[b];
    }
    E_total /= 64;
    E_total *= pow(10.0, -40/10);
    double noiseFloor = pow(2.0, -32);
    if (E_total > noiseFloor)
    {
        noiseFloor = E_total;
    }
    double* E_P2 = E_local;
    for (uint8_t b=0; b < N_b; b++)
    {
        E_P2[b] = (E_P[b] > noiseFloor) ? E_P[b] : noiseFloor;
    }

    // 3.3.7.2.4 Logarithm (d09r02_F2F)
    double* E_L = E_local;
    for (uint8_t b=0; b < N_b; b++)
    {
        E_L[b] = log2(1e-31 + E_P2[b]) / 2;
    }

    // 3.3.7.2.5 Band energy grouping (d09r02_F2F)
    double E_L4[Nscales];
    uint8_t b2 = 0;
    E_L4[b2] = w[0]*E_L[0];
    for (uint8_t k=1; k<=5; k++)
    {
        E_L4[b2] += w[k] * E_L[4*b2 +k -1];
    }
    for (b2=1; b2 < 15; b2++)
    {
        E_L4[b2] = 0;
        for (uint8_t k=0; k<=5; k++)
        {
            E_L4[b2] += w[k] * E_L[4*b2 +k -1];
        }
    }
    b2 = 15;
    E_L4[b2] =  w[5]*E_L[63];
    for (uint8_t k=0; k<=4; k++)
    {
        E_L4[b2] += w[k] * E_L[4*b2 +k -1];
    }

    // 3.3.7.2.7 Mean removal and scaling, attack handling (d09r06_FhG)
    double E4_total = 0;
    for (uint8_t b2=0; b2 < Nscales; b2++)
    {
        E4_total += E_L4[b2];
    }
    E4_total /= Nscales;
    double scf_buffer[Nscales];
    double* scf_0 = scf;
    if (F_att)
    {
        scf_0 = scf_buffer;
    }
    for (uint8_t b2=0; b2 < Nscales; b2++)
    {
        scf_0[b2] = 0.85 * (E_L4[b2] -  E4_total);
    }
    if (F_att)
    {
        // Note:
        //  This section is not covered by intermediate encoder output
        //  in section C.3. "Encoder intermediate output"  (d09r02_F2F)
        double* scf_1 = scf;
        scf_1[0] = (scf_0[0] + scf_0[1] + scf_0[2]) / 3;
        scf_1[1] = (scf_0[0] + scf_0[1] + scf_0[2] + scf_0[3]) / 4;
        for (uint8_t n=2; n <= 13; n++)
        {
            scf_1[n] = 0;
            for (int8_t m=-2; m <= 2; m++)
            {
                scf_1[n] += scf_0[n+m];
            }
            scf_1[n] /= 5;
        }
        scf_1[14] = (scf_0[12] + scf_0[13] + scf_0[14] + scf_0[15]) / 4;
        scf_1[15] = (scf_0[13] + scf_0[14] + scf_0[15]) / 3;

        if (nullptr!=datapoints)
        {
            datapoints->log("scf_0", scf_0, sizeof(double)*Nscales);
            datapoints->log("scf_1", scf_1, sizeof(double)*Nscales);
        }

        double scf_1_total = 0;
        for (uint8_t b=0; b<Nscales; b++)
        {
            scf_1_total += scf_1[b];
        }
        scf_1_total /= Nscales;
        const double f_att = (lc3Config.N_ms == Lc3Config::FrameDuration::d10ms) ? 0.5 : 0.3;
        for (uint8_t n=0; n<Nscales; n++)
        {
            scf[n] = f_att * (scf_1[n] - scf_1_total);
        }
    }

    // 3.3.7.3 SNS quantization (d09r02_F2F)
    snsQuantization.run(scf);

    // 3.3.7.4 SNS scale factors interpolation (d09r02_F2F)
    const double* scfQ = snsQuantization.scfQ;
    double scfQint[N_b];
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

    // add special handling for N_b_in=60 (happens for 7.5ms and fs=8kHz)
    // (see end of section 3.3.7.4 SNS scale factors interpolation (d09r06_FhG)
    for (uint8_t i=0; i < n2; i++)
    {
        scfQint[i] = (scfQint[2*i]+scfQint[2*i+1])/2;
    }
    if (n2 != 0)
    {
        // code is consistent with Errata 15012 (see d1.0r03)
        for (uint8_t i=n2; i < lc3Config.N_b; i++)
        {
            scfQint[i] = scfQint[i+n2];
        }
    }

    // The scale factors are transformed back into the linear domain
    for (uint8_t b = 0; b < lc3Config.N_b; b++)
    {
        g_SNS[b] = exp2(-scfQint[b]);
    }

    // 3.3.7.5 Spectral shaping (d09r02_F2F)
    for (uint8_t b=0; b < lc3Config.N_b; b++)
    {
        for (uint16_t k=I_fs[b]; k < I_fs[b+1] ; k++)
        {
            X_S[k] = X[k] * g_SNS[b];
        }
    }

}


void SpectralNoiseShaping::registerDatapoints(DatapointContainer* datapoints_)
{
    datapoints = datapoints_;
    if (nullptr != datapoints)
    {
        datapoints->addDatapoint( "scf", &scf[0], sizeof(double)*Nscales );
        datapoints->addDatapoint( "g_sns", &g_SNS[0], sizeof(double)*N_b );
        datapoints->addDatapoint( "X_S", &X_S[0], sizeof(double)*lc3Config.NE );

        snsQuantization.registerDatapoints(datapoints);
    }
}

}//namespace Lc3Enc
