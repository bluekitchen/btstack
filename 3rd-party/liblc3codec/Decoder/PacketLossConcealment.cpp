/*
 * PacketLossConcealment.cpp
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

#include "PacketLossConcealment.hpp"

namespace Lc3Dec
{

PacketLossConcealment::PacketLossConcealment(uint16_t NE_) :
    NE(NE_),
    plc_seed(24607), // this initialization need not be done every frame as clarified by Errata 15114
    nbLostCmpt(0),
    alpha(1),
    X_hat_lastGood(nullptr)
{
    X_hat_lastGood   = new double[NE];
    for (uint16_t k=0; k < NE; k++)
    {
        X_hat_lastGood[k] = 0.0;
    }
}

PacketLossConcealment::~PacketLossConcealment()
{
    delete[] X_hat_lastGood;
}

void PacketLossConcealment::run(uint8_t BEC_detect, double* X_hat, int16_t& ltpf_active)
{
    // Appendix B. Packet Loss Concealment   (d09r02_F2F)
    if (0==BEC_detect)
    {
        nbLostCmpt = 0;
        alpha = 1;
        for (uint16_t k=0; k < NE; k++)
        {
            X_hat_lastGood[k] = X_hat[k];
        }
    }
    else
    {
        ltpf_active = 0; // errata 15097 implemented

        if (nbLostCmpt < 0xFF)
        {
            nbLostCmpt++;
        }

        // Note: from (d09r02_F2F) its is not perfectly clear,
        //   whether alpha is modified before or after applying
        //   it to the spectrum -> we may have to check the
        //   given implementation with the LC3.exe reference
        if (nbLostCmpt >= 8)
        {
            alpha = 0.85*alpha;
        }
        else if (nbLostCmpt >= 4)
        {
            alpha = 0.9*alpha;
        }

        for (uint16_t k=0; k < NE; k++)
        {
            plc_seed = (16831 + plc_seed*12821) & 0xFFFF;
            if (plc_seed < 0x8000)
            {
                X_hat[k] = alpha*X_hat_lastGood[k];
            }
            else
            {
                X_hat[k] = -alpha*X_hat_lastGood[k];
            }
        }
    }

}


void PacketLossConcealment::registerDatapoints(DatapointContainer* datapoints)
{
    if (nullptr != datapoints)
    {
        datapoints->addDatapoint( "X_hat_lastGood", &X_hat_lastGood[0], sizeof(double)*NE);
    }
}

}//namespace Lc3Dec

