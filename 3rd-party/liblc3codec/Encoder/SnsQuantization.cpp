/*
 * SnsQuantization.cpp
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

#include "SnsQuantization.hpp"
#include "SnsQuantizationTables.hpp"
#include <cmath>
#include <limits>

namespace Lc3Enc
{

SnsQuantization::SnsQuantization() :
    ind_LF(0),
    ind_HF(0),
    shape_j(0),
    Gind(0),
    LS_indA(0),
    LS_indB(0),
    index_joint_j(0),
    idxA(0),
    idxB(0),
    datapoints(nullptr)
{
}

SnsQuantization::~SnsQuantization()
{
}


void SnsQuantization::run(const double* const scf)
{
    // 3.3.7.3.2 Stage 1  (d09r02_F2F)
    if ( !std::numeric_limits<double>::has_infinity )
    {
        // we never should reach this line, but really want to make sure to
        // go further when the assumed infinity() feature is not supported
        return;
    }
    double dMSE_LF_min = std::numeric_limits<double>::infinity();
    double dMSE_HF_min = std::numeric_limits<double>::infinity();
    for (uint8_t i=0; i < 32; i++)
    {
        double dMSE_LF_i = 0;
        double dMSE_HF_i = 0;
        for (uint8_t n=0; n < 8; n++)
        {
            dMSE_LF_i += (scf[n] - LFCB[i][n]) * (scf[n] - LFCB[i][n]);
            dMSE_HF_i += (scf[n+8] - HFCB[i][n]) * (scf[n+8] - HFCB[i][n]);
        }
        if (dMSE_LF_i < dMSE_LF_min)
        {
            ind_LF = i;
            dMSE_LF_min = dMSE_LF_i;
        }
        if (dMSE_HF_i < dMSE_HF_min)
        {
            ind_HF = i;
            dMSE_HF_min = dMSE_HF_i;
        }
    }

    //The first stage vector is composed as:
    //𝑠𝑡1(𝑛) = 𝐿𝐹𝐶𝐵𝑖𝑛𝑑_𝐿𝐹 (𝑛), 𝑓𝑜𝑟 𝑛 = [0 … 7], # (33)
    //𝑠𝑡1(𝑛 + 8) = 𝐻𝐹𝐶𝐵𝑖𝑛𝑑_𝐻𝐹(𝑛), 𝑓𝑜𝑟 𝑛 = [0 … 7], # (34)
    double st1[16];
    for (uint8_t n = 0; n<8; n++)
    {
        st1[n]   = LFCB[ind_LF][n];
        st1[n+8] = HFCB[ind_HF][n];
    }
    double r1[16];
    for (uint8_t n=0; n<16; n++)
    {
        r1[n] = scf[n] - st1[n];
    }

    // 3.3.7.3.3 Stage 2  (d09r02_F2F)
    // 3.3.7.3.3.3 Stage 2 target preparation  (d09r02_F2F)
    for (uint8_t n=0; n < 16; n++)
    {
        t2rot[n] = 0;
        for (uint8_t row=0; row < 16; row++)
        {
            t2rot[n] += r1[row] * D[row][n];
        }
    }

    // 3.3.7.3.3.4 Shape candidates  (d09r02_F2F)
    // 3.3.7.3.3.5 Stage 2 shape search (d09r02_F2F)

    //   (3) Shape name: ‘outlier_far’
    //   Scale factor set A: {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15}
    //   Scale factor set B: Empty set
    //   Pulse configuration, Set A, PVQ(NA, KA): PVQ(16, 6)
    //   Pulse configuration, Set B, PVQ(NB, KB): Empty

    // step 1  -> y3,start
    //   Project to or below pyramid N=16, K=6,
    //   (and update energy energyy and correlation corrxy terms to reflect
    //   the pulses present in y3, start )
    uint8_t K = 6;
    uint8_t N = 16;
    double proj_fac = 0;
    double abs_x[N];
    for (uint8_t n =0; n < N; n++)
    {
        abs_x[n] = fabs(t2rot[n]);
        proj_fac += abs_x[n];
    }
    proj_fac = (K-1) / proj_fac;

    uint8_t k = 0;
    double corr_xy = 0;
    double energy_y = 0;
    for (uint8_t n =0; n < N; n++)
    {
        sns_Y3[n] = floor( abs_x[n]*proj_fac );
        if (sns_Y3[n] != 0)
        {
            k += sns_Y3[n];
            corr_xy += sns_Y3[n]*abs_x[n];
            energy_y += sns_Y3[n]*sns_Y3[n];
        }
    }

    // step 2 -> y3 -> y2,start
    //   Add unit pulses until you reach K=6 over N=16 samples, save y3
    double corr_xy_last = corr_xy;
    double energy_y_last = energy_y;
    for (/*start with existing k intentionally*/; k < K; k++)
    {
        uint8_t n_c = 0;
        uint8_t n_best = 0;
        corr_xy = corr_xy_last + 1*abs_x[n_c];
        double bestCorrSq = corr_xy * corr_xy;
        double bestEn = energy_y_last + 2*1*sns_Y3[n_c] + 1*1;
        for (n_c=1; n_c < N; n_c++)
        {
            corr_xy  = corr_xy_last + 1*abs_x[n_c];
            energy_y = energy_y_last + 2*1*sns_Y3[n_c] + 1*1;
            if ( corr_xy*corr_xy*bestEn > bestCorrSq*energy_y)
            {
                n_best = n_c;
                bestCorrSq = corr_xy*corr_xy;
                bestEn     = energy_y;
            }
        }
        corr_xy_last  += 1*abs_x[n_best];
        energy_y_last += 2*1*sns_Y3[n_best] + 1*1;
        sns_Y3[n_best]++;
    }

    //   (2) Shape name: ‘outlier_near’
    //   Scale factor set A: {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15}
    //   Scale factor set B: Empty set
    //   Pulse configuration, Set A, PVQ(NA, KA): PVQ(16, 8)
    //   Pulse configuration, Set B, PVQ(NB, KB): Empty

    // step 3 -> y2 -> y1,pre-start
    //   Add unit pulses until you reach K=8 over N=16 samples, save y2
    N = 16;
    K = 8;
    for (uint8_t n=0; n < N; n++)
    {
        sns_Y2[n] = sns_Y3[n];
    }
    for (/*start with existing k intentionally*/; k < K; k++)
    {
        uint8_t n_c = 0;
        uint8_t n_best = 0;
        corr_xy = corr_xy_last + 1*abs_x[n_c];
        double bestCorrSq = corr_xy * corr_xy;
        double bestEn = energy_y_last + 2*1*sns_Y2[n_c] + 1*1;
        for (n_c=1; n_c < N; n_c++)
        {
            corr_xy  = corr_xy_last + 1*abs_x[n_c];
            energy_y = energy_y_last + 2*1*sns_Y2[n_c] + 1*1;
            if ( corr_xy*corr_xy*bestEn > bestCorrSq*energy_y)
            {
                n_best = n_c;
                bestCorrSq = corr_xy*corr_xy;
                bestEn     = energy_y;
            }
        }
        corr_xy_last  += 1*abs_x[n_best];
        energy_y_last += 2*1*sns_Y2[n_best] + 1*1;
        sns_Y2[n_best]++;
    }

    //   (1) Shape name: ‘regular_lf’
    //   Scale factor set A: {0,1,2,3,4,5,6,7,8,9}
    //   Scale factor set B: {10,11,12,13,14,15}
    //   Pulse configuration, Set A, PVQ(NA, KA): PVQ(10, 10)
    //   Pulse configuration, Set B, PVQ(NB, KB): Zeroed

    // step 4 -> y1,start
    //   Remove any unit pulses in y1,pre-start that are not part of set A to yield y1, start
    N = 10;
    K = 10;
    for (uint8_t n=0; n < N; n++)
    {
        sns_Y1[n] = sns_Y2[n];
    }

    // step 5
    //   Update energy energyy and correlation corrxy terms to reflect the
    //   pulses present in y1, start
    for (uint8_t n=N; n < 16; n++)
    {
        if (sns_Y2[n] != 0)
        {
            k -= sns_Y2[n];
            corr_xy_last  -= sns_Y2[n]*abs_x[n];
            energy_y_last -= sns_Y2[n]*sns_Y2[n];
        }
    }

    // step 6 ->  y1 -> y0,start
    //   Add unit pulses until you reach K=10 over N=10 samples (in set A),
    //   save y1
    for (/*start with existing k intentionally*/; k < K; k++)
    {
        uint8_t n_c = 0;
        uint8_t n_best = 0;
        corr_xy = corr_xy_last + 1*abs_x[n_c];
        double bestCorrSq = corr_xy * corr_xy;
        double bestEn = energy_y_last + 2*1*sns_Y1[n_c] + 1*1;
        //std::cout << static_cast<uint16_t>(k) << " n_c: " << static_cast<uint16_t>(n_c) << " -> " <<  (bestCorrSq/bestEn) << " x: " << abs_x[n_c] << std::endl;
        for (n_c=1; n_c < N; n_c++)
        {
            corr_xy  = corr_xy_last + 1*abs_x[n_c];
            energy_y = energy_y_last + 2*1*sns_Y1[n_c] + 1*1;
            //std::cout << static_cast<uint16_t>(k) << " n_c: " << static_cast<uint16_t>(n_c) << " -> " <<  (corr_xy*corr_xy/energy_y) << " x: " << abs_x[n_c] <<  std::endl;
            if ( corr_xy*corr_xy*bestEn > bestCorrSq*energy_y)
            {
                n_best = n_c;
                bestCorrSq = corr_xy*corr_xy;
                bestEn     = energy_y;
            }
        }
        corr_xy_last  += 1*abs_x[n_best];
        energy_y_last += 2*1*sns_Y1[n_best] + 1*1;
        sns_Y1[n_best]++;
        //std::cout << static_cast<uint16_t>(k) << " n_best: " << static_cast<uint16_t>(n_best) << " -> " <<  (corr_xy_last*corr_xy_last/energy_y_last) << std::endl;
    }

    //   (0) Shape name: ‘regular’
    //   Scale factor set A: {0,1,2,3,4,5,6,7,8,9}
    //   Scale factor set B: {10,11,12,13,14,15}
    //   Pulse configuration, Set A, PVQ(NA, KA): PVQ(10, 10)
    //   Pulse configuration, Set B, PVQ(NB, KB): PVQ(6, 1)

    // step 7 -> y0
    //   Add unit pulses to y0,start until you reach K=1 over N=6 samples
    //  (in set B), save y0
    // A
    N = 10;
    K = 10;
    for (uint8_t n=0; n < N; n++)
    {
        sns_Y0[n] = sns_Y1[n];
    }
    // B
    N = 6;
    K = 1;
    double max_abs_x = 0;
    uint8_t n_best = 0;
    for (uint8_t n_c=10; n_c < N+10; n_c++)
    {
        sns_Y0[n_c] = 0;
        if (abs_x[n_c] > max_abs_x)
        {
            max_abs_x = abs_x[n_c];
            n_best = n_c;
        }
    }
    sns_Y0[n_best] = 1;

    // step 8 -> y3, y2, y1, y0
    //   Add signs to non-zero positions of each yj vector from the target
    //   vector x, save y3, y2, y1, y0 as shape vector candidates (and for
    //   subsequent indexing of one of them)
    for (uint8_t n=0; n < 10; n++)
    {
        if (t2rot[n] < 0)
        {
            sns_Y0[n] *= -1;
            sns_Y1[n] *= -1;
            sns_Y2[n] *= -1;
            sns_Y3[n] *= -1;
        }
    }
    for (uint8_t n=10; n < 16; n++)
    {
        if (t2rot[n] < 0)
        {
            sns_Y0[n] *= -1;
            sns_Y2[n] *= -1;
            sns_Y3[n] *= -1;
        }
    }

    // step 9
    // Unit energy normalize each yj vector to candidate vector xq_j
    normalizeCandidate(sns_Y0, sns_XQ0, 16);
    normalizeCandidate(sns_Y1, sns_XQ1, 10);
    normalizeCandidate(sns_Y2, sns_XQ2, 16);
    normalizeCandidate(sns_Y3, sns_XQ3, 16);


    // 3.3.7.3.3.6 Adjustment gain candidates (d09r02_F2F)
    // 3.3.7.3.3.7 Shape and gain combination determination (d09r02_F2F)
    shape_j = 0;
    Gind    = 0;
    double G_gain_i_shape_j = 0;
    double* sns_XQ_shape_j = nullptr;
    double dMSE_min = std::numeric_limits<double>::infinity();
    for (uint8_t j=0; j < 4; j++)
    {
        uint8_t Gmaxind_j;
        double* sns_vq_gains;
        double* sns_XQ;
        switch(j)
        {
			default: ; // intentionally fall through to case 0; just to avoid compiler warning
            case 0: {
                Gmaxind_j = 1;
                sns_vq_gains = sns_vq_reg_adj_gains;
                sns_XQ = sns_XQ0;
                break;
            }
            case 1: {
                Gmaxind_j = 3;
                sns_vq_gains = sns_vq_reg_lf_adj_gains;
                sns_XQ = sns_XQ1;
                break;
            }
            case 2: {
                Gmaxind_j = 3;
                sns_vq_gains = sns_vq_near_adj_gains;
                sns_XQ = sns_XQ2;
                break;
            }
            case 3: {
                Gmaxind_j = 7;
                sns_vq_gains = sns_vq_far_adj_gains;
                sns_XQ = sns_XQ3;
                break;
            }
        }
        for (uint8_t i=0; i <= Gmaxind_j; i++)
        {
            double dMSE =0;
            for (uint8_t n=0; n < Nscales; n++)
            {
                double diff = t2rot[n] - sns_vq_gains[i]*sns_XQ[n];
                dMSE += diff*diff;
            }
            if (dMSE < dMSE_min)
            {
                shape_j = j;
                Gind    = i;
                dMSE_min = dMSE;
                G_gain_i_shape_j = sns_vq_gains[Gind];
                sns_XQ_shape_j   = sns_XQ;
            }
        }
    }
    uint8_t LSB_gain = Gind & 1;

    // 3.3.7.3.3.8 Enumeration of the selected PVQ pulse configurations     (d09r02_F2F)
    switch(shape_j)
    {
        case 0: {
            MPVQenum (
                idxA,
                LS_indA,
                10, /* i : dimension of vec_in */
                sns_Y0 /* i : PVQ integer pulse train */
                );
            MPVQenum (
                idxB,
                LS_indB,
                6, /* i : dimension of vec_in */
                &sns_Y0[10] /* i : PVQ integer pulse train */
                );
            const uint32_t SZ_shapeA_0 = 2390004;
            index_joint_j = (2*idxB + LS_indB + 2)*SZ_shapeA_0 + idxA;
            break;
        }
        case 1: {
            MPVQenum (
                idxA,
                LS_indA,
                10, /* i : dimension of vec_in */
                sns_Y1 /* i : PVQ integer pulse train */
                );
            const uint32_t SZ_shapeA_1 = 2390004;
            index_joint_j = LSB_gain * SZ_shapeA_1 + idxA;
            break;
        }
        case 2: {
            MPVQenum (
                idxA,
                LS_indA,
                16, /* i : dimension of vec_in */
                sns_Y2 /* i : PVQ integer pulse train */
                );
            index_joint_j = idxA;
            break;
        }
        case 3: {
            MPVQenum (
                idxA,
                LS_indA,
                16, /* i : dimension of vec_in */
                sns_Y3 /* i : PVQ integer pulse train */
                );
            const uint32_t SZ_shapeA_2 = 15158272;
            index_joint_j = SZ_shapeA_2 + LSB_gain + (2*idxA);
            break;
        }
    }


    // 3.3.7.3.4 Multiplexing of SNS VQ codewords (d09r02_F2F)
    // *** final multiplexing skipped for now ****
    //  computation of index_joint_j already done above



    // 5.3.7.3.5 Synthesis of the Quantized SNS scale factor vector (d09r01)
    // [3.3.7.3.4 got lost] Synthesis of the Quantized SNS scale factor vector  (d09r02_F2F)
    for (uint8_t n = 0; n < Nscales; n++)
    {
        double factor=0;
        for (uint8_t col=0; col < 16; col++)
        {
            factor += sns_XQ_shape_j[col] * D[n][col];
        }
        scfQ[n] = st1[n] + G_gain_i_shape_j * factor;
    }

}

void SnsQuantization::normalizeCandidate(int8_t* y, double* XQ, uint8_t N)
{
    double normValue = 0;
    for (uint8_t n=0; n < N; n++)
    {
        if (y[n] != 0) // try to save some computations
        {
            normValue += y[n]*y[n];
        }
    }
    normValue = sqrt(normValue);
    for (uint8_t n=0; n < N; n++)
    {
        XQ[n] = y[n];
        if (y[n] != 0) // try to save some computations
        {
            XQ[n] /= normValue;
        }
    }
    // ensure trailing zero values for N<Nscales (e.g. for XQ1)
    for (uint8_t n=N; n < Nscales; n++)
    {
        XQ[n] = 0;
    }
}

void SnsQuantization::MPVQenum (
    int32_t& index,
    int32_t& lead_sign_ind,
    uint8_t dim_in, /* i : dimension of vec_in */
    int8_t vec_in[] /* i : PVQ integer pulse train */
    )
{
    /* init */
    int32_t next_sign_ind = 0x80000000U; /* sentinel for first sign */
    int32_t k_val_acc = 0;
    int8_t pos = dim_in;
    index = 0;
    uint8_t n = 0;
    unsigned int* row_ptr = &(MPVQ_offsets[n][0]);
    /* MPVQ-index composition loop */
    unsigned int tmp_h_row = row_ptr[0];
    for (pos--; pos >= 0; pos--)
    {
        int8_t tmp_val = vec_in[pos];

        //[index, next_sign_ind] = encPushSign(tmp_val, next_sign_ind, index);
        index = encPushSign(tmp_val, next_sign_ind, index);
        index += tmp_h_row;
        k_val_acc += (tmp_val<0)?-tmp_val:tmp_val;
        if ( pos != 0 )
        {
            n += 1; /* switch row in offset table MPVQ_offsets(n, k) */
        }
        row_ptr = &(MPVQ_offsets[n][0]);
        tmp_h_row = row_ptr[k_val_acc];
    }
    lead_sign_ind = next_sign_ind;
    //return [ index, lead_sign_ind ] ;
}

//[ index, next_sign_ind ] =
//encPushSign( val, next_sign_ind_in, index_in)
uint32_t SnsQuantization::encPushSign(
    int8_t val,
    int32_t& next_sign_ind_in,
    int32_t index_in
    )
{
    int32_t index = index_in;
    if ( ((next_sign_ind_in & 0x80000000U) == 0) && (val != 0) )
    {
        index = 2*index_in + next_sign_ind_in;
    }
    //next_sign_ind = next_sign_ind_in;
    if ( val < 0 )
    {
        next_sign_ind_in = 1;
    }
    if ( val > 0 )
    {
        next_sign_ind_in = 0;
    }
    /* if val==0, there is no new sign information to “push”,
    i.e. next_sign_ind is not changed */
    //return [ index, next_sign_ind ];
    return index;
}

void SnsQuantization::registerDatapoints(DatapointContainer* datapoints_)
{
    datapoints = datapoints_;
    if (nullptr != datapoints)
    {
        datapoints->addDatapoint( "ind_LF", &ind_LF, sizeof(ind_LF) );
        datapoints->addDatapoint( "ind_HF", &ind_HF, sizeof(ind_HF) );
        datapoints->addDatapoint( "shape_j", &shape_j, sizeof(shape_j) );
        datapoints->addDatapoint( "Gind", &Gind, sizeof(Gind) );
        datapoints->addDatapoint( "LS_indA", &LS_indA, sizeof(LS_indA) );
        datapoints->addDatapoint( "LS_indB", &LS_indB, sizeof(LS_indB) );
        datapoints->addDatapoint( "idxA", &idxA, sizeof(idxA) );
        datapoints->addDatapoint( "idxB", &idxB, sizeof(idxB) );

        datapoints->addDatapoint( "t2rot", &t2rot[0], sizeof(double)*Nscales );
        datapoints->addDatapoint( "scfQ", &scfQ[0], sizeof(double)*Nscales );

        datapoints->addDatapoint( "sns_Y0", &sns_Y0[0], sizeof(int8_t)*16 );
        datapoints->addDatapoint( "sns_Y1", &sns_Y1[0], sizeof(int8_t)*10 );
        datapoints->addDatapoint( "sns_Y2", &sns_Y2[0], sizeof(int8_t)*16 );
        datapoints->addDatapoint( "sns_Y3", &sns_Y3[0], sizeof(int8_t)*16 );

        datapoints->addDatapoint( "sns_XQ0", &sns_XQ0[0], sizeof(double)*16 );
        datapoints->addDatapoint( "sns_XQ1", &sns_XQ1[0], sizeof(double)*10 );
        datapoints->addDatapoint( "sns_XQ2", &sns_XQ2[0], sizeof(double)*16 );
        datapoints->addDatapoint( "sns_XQ3", &sns_XQ3[0], sizeof(double)*16 );
    }
}

}//namespace Lc3Enc
