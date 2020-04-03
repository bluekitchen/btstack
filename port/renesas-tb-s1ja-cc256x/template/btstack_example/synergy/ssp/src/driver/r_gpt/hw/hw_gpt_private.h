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

/**********************************************************************************************************************
 * File Name    : hw_gpt_private.h
 * Description  : Lower level driver interface to GPT.  Register setting and reading is done here.
 **********************************************************************************************************************/

#ifndef HW_GPT_PRIVATE_H
#define HW_GPT_PRIVATE_H

/**********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "bsp_api.h"
#include "r_gpt.h"

/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER
/*******************************************************************************************************************//**
 * @addtogroup GPT
 * @{
 **********************************************************************************************************************/
/**********************************************************************************************************************
 * Macro Definitions
 **********************************************************************************************************************/

/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
/** Interrupt sources */
typedef enum e_gpt_int
{
    GPT_INT_A_COMPARE_CAPTURE,
    GPT_INT_B_COMPARE_CAPTURE,
    GPT_INT_C_COMPARE,
    GPT_INT_D_COMPARE,
    GPT_INT_E_COMPARE,
    GPT_INT_F_COMPARE,
    GPT_INT_OVERFLOW,
    GPT_INT_UNDERFLOW,
    GPT_INT_AD_START_A,
    GPT_INT_AD_START_B
} gpt_int_t;

/** Timer modes */
typedef enum e_gpt_mode
{
    GPT_MODE_PERIODIC  = 0,       ///< Timer will restart after delay expires.
    GPT_MODE_ONE_SHOT  = 1,       ///< Timer will stop after delay expires.
    GPT_MODE_INVALID_2 = 2,       ///< Setting prohibited.  Do not set this value.
    GPT_MODE_INVALID_3 = 3,       ///< Setting prohibited.  Do not set this value.
    GPT_MODE_TW_PWM_1  = 4,       ///< Triangle wave PWM mode 1 (16-bit transfer at crest) (single or double buffer)
    GPT_MODE_TW_PWM_2  = 5,       ///< Triangle wave PWM mode 2 (16-bit transfer at trough) (single or double buffer)
    GPT_MODE_TW_PWM_3  = 6,       ///< Triangle wave PWM mode 3 (32-bit transfer at trough) (fixed buffer)
    GPT_MODE_INVALID_7 = 7,       ///< Setting prohibited.  Do not set this value.
} gpt_mode_t;

/** Count direction */
typedef enum e_gpt_dir
{
    GPT_DIR_COUNT_DOWN = 0,
    GPT_DIR_COUNT_UP   = 1
} gpt_dir_t;

/** PCLK divisors */
typedef enum e_gpt_pclk_div
{
    GPT_PCLK_DIV_BY_1    = 0,
    GPT_PCLK_DIV_BY_4    = 1,
    GPT_PCLK_DIV_BY_16   = 2,
    GPT_PCLK_DIV_BY_64   = 3,
    GPT_PCLK_DIV_BY_256  = 4,
    GPT_PCLK_DIV_BY_1024 = 5
} gpt_pclk_div_t;

/** Input capture/compare match registers */
typedef enum e_gpt_capture_compare
{
    GPT_CAPTURE_COMPARE_MATCH_A,
    GPT_CAPTURE_COMPARE_MATCH_B,
    GPT_COMPARE_MATCH_C,
    GPT_COMPARE_MATCH_D,
    GPT_COMPARE_MATCH_E,
    GPT_COMPARE_MATCH_F
} gpt_capture_compare_t;

/** Input/Output pins */
typedef enum e_gpt_gtioc
{
    GPT_GTIOCA = 0,
    GPT_GTIOCB = 1
} gpt_gtioc_t;

/** Start/stop status to write to GTCR.CST bit */
typedef enum e_gpt_start_status
{
    GPT_STOP  = 0,
    GPT_START = 1,
} gpt_start_status_t;

/** Duty cycle mode. */
typedef enum e_gpt_duty_cycle_mode
{
    GPT_DUTY_CYCLE_MODE_REGISTER     = 0, ///< Duty cycle depends on compare match
    GPT_DUTY_CYCLE_MODE_0_PERCENT    = 2, ///< Output low
    GPT_DUTY_CYCLE_MODE_100_PERCENT  = 3, ///< Output high
} gpt_duty_cycle_mode_t;

#if defined(__CC_ARM)
#pragma push
#pragma anon_unions
#elif defined(__ICCARM__)
#pragma language=extended
#elif defined(__GNUC__)
/* anonymous unions are enabled by default */
#elif defined(__TMS470__)
/* anonymous unions are enabled by default */
#elif defined(__TASKING__)
#pragma warning 586
#else // if defined(__CC_ARM)
#warning Not supported compiler type
#endif // if defined(__CC_ARM)

typedef union
{
    __IO uint16_t  GTIOR;                           /*!< (@ 0x40078034) General PWM Timer I/O Register
                                                     *                        */

    /*LDRA_INSPECTED 381 S Anonymous structures and unions are allowed in SSP code. */
    struct
    {
        __IO uint16_t  GTIOCM :  2;                 /*!< [0..1] GTIOCM Compare Match Output
                                                     *                                   */
        __IO uint16_t  GTIOCE :  2;                 /*!< [2..3] GTIOCE Cycle End Output
                                                     *                                       */
        __IO uint16_t  GTIOCI :  1;                 /*!< [4..4] GTIOCI Initial Output
                                                     *                                         */
        uint16_t              :  1;
        __IO uint16_t  OADFLT :  2;                 /*!< [6..7] GTIOCA Pin Output Value Setting at the Count Stop
                                                     *             */
        __IO uint16_t  OAE    :  1;                 /*!< [8..8] GTIOCA Pin Output Enable
                                                     *                                      */
        __IO uint16_t  OADF   :  2;                 /*!< [9..10] GTIOCA Pin Disable Value Setting
                                                     *                             */
        uint16_t              :  2;
        __IO uint16_t  NFAEN  :  1;                 /*!< [13..13] Noise Filter A Enable
                                                     *                                       */
        __IO uint16_t  NFCSA  :  2;                 /*!< [14..15] Noise Filter A Sampling Clock Select
                                                     *                        */
    }  GTIOR_b;                                     /*!< [32] BitSize
                                                     *                                                         */
} gtior_t;

/* --------------------  End of section using anonymous unions  ------------------- */
#if defined(__CC_ARM)
#pragma pop
#elif defined(__ICCARM__)
/* leave anonymous unions enabled */
#elif defined(__GNUC__)
/* anonymous unions are enabled by default */
#elif defined(__TMS470__)
/* anonymous unions are enabled by default */
#elif defined(__TASKING__)
#pragma warning restore
#else // if defined(__CC_ARM)
#warning Not supported compiler type
#endif // if defined(__CC_ARM)

/**********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/

/**********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
/* Select implementation based on BSP here */
#include "common/hw_gpt_common.h"

/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER

#endif /* HW_GPT_PRIVATE_H */
/*******************************************************************************************************************//**
 * @} (end addtogroup GPT)
 **********************************************************************************************************************/
