/*
 * EncoderFrame.cpp
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

#include "EncoderFrame.hpp"

#include <cstring>
#include <cmath>

namespace Lc3Enc
{

EncoderFrame::EncoderFrame(
        MdctEnc& mdctEnc_,
        AttackDetector& attackDetector_,
        SpectralNoiseShaping& spectralNoiseShaping_,
        TemporalNoiseShaping& temporalNoiseShaping_,
        SpectralQuantization& spectralQuantization_,
        NoiseLevelEstimation& noiseLevelEstimation_,
        const Lc3Config& lc3Config_,
        uint16_t nbytes_) :
    lc3Config(lc3Config_),
    nbytes(nbytes_),
    datapoints(nullptr),
    mdctEnc(mdctEnc_),
    bandwidthDetector(lc3Config),
    attackDetector(attackDetector_),
    spectralNoiseShaping(spectralNoiseShaping_),
    temporalNoiseShaping(temporalNoiseShaping_),
    longTermPostfilter(lc3Config, nbytes_ * 8),
    spectralQuantization(spectralQuantization_),
    noiseLevelEstimation(noiseLevelEstimation_),
    bitstreamEncoding(lc3Config.NE, nbytes),
    frameN(0)
{
}

EncoderFrame::~EncoderFrame()
{
}

void EncoderFrame::linkPreviousFrame(EncoderFrame* previousFrame)
{
    if (NULL != previousFrame)
    {
        longTermPostfilter = previousFrame->longTermPostfilter;
        this->frameN = previousFrame->frameN;
    }
}

void EncoderFrame::run(const int16_t* x_s, uint8_t* bytes)
{
    // increment frame counter
    frameN++;


    // 3.3.3 Input signal scaling (d09r02_F2F)
    // -> should be made in calling code due to different data types being needed

    //3.3.4 Low delay MDCT analysis   (d09r02_F2F)
    mdctEnc.run(x_s);

    //3.3.5 Bandwidth detector   (d09r02_F2F)
    bandwidthDetector.run(mdctEnc.E_B);

    //3.3.6 Time domain attack detector   (d09r02_F2F)
    attackDetector.run(x_s, nbytes);

    //3.3.7 Spectral Noise Shaping (SNS)   (d09r02_F2F)
    spectralNoiseShaping.run(mdctEnc.X, mdctEnc.E_B, attackDetector.F_att);

    //3.3.8 Temporal Noise Shaping (TNS)   (d09r02_F2F)
    const uint16_t  nbits = nbytes * 8;
    temporalNoiseShaping.run(spectralNoiseShaping.X_S, bandwidthDetector.P_bw, nbits, mdctEnc.near_nyquist_flag);

    //3.3.9 Long Term Postfilter  (d09r02_F2F)
    longTermPostfilter.run(x_s, mdctEnc.near_nyquist_flag);


    //3.3.10 Spectral quantization  (d09r02_F2F)
    //3.3.10.1 Bit budget  (d09r02_F2F)
    uint16_t nbits_ari = ceil(log2(lc3Config.NE/2));
    if ( nbits <= 1280 )
    {
        nbits_ari += 3;
    }
    else if ( nbits <= 2560 )
    {
        nbits_ari += 4;
    }
    else
    {
        nbits_ari += 5;
    }

    const uint8_t nbits_gain = 8;
    const uint8_t nbits_nf = 3;
    uint16_t nbits_spec = nbits - (
        bandwidthDetector.nbits_bw +
        temporalNoiseShaping.nbits_TNS +
        longTermPostfilter.nbits_LTPF +
        spectralNoiseShaping.nbits_SNS +
        nbits_gain +
        nbits_nf +
        nbits_ari
        );
    spectralQuantization.run(temporalNoiseShaping.X_f, nbits, nbits_spec);

    // 3.3.11 Residual coding  (d09r02_F2F)
    // nbits_residual_max = 𝑛𝑏𝑖𝑡𝑠𝑠𝑝𝑒𝑐 - 𝑛𝑏𝑖𝑡𝑠𝑡𝑟𝑢𝑛𝑐 + 4;
    int16_t nbits_residual_max = nbits_spec
                      - spectralQuantization.nbits_trunc + 4;
    if (nbits_residual_max < 0)
    {
        nbits_residual_max = 0;
    }
    uint16_t nbits_residual = 0;
    uint8_t res_bits[nbits_residual_max]; // TODO check whether the degenerated case with nbits_residual_max==0 works
    if (nbits_residual_max > 0)
    {// converted pseudo-code from page 54  (d09r02_F2F)
        uint16_t k = 0;
        while ( (k < spectralQuantization.NE) && (nbits_residual < nbits_residual_max) )
        {
            //if (𝑋𝑞[k]!= 0)
            if (spectralQuantization.X_q[k]!= 0)
            {
                //if (𝑋𝑓[k] >= 𝑋𝑞[k]*gg)
                if (temporalNoiseShaping.X_f[k] >=
                    spectralQuantization.X_q[k]*spectralQuantization.gg)
                {
                    res_bits[nbits_residual] = 1;
                }
                else
                {
                    res_bits[nbits_residual] = 0;
                }
                nbits_residual++;
            }
            k++;
        }
    }
    if (nullptr!=datapoints)
    {
        datapoints->log("res_bits", &res_bits, sizeof(uint8_t)*nbits_residual_max);
    }

    // 3.3.12 Noise level estimation  (d09r02_F2F)
    noiseLevelEstimation.run(
        temporalNoiseShaping.X_f,
        spectralQuantization.X_q,
        bandwidthDetector.P_bw,
        spectralQuantization.gg);


    // 3.3.13 Bitstream encoding  (d09r02_F2F)
    // 3.3.13.2 Initialization  (d09r02_F2F)
    bitstreamEncoding.init(bytes);
    // 3.3.13.3 Side information  (d09r02_F2F)
    /* Bandwidth */
    bitstreamEncoding.bandwidth(bandwidthDetector.P_bw, bandwidthDetector.nbits_bw);
    /* Last non-zero tuple */
    bitstreamEncoding.lastNonzeroTuple(spectralQuantization.lastnz_trunc);
    /* LSB mode bit */
    bitstreamEncoding.lsbModeBit(spectralQuantization.lsbMode);
    /* Global Gain */
    bitstreamEncoding.globalGain(spectralQuantization.gg_ind);
    /* TNS activation flag */
    bitstreamEncoding.tnsActivationFlag(
        temporalNoiseShaping.num_tns_filters,
        temporalNoiseShaping.rc_order
        );
    /* Pitch present flag */
    bitstreamEncoding.pitchPresentFlag(longTermPostfilter.pitch_present);
    /* Encode SCF VQ parameters - 1st stage (10 bits) */
    bitstreamEncoding.encodeScfVq1stStage(
        spectralNoiseShaping.get_ind_LF(),
        spectralNoiseShaping.get_ind_HF()
        );
    /* Encode SCF VQ parameters - 2nd stage side-info (3-4 bits) */
    /* Encode SCF VQ parameters - 2nd stage MPVQ data */
    bitstreamEncoding.encodeScfVq2ndStage(
        spectralNoiseShaping.get_shape_j(),
        spectralNoiseShaping.get_Gind(),
        spectralNoiseShaping.get_LS_indA(),
        spectralNoiseShaping.get_index_joint_j()
        );
    /* LTPF data */
    if (longTermPostfilter.pitch_present != 0)
    {
        bitstreamEncoding.ltpfData(
            longTermPostfilter.ltpf_active,
            longTermPostfilter.pitch_index
            );
    }
    /* Noise Factor */
    bitstreamEncoding.noiseFactor(noiseLevelEstimation.F_NF);

    if (nullptr!=datapoints)
    {
        datapoints->log("bytes_side_info", &bytes[0], sizeof(uint8_t)*nbytes);
    }

    // 3.3.13.4 Arithmetic encoding  (d09r02_F2F)
    /* Arithmetic Encoder Initialization */
    bitstreamEncoding.ac_enc_init();
    /* TNS data */
    bitstreamEncoding.tnsData(
        temporalNoiseShaping.tns_lpc_weighting,
        temporalNoiseShaping.num_tns_filters,
        temporalNoiseShaping.rc_order,
        temporalNoiseShaping.rc_i
        );
    /* Spectral data */
    uint8_t lsbs[spectralQuantization.nbits_lsb]; // TODO check whether the degenerated case with nlsbs==0 works
    bitstreamEncoding.spectralData(
        spectralQuantization.lastnz_trunc,
        spectralQuantization.rateFlag,
        spectralQuantization.lsbMode,
        spectralQuantization.X_q,
        spectralQuantization.nbits_lsb,
        lsbs
        );

    // 3.3.13.5 Residual data and finalization  (d09r02_F2F)
    bitstreamEncoding.residualDataAndFinalization(
        spectralQuantization.lsbMode,
        nbits_residual,
        res_bits
        );


    if (nullptr!=datapoints)
    {
        datapoints->log("nbits_spec", &nbits_spec, sizeof(nbits_spec) );
        datapoints->log("bytes_ari", &bytes[0], sizeof(uint8_t)*nbytes);
    }

}

void EncoderFrame::registerDatapoints(DatapointContainer* datapoints_)
{
    datapoints = datapoints_;
    if (nullptr != datapoints)
    {
        bandwidthDetector.registerDatapoints(datapoints);
        longTermPostfilter.registerDatapoints(datapoints);
        bitstreamEncoding.registerDatapoints(datapoints);
    }
}

}//namespace Lc3Enc
