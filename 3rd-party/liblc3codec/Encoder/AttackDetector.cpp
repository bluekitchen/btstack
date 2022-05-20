/*
 * AttackDetector.cpp
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

#include "AttackDetector.hpp"
#include <cmath>

namespace Lc3Enc
{

AttackDetector::AttackDetector(const Lc3Config& lc3Config_) :
    lc3Config(lc3Config_),
    M_F( (lc3Config.N_ms == Lc3Config::FrameDuration::d10ms) ? 160 : 120), // 16*N_ms
    F_att(0),
    E_att_last(0),
    A_att_last(0),
    P_att_last(-1)
{
    // make sure these states are initially zero as demanded by the specification
    x_att_last[0] = 0;
    x_att_last[1] = 0;
}

AttackDetector::~AttackDetector()
{
}


void AttackDetector::run(const int16_t* const x_s, uint16_t nbytes)
{
    // 3.3.6.1 Overview (d09r06_FhG)
    // -> attack detection active only for higher bitrates and fs>=32000; otherwise defaults are set
    F_att = 0;
    if ( lc3Config.Fs < 32000 )
    {
        return;
    }
    bool isActive =
            ( (lc3Config.N_ms == Lc3Config::FrameDuration::d10ms) && (lc3Config.Fs==32000) && (nbytes>80) ) ||
            ( (lc3Config.N_ms == Lc3Config::FrameDuration::d10ms) && (lc3Config.Fs>=44100) && (nbytes>=100) ) ||
            ( (lc3Config.N_ms == Lc3Config::FrameDuration::d7p5ms) && (lc3Config.Fs==32000) && (nbytes>=61) && (nbytes<150) ) ||
            ( (lc3Config.N_ms == Lc3Config::FrameDuration::d7p5ms) && (lc3Config.Fs>=44100) && (nbytes>=75) && (nbytes<150) );
    if ( !isActive )
    {
        // Note: in bitrate switching situations we have to set proper states
        E_att_last = 0;
        A_att_last = 0;
        P_att_last = -1;
        return;
    }

    // 3.3.6.2 Downsampling and filtering of input signal (d09r02_F2F)
    // Note: the following section might be converted to int32 instead
    //       of double computation (maybe something for optimization)
    double x_att_extended[M_F+2];
    x_att_extended[0] = x_att_last[0];
    x_att_extended[1] = x_att_last[1];
    double* x_att = &x_att_extended[2];
    for (uint8_t n=0; n < M_F; n++)  // downsampling
    {
        x_att[n] = 0;
        for (uint8_t m=0; m < lc3Config.NF/M_F; m++)
        {
            x_att[n] += x_s[ lc3Config.NF/M_F*n + m ];
        }
    }
    x_att_last[0] = x_att[M_F-2];
    x_att_last[1] = x_att[M_F-1];
    double* x_hp = x_att_extended; // just for improve readability
    for (uint8_t n=0; n < M_F; n++)  // highpass-filtering (in-place!)
    {
        x_hp[n] = 0.375*x_att[n] - 0.5*x_att[n-1] + 0.125*x_att[n-2];
    }

    // 3.3.6.3 Energy calculation & 3.3.6.4 Attack detection (d09r06_FhG)
    int8_t P_att = -1;
    const uint8_t N_blocks = (lc3Config.N_ms == Lc3Config::FrameDuration::d10ms) ? 4 : 3; // N_ms/2.5
    for (uint8_t n=0; n < N_blocks; n++)
    {
        double E_att = 0;
        for (uint8_t l=40*n; l <= (40*n+39); l++)
        {
            E_att += x_hp[l]*x_hp[l];
        }
        double A_att = (0.25*A_att_last > E_att_last) ? 0.25*A_att_last : E_att_last;
        if (E_att > 8.5*A_att)
        {
            // attack detected
            P_att = n;
        }
        E_att_last = E_att;
        A_att_last = A_att;
    }
    const uint8_t T_att = (lc3Config.N_ms == Lc3Config::FrameDuration::d10ms) ? 2 : 1; // floor(N_blocks/2)
    F_att = (P_att >= 0) || (P_att_last >= T_att);
    P_att_last = P_att; // prepare next frame
}


void AttackDetector::registerDatapoints(DatapointContainer* datapoints)
{
    if (nullptr != datapoints)
    {
        datapoints->addDatapoint( "F_att", &F_att, sizeof(F_att) );
    }
}

}//namespace Lc3Enc

