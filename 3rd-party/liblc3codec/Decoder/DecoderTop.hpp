/*
 * DecoderTop.hpp
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

#ifndef __DECODER_TOP_HPP_
#define __DECODER_TOP_HPP_

#include <cstdint>
#include "Lc3Config.hpp"
#include "DecoderFrame.hpp"
#include "PacketLossConcealment.hpp"
#include "ResidualSpectrum.hpp"
#include "SpectralNoiseShaping.hpp"
#include "MdctDec.hpp"
#include "Datapoints.hpp"

namespace Lc3Dec
{

class DecoderTop
{
    public:
        DecoderTop(uint16_t fs_, DatapointContainer* datapoints_=nullptr);
        DecoderTop(Lc3Config lc3Config_, DatapointContainer* datapoints_=nullptr);
        virtual ~DecoderTop();

        template<typename T>
        void run(
                const uint8_t *bytes,
                uint16_t nbytes,
                uint8_t BFI,
                uint8_t bits_per_audio_sample_dec,
                T* x_out,
                uint8_t& BEC_detect
                )
        {
            if ( (nullptr == decoderCurrentFrame) || (decoderCurrentFrame->nbytes != nbytes) )
            {
                // We need to instantiate a new DecoderFrame only
                // when byte_count has been changed. Thus, frequent
                // dynamic memory allocation can be constrained to
                // rate optimized operation.
                if (nullptr != decoderPreviousFrame)
                {
                    delete decoderPreviousFrame;
                }
                decoderPreviousFrame = decoderCurrentFrame;
                decoderCurrentFrame = new DecoderFrame(
                    residualSpectrum,
                    spectralNoiseShaping,
                    packetLossConcealment,
                    mdctDec,
                    lc3Config,
                    nbytes );
                decoderCurrentFrame->linkPreviousFrame(decoderPreviousFrame);
                if (nullptr != datapoints)
                {
                    // Another way to make to call optional would be to split run() into
                    //  init(), registerDatapoints() & remaining run() -> we preferred the simpler
                    // API and thus designed this registration being dependent on the availability of datapoints.
                    decoderCurrentFrame->registerDatapoints( datapoints );
                }
            }
            decoderCurrentFrame->run<T>(bytes, BFI, bits_per_audio_sample_dec, x_out, BEC_detect);
        }

        const Lc3Config lc3Config;

    private:
        DatapointContainer* datapoints;
        DecoderFrame* decoderCurrentFrame;
        DecoderFrame* decoderPreviousFrame;
        ResidualSpectrum residualSpectrum;
        SpectralNoiseShaping spectralNoiseShaping;
        PacketLossConcealment packetLossConcealment;
        MdctDec mdctDec;
};

}//namespace Lc3Dec

#endif // __DECODER_TOP_HPP_
