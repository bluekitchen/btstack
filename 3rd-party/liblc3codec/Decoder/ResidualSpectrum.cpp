/*
 * ResidualSpectrum.cpp
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

#include "ResidualSpectrum.hpp"
#include "BitReader.hpp"

#include <cmath>

namespace Lc3Dec
{

ResidualSpectrum::ResidualSpectrum(uint16_t NE_)
    :
    NE(NE_),
    X_hat_q_residual(nullptr),
    nResBits(0)
{
    X_hat_q_residual = new double[NE];
}

ResidualSpectrum::~ResidualSpectrum()
{
    delete[] X_hat_q_residual;
}

void ResidualSpectrum::run(
            const uint8_t* bytes,
            uint16_t& bp_side,
            uint8_t& mask_side,
            const uint16_t lastnz,
            const int16_t* const X_hat_q_ari,
            uint16_t const nbits_residual, // the const is implementation dependent and thus not repeated in header declaration
            uint8_t* save_lev,
            const uint8_t& lsbMode,
            uint16_t& nf_seed,
            uint16_t& zeroFrame,
            const int16_t gg_ind,
            int16_t F_NF
        )
{
    // 3.4.2.6 Residual data and finalization (d09r02_F2F)
    /* Decode residual bits */
    for (uint16_t k = 0; k < lastnz; k++)
    {
        X_hat_q_residual [k] = X_hat_q_ari[k];
    }
    //for (k = lastnz; k < 𝑁𝐸; k++)
    for (uint16_t k = lastnz; k < NE; k++)
    {
        //𝑋𝑞 ̂[k] = 0;
        X_hat_q_residual [k] = 0;
    }
    uint8_t resBits[nbits_residual];
    uint16_t remaining_nbits_residual = nbits_residual; // changed relative to specification to ensure const input into array allocation
    nResBits = 0;
    if (lsbMode == 0)
    {
        //for (k = 0; k < 𝑁𝐸; k++)
        for (uint16_t k = 0; k < NE; k++)
        {
            //if (𝑋𝑞 ̂[k] != 0)
            if (X_hat_q_residual[k] != 0)
            {
                if (nResBits == remaining_nbits_residual)
                {
                    break;
                }
                resBits[nResBits++] = read_bit(bytes, &bp_side, &mask_side);
            }
        }
    }
    else
    {
        for (uint16_t k = 0; k < lastnz; k+=2)
        {
            if (save_lev[k] > 0)
            {
                if (remaining_nbits_residual == 0)
                {
                    break;
                }
                uint8_t bit = read_bit(bytes, &bp_side, &mask_side);
                remaining_nbits_residual--;
                if (bit == 1)
                {
                    //if (𝑋𝑞 ̂[k] > 0)
                    if (X_hat_q_residual[k] > 0)
                    {
                        //𝑋𝑞 ̂[k] += 1;
                        X_hat_q_residual[k] += 1;
                    }
                    //else if (𝑋𝑞 ̂[k] < 0)
                    else if (X_hat_q_residual[k] < 0)
                    {
                        //𝑋𝑞 ̂[k] -= 1;
                        X_hat_q_residual[k] -= 1;
                    }
                    else
                    {
                        if (remaining_nbits_residual == 0)
                        {
                            break;
                        }
                        bit = read_bit(bytes, &bp_side, &mask_side);
                        remaining_nbits_residual--;
                        if (bit == 0)
                        {
                            //𝑋𝑞 ̂[k] = 1;
                            X_hat_q_residual[k] = 1;
                        }
                        else
                        {
                            //𝑋𝑞 ̂[k] = -1;
                            X_hat_q_residual[k] = -1;
                        }
                    }
                }
                if (remaining_nbits_residual == 0)
                {
                    break;
                }
                bit = read_bit(bytes, &bp_side, &mask_side);
                remaining_nbits_residual--;
                if (bit == 1)
                {
                    //if (𝑋𝑞 ̂[k+1] > 0)
                    if (X_hat_q_residual[k+1] > 0)
                    {
                        //𝑋𝑞 ̂[k+1] += 1;
                        X_hat_q_residual[k+1] += 1;
                    }
                    //else if (𝑋𝑞 ̂[k+1] < 0)
                    else if (X_hat_q_residual[k+1] < 0)
                    {
                        //𝑋𝑞 ̂[k+1] -= 1;
                        X_hat_q_residual[k+1] -= 1;
                    }
                    else
                    {
                        if (remaining_nbits_residual == 0)
                        {
                            break;
                        }
                        bit = read_bit(bytes, &bp_side, &mask_side);
                        remaining_nbits_residual--;
                        if (bit == 0)
                        {
                            //𝑋𝑞 ̂[k+1] = 1;
                            X_hat_q_residual[k+1] = 1;
                        }
                        else
                        {
                            //𝑋𝑞 ̂[k+1] = -1;
                            X_hat_q_residual[k+1] = -1;
                        }
                    }
                }
            }
        }
    }

    /* Noise Filling Seed */
    int16_t tmp = 0;
    //for (k = 0; k < 𝑁𝐸; k++)
    for (uint16_t k = 0; k < NE; k++)
    {
        //tmp += abs(𝑋𝑞 ̂[k]) * k;
        tmp += abs(X_hat_q_residual[k]) * k;
    }
    nf_seed = tmp & 0xFFFF; /* Note that both tmp and nf_seed are 32-bit int*/
    /* Zero frame flag */
    //if (lastnz == 2 && 𝑋𝑞 ̂[0] == 0 && 𝑋𝑞 ̂[1] == 0 && 𝑔𝑔𝑖𝑛𝑑 == 0 && 𝐹𝑁𝐹 == 7)
    if (
        (lastnz == 2) && (X_hat_q_residual[0] == 0.0) &&
        (X_hat_q_residual[1] == 0.0) &&
        (gg_ind == 0) &&
        (F_NF == 7)
       )
    {
        zeroFrame = 1;
    }
    else
    {
        zeroFrame = 0;
    }


    //3.4.3 Residual decoding  (d09r02_F2F)
    //Residual decoding is performed only when lsbMode is 0.
    if (lsbMode == 0)
    {
        uint16_t k, n;
        k = n = 0;
        //while (k < 𝑁𝐸 && n < nResBits)
        while (k < NE && n < nResBits)
        {
            //if (𝑋𝑞 ̂[k] != 0)
            if (X_hat_q_residual[k] != 0)
            {
                if (resBits[n++] == 0)
                {
                    //if (𝑋𝑞 ̂[k] > 0)
                    if (X_hat_q_residual[k] > 0)
                    {
                        //𝑋𝑞 ̂[k] -= 0.1875;
                        X_hat_q_residual[k] -= 0.1875;
                    }
                    else
                    {
                        //𝑋𝑞 ̂[k] -= 0.3125;
                        X_hat_q_residual[k] -= 0.3125;
                    }
                }
                else
                {
                    //if (𝑋𝑞 ̂[k] > 0)
                    if (X_hat_q_residual[k] > 0)
                    {
                        //𝑋𝑞 ̂[k] += 0.3125;
                        X_hat_q_residual[k] += 0.3125;
                    }
                    else
                    {
                        //𝑋𝑞 ̂[k] += 0.1875;
                        X_hat_q_residual[k] += 0.1875;
                    }
                }
            }
            k++;
        }
    }

}


void ResidualSpectrum::registerDatapoints(DatapointContainer* datapoints)
{
    if (nullptr != datapoints)
    {
        datapoints->addDatapoint( "X_hat_q_residual", &X_hat_q_residual[0], sizeof(double)*NE );
    }
}

}//namespace Lc3Dec
