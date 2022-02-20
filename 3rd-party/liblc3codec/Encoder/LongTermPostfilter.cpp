/*
 * LongTermPostfilter.cpp
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

#include "LongTermPostfilter.hpp"
#include "LongTermPostfilterCoefficients.hpp"
#include <cmath>
#include <algorithm>

namespace Lc3Enc
{

LongTermPostfilter::LongTermPostfilter(const Lc3Config& lc3Config_, uint16_t nbits) :
    lc3Config(lc3Config_),
    len12p8( (lc3Config.N_ms==Lc3Config::FrameDuration::d10ms) ? 128 : 96),
    len6p4( (lc3Config.N_ms==Lc3Config::FrameDuration::d10ms) ? 64 : 48),
    D_LTPF( (lc3Config.N_ms==Lc3Config::FrameDuration::d10ms) ? 24 : 44),
    P( getP(lc3Config.Fs) ),
    P_times_res_fac( (lc3Config.Fs == 8000) ? 192000/8000/2 : getP(lc3Config.Fs) ),
    gain_ltpf_on( getGainLtpfOn(nbits, lc3Config_) ),
    pitch_index(0),
    pitch_present(0),
    ltpf_active(0),
    nbits_LTPF(0),
    T_prev(k_min),
    mem_pitch(0),
    mem_ltpf_active(0),
    mem_nc(0),
    mem_mem_nc(0),
    x_s_extended(nullptr),
    x_tilde_12p8D_extended(nullptr),
    datapoints(nullptr)
{
    x_s_extended = new double[240/P+lc3Config.NF];
    for (uint16_t n=0; n < 240/P+lc3Config.NF; n++)
    {
        x_s_extended[n]=0;
    }
    x_tilde_12p8D_extended = new double[len12p8+D_LTPF+Nmem12p8D];
    for (uint16_t n=0; n < len12p8+D_LTPF+Nmem12p8D; n++)
    {
        x_tilde_12p8D_extended[n] = 0;
    }
    h50_mem[0] = 0.0;
    h50_mem[1] = 0.0;
    for (uint8_t n=0; n < 64+k_max; n++)
    {
        x_6p4_extended[n] = 0;
    }
}

LongTermPostfilter::~LongTermPostfilter()
{
    if (nullptr!=x_s_extended)
    {
        delete[] x_s_extended;
    }
    if (nullptr!=x_tilde_12p8D_extended)
    {
        delete[] x_tilde_12p8D_extended;
    }
}

uint8_t LongTermPostfilter::getP(uint16_t fs)
{
    // Note: we assume that the calling code ensures that
    //       only valid fs values are provided
    if (fs==44100)
    {
        return 4;
    }
    else
    {
        return 192000/fs;
    }
}

uint8_t LongTermPostfilter::getGainLtpfOn(uint16_t nbits, const Lc3Config& lc3Config)
{
    // this is derived from pseudo-code in section 3.4.9.4 (d1.0r03) referenced
    // by Errata 15013; since the encode just needs to check gain_ltpf != 0,
    // the full code needed in the decoder is reduced to the simple computation of
    // gain_ltpf_on instead of gain_ltpf
    {
        uint16_t t_nbits = (lc3Config.N_ms == Lc3Config::FrameDuration::d7p5ms) ?
                round(nbits*10/7.5) : nbits;

        if (t_nbits < 560 + lc3Config.Fs_ind*80)
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }

}

LongTermPostfilter& LongTermPostfilter::operator= ( const LongTermPostfilter & src)
{
    // TODO we should assert, that NF is equal in src and *this!
    T_prev          = src.T_prev;
    mem_pitch       = src.mem_pitch;
    mem_ltpf_active = src.mem_ltpf_active;
    mem_nc          = src.mem_nc;
    mem_mem_nc      = src.mem_mem_nc;
    for (uint16_t n=0; n<240/P+lc3Config.NF; n++)
    {
        x_s_extended[n] = src.x_s_extended[n];
    }
    for (uint16_t n=0; n<len12p8+D_LTPF+Nmem12p8D; n++)
    {
        x_tilde_12p8D_extended[n] = src.x_tilde_12p8D_extended[n];
    }
    h50_mem[0] = src.h50_mem[0];
    h50_mem[1] = src.h50_mem[1];
    for (uint8_t n=0; n<64+k_max; n++)
    {
        x_6p4_extended[n] = src.x_6p4_extended[n];
    }

    return *this;
}


uint8_t LongTermPostfilter::pitchDetection(float& normcorr)
{
    //3.3.9.5 Pitch detection algorithm  (d09r02_F2F)
    const double* x_tilde_12p8D = &x_tilde_12p8D_extended[Nmem12p8D];
    const double h_2[5] = {
            0.1236796411180537, 0.2353512128364889, 0.2819382920909148,
            0.2353512128364889, 0.1236796411180537};

    double* x_6p4 = &x_6p4_extended[k_max]; // define current view on this signal
    for (uint8_t n = 0; n < len6p4; n++)
    {
        x_6p4[n] = 0;
        for (uint8_t k = 0; k < 5; k++)
        {
            x_6p4[n] += x_tilde_12p8D[2*n + k -3] * h_2[k];
        }
    }
    double R_6p4[k_max-k_min+1];
    double R_w_6p4[k_max-k_min+1];
    for (uint8_t k = k_min; k <= k_max; k++)
    {
        R_6p4[k-k_min] = 0;
        for (uint8_t n=0; n < len6p4; n++)
        {
            R_6p4[k-k_min] += x_6p4[n] * x_6p4[n-k];
        }
        double w = 1 - 0.5*(k-k_min)/(1.0*(k_max-k_min));
        R_w_6p4[k-k_min] = w * R_6p4[k-k_min];
    }
    if (nullptr != datapoints)
    {
        datapoints->log("R_6p4", &R_6p4[0], sizeof(double)*(k_max-k_min+1) );
        datapoints->log("R_w_6p4", &R_w_6p4[0], sizeof(double)*(k_max-k_min+1) );
    }
    uint8_t T1 = k_min;
    double R_w_6p4_max = R_w_6p4[0];
    for (uint8_t k = k_min+1; k <= k_max; k++)
    {
        if (R_w_6p4[k-k_min] > R_w_6p4_max)
        {
            R_w_6p4_max = R_w_6p4[k-k_min];
            T1 = k;
        }
    }
    // the following is not working with the static const members
    //const uint8_t k_s_min = std::max( k_min, static_cast<uint8_t>(T_prev-4));
    //const uint8_t k_s_max = std::min( k_max, static_cast<uint8_t>(T_prev+4) );
    uint8_t k_s_min = (k_min < (T_prev-4))
                    ? (T_prev-4) : k_min;
    uint8_t k_s_max = (k_max > (T_prev+4))
                    ? (T_prev+4) : k_max;
    uint8_t T2 = k_s_min;
    double R_6p4_max = R_6p4[k_s_min-k_min];
    for (uint8_t k = k_s_min+1; k <= k_s_max; k++)
    {
        if (R_6p4[k-k_min] > R_6p4_max)
        {
            R_6p4_max = R_6p4[k-k_min];
            T2 = k;
        }
    }

    // compute normalized correlation
    const uint8_t corr_len = len6p4;
    float normvalue0 = compute_normvalue(x_6p4, corr_len, 0);
    float normvalue1 = compute_normvalue(x_6p4, corr_len, T1);
    float normvalue = sqrt(normvalue0*normvalue1);
    float normcorr1 = 0; // is this a proper initialization?
    if (normvalue > 0)
    {
        normcorr1 = R_6p4[T1-k_min] / normvalue;
    }
    if (normcorr1 < 0) // implements max(0,normcorr1) from eq. (90) in (d09r06_KLG_AY_NH_FhG, 2019-12-20)
    {
        normcorr1 = 0;
    }
    float normcorr2 = normcorr1;
    if (T1 != T2)
    {
        float normvalue2 = compute_normvalue(x_6p4, corr_len, T2);
        normvalue = sqrt(normvalue0*normvalue2);
        if (normvalue > 0)
        {
            normcorr2 = R_6p4[T2-k_min] / normvalue;
        }
        if (normcorr2 < 0) // implements max(0,normcorr2) from eq. (90) in (d09r06_KLG_AY_NH_FhG, 2019-12-20)
        {
            normcorr2 = 0;
        }
    }
    uint8_t T_curr = T1;
    normcorr = normcorr1;
    if ( normcorr2 > 0.85*normcorr1 )
    {
        T_curr   = T2;
        normcorr = normcorr2;
    }

    if (nullptr != datapoints)
    {
        datapoints->log("T1", &T1, sizeof(T1) );
        datapoints->log("T2", &T2, sizeof(T2) );
        datapoints->log("T_curr", &T_curr, sizeof(T_curr) );
        datapoints->log("T_prev", &T_prev, sizeof(T_prev) );
        datapoints->log("normcorr1", &normcorr1, sizeof(normcorr1) );
        datapoints->log("normcorr2", &normcorr2, sizeof(normcorr2) );
        datapoints->log("normcorr", &normcorr, sizeof(normcorr) );
    }

    return T_curr;
}

float LongTermPostfilter::interp(const float* R_12p8, uint8_t pitch_int_rel, int8_t d)
{
    float interp_d = 0;
    for (int8_t m = -4; m <= 4; m++)
    {
        int8_t n = 4*m-d;
        if ( (-16 < n) && (n < 16) )
        {
            interp_d += R_12p8[pitch_int_rel + m] * tab_ltpf_interp_R[n+15];
        }
    }
    return interp_d;
}

void LongTermPostfilter::pitchLagParameter(uint8_t T_curr, uint8_t& pitch_int, int8_t& pitch_fr)
{
    // 3.3.9.7 LTPF pitch-lag parameter    (d09r02_F2F)
    uint8_t k_ss_min = (32 > (2*T_curr-4))
                    ? 32 : (2*T_curr-4);
    uint8_t k_ss_max = (228 < (2*T_curr+4))
                    ? 228 : (2*T_curr+4);

    const double* x_tilde_12p8D = &x_tilde_12p8D_extended[Nmem12p8D];
    float R_12p8[k_ss_max+4-(k_ss_min-4)+1];
    float R_12p8_max = 0;
    pitch_int = k_ss_min;
    for (uint8_t k=(k_ss_min-4); k <= k_ss_max+4; k++)
    {
        float corrv = 0;
        for (uint8_t n=0; n < len12p8; n++)
        {
            corrv += x_tilde_12p8D[n] * x_tilde_12p8D[n-k];
        }
        R_12p8[k-(k_ss_min-4)] = corrv;
        if ( (corrv > R_12p8_max) && (k>=k_ss_min) && (k<=k_ss_max) )
        {
            R_12p8_max = corrv;
            pitch_int  = k;
        }
    }
    if (nullptr != datapoints)
    {
        datapoints->log("R12.8", &R_12p8[0], sizeof(float)*17 ); //Note: sometimes less than 17 values are given; ignore this for debugging purpose
    }

    uint8_t pitch_int_rel = pitch_int - (k_ss_min-4);
    pitch_fr=0;
    if (32 == pitch_int)
    {
        float interp_d_max = 0;
        for (int8_t d=0; d <= 3; d++)
        {
            float interp_d = interp(R_12p8, pitch_int_rel, d);
            if (interp_d > interp_d_max)
            {
                interp_d_max = interp_d;
                pitch_fr = d;
            }
        }
    }
    else if ( (127 > pitch_int) && (pitch_int > 32) )
    {
        float interp_d_max = 0;
        for (int8_t d=-3; d <= 3; d++)
        {
            float interp_d = interp(R_12p8, pitch_int_rel, d);
            if (interp_d > interp_d_max)
            {
                interp_d_max = interp_d;
                pitch_fr = d;
            }
        }
    }
    else if ( (157 > pitch_int) && (pitch_int >= 127) )
    {
        float interp_d_max = 0;
        for (int8_t d=-2; d <= 2; d+=2)
        {
            float interp_d = interp(R_12p8, pitch_int_rel, d);
            if (interp_d > interp_d_max)
            {
                interp_d_max = interp_d;
                pitch_fr = d;
            }
        }
    }

    if (pitch_fr < 0)
    {
        pitch_int--;
        pitch_fr += 4;
    }

    if (127 > pitch_int)
    {
        pitch_index = 4*static_cast<uint16_t>(pitch_int) + pitch_fr - 128;
    }
    else if ( (157 > pitch_int) && (pitch_int >= 127) )
    {
        pitch_index = 2*static_cast<uint16_t>(pitch_int) + pitch_fr/2 + 126;
    }
    else
    {
        pitch_index = static_cast<uint16_t>(pitch_int) + 283;
    }
}

float LongTermPostfilter::x_i_n_d(int16_t n, int8_t d)
{
    const double* x_tilde_12p8D = &x_tilde_12p8D_extended[Nmem12p8D];
    float result = 0;
    for (int8_t k = -2; k <= 2; k++)
    {
        int8_t h_i_index = 4*k-d;
        if ( (-8 < h_i_index) && (h_i_index < 8) )
        {
            result += x_tilde_12p8D[n-k] * tab_ltpf_interp_x12k8[h_i_index+7];
        }
    }
    return result;
}


void LongTermPostfilter::activationBit(uint8_t pitch_int, uint8_t pitch_fr, uint8_t near_nyquist_flag, float& nc, float& pitch)
{
    // Note: given tests confirm that float is sufficient (at least for 16 bit PCM input)
    float nc_numerator = 0;
    float nc_norm1 = 0;
    float nc_norm2 = 0;
    for (uint8_t n=0; n < len12p8; n++)
    {
        float x_i_n_0 = x_i_n_d(n, 0);
        float x_i_n_shifted = x_i_n_d(static_cast<int16_t>(n) - static_cast<int16_t>(pitch_int), static_cast<int8_t>(pitch_fr));

        nc_numerator += x_i_n_0 * x_i_n_shifted;
        nc_norm1     += x_i_n_0 * x_i_n_0;
        nc_norm2     += x_i_n_shifted * x_i_n_shifted;
    }
    float nc_denominator = sqrt( nc_norm1 * nc_norm2 );
    nc = 0; // Is this ok? Or should we set other values for nc_denominator==0
    if (nc_denominator > 0)
    {
        nc = nc_numerator / nc_denominator;
    }
    if (nullptr != datapoints)
    {
        datapoints->log("nc_num", &nc_numerator, sizeof(nc_numerator) );
        datapoints->log("nc_den", &nc_denominator, sizeof(nc_denominator) );
        datapoints->log("nc", &nc, sizeof(nc) ); // somewhat redundant to "nc_ltpf", but never zeroed due to pitch_present
    }

    pitch = pitch_int + pitch_fr/4.0;

    // part of 3.3.9.8 LTPF activation bit including Errata 15013 (see d1.0r03) and Errate 15250
    if ( (gain_ltpf_on != 0) && (near_nyquist_flag==0) )
    {
        if (
            ( (mem_ltpf_active==0) && ( (lc3Config.N_ms==Lc3Config::FrameDuration::d10ms) || (mem_mem_nc>0.94) ) &&
              (mem_nc>0.94) && (nc>0.94) ) ||
            ( (mem_ltpf_active==1) && (nc>0.9)) ||
            ( (mem_ltpf_active==1) && (fabs(pitch-mem_pitch)<2) && ((nc-mem_nc)>-0.1) && (nc>0.84) )
           )
        {
            ltpf_active = 1;
        }
        else
        {
            ltpf_active = 0;
        }
    }
    else
    {
        ltpf_active = 0;
    }

}

void LongTermPostfilter::run(const int16_t* const x_s_, uint8_t near_nyquist_flag)
{
    // 3.3.9.2 Time-domain signals  (d09r02_F2F)
    // ...just some Notes on dependencies on samples in the previous block

    // shift x_s_extended by one frame and append new input "x_s_"
    for (uint16_t m = 0; m < 240/P; m++)
    {
        x_s_extended[m] = x_s_extended[lc3Config.NF+m];
    }
    for (uint16_t m = 240/P; m < (240/P+lc3Config.NF); m++)
    {
        x_s_extended[m] = x_s_[m-240/P];
    }
    double* x_s = &x_s_extended[240/P];
    // shift x_tilde_12p8D_extended by one frame
    for (uint16_t n = 0; n < D_LTPF+Nmem12p8D; n++)
    {
        x_tilde_12p8D_extended[n] = x_tilde_12p8D_extended[n+len12p8];
    }
    // shift x_6p4_extended by one frame
    for (uint8_t n = 0; n < k_max; n++)
    {
        x_6p4_extended[n] = x_6p4_extended[n+len6p4];
    }

    // 3.3.9.3 Resampling  (d09r02_F2F)
    double* h_12p8 = &tab_resamp_filter[119];
    double* x_12p8 = &x_tilde_12p8D_extended[D_LTPF+Nmem12p8D];
    for (int16_t n = 0; n < len12p8; n++)
    {
        x_12p8[n] = 0;
        for (int16_t k = -120/P; k <= 120/P; k++)
        {
            const int16_t index_x_s = (15*n)/P + k - 120/P;
            const int16_t index_h = P*k - ( (15*n)%P );
            if ( (-120 < index_h) && (index_h < 120) )
            {
                x_12p8[n] += x_s[ index_x_s ] * h_12p8[ index_h ];
            }
        }
        x_12p8[n] *= P_times_res_fac; // P x res_fac according to Errata 15217
    }

    // 3.3.9.4 High-pass filtering  (d09r02_F2F)
    filterH50(x_12p8); // in-place

    //3.3.9.5 Pitch detection algorithm  (d09r06_FhG)
    float normcorr=0;
    uint8_t T_curr = pitchDetection(normcorr);


    // 3.3.9.7 LTPF pitch-lag parameter    (d09r02_F2F)
    uint8_t pitch_int;
    int8_t pitch_fr;
    pitchLagParameter(T_curr, pitch_int, pitch_fr);

    // 3.3.9.8 LTPF activation bit   (d09r06_FhG)
    float nc = 0;
    float pitch = 0;
    activationBit(pitch_int, pitch_fr, near_nyquist_flag, nc, pitch);


    // 3.3.9.6 LTPF Bitstream  (d09r06_KLG_AY_NH_FhG, 2019-12-20)
    pitch_present = (normcorr > 0.6) ? 1 : 0;

    nbits_LTPF = (0==pitch_present) ? 1 : 11;

    if (0==pitch_present)
    {
        // resetting of these variables not found explicitly within the specification, but the
        // specified intermediate encoder results suggest that the reset is needed here
        pitch_index = 0;
        nc = 0;
    }

    if (nullptr != datapoints)
    {
        datapoints->log("pitch_int", &pitch_int, sizeof(pitch_int) );
        datapoints->log("pitch_fr", &pitch_fr, sizeof(pitch_fr));
        datapoints->log("nc_ltpf", &nc, sizeof(nc) );
        datapoints->log("mem_ltpf_active", &mem_ltpf_active, sizeof(mem_ltpf_active) );
        datapoints->log("mem_nc_ltpf", &mem_nc, sizeof(mem_nc) );
    }

    // prepare states for next run
    T_prev = T_curr;
    mem_mem_nc = mem_nc;
    if (0==pitch_present)
    {
        mem_pitch = 0;
        mem_ltpf_active = 0;
        mem_nc = 0;
    }
    else
    {
        mem_pitch = pitch;
        mem_ltpf_active = ltpf_active; // is it ok to set this here?
        mem_nc = nc;
    }
}

float LongTermPostfilter::compute_normvalue(const double* const x, uint8_t L, uint8_t T)
{
    float normvalue = 0;
    for (uint8_t n=0; n < L; n++)
    {
        normvalue += x[n-T]*x[n-T];
    }
    return normvalue;
}

void LongTermPostfilter::filterH50(double* xy) // operate in-place
{
    // TODO: check whether this implementation in Direct-Form_II
    //       is appropriate here (mainly in terms of robustness)
    const double b0 = 0.9827947082978771;
    const double b1 = -1.965589416595754;
    const double b2 = 0.9827947082978771;
    const double a1 = -1.9652933726226904;
    const double a2 = 0.9658854605688177;
    double* x = xy;
    double* y = xy;
    for (uint8_t n = 0; n < len12p8; n++)
    {
        double v = x[n] - a1*h50_mem[0] - a2*h50_mem[1];
        y[n]     = b0*v + b1*h50_mem[0] + b2*h50_mem[1];
        h50_mem[1] = h50_mem[0];
        h50_mem[0] = v;
    }
}


void LongTermPostfilter::registerDatapoints(DatapointContainer* datapoints_)
{
    datapoints = datapoints_;
    if (nullptr != datapoints)
    {
        datapoints->addDatapoint( "x_tilde_12.8D", &x_tilde_12p8D_extended[Nmem12p8D], sizeof(double)*(len12p8+1) );
        datapoints->addDatapoint( "x_tilde_12.8D_ext", &x_tilde_12p8D_extended[0], sizeof(double)*(len12p8+D_LTPF+Nmem12p8D) );
        datapoints->addDatapoint( "x_6p4", &x_6p4_extended[k_max], sizeof(double)*64 );

        datapoints->addDatapoint( "pitch_index", &pitch_index, sizeof(pitch_index) );
        datapoints->addDatapoint( "pitch_present", &pitch_present, sizeof(pitch_present) );
        datapoints->addDatapoint( "ltpf_active", &ltpf_active, sizeof(ltpf_active) );
        datapoints->addDatapoint( "nbits_LTPF", &nbits_LTPF, sizeof(nbits_LTPF) );
    }
}

}//namespace Lc3Enc
