/*
 * DecoderFrame.hpp
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

#ifndef __DECODER_FRAME_HPP_
#define __DECODER_FRAME_HPP_

#include <cstdint>
#include "Datapoints.hpp"
#include "SideInformation.hpp"
#include "ArithmeticDec.hpp"
#include "ResidualSpectrum.hpp"
#include "SpectralNoiseShaping.hpp"
#include "PacketLossConcealment.hpp"
#include "MdctDec.hpp"
#include "LongTermPostfilter.hpp"
#include "Lc3Config.hpp"

namespace Lc3Dec
{

class DecoderFrame
{
    public:
        DecoderFrame(
            ResidualSpectrum& residualSpectrum_,
            SpectralNoiseShaping& spectralNoiseShaping_,
            PacketLossConcealment& packetLossConcealment_,
            MdctDec& mdctDec_,
            const Lc3Config& lc3Config_,
            uint16_t nbytes_);
        virtual ~DecoderFrame();

        void registerDatapoints(DatapointContainer* datapoints_);

        template<typename T>
        void run(
                const uint8_t *bytes,
                uint8_t BFI,
                uint16_t bits_per_audio_sample_dec,
                T* x_out,
                uint8_t& BEC_detect
                )
        {
            if (!lc3Config.isValid())
            {
                return;
            }

            // main decoder implementation: 3.4.1 until 3.4.9 (d09r06)
            runFloat(bytes, BFI, BEC_detect);

            //3.4.10 Output signal scaling and rounding   (d09r06)
            const uint32_t outputScale = 1UL << (-15+bits_per_audio_sample_dec-1);
            for (uint16_t k=0; k<lc3Config.NF; k++)
            {
                double x_hat_clip_local;
                if ( longTermPostfilter.x_hat_ltpf[k] > 32767)
                {
                    x_hat_clip_local = 32767;
                }
                else if ( longTermPostfilter.x_hat_ltpf[k] < -32768)
                {
                    x_hat_clip_local = -32768;
                }
                else
                {
                    x_hat_clip_local = longTermPostfilter.x_hat_ltpf[k];
                }

                // datapoint buffer for x_hat_clip only prepared when datapoints are available
                if (nullptr!=x_hat_clip)
                {
                    x_hat_clip[k] = x_hat_clip_local;
                }

                // Round 𝑥 to nearest integer, e.g., ⌊−4.5⌉ = −5, ⌊−3.2⌉ = −3, ⌊3.2⌉ = 3, ⌊4.5⌉ = 5,
                if (x_hat_clip_local > 0)
                {
                    x_out[k] = outputScale*x_hat_clip_local+0.5;
                }
                else
                {
                    x_out[k] = outputScale*x_hat_clip_local-0.5;
                }
            }

            if (nullptr!=datapoints)
            {
                datapoints->log( "x_out", &x_out[0], sizeof(T)*lc3Config.NF );
                datapoints->log("BER_detect", &BEC_detect, sizeof(BEC_detect) );
            }
        }

        void linkPreviousFrame(DecoderFrame* previousFrame);

        // per instance constant parameter
        const uint16_t nbytes;
        const uint16_t nbits;
        const Lc3Config& lc3Config;
        const uint8_t tns_lpc_weighting;

    private:
        void runFloat(const uint8_t *bytes, uint8_t BFI, uint8_t& BEC_detect);

        void noiseFilling();
        void applyGlobalGain();
        void temporalNoiseShaping();

        SideInformation sideInformation;
        ArithmeticDec arithmeticDec;
        ResidualSpectrum& residualSpectrum;
        SpectralNoiseShaping& spectralNoiseShaping;
        PacketLossConcealment& packetLossConcealment;
        MdctDec& mdctDec;
        LongTermPostfilter longTermPostfilter;

        DatapointContainer* datapoints;

        // states & outputs
        int16_t frameN;

        int16_t lastnz;
        int16_t P_BW;
        uint8_t lsbMode;
        int16_t gg_ind;
        int16_t num_tns_filters;
        int16_t rc_order[2];
        uint8_t pitch_present;
        int16_t pitch_index;
        int16_t ltpf_active;
        int16_t F_NF;
        int16_t ind_LF;
        int16_t ind_HF;
        int16_t Gind;
        int16_t LS_indA;
        int16_t LS_indB;
        int32_t idxA;
        int16_t idxB;
        uint16_t nf_seed;
        uint16_t zeroFrame;
        int16_t gg_off;
        double* X_hat_q_nf;
        double* X_hat_f;
        double* X_s_tns;
        double* X_hat_ss;
        double* x_hat_clip;
};

}//namespace Lc3Dec

#endif // __DECODER_FRAME_HPP_
