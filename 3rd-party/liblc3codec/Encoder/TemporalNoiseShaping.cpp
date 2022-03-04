/*
 * TemporalNoiseShaping.cpp
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

#include "TemporalNoiseShaping.hpp"
#include "TemporalNoiseShapingTables.hpp"
#include <cmath>

namespace Lc3Enc
{

TemporalNoiseShaping::TemporalNoiseShaping(const Lc3Config& lc3Config_) :
    lc3Config(lc3Config_),
    nbits_TNS(0),
    X_f(nullptr),
    tns_lpc_weighting(0),
    num_tns_filters(0),
    datapoints(nullptr)
{
    X_f = new double[lc3Config.NE];
}

TemporalNoiseShaping::~TemporalNoiseShaping()
{
    if (nullptr != X_f)
    {
        delete[] X_f;
    }
}


void TemporalNoiseShaping::run(const double* const X_s, uint8_t P_BW, uint16_t nbits, uint8_t near_nyquist_flag)
{
    // 3.3.8.1 Overview / Table 3.14: TNS encoder parameters  (d09r06_FhG)
    uint16_t start_freq[2];
    uint16_t stop_freq[2];
    uint16_t sub_start[2][3];
    uint16_t sub_stop[2][3];

    if (lc3Config.N_ms==Lc3Config::FrameDuration::d10ms)
    {
        start_freq[0] = 12;
        if (4!=P_BW)
        {
            start_freq[1] = 160;
        }
        else
        {
            start_freq[1] = 200;
        }
        switch(P_BW)
        {
            default:
                // Note: this default setting avoids uninitialized variables, but in
                //       practice the calling code has the responsiblity to
                //       ensure proper values of P_BW
            case 0:
                num_tns_filters = 1;
                stop_freq[0] = 80;
                sub_start[0][0] = 12;
                sub_start[0][1] = 34;
                sub_start[0][2] = 57;
                sub_stop[0][0] = 34;
                sub_stop[0][1] = 57;
                sub_stop[0][2] = 80;
                break;
            case 1:
                num_tns_filters = 1;
                stop_freq[0] = 160;
                sub_start[0][0] = 12;
                sub_start[0][1] = 61;
                sub_start[0][2] = 110;
                sub_stop[0][0] = 61;
                sub_stop[0][1] = 110;
                sub_stop[0][2] = 160;
                break;
            case 2:
                num_tns_filters = 1;
                stop_freq[0] = 240;
                sub_start[0][0] = 12;
                sub_start[0][1] = 88;
                sub_start[0][2] = 164;
                sub_stop[0][0] = 88;
                sub_stop[0][1] = 164;
                sub_stop[0][2] = 240;
                break;
            case 3:
                num_tns_filters = 2;
                stop_freq[0] = 160;
                stop_freq[1] = 320;
                sub_start[0][0] = 12;
                sub_start[0][1] = 61;
                sub_start[0][2] = 110;
                sub_start[1][0] = 160;
                sub_start[1][1] = 213;
                sub_start[1][2] = 266;
                sub_stop[0][0] = 61;
                sub_stop[0][1] = 110;
                sub_stop[0][2] = 160;
                sub_stop[1][0] = 213;
                sub_stop[1][1] = 266;
                sub_stop[1][2] = 320;
                break;
            case 4:
                num_tns_filters = 2;
                stop_freq[0] = 200;
                stop_freq[1] = 400;
                sub_start[0][0] = 12;
                sub_start[0][1] = 74;
                sub_start[0][2] = 137;
                sub_start[1][0] = 200;
                sub_start[1][1] = 266;
                sub_start[1][2] = 333;
                sub_stop[0][0] = 74;
                sub_stop[0][1] = 137;
                sub_stop[0][2] = 200;
                sub_stop[1][0] = 266;
                sub_stop[1][1] = 333;
                sub_stop[1][2] = 400;
                break;
        }
    }
    else
    {//7.5ms frame duration
        start_freq[0] = 9;
        if (4!=P_BW)
        {
            start_freq[1] = 120; // Errata 15098 implemented
        }
        else
        {
            start_freq[1] = 150;
        }
        switch(P_BW)
        {
            default:
                // Note: this default setting avoids uninitialized variables, but in
                //       practice the calling code has the responsiblity to
                //       ensure proper values of P_BW
            case 0:
                num_tns_filters = 1;
                stop_freq[0] = 60;
                sub_start[0][0] = 9;
                sub_start[0][1] = 26;
                sub_start[0][2] = 43;
                sub_stop[0][0] = 26;
                sub_stop[0][1] = 43;
                sub_stop[0][2] = 60;
                break;
            case 1:
                num_tns_filters = 1;
                stop_freq[0] = 120; // Errata 15098 implemented
                sub_start[0][0] = 9;
                sub_start[0][1] = 46;
                sub_start[0][2] = 83;
                sub_stop[0][0] = 46;
                sub_stop[0][1] = 83;
                sub_stop[0][2] = 120;
                break;
            case 2:
                num_tns_filters = 1;
                stop_freq[0] = 180;
                sub_start[0][0] = 9;
                sub_start[0][1] = 66;
                sub_start[0][2] = 123;
                sub_stop[0][0] = 66;
                sub_stop[0][1] = 123;
                sub_stop[0][2] = 180;
                break;
            case 3:
                num_tns_filters = 2;
                stop_freq[0] = 120;
                stop_freq[1] = 240;
                sub_start[0][0] = 9;
                sub_start[0][1] = 46;
                sub_start[0][2] = 82;
                sub_start[1][0] = 120; // Errata 15098 implemented
                sub_start[1][1] = 159;
                sub_start[1][2] = 200;
                sub_stop[0][0] = 46;
                sub_stop[0][1] = 82;
                sub_stop[0][2] = 120; // Errata 15098 implemented
                sub_stop[1][0] = 159;
                sub_stop[1][1] = 200;
                sub_stop[1][2] = 240;
                break;
            case 4:
                num_tns_filters = 2;
                stop_freq[0] = 150;
                stop_freq[1] = 300;
                sub_start[0][0] = 9;
                sub_start[0][1] = 56;
                sub_start[0][2] = 103;
                sub_start[1][0] = 150;
                sub_start[1][1] = 200;
                sub_start[1][2] = 250;
                sub_stop[0][0] = 56;
                sub_stop[0][1] = 103;
                sub_stop[0][2] = 150;
                sub_stop[1][0] = 200;
                sub_stop[1][1] = 250;
                sub_stop[1][2] = 300;
                break;
        }
    }

    // 3.3.8.2 TNS analysis  (d09r02_F2F, d09r08)
    tns_lpc_weighting = (nbits < ((lc3Config.N_ms==Lc3Config::FrameDuration::d10ms) ? 480 : 360)  ) ? 1 : 0;
    const double pi = std::acos(-1);
    const double expFactor = -0.5*(0.02*pi)*(0.02*pi);
    for (uint8_t f=0; f < num_tns_filters; f++)
    {
        double r[9];
        double* rw=r;
        for (uint8_t k=0; k<=8; k++)
        {
            double r0 = (0==k) ? 3 : 0;
            double rk = 0;
            double eProd = 1;
            for (uint8_t s=0; s < 3; s++)
            {
                double es = 0;
                double ac = 0;
                for (int16_t n = sub_start[f][s]; n < sub_stop[f][s]; n++)
                {
                    es += X_s[n]*X_s[n];
                    if (n < (sub_stop[f][s] - k) )
                    {
                        ac += X_s[n]*X_s[n+k];
                    }
                }
                eProd *= es;
                rk += ac/es;
            }
            if (0==eProd) // is this floating-point comparision with 0 stable enough?
            {
                r[k] = r0;
            }
            else
            {
                r[k] = rk;
            }

            // lag-windowing (Note: in-place)
            rw[k] = r[k]*exp(expFactor*k*k);
        }

        // The Levinson-Durbin recursion is used to obtain LPC coefficients
        // and a predictor error
        double a_memory[2][9];
        double* a = a_memory[0];
        double* a_last = a_memory[1];
        double e = rw[0];
        a[0] = 1;
        for (uint8_t k=1; k <= 8; k++)
        {
            // swap buffer for a and a_last
            double* tmp = a_last;
            a_last = a;
            a = tmp;
            // a:=a^k; a_last:=a^(k-1)
            double rc = 0;
            for (uint8_t n=0; n < k; n++)
            {
                rc -= a_last[n] * rw[k-n];
            }
            if (0==e)
            {
                // is this handling to avoid division by 0 ok?
                e = 1;
            }
            rc /= e;
            a[0] = 1;
            for (uint8_t n=1; n < k; n++)
            {
                a[n] = a_last[n] + rc*a_last[k-n];
            }
            a[k] = rc;
            e = (1-rc*rc)*e;
        }
        double predGain = rw[0]/e; // again a suspicious possible division-by-zero! (should be handled above)
        const double thresh = 1.5;
        if (  (predGain <= thresh) || (near_nyquist_flag>0) )
        {
            // turn TNS filter f off
            double* rc = &rc_q[f*8+0];
            for (uint8_t n=0; n < 8; n++)
            {
                rc[n] = 0;
            }
        }
        else
        {
            // turn TNS filter f on
            // -> we need to compute reflection coefficients
            double gamma = 1;
            const double thresh2 = 2;
            if ( (tns_lpc_weighting>0) && (predGain < thresh2) )
            {
                const double gamma_min = 0.85;
                gamma -= (1-gamma_min) * (thresh2-predGain) / (thresh2-thresh);
            }
            double* aw = a; // compute in-place
            for (uint8_t k=0; k < 9; k++)
            {
                aw[k] = pow(gamma, k)*a[k];
            }
            double* rc = &rc_q[f*8+0];
            double* a_k   = aw;
            double* a_km1 = a_last;
            for (uint8_t k=8; k > 0; k--)
            {
                rc[k-1] = a_k[k];
                double e = (1-rc[k-1]*rc[k-1]);
                for (uint8_t n=1; n < k; n++)
                {
                    a_km1[n] = a_k[n] - rc[k-1]*a_k[k-n];
                    a_km1[n] /= e;
                }
                // swap buffer for a_k and a_km1
                double* tmp = a_k;
                a_k = a_km1;
                a_km1 = tmp;
            }
        }
    }

    // 3.3.8.3 Quantization  (d09r02_F2F)
    // with Δ =π/17
    const double quantizer_stepsize = pi/17;
    for (uint8_t f=0; f < num_tns_filters; f++)
    {
        double* rc = &rc_q[f*8+0]; // source (see above)

        for (uint8_t k=0; k < 8; k++)
        {
            // attention: rc in place of rc_q!
            rc_i[f*8+k] = nint( asin(rc[k]) / quantizer_stepsize ) + 8;
            rc_q[f*8+k] = sin( quantizer_stepsize * (rc_i[f*8+k]-8) );
        }

        // determine order of quantized reflection coefficients
        int8_t k=7; // need signed to stop while properly
        //while( (k>=0) && (0==rc_q[f*8+k]) ) // specification
        while( (k>=0) && (8==rc_i[f*8+k]) )  // alternative solution that should be more robust (and faster)
        {
            k--;
        }
        rc_order[f] = k+1;
    }
    for (uint8_t f=num_tns_filters; f < 2; f++)
    {
        for (uint8_t k=0; k < 8; k++)
        {
            rc_i[f*8+k] = 8;
            rc_q[f*8+k] = 0;
        }
        rc_order[f] = 0;
    }

    // bit budget
    nbits_TNS = 0;
    for (uint8_t f=0; f < num_tns_filters; f++)
    {
        uint32_t nbits_TNS_order = 0;
        if (rc_order[f] > 0)
        {
            nbits_TNS_order = ac_tns_order_bits[tns_lpc_weighting][rc_order[f]-1];
        }
        uint32_t nbits_TNS_coef = 0;
        for (uint8_t k=0; k < rc_order[f]; k++)
        {
            nbits_TNS_coef += ac_tns_coef_bits[k][rc_i[f*8+k]];
        }

        if (nullptr!=datapoints)
        {
            if (f==0)
            {
                datapoints->log("nbits_TNS_order_0", &nbits_TNS_order, sizeof(uint32_t));
                datapoints->log("nbits_TNS_coef_0", &nbits_TNS_coef, sizeof(uint32_t));
            }
            else
            {
                datapoints->log("nbits_TNS_order_1", &nbits_TNS_order, sizeof(uint32_t));
                datapoints->log("nbits_TNS_coef_1", &nbits_TNS_coef, sizeof(uint32_t));
            }
        }

        uint32_t nbits_TNS_local = 2048 + nbits_TNS_order + nbits_TNS_coef;
        nbits_TNS += ceil( nbits_TNS_local / 2048.0 ); // this code integrates Errata 15028 (see d1.0r03)
    }

    // 3.3.8.4 Filtering  (d09r02_F2F)
    for (uint16_t k = 0; k < lc3Config.NE; k++)
    {
        X_f[k] = X_s[k];
    }
    double st[8];
    for (uint8_t k = 0; k<8; k++)
    {
        st[k]=0;
    }
    for (uint8_t f=0; f < num_tns_filters; f++)
    {
        if (rc_order[f]>0)
        {
            for (uint16_t n = start_freq[f]; n < stop_freq[f]; n++)
            {
                double t = X_s[n];
                double st_save = t;
                for (uint8_t k=0; k < rc_order[f]-1; k++)
                {
                    double st_tmp = rc_q[f*8+k] * t + st[k];
                    t       += rc_q[f*8+k] * st[k];
                    st[k]    = st_save;
                    st_save  = st_tmp;
                }
                t += rc_q[f*8 + rc_order[f]-1] * st[rc_order[f]-1];
                st[rc_order[f]-1] = st_save;
                X_f[n] = t;
            }
        }
    }
}

int8_t TemporalNoiseShaping::nint(double x)
{
    if (x >= 0)
    {
        return static_cast<int8_t>(x+0.5);
    }
    else
    {
        return -static_cast<int8_t>(-x+0.5);
    }
}

void TemporalNoiseShaping::registerDatapoints(DatapointContainer* datapoints_)
{
    datapoints = datapoints_;
    if (nullptr != datapoints)
    {
        datapoints->addDatapoint( "X_f", &X_f[0], sizeof(double)*lc3Config.NE );
        datapoints->addDatapoint( "num_tns_filters", &num_tns_filters, sizeof(num_tns_filters) );
        datapoints->addDatapoint( "rc_order", &rc_order[0], sizeof(uint8_t)*2 );
        datapoints->addDatapoint( "rc_i_1", &rc_i[0], sizeof(uint8_t)*8 );
        datapoints->addDatapoint( "rc_i_2", &rc_i[8], sizeof(uint8_t)*8 );
        datapoints->addDatapoint( "rc_q_1", &rc_q[0], sizeof(double)*8 );
        datapoints->addDatapoint( "rc_q_2", &rc_q[8], sizeof(double)*8 );
        datapoints->addDatapoint( "nbits_TNS", &nbits_TNS, sizeof(nbits_TNS) );
    }
}

}//namespace Lc3Enc

