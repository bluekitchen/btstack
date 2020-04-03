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
 * File Name    : r_gpt.h
 * Description  : Prototypes of GPT functions used to implement various timer interfaces.
 **********************************************************************************************************************/

#ifndef R_GPT_H
#define R_GPT_H

/*******************************************************************************************************************//**
 * @ingroup HAL_Library
 * @defgroup GPT GPT
 * @brief Driver for the General PWM Timer (GPT).
 *
 * @section GPT_SUMMARY Summary
 * Extends @ref TIMER_API.
 *
 * This module implements the @ref TIMER_API using the General PWM Timer (GPT) peripherals GPT32EH, GPT32E, GPT32.
 * It also provides an output compare
 * extension to output the timer signal to the GTIOC pin.
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "bsp_api.h"
#include "r_timer_api.h"

/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define GPT_CODE_VERSION_MAJOR (1U)
#define GPT_CODE_VERSION_MINOR (13U)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
/** Level of GPT pin */
typedef enum e_gpt_pin_level
{
    GPT_PIN_LEVEL_LOW      = 0,     ///< Pin level low
    GPT_PIN_LEVEL_HIGH     = 1,     ///< Pin level high
    GPT_PIN_LEVEL_RETAINED = 2      ///< Pin level retained
} gpt_pin_level_t;

/** GPT PWM shortest pin level */
typedef enum e_gpt_shortest_level
{
    /** 1 extra PCLK in ON time. Minimum ON time will be limited to 2 PCLK raw counts. */
    GPT_SHORTEST_LEVEL_OFF      = 0,
    /** 1 extra PCLK in OFF time. Minimum ON time will be limited to 1 PCLK raw counts. */
    GPT_SHORTEST_LEVEL_ON       = 1,
}gpt_shortest_level_t;

/** Sources can be used to start the timer, stop the timer, count up, or count down. */
typedef enum e_gpt_trigger
{
    /** No action performed. */
    GPT_TRIGGER_NONE                             = 0,
    /** Action performed when GTIOCA input rises while GTIOCB is low. **/
    GPT_TRIGGER_GTIOCA_RISING_WHILE_GTIOCB_LOW   = (1UL << 8),
    /** Action performed when GTIOCA input rises while GTIOCB is high. **/
    GPT_TRIGGER_GTIOCA_RISING_WHILE_GTIOCB_HIGH  = (1UL << 9),
    /** Action performed when GTIOCA input falls while GTIOCB is low. **/
    GPT_TRIGGER_GTIOCA_FALLING_WHILE_GTIOCB_LOW  = (1UL << 10),
    /** Action performed when GTIOCA input falls while GTIOCB is high. **/
    GPT_TRIGGER_GTIOCA_FALLING_WHILE_GTIOCB_HIGH = (1UL << 11),
    /** Action performed when GTIOCB input rises while GTIOCA is low. **/
    GPT_TRIGGER_GTIOCB_RISING_WHILE_GTIOCA_LOW   = (1UL << 12),
    /** Action performed when GTIOCB input rises while GTIOCA is high. **/
    GPT_TRIGGER_GTIOCB_RISING_WHILE_GTIOCA_HIGH  = (1UL << 13),
    /** Action performed when GTIOCB input falls while GTIOCA is low. **/
    GPT_TRIGGER_GTIOCB_FALLING_WHILE_GTIOCA_LOW  = (1UL << 14),
    /** Action performed when GTIOCB input falls while GTIOCA is high. **/
    GPT_TRIGGER_GTIOCB_FALLING_WHILE_GTIOCA_HIGH = (1UL << 15),
    /** Enables settings in the Source Select Register. **/
    GPT_TRIGGER_SOURCE_REGISTER_ENABLE           = (1UL << 31)
} gpt_trigger_t;

/** Output level used when selecting what happens at compare match or cycle end. */
typedef enum e_gpt_output
{
    GPT_OUTPUT_RETAINED = 0,       ///< Output retained
    GPT_OUTPUT_LOW      = 1,       ///< Output low
    GPT_OUTPUT_HIGH     = 2,       ///< Output high
    GPT_OUTPUT_TOGGLED  = 3        ///< Output toggled
} gpt_output_t;

/** Configurations for output pins. */
typedef struct s_gpt_output_pin
{
    bool             output_enabled; ///< Set to true to enable output, false to disable output
    gpt_pin_level_t  stop_level;     ///< Select a stop level from ::gpt_pin_level_t
} gpt_output_pin_t;

/** Channel control block. DO NOT INITIALIZE.  Initialization occurs when timer_api_t::open is called. */
typedef struct st_gpt_instance_ctrl
{
    /** Callback provided when a timer ISR occurs.  NULL indicates no CPU interrupt. */
    void (* p_callback)(timer_callback_args_t * p_args);

    /** Placeholder for user data.  Passed to the user callback in ::timer_callback_args_t. */
    void const            * p_context;
    void                  * p_reg;                  ///< Base register for this channel
    uint32_t                open;                   ///< Whether or not channel is open
    uint8_t                 channel;                ///< Channel number.
    bool                    one_shot;               ///< Whether or not timer is in one shot mode
    bool                    gtioca_output_enabled;  ///< Set to true to enable gtioca pin output
    bool                    gtiocb_output_enabled;  ///< Set to true to enable gtiocb pin output
    IRQn_Type               irq;                    ///< Counter overflow IRQ number
    timer_variant_t         variant;                ///< Timer variant
    gpt_shortest_level_t    shortest_pwm_signal;    ///< Shortest PWM signal level
} gpt_instance_ctrl_t;

/** GPT extension configures the output pins for GPT. */
typedef struct st_timer_on_gpt_cfg
{
    gpt_output_pin_t  gtioca;       ///< Configuration for GPT I/O pin A
    gpt_output_pin_t  gtiocb;       ///< Configuration for GPT I/O pin B
    gpt_shortest_level_t  shortest_pwm_signal;      ///< Shortest PWM signal level
} timer_on_gpt_cfg_t;

/**********************************************************************************************************************
 * Exported global variables
 **********************************************************************************************************************/
/** @cond INC_HEADER_DEFS_SEC */
/** Filled in Interface API structure for this Instance. */
extern const timer_api_t g_timer_on_gpt;
/** @endcond */

/*******************************************************************************************************************//**
 * @} (end defgroup GPT)
 **********************************************************************************************************************/

/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER

#endif /* R_GPT_H */
