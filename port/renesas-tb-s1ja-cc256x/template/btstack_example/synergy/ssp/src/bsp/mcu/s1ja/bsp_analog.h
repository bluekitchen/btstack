/***********************************************************************************************************************
 * Copyright [2015-2017] Renesas Electronics Corporation and/or its licensors. All Rights Reserved.
 * 
 * This file is part of Renesas SynergyTM Software Package (SSP)
 *
 * The contents of this file (the "contents") are proprietary and confidential to Renesas Electronics Corporation
 * and/or its licensors ("Renesas") and subject to statutory and contractual protections.
 *
 * This file is subject to a Renesas SSP license agreement. Unless otherwise agreed in an SSP license agreement with
 * Renesas: 1) you may not use, copy, modify, distribute, display, or perform the contents; 2) you may not use any name
 * or mark of Renesas for advertising or publicity purposes or in connection with your use of the contents; 3) RENESAS
 * MAKES NO WARRANTY OR REPRESENTATIONS ABOUT THE SUITABILITY OF THE CONTENTS FOR ANY PURPOSE; THE CONTENTS ARE PROVIDED
 * "AS IS" WITHOUT ANY EXPRESS OR IMPLIED WARRANTY, INCLUDING THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE, AND NON-INFRINGEMENT; AND 4) RENESAS SHALL NOT BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, OR
 * CONSEQUENTIAL DAMAGES, INCLUDING DAMAGES RESULTING FROM LOSS OF USE, DATA, OR PROJECTS, WHETHER IN AN ACTION OF
 * CONTRACT OR TORT, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THE CONTENTS. Third-party contents
 * included in this file may be subject to different terms.
 **********************************************************************************************************************/
/***********************************************************************************************************************
* File Name    : bsp_analog.h
* Description  : Analog pin connections available on this MCU.
***********************************************************************************************************************/

#ifndef BSP_ANALOG_H_
#define BSP_ANALOG_H_

/*******************************************************************************************************************//**
 * @ingroup BSP_MCU_S1JA
 * @defgroup BSP_MCU_ANALOG_S1JA Analog Connections
 *
 * This group contains a list of enumerations that can be used with the @ref ANALOG_CONNECT_API.
 *
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#include "../all/bsp_common_analog.h"

/***********************************************************************************************************************
Macro definitions
***********************************************************************************************************************/

/***********************************************************************************************************************
Typedef definitions
***********************************************************************************************************************/

/** List of analog connections that can be made on S1JA
 * @note This list may change based on device. This list is for S1JA.
 * */
typedef enum e_analog_connect
{
    /* Connections for ACMPHS channel 0 VCMP input. */
    /* AN000 = P500 */
    /** Connect ACMPHS0 IVCMP to PORT5 P500. */
    ANALOG_CONNECT_ACMPHS0_IVCMP_TO_PORT5_P500      = ANALOG_CONNECT_DEFINE(ACMPHS, 0, CMPSEL0, IVCMP0, FLAG_CLEAR),
    /* AN005 = P013 */
    /** Connect ACMPHS0 IVCMP to PORT0 P013. */
    ANALOG_CONNECT_ACMPHS0_IVCMP_TO_PORT0_P013      = ANALOG_CONNECT_DEFINE(ACMPHS, 0, CMPSEL0, IVCMP1, FLAG_CLEAR),
    /* AN016 = P100 */
    /** Connect ACMPHS0 IVCMP to PORT1 P100. */
    ANALOG_CONNECT_ACMPHS0_IVCMP_TO_PORT1_P100      = ANALOG_CONNECT_DEFINE(ACMPHS, 0, CMPSEL0, IVCMP2, FLAG_CLEAR),

    /* Connections for ACMPHS channel 0 VREF input. */
    /* AN001 = P501 */
    /** Connect ACMPHS0 IVREF to PORT5 P501. */
    ANALOG_CONNECT_ACMPHS0_IVREF_TO_PORT5_P501      = ANALOG_CONNECT_DEFINE(ACMPHS, 0, CMPSEL1, IVREF0, FLAG_CLEAR),
    /* AN004 = P014 */
    /** Connect ACMPHS0 IVREF to PORT0 P014. */
    ANALOG_CONNECT_ACMPHS0_IVREF_TO_PORT0_P014      = ANALOG_CONNECT_DEFINE(ACMPHS, 0, CMPSEL1, IVREF1, FLAG_CLEAR),
    /* AN017 = P101 */
    /** Connect ACMPHS0 IVREF to PORT1 P101. */
    ANALOG_CONNECT_ACMPHS0_IVREF_TO_PORT1_P101      = ANALOG_CONNECT_DEFINE(ACMPHS, 0, CMPSEL1, IVREF2, FLAG_CLEAR),
    /** Connect ACMPHS0 IVREF to DAC80 DA. */
    ANALOG_CONNECT_ACMPHS0_IVREF_TO_DAC80_DA        = ANALOG_CONNECT_DEFINE(ACMPHS, 0, CMPSEL1, IVREF3, FLAG_CLEAR),
    /** Connect ACMPHS0 IVREF to DAC120 DA. */
    ANALOG_CONNECT_ACMPHS0_IVREF_TO_DAC120_DA       = ANALOG_CONNECT_DEFINE(ACMPHS, 0, CMPSEL1, IVREF4, FLAG_CLEAR),
    /* ANALOG0_VREF is the internal reference voltage. */
    /** Connect ACMPHS0 IVREF to ANALOG0 VREF. */
    ANALOG_CONNECT_ACMPHS0_IVREF_TO_ANALOG0_VREF    = ANALOG_CONNECT_DEFINE(ACMPHS, 0, CMPSEL1, IVREF5, FLAG_SET),

    /* Connections for shared ACMPLP IVREF0 input. */
    /* CMPREF0 = P109 */
    /** Connect ACMPLP0 IVREF0 to PORT1 P109. */
    ANALOG_CONNECT_ACMPLP0_IVREF0_TO_PORT1_P109     = ANALOG_CONNECT_DEFINE(ACMPLP, 0, COMPSEL1, CRVS0, FLAG_CLEAR),
    /** Connect ACMPLP0 IVREF0 to DAC80 DA. */
    ANALOG_CONNECT_ACMPLP0_IVREF0_TO_DAC80_DA       = ANALOG_CONNECT_DEFINE(ACMPLP, 0, COMPSEL1, CRVS1, FLAG_CLEAR),

    /* Connections for shared ACMPLP IVREF1 input. */
    /* CMPREF1 = P110 */
    /** Connect ACMPLP1 IVREF1 to PORT1 P110. */
    ANALOG_CONNECT_ACMPLP1_IVREF1_TO_PORT1_P110     = ANALOG_CONNECT_DEFINE(ACMPLP, 0, COMPSEL1, CRVS4, FLAG_CLEAR),
    /** Connect ACMPLP1 IVREF1 to DAC81 DA. */
    ANALOG_CONNECT_ACMPLP1_IVREF1_TO_DAC81_DA       = ANALOG_CONNECT_DEFINE(ACMPLP, 0, COMPSEL1, CRVS5, FLAG_CLEAR),

    /* Connections for ACMPLP channel 0 VCMP input. */
    /* CMPIN0 = P400 */
    /** Connect ACMPLP0 IVCMP to PORT4 P400. */
    ANALOG_CONNECT_ACMPLP0_IVCMP_TO_PORT4_P400      = ANALOG_CONNECT_DEFINE(ACMPLP, 0, COMPSEL0, CMPSEL0, FLAG_CLEAR),
    /** Connect ACMPLP0 IVCMP to OPAMP0 AMPO. */
    ANALOG_CONNECT_ACMPLP0_IVCMP_TO_OPAMP0_AMPO     = ANALOG_CONNECT_DEFINE(ACMPLP, 0, COMPSEL0, CMPSEL1, FLAG_CLEAR),

    /* Connections for ACMPLP channel 0 VREF input. */
    /* ANALOG0_VREF is the internal reference voltage. */
    /** Connect ACMPLP0 IVREF to ANALOG0 VREF. */
    ANALOG_CONNECT_ACMPLP0_IVREF_TO_ANALOG0_VREF    = ANALOG_CONNECT_DEFINE(ACMPLP, 0, COMPMDR, C0VRF, FLAG_CLEAR),
    /** Connect ACMPLP0 IVREF to ACMPLP0 IVREF0. */
    ANALOG_CONNECT_ACMPLP0_IVREF_TO_ACMPLP0_IVREF0  = ANALOG_CONNECT_DEFINE(ACMPLP, 0, COMPMDR, CLEAR_C0VRF, FLAG_CLEAR),

    /* Connections for ACMPLP channel 1 VCMP input. */
    /* CMPIN1 = P408 */
    /** Connect ACMPLP1 IVCMP to PORT4 P408. */
    ANALOG_CONNECT_ACMPLP1_IVCMP_TO_PORT4_P408      = ANALOG_CONNECT_DEFINE(ACMPLP, 0, COMPSEL0, CMPSEL4, FLAG_CLEAR),
    /** Connect ACMPLP1 IVCMP to OPAMP1 AMPO. */
    ANALOG_CONNECT_ACMPLP1_IVCMP_TO_OPAMP1_AMPO     = ANALOG_CONNECT_DEFINE(ACMPLP, 0, COMPSEL0, CMPSEL5, FLAG_CLEAR),

    /* Connections for ACMPLP channel 1 VREF input. */
    /* ANALOG0_VREF is the internal reference voltage. */
    /** Connect ACMPLP1 IVREF to ANALOG0 VREF. */
    ANALOG_CONNECT_ACMPLP1_IVREF_TO_ANALOG0_VREF    = ANALOG_CONNECT_DEFINE(ACMPLP, 0, COMPMDR, C1VRF, FLAG_CLEAR),
    /** Connect ACMPLP1 IVREF to ACMPLP0 IVREF0. */
    ANALOG_CONNECT_ACMPLP1_IVREF_TO_ACMPLP0_IVREF0  = ANALOG_CONNECT_DEFINE(ACMPLP, 0, COMPSEL1, CLEAR_C1VRF2, FLAG_CLEAR), // Also clear C1VRF
    /** Connect ACMPLP1 IVREF to ACMPLP1 IVREF1. */
    ANALOG_CONNECT_ACMPLP1_IVREF_TO_ACMPLP1_IVREF1  = ANALOG_CONNECT_DEFINE(ACMPLP, 0, COMPSEL1, C1VRF2, FLAG_CLEAR),       // Also clear C1VRF

    /* Connections for OPAMP channel 0 output. */
    /** Break all connections to OPAMP0 AMPO. */
    ANALOG_CONNECT_OPAMP0_AMPO_BREAK              = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP0OS, BREAK, FLAG_CLEAR),
    /* AMP1- = P014 */
    /** Connect OPAMP0 AMPO to PORT0 P014. */
    ANALOG_CONNECT_OPAMP0_AMPO_TO_PORT0_P014      = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP0OS, AMPOS0, FLAG_CLEAR),
    /* AMP1+ = P013 */
    /** Connect OPAMP0 AMPO to PORT0 P013. */
    ANALOG_CONNECT_OPAMP0_AMPO_TO_PORT0_P013      = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP0OS, AMPOS1, FLAG_CLEAR),
    /* AMP2- = P003 */
    /** Connect OPAMP0 AMPO to PORT0 P003. */
    ANALOG_CONNECT_OPAMP0_AMPO_TO_PORT0_P003      = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP0OS, AMPOS2, FLAG_CLEAR),
    /* AMP2+ = P002 */
    /** Connect OPAMP0 AMPO to PORT0 P002. */
    ANALOG_CONNECT_OPAMP0_AMPO_TO_PORT0_P002      = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP0OS, AMPOS3, FLAG_CLEAR),

    /* Connections for OPAMP channel 0- input. */
    /** Break all connections to OPAMP0 AMPM. */
    ANALOG_CONNECT_OPAMP0_AMPM_BREAK              = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP0MS, BREAK, FLAG_CLEAR),
    /* AMP0- = P501 */
    /** Connect OPAMP0 AMPM to PORT5 P501. */
    ANALOG_CONNECT_OPAMP0_AMPM_TO_PORT5_P501      = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP0MS, AMPMS0, FLAG_CLEAR),
    /* AMP0+ = P500 */
    /** Connect OPAMP0 AMPM to PORT5 P500. */
    ANALOG_CONNECT_OPAMP0_AMPM_TO_PORT5_P500      = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP0MS, AMPMS1, FLAG_CLEAR),
    /* AMP1- = P014 */
    /** Connect OPAMP0 AMPM to PORT0 P014. */
    ANALOG_CONNECT_OPAMP0_AMPM_TO_PORT0_P014      = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP0MS, AMPMS2, FLAG_CLEAR),
    /* AMP1+ = P013 */
    /** Connect OPAMP0 AMPM to PORT0 P013. */
    ANALOG_CONNECT_OPAMP0_AMPM_TO_PORT0_P013      = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP0MS, AMPMS3, FLAG_CLEAR),
    /* AMP2- = P003 */
    /** Connect OPAMP0 AMPM to PORT0 P003. */
    ANALOG_CONNECT_OPAMP0_AMPM_TO_PORT0_P003      = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP0MS, AMPMS4, FLAG_CLEAR),
    /** Connect OPAMP0 AMPM to OPAMP0 AMPO. */
    ANALOG_CONNECT_OPAMP0_AMPM_TO_OPAMP0_AMPO     = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP0MS, AMPMS7, FLAG_CLEAR),

    /* Connections for OPAMP channel 0+ input. */
    /** Break all connections to OPAMP0 AMPP. */
    ANALOG_CONNECT_OPAMP0_AMPP_BREAK               = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP0PS, BREAK, FLAG_CLEAR),
    /* AMP0+ = P500 */
    /** Connect OPAMP0 AMPP to PORT5 P500. */
    ANALOG_CONNECT_OPAMP0_AMPP_TO_PORT5_P500       = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP0PS, AMPPS0, FLAG_CLEAR),
    /* AMP1- = P014 */
    /** Connect OPAMP0 AMPP to PORT0 P014. */
    ANALOG_CONNECT_OPAMP0_AMPP_TO_PORT0_P014       = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP0PS, AMPPS1, FLAG_CLEAR),
    /* AMP1+ = P013 */
    /** Connect OPAMP0 AMPP to PORT0 P013. */
    ANALOG_CONNECT_OPAMP0_AMPP_TO_PORT0_P013       = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP0PS, AMPPS2, FLAG_CLEAR),
    /* AMP2+ = P002 */
    /** Connect OPAMP0 AMPP to PORT0 P002. */
    ANALOG_CONNECT_OPAMP0_AMPP_TO_PORT0_P002       = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP0PS, AMPPS3, FLAG_CLEAR),
    /** Connect OPAMP0 AMPP to DAC120 DA. */
    ANALOG_CONNECT_OPAMP0_AMPP_TO_DAC120_DA        = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP0PS, AMPPS7, FLAG_CLEAR),

    /* Connections for OPAMP channel 1- input. */
    /** Break all connections to OPAMP1 AMPM. */
    ANALOG_CONNECT_OPAMP1_AMPM_BREAK              = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP1MS, BREAK, FLAG_CLEAR),
    /* AMP1- = P014 */
    /** Connect OPAMP1 AMPM to PORT0 P014. */
    ANALOG_CONNECT_OPAMP1_AMPM_TO_PORT0_P014      = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP1MS, AMPMS0, FLAG_CLEAR),
    /** Connect OPAMP1 AMPM to OPAMP1 AMPO. */
    ANALOG_CONNECT_OPAMP1_AMPM_TO_OPAMP1_AMPO     = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP1MS, AMPMS7, FLAG_CLEAR),

    /* Connections for OPAMP channel 1+ input. */
    /** Break all connections to OPAMP1 AMPP. */
    ANALOG_CONNECT_OPAMP1_AMPP_BREAK               = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP1PS, BREAK, FLAG_CLEAR),
    /* AMP1- = P014 */
    /** Connect OPAMP1 AMPP to PORT0 P014. */
    ANALOG_CONNECT_OPAMP1_AMPP_TO_PORT0_P014       = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP1PS, AMPPS0, FLAG_CLEAR),
    /* AMP1+ = P013 */
    /** Connect OPAMP1 AMPP to PORT0 P013. */
    ANALOG_CONNECT_OPAMP1_AMPP_TO_PORT0_P013       = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP1PS, AMPPS1, FLAG_CLEAR),
    /* AMP2- = P003 */
    /** Connect OPAMP1 AMPP to PORT0 P003. */
    ANALOG_CONNECT_OPAMP1_AMPP_TO_PORT0_P003       = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP1PS, AMPPS2, FLAG_CLEAR),
    /* AMP2+ = P002 */
    /** Connect OPAMP1 AMPP to PORT0 P002. */
    ANALOG_CONNECT_OPAMP1_AMPP_TO_PORT0_P002       = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP1PS, AMPPS3, FLAG_CLEAR),
    /** Connect OPAMP1 AMPP to DAC80 DA. */
    ANALOG_CONNECT_OPAMP1_AMPP_TO_DAC80_DA         = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP1PS, AMPPS7, FLAG_CLEAR),

    /* Connections for OPAMP channel 2- input. */
    /** Break all connections to OPAMP2 AMPM. */
    ANALOG_CONNECT_OPAMP2_AMPM_BREAK              = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP2MS, BREAK, FLAG_CLEAR),
    /* AMP2- = P003 */
    /** Connect OPAMP2 AMPM to PORT0 P003. */
    ANALOG_CONNECT_OPAMP2_AMPM_TO_PORT0_P003      = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP2MS, AMPMS0, FLAG_CLEAR),
    /** Connect OPAMP2 AMPM to OPAMP2 AMPO. */
    ANALOG_CONNECT_OPAMP2_AMPM_TO_OPAMP2_AMPO     = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP2MS, AMPMS7, FLAG_CLEAR),

    /* Connections for OPAMP channel 2+ input. */
    /** Break all connections to OPAMP2 AMPP. */
    ANALOG_CONNECT_OPAMP2_AMPP_BREAK               = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP2PS, BREAK, FLAG_CLEAR),
    /* AMP2- = P003 */
    /** Connect OPAMP2 AMPP to PORT0 P003. */
    ANALOG_CONNECT_OPAMP2_AMPP_TO_PORT0_P003       = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP2PS, AMPPS0, FLAG_CLEAR),
    /* AMP2+ = P002 */
    /** Connect OPAMP2 AMPP to PORT0 P002. */
    ANALOG_CONNECT_OPAMP2_AMPP_TO_PORT0_P002       = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP2PS, AMPPS1, FLAG_CLEAR),
    /** Connect OPAMP2 AMPP to DAC81 DA. */
    ANALOG_CONNECT_OPAMP2_AMPP_TO_DAC81_DA         = ANALOG_CONNECT_DEFINE(OPAMP, 0, AMP2PS, AMPPS7, FLAG_CLEAR),
} analog_connect_t;

/** @} (end defgroup BSP_MCU_ANALOG_S1JA) */

#endif /* BSP_ANALOG_H_ */
