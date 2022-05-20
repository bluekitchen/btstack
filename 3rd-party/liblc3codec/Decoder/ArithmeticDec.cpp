/*
 * ArithmeticDec.cpp
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

#include "ArithmeticDec.hpp"
#include "TemporalNoiseShapingTables.hpp"
#include "SpectralDataTables.hpp"

#include "BitReader.hpp"

#include <cstring>
#include <cmath>

#include <algorithm>    // std::min

namespace Lc3Dec
{

ArithmeticDec::ArithmeticDec(uint16_t NF_, uint16_t NE_, uint16_t rateFlag_, uint8_t tns_lpc_weighting_)
    :
    NF(NF_),
    NE(NE_),
    rateFlag(rateFlag_),
    tns_lpc_weighting(tns_lpc_weighting_),
    X_hat_q_ari(nullptr),
    save_lev(nullptr),
    nf_seed(0),
    nbits_residual(0)
{
    X_hat_q_ari = new int16_t[NE];
    save_lev = new uint8_t[NE];
}

ArithmeticDec::~ArithmeticDec()
{
    delete[] X_hat_q_ari;
    delete[] save_lev;
}

void ac_dec_init(const uint8_t bytes[], uint16_t* bp, struct ac_dec_state* st)
{
    st->low = 0;
    st->range = 0x00ffffff;
    for (uint8_t i = 0; i < 3; i++)
    {
        st->low <<= 8;
        st->low += bytes[(*bp)++];
    }
}

uint8_t ac_decode(
    const uint8_t bytes[],
    uint16_t* bp,
    struct ac_dec_state* st,
    int16_t cum_freq[],
    int16_t sym_freq[],
    uint8_t numsym,
    uint8_t& BEC_detect
    )
{
    uint32_t tmp = st->range >> 10;
    if (st->low >= (tmp<<10))
    {
        BEC_detect = 1;
        return 0;
    }
    uint8_t val = numsym-1;
    while (st->low < tmp * cum_freq[val])
    {
        val--;
    }
    st->low -= tmp * cum_freq[val];
    st->range = tmp * sym_freq[val];
    while (st->range < 0x10000)
    {
        st->low <<= 8;
        st->low &= 0x00ffffff;
        st->low += bytes[(*bp)++];
        st->range <<= 8;
    }
    return val;
}

double ArithmeticDec::rc_q(uint8_t k, uint8_t f)
{
    // with Δ =π/17
    const double pi = std::acos(-1);
    double quantizer_stepsize = pi / 17.0;
    return sin(  quantizer_stepsize * ( rc_i[k + 8*f] -8 ) );
}

void ArithmeticDec::run(
                const uint8_t* bytes,
                uint16_t& bp,
                uint16_t& bp_side,
                uint8_t& mask_side,
                int16_t& num_tns_filters,
                int16_t rc_order[],
                const uint8_t& lsbMode,
                const int16_t& lastnz,
                uint16_t nbits,
                uint8_t& BEC_detect
            )
{
    int16_t c = 0;

    // make local copy of rc_order (is this really what we want)
    rc_order_ari[0] = rc_order[0];
    rc_order_ari[1] = rc_order[1];

    /* Arithmetic Decoder Initialization */
    ac_dec_init(bytes, &bp, &st);

    /* TNS data */
    // Note: some initialization code like that below can be found in d09r02,
    //       but there has been none in d09r01. However, the complete initialization
    //       has been added here, in order to get a proper match to the reference output data
    for (uint8_t f = 0; f < 2; f++)
    {
        for (uint8_t k = 0; k < 8; k++)
        {
            rc_i[k + 8*f] = 8;
        }
    }
    for (uint8_t f = 0; f < num_tns_filters; f++)
    {
        //if (𝑟𝑐𝑜𝑟𝑑𝑒𝑟(𝑓) > 0)
        if (rc_order[f] > 0)
        {
            //𝑟𝑐𝑜𝑟𝑑𝑒𝑟(𝑓) = ac_decode(bytes, &bp, &st,
            rc_order_ari[f] = ac_decode(bytes, &bp, &st,
                                    ac_tns_order_cumfreq[tns_lpc_weighting],
                                    ac_tns_order_freq[tns_lpc_weighting], 8,
                                    BEC_detect);
            if (BEC_detect)
            {
                // early exit to avoid unpredictable side-effects
                return;
            }

            rc_order_ari[f] = rc_order_ari[f] + 1;
            // specification (d09r02_F2F) proposes initialization
            // of rc_i at this place; here implemented above in order
            // to be performed independet from num_tns_filters
            for (uint8_t k = 0; k < rc_order_ari[f]; k++)
            {
                //𝑟𝑐𝑖(𝑘,𝑓) = ac_decode(bytes, &bp, &st, ac_tns_coef_cumfreq[k],
                //rc_i[k][f] = ac_decode(bytes, &bp, &st, ac_tns_coef_cumfreq[k],
                rc_i[k + 8*f] = ac_decode(bytes, &bp, &st, ac_tns_coef_cumfreq[k],
                                        ac_tns_coef_freq[k], 17,
                                        BEC_detect);
                if (BEC_detect)
                {
                    // early exit to avoid unpredictable side-effects
                    return;
                }
            }
        }
    }

    /* Spectral data */
    for (uint16_t k = 0; k < lastnz; k += 2)
    {
        uint16_t t = c + rateFlag;
        //if (k > 𝑁𝐸/2)
        if (k > NE/2)
        {
            t += 256;
        }
        //𝑋𝑞̂[k] = 𝑋𝑞̂[k+1] = 0;
        X_hat_q_ari[k] = X_hat_q_ari[k+1] = 0;
        uint8_t lev;
        uint8_t sym;
        for (lev = 0; lev < 14; lev++)
        {
            uint8_t pki = ac_spec_lookup[t+std::min(lev,static_cast<uint8_t>(3U))*1024];
            sym = ac_decode(bytes, &bp, &st, ac_spec_cumfreq[pki],
                            ac_spec_freq[pki], 17,
                            BEC_detect);
            if (BEC_detect)
            {
                // early exit to avoid unpredictable side-effects
                return;
            }
            if (sym < 16)
            {
                break;
            }
            if (lsbMode == 0 || lev > 0)
            {
                uint8_t bit = read_bit(bytes, &bp_side, &mask_side);
                //𝑋𝑞̂[k] += bit << lev;
                X_hat_q_ari[k] += bit << lev;
                bit = read_bit(bytes, &bp_side, &mask_side);
                //𝑋𝑞̂[k+1] += bit << lev;
                X_hat_q_ari[k+1] += bit << lev;
            }
        }
        if (lev == 14)
        {
            BEC_detect = 1;
            return;
        }
        if (lsbMode == 1)
        {
            save_lev[k] = lev;
        }
        uint8_t a = sym & 0x3;
        uint8_t b = sym >> 2;
        //𝑋𝑞̂[k] += a << lev;
        //𝑋𝑞̂[k+1] += b << lev;
        //if (𝑋𝑞̂[k] > 0)
        X_hat_q_ari[k] += a << lev;
        X_hat_q_ari[k+1] += b << lev;
        if (X_hat_q_ari[k] > 0)
        {
            uint8_t bit = read_bit(bytes, &bp_side, &mask_side);
            if (bit == 1)
            {
                //𝑋𝑞̂[k] = -𝑋𝑞̂[k];
                X_hat_q_ari[k] = -X_hat_q_ari[k];
            }
        }
        //if (𝑋𝑞̂[k+1] > 0)
        if (X_hat_q_ari[k+1] > 0)
        {
            uint8_t bit = read_bit(bytes, &bp_side, &mask_side);
            if (bit == 1)
            {
                //𝑋𝑞̂[k+1] = -𝑋𝑞̂[k+1];
                X_hat_q_ari[k+1] = -X_hat_q_ari[k+1];
            }
        }
        lev = std::min(lev,static_cast<uint8_t>(3));
        if (lev <= 1)
        {
            t = 1 + (a+b)*(lev+1);
        }
        else
        {
            t = 12 + lev;
        }
        c = (c&15)*16 + t;
        // Note: specification of the following line hase been changed from d09r01 to d09r02_F2F
        if (bp - bp_side > 3)
        {
            BEC_detect = 1;
            return;
        }
    }
    // reset remaining fields in array X_hat_q_ari to simplify testing
    for (int16_t k = lastnz; k < NE; k++)
    {
        X_hat_q_ari [k] = 0;
    }


    // 3.4.2.6 Residual data and finalization (d09r02_F2F)
    /* Number of residual bits */
    int16_t nbits_side = nbits - (8 * bp_side + 8 - log2(mask_side));
    int16_t nbits_ari = (bp - 3) * 8;
    nbits_ari += 25 - floor(log2(st.range));
    int16_t nbits_residual_tmp = nbits - (nbits_side + nbits_ari);
    if (nbits_residual_tmp < 0)
    {
        BEC_detect = 1;
        return;
    }
    nbits_residual = nbits_residual_tmp;

}

void ArithmeticDec::registerDatapoints(DatapointContainer* datapoints)
{
    if (nullptr != datapoints)
    {
        datapoints->addDatapoint( "rateFlag", &rateFlag, sizeof(rateFlag) );
        datapoints->addDatapoint( "tns_lpc_weighting", &tns_lpc_weighting, sizeof(tns_lpc_weighting) );
        datapoints->addDatapoint( "rc_order_ari", &rc_order_ari[0], sizeof(rc_order_ari) );
        datapoints->addDatapoint( "rc_i", &rc_i[0], sizeof(rc_i) );
        datapoints->addDatapoint( "X_hat_q_ari", &X_hat_q_ari[0], sizeof(int16_t)*NE );
        datapoints->addDatapoint( "nbits_residual", &nbits_residual, sizeof(nbits_residual) );
    }
}

}//namespace Lc3Dec
