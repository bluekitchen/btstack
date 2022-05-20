/*
 * DecoderFrame.cpp
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

#include "DecoderFrame.hpp"
#include "BitReader.hpp"

#include <cstring>
#include <cmath>

namespace Lc3Dec
{

DecoderFrame::DecoderFrame(
    ResidualSpectrum& residualSpectrum_,
    SpectralNoiseShaping& spectralNoiseShaping_,
    PacketLossConcealment& packetLossConcealment_,
    MdctDec& mdctDec_,
    const Lc3Config& lc3Config_,
    uint16_t nbytes_)
    :
    nbytes(nbytes_),
    nbits(nbytes_ * 8),
    lc3Config(lc3Config_),
    tns_lpc_weighting((nbits < ((lc3Config.N_ms==Lc3Config::FrameDuration::d10ms) ? 480 : 360)  ) ? 1 : 0),
    sideInformation(lc3Config.NF, lc3Config.NE, lc3Config.Fs_ind),
    arithmeticDec(lc3Config.NF, lc3Config.NE, (nbits > (160 + lc3Config.Fs_ind * 160)) ? 512:0, tns_lpc_weighting),
    residualSpectrum(residualSpectrum_),
    spectralNoiseShaping(spectralNoiseShaping_),
    packetLossConcealment(packetLossConcealment_),
    mdctDec(mdctDec_),
    longTermPostfilter(lc3Config, nbits),

    datapoints(nullptr),

    frameN(0),
    lastnz(0),
    P_BW(0),
    lsbMode(0),
    gg_ind(0),
    num_tns_filters(0),
    pitch_present(0),
    pitch_index(0),
    ltpf_active(0),
    F_NF(0),
    ind_LF(0),
    ind_HF(0),
    Gind(0),
    LS_indA(0),
    LS_indB(0),
    idxA(0),
    idxB(0),
    nf_seed(0),
    zeroFrame(0),
    gg_off(0),
    X_hat_q_nf(nullptr),
    X_hat_f(nullptr),
    X_s_tns(nullptr),
    X_hat_ss(nullptr),
    x_hat_clip(nullptr)
{
    rc_order[0] = 0;
    rc_order[1] = 0;

    X_hat_q_nf = new double[lc3Config.NE];
    X_hat_f = new double[lc3Config.NE];
    X_s_tns = new double[lc3Config.NE];
    X_hat_ss = new double[lc3Config.NE];

}

DecoderFrame::~DecoderFrame()
{
    if (nullptr != X_hat_q_nf)
    {
        delete[] X_hat_q_nf;
    }
    if (nullptr != X_hat_f)
    {
        delete[] X_hat_f;
    }
    if (nullptr != X_s_tns)
    {
        delete[] X_s_tns;
    }
    if (nullptr != X_hat_ss)
    {
        delete[] X_hat_ss;
    }
    if (nullptr != x_hat_clip)
    {
        delete[] x_hat_clip;
    }
}

void DecoderFrame::linkPreviousFrame(DecoderFrame* previousFrame)
{
    if (nullptr != previousFrame)
    {
        longTermPostfilter = previousFrame->longTermPostfilter;
        frameN = previousFrame->frameN;
    }
}

void DecoderFrame::noiseFilling()
{
    //3.4.4 Noise filling (d09r02_F2F)
    // including extensions according to:
    // section 3.4.4. Noise filling (d09r04)
    //Noise filling is performed only when zeroFrame is 0.
    for (int16_t k = 0; k < lc3Config.NE; k++)
    {
        X_hat_q_nf[k] = residualSpectrum.X_hat_q_residual[k];
    }
    if (0 == zeroFrame)
    {
        //bandwidth(𝑃𝑏𝑤)
        //       NB   WB  SSWB   SWB   FB
        //𝑏𝑤_𝑠𝑡𝑜𝑝 80  160  240    320  400
        uint16_t bw_stop_table[5] = {80,  160,  240,    320,  400};
        uint16_t bw_stop = bw_stop_table[P_BW];
        if (lc3Config.N_ms==Lc3Config::FrameDuration::d7p5ms)
        {
            bw_stop *= 3;
            bw_stop /= 4;
        }

        uint16_t NFstart = (lc3Config.N_ms==Lc3Config::FrameDuration::d10ms) ? 24 : 18;
        uint16_t NFwidth = (lc3Config.N_ms==Lc3Config::FrameDuration::d10ms) ? 3 : 2;

        /*
        𝐿𝑁𝐹 ̂ = (8-𝐹𝑁𝐹)/16;
        for k=0..bw_stop-1
            if 𝐼𝑁𝐹(k)==1
                nf_seed = (13849+nf_seed*31821) & 0xFFFF;
            if nf_seed<0x8000
                𝑋𝑞 ̂(𝑘) = 𝐿𝑁𝐹 ̂ ;
            else
                𝑋𝑞 ̂(𝑘) = −𝐿𝑁𝐹 ̂ ;
        */
        uint16_t nf_state = nf_seed;
        double L_NF_hat = (8-F_NF) / 16.0;
        for (uint16_t k=0; k < bw_stop; k++)
        {
            /*
            The indices for the relevant spectral coefficients are given by:
            𝐼𝑁𝐹 (𝑘) = {
                1 if 24 ≤ 𝑘 < 𝑏𝑤_𝑠𝑡𝑜𝑝 𝑎𝑛𝑑 𝑋𝑞 ̂(𝑖) == 0 𝑓𝑜𝑟 𝑎𝑙𝑙 𝑖 = 𝑘 − 3. . min(𝑏𝑤𝑠𝑡𝑜𝑝 − 1, 𝑘 + 3)
                0 otherwise
            # (109)
            where 𝑏𝑤_𝑠𝑡𝑜𝑝 depends on the bandwidth information (see Section 3.4.2.4) as defined in Table 3.17.
            */
            uint8_t I_NF_k = 0;
            if (  (NFstart <= k)  && (k < bw_stop) )
            {
                uint16_t limit = ((bw_stop - 1) <  (k + NFwidth)) ? (bw_stop - 1) : (k + NFwidth);
                I_NF_k = 1;
                for (uint16_t i = k-NFwidth; i <= limit; i++)
                {
                    if (0 != residualSpectrum.X_hat_q_residual[i])
                    {
                        I_NF_k = 0;
                        break;
                    }
                }
            }

            if ( 1 == I_NF_k )
            {
                nf_state = (13849+nf_state*31821) & 0xFFFF;
                if ( nf_state < 0x8000 )
                {
                    X_hat_q_nf[k] = L_NF_hat;
                }
                else
                {
                    X_hat_q_nf[k] = -L_NF_hat;
                }
            }
        }
    }
}


void DecoderFrame::applyGlobalGain()
{
    //3.4.5 Global gain (d09r02_F2F)
    //The global gain is applied to the spectrum after noise filling has been applied using the following formula (110) & (111)
    int16_t v1 = nbits / (10*(lc3Config.Fs_ind+1));
    if (v1 > 115)
    {
        gg_off = -115;
    }
    else
    {
        gg_off = -v1;
    }
    gg_off -= 105;
    gg_off -= 5*(lc3Config.Fs_ind+1);

    double exponent = (gg_ind + gg_off) / 28.0;
    double gg = pow( 10.0, exponent);
    for (int16_t k = 0; k < lc3Config.NE; k++)
    {
        X_hat_f[k] = gg * X_hat_q_nf[k];
    }
}

void DecoderFrame::temporalNoiseShaping()
{
    // 3.4.6 TNS DecoderFrame (d09r02_F2F)
    /*
    for 𝑘 = 0 to 𝑁𝐸 − 1 do {
        𝑋𝑠 ̂(𝑛) = 𝑋𝑓 ̂(𝑛)
    }
    s0 = s1 = s2 = s3 = s4 = s5 = s6 = s7 = 0
    for 𝑓 = 0 to num_tns_filters-1 do {
        if (𝑟𝑐𝑜𝑟𝑑𝑒𝑟 (𝑓) > 0)
        {
            for 𝑛 = start_freq(𝑓) to stop_freq(f) − 1 do {
                t = 𝑋𝑓 ̂ (𝑛) − 𝑟𝑐𝑞 (𝑟𝑐𝑜𝑟𝑑𝑒𝑟 (𝑓) − 1 , 𝑓) ∙ 𝑠𝑟𝑐𝑜𝑟𝑑𝑒𝑟(𝑓)−1
                for 𝑘 = 𝑟𝑐𝑜𝑟𝑑𝑒𝑟 (𝑓) − 2 to 0 do {
                    𝑡 = 𝑡 − 𝑟𝑐𝑞 (𝑘, 𝑓) ∙ 𝑠𝑘
                    𝑠𝑘+1 = 𝑟𝑐𝑞 (𝑘, 𝑓) ∙ 𝑡 + 𝑠𝑘
                }
                𝑋𝑆 ̂(𝑛) = 𝑡
                𝑠0 = 𝑡
            }
        }
    }
    */
    uint16_t start_freq[2] = {12, 160};
    uint16_t stop_freq[2];
    if (lc3Config.N_ms==Lc3Config::FrameDuration::d10ms)
    {
        if (4==P_BW) start_freq[1] = 200;
        switch(P_BW)
        {
            case 0:
                stop_freq[0] = 80;
                break;
            case 1:
                stop_freq[0] = 160;
                break;
            case 2:
                stop_freq[0] = 240;
                break;
            case 3:
                stop_freq[0] = 160;
                stop_freq[1] = 320;
                break;
            case 4:
                stop_freq[0] = 200;
                stop_freq[1] = 400;
                break;
        }
    }
    else
    {
        start_freq[0] = 9;
        if (3==P_BW) start_freq[1] = 120;  // Errata 15098 implemented
        if (4==P_BW) start_freq[1] = 150;
        switch(P_BW)
        {
            case 0:
                stop_freq[0] = 60;
                break;
            case 1:
                stop_freq[0] = 120;
                break;
            case 2:
                stop_freq[0] = 180;
                break;
            case 3:
                //stop_freq[0] = 119; // this value is specified in Table 3.19 (d09r06_KLG_AY_NH_FhG, 2019-12-20), but gives poor match to 32kHz decoder tests compared to reference decoder
                stop_freq[0] = 120; // this value gives good match to reference decoder and is more consistent to 10ms case
                stop_freq[1] = 240;
                break;
            case 4:
                stop_freq[0] = 150;
                stop_freq[1] = 300;
                break;
        }
    }


    for (int16_t k = 0; k < lc3Config.NE; k++)
    {
        X_s_tns[k] = X_hat_f[k];
    }
    double s[8];
    for (uint8_t k=0; k<8; k++)
    {
        s[k] = 0.0;
    }
    for (uint8_t f=0; f < num_tns_filters; f++)
    {

        if (arithmeticDec.rc_order_ari[f] > 0)
        {
            for (uint16_t n=start_freq[f]; n < stop_freq[f]; n++)
            {
                double t = X_hat_f[n]
                            - arithmeticDec.rc_q( arithmeticDec.rc_order_ari[f]-1, f)
                            * s[arithmeticDec.rc_order_ari[f]-1];
                for (int8_t k = arithmeticDec.rc_order_ari[f]-2; k >=0; k--)
                {
                    t = t - arithmeticDec.rc_q( k, f) * s[k];
                    s[k+1] = arithmeticDec.rc_q( k, f)*t + s[k];
                }
                X_s_tns[n] = t;
                s[0] = t;
            }
        }
    }
}

void DecoderFrame::runFloat(const uint8_t *bytes, uint8_t BFI, uint8_t& BEC_detect)
{
    // increment frame counter
    frameN++;

    // 5.4.2.2 Initialization
    uint16_t bp = 0;
    uint16_t bp_side = nbytes - 1;
    uint8_t mask_side = 1;
    BEC_detect = BFI;  // Note: the base specification initializes BEC_detect with zero, but initialization with BFI is more meaningful

    // 5.4.2.3 Side information
    if (!BEC_detect)
    {
        sideInformation.run(
            bytes,
            bp_side,
            mask_side,
            P_BW,
            lastnz,
            lsbMode,
            gg_ind,
            num_tns_filters,
            rc_order,
            pitch_present,
            pitch_index,
            ltpf_active,
            F_NF,
            ind_LF,
            ind_HF,
            Gind,
            LS_indA,
            LS_indB,
            idxA,
            idxB,
            BEC_detect
        );
    }

    // 3.4.2.4 Bandwidth interpretation (d09r02_F2F)
    // ...included somewhere else?

    // 3.4.2.5 Arithmetic decoding (d09r02_F2F)
    if (!BEC_detect)
    {
        arithmeticDec.run(
            bytes,
            bp,
            bp_side,
            mask_side,
            num_tns_filters,
            rc_order,
            lsbMode,
            lastnz,
            nbits,
            BEC_detect
        );
    }

    if (!BEC_detect)
    {
        /* Decode residual bits */
        // and 3.4.3 Residual decoding  (d09r02_F2F)
        residualSpectrum.run(
            bytes,
            bp_side,
            mask_side,
            lastnz,
            arithmeticDec.X_hat_q_ari,
            arithmeticDec.nbits_residual,
            arithmeticDec.save_lev,
            lsbMode,
            nf_seed,
            zeroFrame,
            gg_ind,
            F_NF
        );

        //3.4.4 Noise filling (d09r02_F2F)
        noiseFilling();

        //3.4.5 Global gain (d09r02_F2F)
        applyGlobalGain();

        // 3.4.6 TNS decoder (d09r02_F2F)
        temporalNoiseShaping();

        //3.4.7 SNS decoder (d09r02_F2F)
        spectralNoiseShaping.run(
            X_s_tns,
            X_hat_ss,
            ind_LF,
            ind_HF,
            sideInformation.submodeMSB,
            sideInformation.submodeLSB,
            Gind,
            LS_indA,
            LS_indB,
            idxA,
            idxB
        );
    }

    // Appendix B. Packet Loss Concealment   (d09r02_F2F)
    packetLossConcealment.run(BEC_detect, X_hat_ss, ltpf_active);

    //3.4.8 Low delay MDCT synthesis   (d09r02_F2F)
    mdctDec.run(X_hat_ss);

    //3.4.9 Long Term Postfilter   (d09r02_F2F)
    if (0 == pitch_present)
    {
        pitch_index = 0;
        ltpf_active = 0;
    }
    longTermPostfilter.setInputX(mdctDec.x_hat_mdct);
    longTermPostfilter.run(ltpf_active, pitch_index);

}

void DecoderFrame::registerDatapoints(DatapointContainer* datapoints_)
{
    datapoints = datapoints_;
    if (nullptr != datapoints)
    {
        datapoints->addDatapoint( "fs_idx", &lc3Config.Fs_ind, sizeof(lc3Config.Fs_ind) );

        datapoints->addDatapoint( "frameN", &frameN, sizeof(frameN) );

        datapoints->addDatapoint( "lastnz", &lastnz, sizeof(lastnz) );
        datapoints->addDatapoint( "P_BW", &P_BW, sizeof(P_BW) );
        datapoints->addDatapoint( "lsbMode", &lsbMode, sizeof(lsbMode) );
        datapoints->addDatapoint( "gg_ind", &gg_ind, sizeof(gg_ind) );
        datapoints->addDatapoint( "num_tns_filters", &num_tns_filters, sizeof(num_tns_filters) );
        datapoints->addDatapoint( "rc_order", &rc_order[0], sizeof(rc_order) );
        datapoints->addDatapoint( "pitch_index", &pitch_index, sizeof(pitch_index) );
        datapoints->addDatapoint( "pitch_present", &pitch_present, sizeof(pitch_present) );
        datapoints->addDatapoint( "ltpf_active", &ltpf_active, sizeof(ltpf_active) );
        datapoints->addDatapoint( "F_NF", &F_NF, sizeof(F_NF) );
        datapoints->addDatapoint( "ind_LF", &ind_LF, sizeof(ind_LF) );
        datapoints->addDatapoint( "ind_HF", &ind_HF, sizeof(ind_HF) );
        datapoints->addDatapoint( "Gind", &Gind, sizeof(Gind) );
        datapoints->addDatapoint( "LS_indA", &LS_indA, sizeof(LS_indA) );
        datapoints->addDatapoint( "idxA", &idxA, sizeof(idxA) );
        datapoints->addDatapoint( "idxB", &idxB, sizeof(idxB) );

        datapoints->addDatapoint( "nf_seed", &nf_seed, sizeof(nf_seed) );
        datapoints->addDatapoint( "zeroFrame", &zeroFrame, sizeof(zeroFrame) );

        datapoints->addDatapoint( "gg_off", &gg_off, sizeof(gg_off) );
        datapoints->addDatapoint( "rc_i_tns", &arithmeticDec.rc_i[0], sizeof(arithmeticDec.rc_i[0])*8 );

        datapoints->addDatapoint( "X_hat_q_nf", &X_hat_q_nf[0], sizeof(double)*lc3Config.NE );
        datapoints->addDatapoint( "X_s_tns", &X_s_tns[0], sizeof(double)*lc3Config.NE );
        datapoints->addDatapoint( "X_hat_ss", &X_hat_ss[0], sizeof(double)*lc3Config.NE );

        if (nullptr==x_hat_clip)
        {
            x_hat_clip = new double[lc3Config.NF];
        }
        datapoints->addDatapoint( "x_hat_clip", &x_hat_clip[0], sizeof(double)*lc3Config.NF );

        sideInformation.registerDatapoints(datapoints);
        arithmeticDec.registerDatapoints(datapoints);
        longTermPostfilter.registerDatapoints(datapoints);
    }
}

}//namespace Lc3Dec
