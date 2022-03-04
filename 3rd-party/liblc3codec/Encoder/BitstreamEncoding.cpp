/*
 * BitstreamEncoding.cpp
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

#include "BitstreamEncoding.hpp"
#include "SnsQuantizationTables.hpp"
#include "TemporalNoiseShapingTables.hpp"
#include "SpectralDataTables.hpp"
#include <cmath>
#include <algorithm>

namespace Lc3Enc
{

BitstreamEncoding::BitstreamEncoding(uint16_t NE_, uint16_t nbytes_) :
    NE(NE_),
    nbytes(nbytes_),
    nbits(nbytes_ * 8),
    bytes(nullptr),
    bp(0),
    bp_side(0),
    mask_side(0),
    nbits_side_initial(0),
    nlsbs(0),
    lsbs(nullptr)
{
}

BitstreamEncoding::~BitstreamEncoding()
{
}

void BitstreamEncoding::write_bit_backward(uint8_t bytes[], uint16_t* bp, uint8_t* mask, uint8_t bit)
{
    if (bit == 0)
    {
        bytes[*bp] &= ~*mask;
    }
    else
    {
        bytes[*bp] |= *mask;
    }
    if (*mask == 0x80)
    {
        *mask = 1;
        *bp -= 1;
    }
    else
    {
        *mask <<= 1;
    }
}

void BitstreamEncoding::write_uint_backward(uint8_t bytes[], uint16_t* bp, uint8_t* mask, uint16_t val, uint8_t numbits)
{
    for (uint8_t k = 0; k < numbits; k++)
    {
        uint8_t bit = val & 1;
        write_bit_backward(bytes, bp, mask, bit);
        val >>= 1;
    }
}

void BitstreamEncoding::write_uint_forward(uint8_t bytes[], uint16_t bp, uint16_t val, uint8_t numbits)
{
    uint8_t mask = 0x80;
    for (uint8_t k = 0; k < numbits; k++)
    {
        uint8_t bit = val & mask;
        if (bit == 0)
        {
            bytes[bp] &= ~mask;
        }
        else
        {
            bytes[bp] |= mask;
        }
        mask >>= 1;
    }
}

void BitstreamEncoding::init(uint8_t* bytes_)
{
    // store pointer to current output buffer
    bytes = bytes_;
    // added initialization of bytes
    // This might be needed for test purpose only, but is added to be on the safe side.
    for (uint16_t k=0; k < nbytes; k++)
    {
        bytes[k] = 0;
    }

    // 3.3.13 Bitstream encoding  (d09r02_F2F)
    // 3.3.13.2 Initialization  (d09r02_F2F)
    bp = 0;
    bp_side = nbytes - 1;
    mask_side = 1;
    nlsbs = 0;
}

void BitstreamEncoding::bandwidth(uint8_t P_bw, uint8_t nbits_bw)
{
    /* Bandwidth */
    //if (𝑛𝑏𝑖𝑡𝑠𝑏𝑤 > 0)
    if (nbits_bw > 0)
    {
        //write_uint_backward(bytes, &bp_side, &mask_side, 𝑃𝑏𝑤, 𝑛𝑏𝑖𝑡𝑠𝑏𝑤);
        write_uint_backward(
            bytes, &bp_side, &mask_side,
            P_bw,
            nbits_bw
            );
    }
}

void BitstreamEncoding::lastNonzeroTuple(uint16_t lastnz_trunc)
{
    /* Last non-zero tuple */
    write_uint_backward(
        bytes, &bp_side, &mask_side,
        (lastnz_trunc >> 1) - 1,
        //ceil(log2(𝑁𝐸/2))
        ceil(log2(NE/2))
        );
}

void BitstreamEncoding::lsbModeBit(uint8_t lsbMode)
{
    /* LSB mode bit */
    write_bit_backward(
        bytes, &bp_side, &mask_side,
        lsbMode
        );
}

void BitstreamEncoding::globalGain(uint8_t gg_ind)
{
    /* Global Gain */
    write_uint_backward(
        bytes, &bp_side, &mask_side,
        gg_ind,
        8
        );
}

void BitstreamEncoding::tnsActivationFlag(uint8_t num_tns_filters, uint8_t rc_order[])
{
    /* TNS activation flag */
    for (uint8_t f=0; f < num_tns_filters; f++)
    {
        write_bit_backward(
            bytes, &bp_side, &mask_side,
            std::min(rc_order[f], static_cast<uint8_t>(1))
            );
    }
}

void BitstreamEncoding::pitchPresentFlag(uint8_t pitch_present)
{
    /* Pitch present flag */
    write_bit_backward(
        bytes, &bp_side, &mask_side,
        pitch_present
        );
}

void BitstreamEncoding::encodeScfVq1stStage(uint8_t ind_LF, uint8_t ind_HF)
{
    /* Encode SCF VQ parameters - 1st stage (10 bits) */
    write_uint_backward(
        bytes, &bp_side, &mask_side,
        ind_LF,
        5
        );
    write_uint_backward(
        bytes, &bp_side, &mask_side,
        ind_HF,
        5
        );
}

void BitstreamEncoding::encodeScfVq2ndStage(
        uint8_t shape_j,
        uint8_t gain_i,
        uint8_t LS_indA, // the type of this is strange over all code places -> finally only one bit is stored!
        uint32_t index_joint_j)
{
    /* Encode SCF VQ parameters - 2nd stage side-info (3-4 bits) */
    write_bit_backward(
        bytes, &bp_side, &mask_side,
        shape_j>>1
        );
    //uint8_t submode_LSB = (shape_j & 0x1); /* shape_j is the stage2 shape_index [0…3] */
    uint8_t submode_MSB = (shape_j>>1);
    uint8_t gain_MSBs = gain_i; /* where gain_i is the SNS-VQ stage 2 gain_index */
    gain_MSBs = (gain_MSBs >> sns_gainLSBbits[shape_j]);
    write_uint_backward(bytes,&bp_side,&mask_side,
        gain_MSBs,
        sns_gainMSBbits[shape_j]
        );
    write_bit_backward(bytes, &bp_side, &mask_side, LS_indA);

    /* Encode SCF VQ parameters - 2nd stage MPVQ data */
    if (submode_MSB == 0)
    {
        /*
        if (submode_LSB == 0)
        {
            tmp = index_joint_0; // Eq. 52
        } else {
            tmp = index_joint_1; // Eq. 53
        }
        write_uint_backward(bytes, &bp_side, &mask_side, tmp, 13)
        write_uint_backward(bytes, &bp_side, &mask_side, tmp>>13, 12);
        */
        write_uint_backward(bytes, &bp_side, &mask_side, index_joint_j, 13);
        write_uint_backward(bytes, &bp_side, &mask_side, index_joint_j>>13, 12);
    }
    else
    {
        /*
        if (submode_LSB == 0)
        {
            tmp = index_joint_2; // Eq. 54
        } else {
            tmp = index_joint_3; // Eq. 55
        }
        write_uint_backward(bytes, &bp_side, &mask_side, tmp, 12);
        write_uint_backward(bytes, &bp_side, &mask_side, tmp>> 12, 12);
        */
        write_uint_backward(bytes, &bp_side, &mask_side, index_joint_j, 12);
        write_uint_backward(bytes, &bp_side, &mask_side, index_joint_j>> 12, 12);
    }
}

void BitstreamEncoding::ltpfData(uint8_t ltpf_active, uint16_t pitch_index)
{
    /* LTPF data */
    write_uint_backward(
        bytes, &bp_side, &mask_side,
        ltpf_active,
        1
        );
    write_uint_backward(
        bytes, &bp_side, &mask_side,
        pitch_index,
        9
        );
}


void BitstreamEncoding::noiseFactor(uint8_t F_NF)
{
    /* Noise Factor */
    write_uint_backward(
        bytes, &bp_side, &mask_side,
        F_NF,
        3
        );
}

void BitstreamEncoding::ac_enc_init()
{
    st.low = 0;
    st.range = 0x00ffffff;
    st.cache = -1;
    st.carry = 0;
    st.carry_count = 0;
}

void BitstreamEncoding::ac_shift(uint8_t bytes[], uint16_t* bp, struct ac_enc_state* st)
{
    if (st->low < 0x00ff0000 || st->carry == 1)
    {
        if (st->cache >= 0)
        {
            bytes[(*bp)++] = (st->cache + st->carry) & 0xff;
        }
        while (st->carry_count > 0)
        {
            bytes[(*bp)++] = (st->carry + 0xff) & 0xff;
            st->carry_count -= 1;
        }
        st->cache = st->low >> 16;
        st->carry = 0;
    }
    else
    {
        st->carry_count += 1;
    }
    st->low <<= 8;
    st->low &= 0x00ffffff;
}

void BitstreamEncoding::ac_encode(uint8_t bytes[], uint16_t* bp, struct ac_enc_state* st, short cum_freq, short sym_freq)
{
    uint32_t r = st->range >> 10;
    st->low += r * cum_freq;
    if (st->low >> 24)
    {
        st->carry = 1;
    }
    st->low &= 0x00ffffff;
    st->range = r * sym_freq;
    while (st->range < 0x10000)
    {
        st->range <<= 8;
        ac_shift(bytes, bp, st);
    }
}

void BitstreamEncoding::ac_enc_finish(uint8_t bytes[], uint16_t* bp, struct ac_enc_state* st)
{
    int8_t bits = 1;
    while ((st->range >> (24-bits)) == 0)
    {
        bits++;
    }
    uint32_t mask = 0x00ffffff >> bits;
    uint32_t val = st->low + mask;
    uint8_t over1 = val >> 24;
    val &= 0x00ffffff;
    uint32_t high = st->low + st->range;
    uint8_t over2 = high >> 24;
    high &= 0x00ffffff;
    val = val & ~mask;
    if (over1 == over2)
    {
        if (val + mask >= high)
        {
            bits += 1;
            mask >>= 1;
            val = ((st->low + mask) & 0x00ffffff) & ~mask;
        }
        if (val < st->low)
        {
            st->carry = 1;
        }
    }
    st->low = val;
    for (; bits > 0; bits -= 8)
    {
        ac_shift(bytes, bp, st);
    }
    bits += 8;
    if (st->carry_count > 0)
    {
        bytes[(*bp)++] = st->cache;
        for (; st->carry_count > 1; st->carry_count--)
        {
            bytes[(*bp)++] = 0xff;
        }
        write_uint_forward(bytes, *bp, 0xff>>(8-bits), bits);
    }
    else
    {
        write_uint_forward(bytes, *bp, st->cache, bits);
    }
}

void BitstreamEncoding::tnsData(
            uint8_t tns_lpc_weighting,
            uint8_t num_tns_filters,
            uint8_t rc_order[],
            uint8_t rc_i[]
            )
{
    /* TNS data */
    for (uint8_t f = 0; f < num_tns_filters; f++)
    {
        //if (𝑟𝑐𝑜𝑟𝑑𝑒𝑟 (𝑓) > 0)
        if (rc_order[f] > 0)
        {
            ac_encode(
                bytes, &bp, &st,
                //ac_tns_order_cumfreq[tns_lpc_weighting][ 𝑟𝑐𝑜𝑟𝑑𝑒𝑟 (𝑓)-1],
                ac_tns_order_cumfreq[tns_lpc_weighting][ rc_order[f]-1],
                //ac_tns_order_freq[tns_lpc_weighting][ 𝑟𝑐𝑜𝑟𝑑𝑒𝑟 (𝑓)-1]
                ac_tns_order_freq[tns_lpc_weighting][ rc_order[f]-1]
                );
            //for (k = 0; k < 𝑟𝑐𝑜𝑟𝑑𝑒𝑟 (𝑓); k++)
            for (uint8_t k = 0; k < rc_order[f]; k++)
            {
                ac_encode(
                    bytes, &bp, &st,
                    //ac_tns_coef_cumfreq[k][𝑟𝑐𝑖 (𝑘, 𝑓)],
                    ac_tns_coef_cumfreq[k][rc_i[k+8*f]],
                    //ac_tns_coef_freq[k][𝑟𝑐𝑖 (𝑘, 𝑓)]
                    ac_tns_coef_freq[k][rc_i[k+8*f]]
                    );
            }
        }
    }
}

uint16_t BitstreamEncoding::nbits_side_written()
{
    return nbits - (8 * bp_side + 8 - log2(mask_side));
}

void BitstreamEncoding::spectralData(
            uint16_t lastnz_trunc,
            uint16_t rateFlag,
            uint8_t lsbMode,
            int16_t* X_q,
            uint16_t nbits_lsb,
            uint8_t *lsbs_
            )
{
    nbits_side_initial = nbits_side_written();
    lsbs = lsbs_; // store this pointer for subsequent use in residualDataAndFinalization
    /* Spectral data */
    uint16_t c = 0;
    for (uint16_t k = 0; k < lastnz_trunc; k += 2)
    {
        uint16_t t = c + rateFlag;
        //if (k > 𝑁𝐸/2)
        if (k > NE/2)
        {
            t += 256;
        }
        //a = abs(𝑋𝑞[k]);
        uint16_t a = abs(X_q[k]);
        uint16_t a_lsb = a;
        //b = abs(𝑋𝑞[k+1]);
        uint16_t b = abs(X_q[k+1]);
        uint16_t b_lsb = b;
        uint8_t lev = 0;
        uint8_t lsb0 =0;
        uint8_t lsb1 =0;
        //while (max(a,b) >= 4)
        uint8_t pki;
        while (std::max(a,b) >= 4)
        {
            //pki = ac_spec_lookup[t+min(lev,3)*1024];
            pki = ac_spec_lookup[t+std::min(lev,static_cast<uint8_t>(3))*1024];
            ac_encode(
                bytes, &bp, &st,
                ac_spec_cumfreq[pki][16],
                ac_spec_freq[pki][16]
                );
            if ( (lsbMode == 1) && (lev == 0) )
            {
                lsb0 = a & 1;
                lsb1 = b & 1;
            }
            else
            {
                write_bit_backward(bytes, &bp_side, &mask_side, a & 1);
                write_bit_backward(bytes, &bp_side, &mask_side, b & 1);
            }
            a >>= 1;
            b >>= 1;
            lev++;
        }
        pki = ac_spec_lookup[t+std::min(lev,static_cast<uint8_t>(3))*1024];
        uint16_t sym = a + 4*b;
        ac_encode(
            bytes, &bp, &st,
            ac_spec_cumfreq[pki][sym],
            ac_spec_freq[pki][sym]
            );
        //a_lsb = abs(𝑋𝑞[k]); -> implemented earlier
        //b_lsb = abs(𝑋𝑞[k+1]); -> implemented earlier
        if ( (lsbMode == 1) && (lev > 0) )
        {
            a_lsb >>= 1;
            b_lsb >>= 1;
            lsbs[nlsbs++] = lsb0;
            //if (a_lsb == 0 && 𝑋𝑞[k] != 0)
            if (a_lsb == 0 && X_q[k] != 0)
            {
                //lsbs[nlsbs++] = 𝑋𝑞[k]>0?0:1;
                lsbs[nlsbs++] = X_q[k]>0?0:1;
            }
            lsbs[nlsbs++] = lsb1;
            //if (b_lsb == 0 && 𝑋𝑞[k+1] != 0)
            if (b_lsb == 0 && X_q[k+1] != 0)
            {
                //lsbs[nlsbs++] = 𝑋𝑞[k+1]>0?0:1;
                lsbs[nlsbs++] = X_q[k+1]>0?0:1;
            }
        }
        if (a_lsb > 0)
        {
            //write_bit_backward(bytes, &bp_side, &mask_side, 𝑋𝑞[k]>0?0:1);
            write_bit_backward(bytes, &bp_side, &mask_side, X_q[k]>0?0:1);
        }
        if (b_lsb > 0)
        {
            //write_bit_backward(bytes, &bp_side, &mask_side, 𝑋𝑞[k+1]>0?0:1);
            write_bit_backward(bytes, &bp_side, &mask_side, X_q[k+1]>0?0:1);
        }
        //lev = min(lev,3);
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
    }
}

// nbits_ari forecast under the assumption that only finalization will be added
uint16_t BitstreamEncoding::nbits_ari_forecast()
{
    uint16_t nbits_ari = bp * 8;
    nbits_ari += 25 - floor(log2(st.range));
    if (st.cache >= 0)
    {
        nbits_ari += 8;
    }
    if (st.carry_count > 0)
    {
        nbits_ari += st.carry_count * 8;
    }
    return nbits_ari;
}

void BitstreamEncoding::residualDataAndFinalization(uint8_t lsbMode, uint16_t nbits_residual, uint8_t* res_bits)
{
    /* Residual bits */
    //uint16_t nbits_side = nbits - (8 * bp_side + 8 - log2(mask_side));
    uint16_t nbits_side = nbits_side_written();
    uint16_t nbits_ari = nbits_ari_forecast();
    uint16_t nbits_residual_enc = nbits - (nbits_side + nbits_ari);
    if (*reinterpret_cast<int16_t*>(&nbits_residual_enc) < 0)
    {
        nbits_residual_enc = 0;
    }
    if (lsbMode == 0)
    {
        nbits_residual_enc = std::min(nbits_residual_enc, nbits_residual);
        for (uint16_t k = 0; k < nbits_residual_enc; k++)
        {
            write_bit_backward(bytes, &bp_side, &mask_side, res_bits[k]);
        }
    }
    else
    {
        nbits_residual_enc = std::min(nbits_residual_enc, nlsbs);
        for (uint16_t k = 0; k < nbits_residual_enc; k++)
        {
            write_bit_backward(bytes, &bp_side, &mask_side, lsbs[k]);
        }
    }

    /* Arithmetic Encoder Finalization */
    ac_enc_finish(bytes, &bp, &st);
}

void BitstreamEncoding::registerDatapoints(DatapointContainer* datapoints)
{
    if (nullptr != datapoints)
    {
        datapoints->addDatapoint( "nbytes", &nbytes, sizeof(nbytes) );
    }
}

}//namespace Lc3Enc
