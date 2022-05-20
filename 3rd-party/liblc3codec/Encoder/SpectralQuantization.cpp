/*
 * SpectralQuantization.cpp
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

#include "SpectralQuantization.hpp"
#include "SpectralDataTables.hpp"
#include <cmath>
#include <algorithm>

namespace Lc3Enc
{

SpectralQuantization::SpectralQuantization(uint16_t NE_, uint8_t fs_ind_) :
    NE(NE_),
    fs_ind(fs_ind_),
    X_q(nullptr),
    lastnz(0),
    nbits_trunc(0),
    rateFlag(0),
    lsbMode(0),
    gg(0),
    lastnz_trunc(0),
    gg_ind(0),
    datapoints(nullptr),
    gg_off(0),
    gg_min(0),
    nbits_offset(0),
    nbits_spec(0),
    nbits_spec_adj(0),
    nbits_est(0),
    reset_offset_old(false),
    nbits_offset_old(0),
    nbits_spec_old(0),
    nbits_est_old(0)

{
    X_q = new int16_t[NE];
}

SpectralQuantization::~SpectralQuantization()
{
    delete[] X_q;
}

void SpectralQuantization::updateGlobalGainEstimationParameter(uint16_t nbits, uint16_t nbits_spec_local)
{
    if (reset_offset_old)
    {
        nbits_offset = 0;
    }
    else
    {
        nbits_offset =
            0.8 * nbits_offset_old +
            0.2 * std::min(
                    40.f,
                    std::max(
                        -40.f,
                        nbits_offset_old + nbits_spec_old - nbits_est_old
                        )
                     );
    }

    nbits_spec_adj = static_cast<uint16_t>( nbits_spec_local + nbits_offset + 0.5);

    gg_off = - std::min( 115, nbits / (10*(fs_ind+1)) )
             - 105 - 5*(fs_ind+1);
}

void SpectralQuantization::computeSpectralEnergy(double* E, const double* X_f)
{
    for (uint16_t k=0; k < NE/4; k++)
    {
        E[k] = pow(2, -31);
        for (uint8_t n=0; n<=3; n++)
        {
            E[k] += X_f[4*k + n]*X_f[4*k + n];
        }
        E[k] = 10 * log10( E[k] );
    }
}

void SpectralQuantization::globalGainEstimation(const double* E)
{
    // converted pseudo-code from page 49/50  (d09r02_F2F)
    int16_t fac = 256;
    //𝑔𝑔𝑖𝑛𝑑 = 255;
    gg_ind = 255;
    for (uint8_t iter = 0; iter < 8; iter++)
    {
        fac >>= 1;
        //𝑔𝑔𝑖𝑛𝑑 -= fac;
        gg_ind -= fac;
        double tmp = 0;
        uint8_t iszero = 1;
        //for (i = 𝑁𝐸/4-1; i >= 0; i--)
        for (int8_t i = NE/4-1; i >= 0; i--)
        {
            //if (E[i]*28/20 < (𝑔𝑔𝑖𝑛𝑑+𝑔𝑔𝑜𝑓𝑓))
            if (E[i]*28/20.0 < (gg_ind+gg_off))
            {
                if (iszero == 0)
                {
                    tmp += 2.7*28/20.0;
                }
            }
            else
            {
                //if ((𝑔𝑔𝑖𝑛𝑑+𝑔𝑔𝑜𝑓𝑓) < E[i]*28/20 - 43*28/20)
                if ((gg_ind+gg_off) < E[i]*28/20.0 - 43*28/20.0)
                {
                    //tmp += 2*E[i]*28/20 – 2*(𝑔𝑔𝑖𝑛𝑑+𝑔𝑔𝑜𝑓𝑓) - 36*28/20;
                    tmp += 2*E[i]*28/20.0 - 2*(gg_ind+gg_off) - 36*28/20.0;
                }
                else
                {
                    //tmp += E[i]*28/20 – (𝑔𝑔𝑖𝑛𝑑+𝑔𝑔𝑜𝑓𝑓) + 7*28/20;
                    tmp += E[i]*28/20.0 - (gg_ind+gg_off) + 7*28/20.0;
                }
                iszero = 0;
            }
        }
        //if (tmp > 𝑛𝑏𝑖𝑡𝑠𝑠𝑝𝑒𝑐′ *1.4*28/20 && iszero == 0)
        if ( (tmp > nbits_spec_adj*1.4*28/20.0) && (iszero == 0) )
        {
            //𝑔𝑔𝑖𝑛𝑑 += fac;
            gg_ind += fac;
        }
    }
}

bool SpectralQuantization::globalGainLimitation(const double* const X_f)
{
    // -> not precisely clear where this limitation should occur
    // -> Is the following converted pseudo code meant?
    // -> Are the chosen datatypes appropriate?
    double X_f_max = 0;
    for (uint16_t n=0; n < NE; n++)
    {
        X_f_max = std::max(X_f_max, fabs(X_f[n]));
    }
    if (X_f_max > 0)
    {
        gg_min = ceil(28*log10(X_f_max/(32768-0.375))) - gg_off;
    }
    else
    {
        gg_min = 0;
    }
    bool reset_offset = false;
    //if (𝑔𝑔𝑖𝑛𝑑 < 𝑔𝑔𝑚𝑖𝑛 || 𝑋𝑓𝑚𝑎𝑥 == 0)
    if ((gg_ind < gg_min) || X_f_max == 0)
    {
        //𝑔𝑔𝑖𝑛𝑑 = 𝑔𝑔𝑚𝑖𝑛;
        //𝑟𝑒𝑠𝑒𝑡𝑜𝑓𝑓𝑠𝑒𝑡 = 1;
        gg_ind = gg_min;
        reset_offset = true;
    }
    else
    {
        //𝑟𝑒𝑠𝑒𝑡𝑜𝑓𝑓𝑠𝑒𝑡 = 0;
        reset_offset = false;
    }
    return reset_offset;
}

void SpectralQuantization::quantizeSpectrum(const double* const X_f)
{
    gg = pow(10.f, (gg_ind+gg_off)/28.0f );
    for (uint16_t n=0; n < NE; n++)
    {
        if (X_f[n] >= 0)
        {
            X_q[n] = X_f[n]/gg + 0.375;
        }
        else
        {
            X_q[n] = X_f[n]/gg - 0.375;
        }
    }
}

void SpectralQuantization::computeBitConsumption(
        uint16_t nbits,
        uint8_t& modeFlag)
{
    //if (nbits > (160 + 𝑓𝑠𝑖𝑛𝑑 * 160))
    if (nbits > (160 + fs_ind * 160))
    {
        rateFlag = 512;
    }
    else
    {
        rateFlag = 0;
    }
    //if (nbits >= (480 + 𝑓𝑠𝑖𝑛𝑑 * 160))
    if (nbits >= (480 + fs_ind * 160))
    {
        modeFlag = 1;
    }
    else
    {
        modeFlag = 0;
    }

    //lastnz = 𝑁𝐸;
    lastnz = NE;
    //while (lastnz>2 && 𝑋𝑞[lastnz-1] == 0 && 𝑋𝑞[lastnz-2] == 0)
    while ( (lastnz>2) && (X_q[lastnz-1] == 0) && (X_q[lastnz-2] == 0) )
    {
        lastnz -= 2;
    }

    //𝑛𝑏𝑖𝑡𝑠𝑒𝑠𝑡 = 0;
    //𝑛𝑏𝑖𝑡𝑠𝑡𝑟𝑢𝑛𝑐 = 0;
    //𝑛𝑏𝑖𝑡𝑠𝑙𝑠𝑏 = 0;
    uint32_t nbits_est_local = 0;
    uint32_t nbits_trunc_local = 0;
    nbits_lsb = 0;
    lastnz_trunc = 2;
    uint16_t c = 0;
    for (uint16_t n = 0; n < lastnz; n=n+2)
    {
        uint16_t t = c + rateFlag;
        //if (n > 𝑁𝐸/2)
        if (n > NE/2)
        {
            t += 256;
        }
        //a = abs(𝑋𝑞[n]);
        uint16_t a = abs(X_q[n]);
        uint16_t a_lsb = a;
        //b = abs(𝑋𝑞[n+1]);
        uint16_t b = abs(X_q[n+1]);
        uint16_t b_lsb = b;
        uint16_t lev = 0;
        //while (max(a,b) >= 4)
        uint8_t pki;
        while (std::max(a,b) >= 4)
        {
            pki = ac_spec_lookup[t+lev*1024];
            //𝑛𝑏𝑖𝑡𝑠𝑒𝑠𝑡 += ac_spec_bits[pki][16];
            nbits_est_local += ac_spec_bits[pki][16];
            if ( (lev == 0) && (modeFlag == 1) )
            {
                //𝑛𝑏𝑖𝑡𝑠𝑙𝑠𝑏 += 2;
                nbits_lsb += 2;
            }
            else
            {
                //𝑛𝑏𝑖𝑡𝑠𝑒𝑠𝑡 += 2*2048;
                nbits_est_local += 2*2048;
            }
            a >>= 1;
            b >>= 1;
            lev = std::min(lev+1,3);
        }
        pki = ac_spec_lookup[t+lev*1024];
        uint16_t sym = a + 4*b;
        //𝑛𝑏𝑖𝑡𝑠𝑒𝑠𝑡 += ac_spec_bits[pki][sym];
        nbits_est_local += ac_spec_bits[pki][sym];
        //a_lsb = abs(𝑋𝑞[n]); -> implemented earlier
        //b_lsb = abs(𝑋𝑞[n+1]); -> implemented earlier
        //𝑛𝑏𝑖𝑡𝑠𝑒𝑠𝑡 += (min(a_lsb,1) + min(b_lsb,1)) * 2048;
        //nbits_est_local += (std::min(a_lsb,static_cast<uint16_t>(1))
        //              + std::min(b_lsb,static_cast<uint16_t>(1))) * 2048;
        // alternative implementation (more clear, more performant?)
        if (a_lsb > 0) nbits_est_local += 2048;
        if (b_lsb > 0) nbits_est_local += 2048;
        if ( (lev > 0) && (modeFlag == 1) )
        {
            a_lsb >>= 1;
            b_lsb >>= 1;
            //if (a_lsb == 0 && 𝑋𝑞[n] != 0)
            if (a_lsb == 0 && X_q[n] != 0)
            {
                //𝑛𝑏𝑖𝑡𝑠𝑙𝑠𝑏++;
                nbits_lsb++;
            }
            //if (b_lsb == 0 && 𝑋𝑞[n+1] != 0)
            if (b_lsb == 0 && X_q[n+1] != 0)
            {
                //𝑛𝑏𝑖𝑡𝑠𝑙𝑠𝑏++;
                nbits_lsb++;
            }
        }

        //if ((𝑋𝑞[n] != 0 || 𝑋𝑞[n+1] != 0) && (𝑛𝑏𝑖𝑡𝑠𝑒𝑠𝑡 <= 𝑛𝑏𝑖𝑡𝑠𝑠𝑝𝑒𝑐*2048))
        //if (((X_q[n] != 0) || (X_q[n+1] != 0)) && (nbits_est_local <= nbits_spec*2048))
        if (((X_q[n] != 0) || (X_q[n+1] != 0)) && (ceil(nbits_est_local/2048.0) <= nbits_spec))
        {
            lastnz_trunc = n + 2;
            //𝑛𝑏𝑖𝑡𝑠𝑡𝑟𝑢𝑛𝑐 = 𝑛𝑏𝑖𝑡𝑠𝑒𝑠𝑡;
            nbits_trunc_local = nbits_est_local;
        }
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
    //𝑛𝑏𝑖𝑡𝑠𝑒𝑠𝑡 = ceil(𝑛𝑏𝑖𝑡𝑠𝑒𝑠𝑡/2048) + 𝑛𝑏𝑖𝑡𝑠𝑙𝑠𝑏;
    nbits_est = ceil(nbits_est_local/2048.0) + nbits_lsb;
    //𝑛𝑏𝑖𝑡𝑠𝑡𝑟𝑢𝑛𝑐 = ceil(𝑛𝑏𝑖𝑡𝑠𝑡𝑟𝑢𝑛𝑐/2048);
    nbits_trunc = ceil(nbits_trunc_local/2048.0);
}

bool SpectralQuantization::globalGainAdjustment()
{
    uint16_t t1[5] = {80, 230, 380, 530, 680};
    uint16_t t2[5] = {500, 1025, 1550, 2075, 2600};
    uint16_t t3[5] = {850, 1700, 2550, 3400, 4250};

    double delta;
    double delta2;
    //if (𝑛𝑏𝑖𝑡𝑠𝑒𝑠𝑡 < t1[𝑓𝑠𝑖𝑛𝑑])
    if (nbits_est < t1[fs_ind])
    {
        //delta = (𝑛𝑏𝑖𝑡𝑠𝑒𝑠𝑡+48)/16;
        delta = (nbits_est+48)/static_cast<double>(16);
    }
    //else if (𝑛𝑏𝑖𝑡𝑠𝑒𝑠𝑡 < t2[𝑓𝑠𝑖𝑛𝑑])
    else if (nbits_est < t2[fs_ind])
    {
        //tmp1 = t1[𝑓𝑠𝑖𝑛𝑑]/16+3;
        //tmp2 = t2[𝑓𝑠𝑖𝑛𝑑]/48;
        //delta = (𝑛𝑏𝑖𝑡𝑠𝑒𝑠𝑡-t1[𝑓𝑠𝑖𝑛𝑑])*(tmp2-tmp1)/(t2[𝑓𝑠𝑖𝑛𝑑]-t1[𝑓𝑠𝑖𝑛𝑑]) + tmp1;
        double tmp1 = t1[fs_ind]/static_cast<double>(16) +3;
        double tmp2 = t2[fs_ind]/static_cast<double>(48);
        delta = (nbits_est-t1[fs_ind])*(tmp2-tmp1)/(t2[fs_ind]-t1[fs_ind]) + tmp1;
    }
    //else if (𝑛𝑏𝑖𝑡𝑠𝑒𝑠𝑡 < t3[𝑓𝑠𝑖𝑛𝑑])
    else if (nbits_est < t3[fs_ind])
    {
        //delta = 𝑛𝑏𝑖𝑡𝑠𝑒𝑠𝑡/48;
        delta = nbits_est/static_cast<double>(48);
    }
    else
    {
        //delta = t3[𝑓𝑠𝑖𝑛𝑑]/48;
        delta = t3[fs_ind]/static_cast<double>(48);
    }
    //delta = nint(delta); // this looks like we need floating point
    // nint(.) is the rounding-to-nearest-integer function.
    delta = static_cast<int16_t>(delta+0.5); // this looks like we need floating point
    delta2 = delta + 2;

    uint16_t gg_ind_origin = gg_ind;

    /*
    if ((𝑔𝑔𝑖𝑛𝑑 < 255 && 𝑛𝑏𝑖𝑡𝑠𝑒𝑠𝑡 > 𝑛𝑏𝑖𝑡𝑠𝑠𝑝𝑒𝑐) ||
        (𝑔𝑔𝑖𝑛𝑑 > 0 && 𝑛𝑏𝑖𝑡𝑠𝑒𝑠𝑡 < 𝑛𝑏𝑖𝑡𝑠𝑠𝑝𝑒𝑐 – delta2))
    {
        if (𝑛𝑏𝑖𝑡𝑠𝑒𝑠𝑡 < 𝑛𝑏𝑖𝑡𝑠𝑠𝑝𝑒𝑐 – delta2)
        {
            𝑔𝑔𝑖𝑛𝑑 -= 1;
        }
        else if (𝑔𝑔𝑖𝑛𝑑 == 254 || 𝑛𝑏𝑖𝑡𝑠𝑒𝑠𝑡 < 𝑛𝑏𝑖𝑡𝑠𝑠𝑝𝑒𝑐 + delta)
        {
            𝑔𝑔𝑖𝑛𝑑 += 1;
        }
        else
        {
            𝑔𝑔𝑖𝑛𝑑 += 2;
        }
            𝑔𝑔𝑖𝑛𝑑 = max(𝑔𝑔𝑖𝑛𝑑, 𝑔𝑔𝑚𝑖𝑛);
    }
    */
    if (( (gg_ind < 255) && (nbits_est > nbits_spec)) ||
        ( (gg_ind > 0) && (nbits_est < nbits_spec - delta2)))
    {
        if (nbits_est < nbits_spec - delta2)
        {
            gg_ind -= 1;
        }
        else if ( (gg_ind == 254) || (nbits_est < nbits_spec + delta) )
        {
            gg_ind += 1;
        }
        else
        {
            gg_ind += 2;
        }
        gg_ind = std::max(gg_ind, gg_min);
    }

    return (gg_ind_origin != gg_ind);
}

void SpectralQuantization::run(const double* const X_f, uint16_t nbits, uint16_t nbits_spec_local)
{
    nbits_spec = nbits_spec_local;

    //3.3.10.2 First global gain estimation  (d09r02_F2F)
    updateGlobalGainEstimationParameter(nbits, nbits_spec_local);

    double E[NE/4];
    computeSpectralEnergy(E, X_f);
    globalGainEstimation(E);
    // Finally, the quantized gain index is limited such that
    // the quantized spectrum stays within the range [-32768,32767]
    bool reset_offset = globalGainLimitation(X_f);

    // 3.3.10.3 Quantization   (d09r02_F2F)
    quantizeSpectrum(X_f);

    // 3.3.10.4 Bit consumption    (d09r02_F2F)
    uint8_t modeFlag;
    computeBitConsumption(
                        nbits,
                        modeFlag);

    if (nullptr!=datapoints)
    {
        datapoints->log("gg_ind", &gg_ind, sizeof(gg_ind));
        datapoints->log("gg", &gg, sizeof(gg)  );
        datapoints->log("X_q", X_q, sizeof(int16_t)*NE);
        datapoints->log("lastnz", &lastnz, sizeof(lastnz));
        datapoints->log("lastnz_trunc", &lastnz_trunc, sizeof(lastnz_trunc));
        datapoints->log("nbits_est", &nbits_est, sizeof(nbits_est) );
        datapoints->log("nbits_trunc", &nbits_trunc, sizeof(nbits_trunc) );
    }

    // 3.3.10.5 Truncation   (d09r02_F2F)
    for (uint16_t k = lastnz_trunc; k < lastnz; k++)
    {
        //𝑋𝑞[k] = 0;
        X_q[k] = 0;
    }

    //if (modeFlag == 1 && 𝑛𝑏𝑖𝑡𝑠𝑒𝑠𝑡 > 𝑛𝑏𝑖𝑡𝑠𝑠𝑝𝑒𝑐)
    if ( (modeFlag == 1) && (nbits_est > nbits_spec) )
    {
        lsbMode = 1;
    }
    else
    {
        lsbMode = 0;
    }

    if (nullptr!=datapoints)
    {
        datapoints->log("lsbMode", &lsbMode, sizeof(lsbMode) );
    }


    // Note: states needs to be transferred prior
    // to spectrum re-quantization!
    nbits_spec_old   = nbits_spec;
    nbits_est_old    = nbits_est;
    reset_offset_old = reset_offset;
    nbits_offset_old = nbits_offset;


    // 3.3.10.6 Global gain adjustment        (d09r02_F2F)
    bool isAdjusted = globalGainAdjustment();
    if (isAdjusted)
    {
        quantizeSpectrum(X_f);
        computeBitConsumption(
                            nbits,
                            modeFlag);

        // 3.3.10.5 Truncation   (d09r02_F2F)
        for (uint16_t k = lastnz_trunc; k < lastnz; k++)
        {
            //𝑋𝑞[k] = 0;
            X_q[k] = 0;
        }

        //if (modeFlag == 1 && 𝑛𝑏𝑖𝑡𝑠𝑒𝑠𝑡 > 𝑛𝑏𝑖𝑡𝑠𝑠𝑝𝑒𝑐)
        if (modeFlag == 1 && nbits_est > nbits_spec)
        {
            lsbMode = 1;
        }
        else
        {
            lsbMode = 0;
        }

    }
    if (nullptr!=datapoints)
    {
        datapoints->log("isAdjusted", &isAdjusted, sizeof(isAdjusted));
        datapoints->log("gg_ind_adj", &gg_ind, sizeof(gg_ind));
        datapoints->log("gg_adj", &gg, sizeof(gg)  );
        datapoints->log("X_q_req", X_q, sizeof(int16_t)*NE);
        datapoints->log("lastnz_req", &lastnz_trunc, sizeof(lastnz_trunc));
        datapoints->log("nbits_est_req", &nbits_est, sizeof(nbits_est) );
        datapoints->log("nbits_trunc_req", &nbits_trunc, sizeof(nbits_trunc) );
        datapoints->log("lsbMode_req", &lsbMode, sizeof(lsbMode) );

    }

}


void SpectralQuantization::registerDatapoints(DatapointContainer* datapoints_)
{
    datapoints = datapoints_;
    if (nullptr != datapoints)
    {
        datapoints->addDatapoint( "gg_off", &gg_off, sizeof(gg_off) );
        datapoints->addDatapoint( "gg_min", &gg_min, sizeof(gg_min) );
        datapoints->addDatapoint( "nbits_offset", &nbits_offset, sizeof(nbits_offset) );
    }
}

}//namespace Lc3Enc
