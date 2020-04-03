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
 * File Name    : r_gpt.c
 * Description  : GPT functions used to implement various timer interfaces.
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "r_gpt.h"
#include "r_gpt_cfg.h"
#include "hw/hw_gpt_private.h"
#include "r_gpt_private_api.h"
#include "r_cgc.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
/** Maximum number of clock counts in 32 bit timer */
#define GPT_MAX_CLOCK_COUNTS_32 (0xFFFFFFFFULL)

/** Maximum number of clock counts in 16 bit timer */
#define GPT_MAX_CLOCK_COUNTS_16 (0xFFFFUL)

/** "GPT" in ASCII, used to determine if channel is open. */
#define GPT_OPEN                (0x00475054ULL)

/** Macro for error logger. */
#ifndef GPT_ERROR_RETURN
/*LDRA_INSPECTED 77 S This macro does not work when surrounded by parentheses. */
#define GPT_ERROR_RETURN(a, err) SSP_ERROR_RETURN((a), (err), &g_module_name[0], &g_gpt_version)
#endif

/** Variant data mask to determine if GPT is 16-bit (0) or 32-bit (1). */
#define GPT_VARIANT_TIMER_SIZE_MASK  (0x4U)

/** Variant data bit to determine if GPT is base (0) or matsu (1). */
#define GPT_VARIANT_BASE_MATSU_BIT   (3)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/
static void gpt_hardware_initialize (gpt_instance_ctrl_t * const p_ctrl,
                                     timer_cfg_t   const * const p_cfg,
                                     gpt_pclk_div_t        const pclk_divisor,
                                     timer_size_t          const pclk_counts,
                                     timer_size_t          const duty_cycle);

static ssp_err_t gpt_period_to_pclk (gpt_instance_ctrl_t * const p_ctrl,
                                     timer_size_t          const period,
                                     timer_unit_t          const unit,
                                     timer_size_t        * const p_period_pclk,
                                     gpt_pclk_div_t      * const p_divisor);

static ssp_err_t gpt_duty_cycle_to_pclk (gpt_shortest_level_t  shortest_pwm_signal,
                                         timer_size_t     const duty_cycle,
                                         timer_pwm_unit_t const unit,
                                         timer_size_t     const period,
                                         timer_size_t   * const p_duty_cycle_pclk);

static void gpt_set_duty_cycle (gpt_instance_ctrl_t * const p_ctrl,
                                timer_size_t          const duty_cycle_counts,
                                uint8_t               const pin);

static ssp_err_t gpt_common_open (gpt_instance_ctrl_t * const p_ctrl,
                                  timer_cfg_t   const * const p_cfg,
                                  ssp_feature_t const * const p_ssp_feature,
                                  gpt_pclk_div_t      * const p_pclk_divisor,
                                  timer_size_t        * const p_pclk_counts,
                                  uint16_t            * const p_variant);

static void gpt_set_output_pin (gpt_instance_ctrl_t * const p_ctrl,
                                gpt_gtioc_t           const gtio,
                                gpt_pin_level_t       const level,
                                timer_mode_t          const mode,
                                timer_size_t          const duty_cycle);

static ssp_err_t gpt_divisor_select(gpt_instance_ctrl_t * const p_ctrl,
                                    uint64_t            *       p_period_counts,
                                    gpt_pclk_div_t      * const p_divisor);

static uint32_t gpt_clock_frequency_get(gpt_instance_ctrl_t * const p_ctrl);

/***********************************************************************************************************************
 * ISR prototypes
 **********************************************************************************************************************/
void gpt_counter_overflow_isr (void);

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
/** Name of module used by error logger macro */
#if BSP_CFG_ERROR_LOG != 0
static const char g_module_name[] = "gpt";
#endif

#if defined(__GNUC__)
/* This structure is affected by warnings from a GCC compiler bug.  This pragma suppresses the warnings in this 
 * structure only. */
/*LDRA_INSPECTED 69 S */
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
/** Version data structure used by error logger macro. */
static const ssp_version_t g_gpt_version =
{
    .api_version_minor  = TIMER_API_VERSION_MINOR,
    .api_version_major  = TIMER_API_VERSION_MAJOR,
    .code_version_major = GPT_CODE_VERSION_MAJOR,
    .code_version_minor = GPT_CODE_VERSION_MINOR
};
#if defined(__GNUC__)
/* Restore warning settings for 'missing-field-initializers' to as specified on command line. */
/*LDRA_INSPECTED 69 S */
#pragma GCC diagnostic pop
#endif

/***********************************************************************************************************************
 * Global Variables
 **********************************************************************************************************************/
/** GPT Implementation of General Timer Driver  */
/*LDRA_INSPECTED 27 D This structure must be accessible in user code. It cannot be static. */
const timer_api_t g_timer_on_gpt =
{
    .open            = R_GPT_TimerOpen,
    .stop            = R_GPT_Stop,
    .start           = R_GPT_Start,
    .reset           = R_GPT_Reset,
    .periodSet       = R_GPT_PeriodSet,
    .counterGet      = R_GPT_CounterGet,
    .dutyCycleSet    = R_GPT_DutyCycleSet,
    .infoGet         = R_GPT_InfoGet,
    .close           = R_GPT_Close,
    .versionGet      = R_GPT_VersionGet
};

/*******************************************************************************************************************//**
 * @addtogroup GPT
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @brief  Powers on GPT, handles required initialization described in hardware manual. Implements timer_api_t::open.
 *
 * The Open function configures a single GPT channel, starts the channel, and provides a handle for use with the GPT API
 * Control and Close functions. This function must be called once prior to calling any other GPT API functions. After a
 * channel is opened, the Open function should not be called again for the same channel without first calling the
 * associated Close function.
 *
 * GPT hardware does not support one-shot functionality natively.  When using one-shot mode, the timer will be stopped
 * in an ISR after the requested period has elapsed.
 *
 * The GPT implementation of the general timer can accept a ::timer_on_gpt_cfg_t extension parameter.
 *
 * @retval SSP_SUCCESS           Initialization was successful and timer has started.
 * @retval SSP_ERR_ASSERTION     One of the following parameters is incorrect.  Either
 *                                 - p_cfg is NULL, OR
 *                                 - p_ctrl is NULL, OR
 * @retval SSP_ERR_INVALID_ARGUMENT  One of the following parameters is invalid:
 *                                 - p_cfg->period: must be in the following range:
 *                                     - Lower bound: (1 / (PCLK frequency)
 *                                     - Upper bound: (0xFFFFFFFF * 1024 / (PCLK frequency))
 *                                 - p_cfg->p_callback not NULL, but ISR is not enabled. ISR must be enabled to
 *                                   use callback function.  Enable channel's overflow ISR in bsp_irq_cfg.h.
 * @retval SSP_ERR_IN_USE        The channel specified has already been opened. No configurations were changed. Call
 *                               the associated Close function or use associated Control commands to reconfigure the
 *                               channel.
 * @retval SSP_ERR_IRQ_BSP_DISABLED  - p_cfg->mode is ::TIMER_MODE_ONE_SHOT, but ISR is not enabled.  ISR must be
 *                                     enabled to use one-shot mode.
 * @retval SSP_ERR_IP_CHANNEL_NOT_PRESENT - The channel requested in the p_cfg parameter is not available on this device.
 *
 * @note This function is reentrant for different channels.  It is not reentrant for the same channel.
 **********************************************************************************************************************/
ssp_err_t R_GPT_TimerOpen  (timer_ctrl_t * const      p_api_ctrl,
                            timer_cfg_t const * const p_cfg)
{
    gpt_instance_ctrl_t * p_ctrl = (gpt_instance_ctrl_t *) p_api_ctrl;
#if GPT_CFG_PARAM_CHECKING_ENABLE
    SSP_ASSERT(NULL != p_cfg);
    SSP_ASSERT(NULL != p_ctrl);
#endif /* if GPT_CFG_PARAM_CHECKING_ENABLE */

    /** Calculate period and store internal variables */
    gpt_pclk_div_t pclk_divisor = GPT_PCLK_DIV_BY_1;
    timer_size_t pclk_counts = 0;
    uint16_t variant = 0;
    ssp_feature_t ssp_feature = {{(ssp_ip_t) 0}};
    ssp_feature.channel = p_cfg->channel;
    ssp_feature.unit = 0U;
    ssp_feature.id = SSP_IP_GPT;
    ssp_err_t err = gpt_common_open(p_ctrl, p_cfg, &ssp_feature, &pclk_divisor, &pclk_counts, &variant);
    GPT_ERROR_RETURN((SSP_SUCCESS == err), err);

    /** Save the configuration  */
    timer_on_gpt_cfg_t * p_ext = (timer_on_gpt_cfg_t *) p_cfg->p_extend;
    p_ctrl->gtioca_output_enabled = p_ext->gtioca.output_enabled;
    p_ctrl->gtiocb_output_enabled = p_ext->gtiocb.output_enabled;
    p_ctrl->shortest_pwm_signal = p_ext->shortest_pwm_signal;

    /** Calculate duty cycle */
    timer_size_t duty_cycle = 0;
    if (TIMER_MODE_PWM == p_cfg->mode)
    {
        err = gpt_duty_cycle_to_pclk(p_ctrl->shortest_pwm_signal, p_cfg->duty_cycle, p_cfg->duty_cycle_unit,
                                         pclk_counts, &duty_cycle);
        GPT_ERROR_RETURN((SSP_SUCCESS == err), err);
    }

    /** Verify channel is not already used */
    ssp_err_t     bsp_err = R_BSP_HardwareLock(&ssp_feature);
    GPT_ERROR_RETURN((SSP_SUCCESS == bsp_err), SSP_ERR_IN_USE);

    /** Power on GPT before setting any hardware registers. Make sure the counter is stopped before setting mode
       register, PCLK divisor register, and counter register. */
    GPT_BASE_PTR p_gpt_reg = (GPT_BASE_PTR) p_ctrl->p_reg;
    R_BSP_ModuleStart(&ssp_feature);
    HW_GPT_CounterStartStop(p_gpt_reg, GPT_STOP);
    HW_GPT_RegisterInit(p_gpt_reg, (variant >> GPT_VARIANT_BASE_MATSU_BIT) & 0xFFFE);

    gpt_hardware_initialize(p_ctrl, p_cfg, pclk_divisor, pclk_counts, duty_cycle);

    p_ctrl->open = GPT_OPEN;

    return SSP_SUCCESS;
} /* End of function R_GPT_TimerOpen */

/*******************************************************************************************************************//**
 * @brief  Stops timer. Implements timer_api_t::stop.
 *
 * @retval SSP_SUCCESS           Timer successfully stopped.
 * @retval SSP_ERR_ASSERTION     The p_ctrl parameter was null.
 * @retval SSP_ERR_NOT_OPEN      The channel is not opened.
 **********************************************************************************************************************/
ssp_err_t R_GPT_Stop (timer_ctrl_t * const p_api_ctrl)
{
    gpt_instance_ctrl_t * p_ctrl = (gpt_instance_ctrl_t *) p_api_ctrl;
#if GPT_CFG_PARAM_CHECKING_ENABLE
    /** Make sure handle is valid. */
    SSP_ASSERT(NULL != p_ctrl);
    SSP_ASSERT(NULL != p_ctrl->p_reg);
#endif

    GPT_ERROR_RETURN(GPT_OPEN == p_ctrl->open, SSP_ERR_NOT_OPEN);
    /** Stop timer */
    GPT_BASE_PTR p_gpt_reg = (GPT_BASE_PTR) p_ctrl->p_reg;
    HW_GPT_CounterStartStop(p_gpt_reg, GPT_STOP);
    return SSP_SUCCESS;
} /* End of function R_GPT_Stop */

/*******************************************************************************************************************//**
 * @brief  Starts timer. Implements timer_api_t::start.
 *
 * @retval SSP_SUCCESS           Timer successfully started.
 * @retval SSP_ERR_ASSERTION     The p_ctrl parameter was null.
 * @retval SSP_ERR_NOT_OPEN      The channel is not opened.
 **********************************************************************************************************************/
ssp_err_t R_GPT_Start (timer_ctrl_t * const p_api_ctrl)
{
    gpt_instance_ctrl_t * p_ctrl = (gpt_instance_ctrl_t *) p_api_ctrl;
#if GPT_CFG_PARAM_CHECKING_ENABLE
    /** Make sure handle is valid. */
    SSP_ASSERT(NULL != p_ctrl);
    SSP_ASSERT(NULL != p_ctrl->p_reg);
#endif

    GPT_ERROR_RETURN(GPT_OPEN == p_ctrl->open, SSP_ERR_NOT_OPEN);
    /** Start timer */
    GPT_BASE_PTR p_gpt_reg = (GPT_BASE_PTR) p_ctrl->p_reg;
    HW_GPT_CounterStartStop(p_gpt_reg, GPT_START);
    return SSP_SUCCESS;
} /* End of function R_GPT_Start */

/*******************************************************************************************************************//**
 * @brief  Sets counter value in provided p_value pointer. Implements timer_api_t::counterGet.
 *
 * @retval SSP_SUCCESS           Counter value read, p_value is valid.
 * @retval SSP_ERR_ASSERTION     The p_ctrl or p_value parameter was null.
 * @retval SSP_ERR_NOT_OPEN      The channel is not opened.
 **********************************************************************************************************************/
ssp_err_t R_GPT_CounterGet (timer_ctrl_t * const p_api_ctrl,
                            timer_size_t * const p_value)
{
    gpt_instance_ctrl_t * p_ctrl = (gpt_instance_ctrl_t *) p_api_ctrl;
#if GPT_CFG_PARAM_CHECKING_ENABLE
    /** Make sure parameters are valid */
    SSP_ASSERT(NULL != p_ctrl);
    SSP_ASSERT(NULL != p_value);
    SSP_ASSERT(NULL != p_ctrl->p_reg);
#endif

    GPT_ERROR_RETURN(GPT_OPEN == p_ctrl->open, SSP_ERR_NOT_OPEN);
    /** Read counter value */
    GPT_BASE_PTR p_gpt_reg = (GPT_BASE_PTR) p_ctrl->p_reg;
    *p_value = HW_GPT_CounterGet(p_gpt_reg);
    return SSP_SUCCESS;
} /* End of function R_GPT_CounterGet */

/*******************************************************************************************************************//**
 * @brief  Resets the counter value to 0. Implements timer_api_t::reset.
 *
 * @retval SSP_SUCCESS           Counter value written successfully.
 * @retval SSP_ERR_ASSERTION     The p_ctrl parameter was null.
 * @retval SSP_ERR_NOT_OPEN      The channel is not opened.
 **********************************************************************************************************************/
ssp_err_t R_GPT_Reset (timer_ctrl_t * const p_api_ctrl)
{
    gpt_instance_ctrl_t * p_ctrl = (gpt_instance_ctrl_t *) p_api_ctrl;
#if GPT_CFG_PARAM_CHECKING_ENABLE
    /** Make sure handle is valid. */
    SSP_ASSERT(NULL != p_ctrl);
    SSP_ASSERT(NULL != p_ctrl->p_reg);
#endif

    GPT_ERROR_RETURN(GPT_OPEN == p_ctrl->open, SSP_ERR_NOT_OPEN);
    /** Write the counter value */
    SSP_CRITICAL_SECTION_DEFINE;
    SSP_CRITICAL_SECTION_ENTER;

    GPT_BASE_PTR p_gpt_reg = (GPT_BASE_PTR) p_ctrl->p_reg;
    gpt_start_status_t status = HW_GPT_CounterStartBitGet(p_gpt_reg);
    HW_GPT_CounterStartStop(p_gpt_reg, GPT_STOP);
    /* Reset the gpt counter value to 0 */
    HW_GPT_ClearCounter (p_gpt_reg, p_ctrl->channel);
    HW_GPT_CounterStartStop(p_gpt_reg, status);

    SSP_CRITICAL_SECTION_EXIT;
    return SSP_SUCCESS;
} /* End of function R_GPT_Reset */

/*******************************************************************************************************************//**
 * @brief  Sets period value provided. Implements timer_api_t::periodSet.
 *
 * @retval SSP_SUCCESS           Period value written successfully.
 * @retval SSP_ERR_ASSERTION     The p_ctrl parameter was null.
 * @retval SSP_ERR_INVALID_ARGUMENT   One of the following is invalid:
 *                                    - p_period->unit: must be one of the options from timer_unit_t
 *                                    - p_period->value: must result in a period in the following range:
 *                                        - Lower bound: (1 / (PCLK frequency))
 *                                        - Upper bound: (0xFFFFFFFF * 1024 / (PCLK frequency))
 * @retval SSP_ERR_NOT_OPEN      The channel is not opened.
 **********************************************************************************************************************/
ssp_err_t R_GPT_PeriodSet (timer_ctrl_t * const p_api_ctrl,
                           timer_size_t   const period,
                           timer_unit_t const   unit)
{
    gpt_instance_ctrl_t * p_ctrl = (gpt_instance_ctrl_t *) p_api_ctrl;
#if GPT_CFG_PARAM_CHECKING_ENABLE
    /** Make sure handle is valid. */
    SSP_ASSERT(NULL != p_ctrl);
    SSP_ASSERT(NULL != p_ctrl->p_reg);
    GPT_ERROR_RETURN((0 != period), SSP_ERR_INVALID_ARGUMENT);
#endif

    GPT_ERROR_RETURN(GPT_OPEN == p_ctrl->open, SSP_ERR_NOT_OPEN);
    /** Delay must be converted to PCLK counts before it can be set in registers */
    ssp_err_t     err          = SSP_SUCCESS;
    timer_size_t      pclk_counts  = 0;
    gpt_pclk_div_t pclk_divisor = GPT_PCLK_DIV_BY_1;
    err = gpt_period_to_pclk(p_ctrl, period, unit, &pclk_counts, &pclk_divisor);

    /** Make sure period is valid. */
    GPT_ERROR_RETURN((SSP_SUCCESS == err), err);

    /** Store current status, then stop timer before setting divisor register */
    GPT_BASE_PTR p_gpt_reg = (GPT_BASE_PTR) p_ctrl->p_reg;
    gpt_start_status_t status = HW_GPT_CounterStartBitGet(p_gpt_reg);
    HW_GPT_CounterStartStop(p_gpt_reg, GPT_STOP);
    HW_GPT_DivisorSet(p_gpt_reg, pclk_divisor);
    HW_GPT_TimerCycleSet(p_gpt_reg, pclk_counts - 1);

    /** Reset counter in case new cycle is less than current count value, then restore state (counting or stopped). */
    HW_GPT_CounterSet(p_gpt_reg, 0);
    HW_GPT_CounterStartStop(p_gpt_reg, status);

    return SSP_SUCCESS;
} /* End of function R_GPT_PeriodSet */

/*******************************************************************************************************************//**
 * @brief  Sets status in provided p_status pointer. Implements pwm_api_t::dutyCycleSet.
 *
 * @retval SSP_SUCCESS                Counter value written successfully.
 * @retval SSP_ERR_ASSERTION          The p_ctrl parameter was null.
 * @retval SSP_ERR_NOT_OPEN           The channel is not opened.
 * @retval SSP_ERR_INVALID_ARGUMENT   The pin value is out of range; Should be either 0 (for GTIOCA) or 1 (for GTIOCB).
 **********************************************************************************************************************/
ssp_err_t R_GPT_DutyCycleSet (timer_ctrl_t   * const p_api_ctrl,
                              timer_size_t     const duty_cycle,
                              timer_pwm_unit_t const unit,
                              uint8_t          const pin)
{
    gpt_instance_ctrl_t * p_ctrl = (gpt_instance_ctrl_t *) p_api_ctrl;
#if GPT_CFG_PARAM_CHECKING_ENABLE
    /** Make sure handle is valid. */
    SSP_ASSERT(NULL != p_ctrl);
    SSP_ASSERT(NULL != p_ctrl->p_reg);
    GPT_ERROR_RETURN(((pin == GPT_GTIOCA) || (pin == GPT_GTIOCB)), SSP_ERR_INVALID_ARGUMENT);
#endif

    GPT_ERROR_RETURN(GPT_OPEN == p_ctrl->open, SSP_ERR_NOT_OPEN);
    /** Converted duty cycle to PCLK counts before it can be set in registers */
    timer_size_t duty_cycle_counts = 0;
    GPT_BASE_PTR p_gpt_reg = (GPT_BASE_PTR) p_ctrl->p_reg;
    ssp_err_t err = gpt_duty_cycle_to_pclk(p_ctrl->shortest_pwm_signal, duty_cycle, unit,
                                               HW_GPT_TimerCycleGet(p_gpt_reg) + 1, &duty_cycle_counts);
    GPT_ERROR_RETURN((SSP_SUCCESS == err), err);

    /** Set duty cycle. */
    gpt_set_duty_cycle(p_ctrl, duty_cycle_counts, pin);
    return SSP_SUCCESS;
} /* End of function R_GPT_DutyCycleSet */

/*******************************************************************************************************************//**
 * @brief  Get timer information and store it in provided pointer p_info.
 * Implements timer_api_t::infoGet.
 *
 * @retval SSP_SUCCESS           Period, count direction, frequency, and status value written to caller's
 *                               structure successfully.
 * @retval SSP_ERR_ASSERTION     The p_ctrl or p_info parameter was null.
 * @retval SSP_ERR_NOT_OPEN      The channel is not opened.
 **********************************************************************************************************************/
ssp_err_t R_GPT_InfoGet (timer_ctrl_t * const p_api_ctrl, timer_info_t * const p_info)
{
    gpt_instance_ctrl_t * p_ctrl = (gpt_instance_ctrl_t *) p_api_ctrl;
#if GPT_CFG_PARAM_CHECKING_ENABLE
    /** Make sure parameters are valid. */
    SSP_ASSERT(NULL != p_ctrl);
    SSP_ASSERT(NULL != p_info);
    SSP_ASSERT(NULL != p_ctrl->p_reg);
#endif

    GPT_ERROR_RETURN(GPT_OPEN == p_ctrl->open, SSP_ERR_NOT_OPEN);
    /** Get and store period */
    GPT_BASE_PTR p_gpt_reg = (GPT_BASE_PTR) p_ctrl->p_reg;
    p_info->period_counts = HW_GPT_TimerCycleGet(p_gpt_reg) + 1;

    /** Get and store clock frequency */
    p_info->clock_frequency = gpt_clock_frequency_get(p_ctrl);

    /** Get and store clock counting direction */
    p_info->count_direction = HW_GPT_DirectionGet(p_gpt_reg);

    bool status = HW_GPT_CounterStartBitGet(p_gpt_reg);
    if (status)
    {
        p_info->status = TIMER_STATUS_COUNTING;
    }
    else
    {
        p_info->status = TIMER_STATUS_STOPPED;
    }

    p_info->elc_event = HW_GPT_GetCounterOverFlowEvent(p_ctrl->channel);

    return SSP_SUCCESS;
} /* End of function R_GPT_InfoGet */

/*******************************************************************************************************************//**
 * @brief      Stops counter, disables output pins, and clears internal driver data.
 *
 * @retval     SSP_SUCCESS          Successful close.
 * @retval     SSP_ERR_ASSERTION    The parameter p_ctrl is NULL.
 * @retval     SSP_ERR_NOT_OPEN     The channel is not opened.
 **********************************************************************************************************************/
ssp_err_t R_GPT_Close (timer_ctrl_t * const p_api_ctrl)
{
    gpt_instance_ctrl_t * p_ctrl = (gpt_instance_ctrl_t *) p_api_ctrl;
    ssp_err_t err = SSP_SUCCESS;

#if GPT_CFG_PARAM_CHECKING_ENABLE
    /** Make sure channel is open.  If not open, return. */
    SSP_ASSERT(NULL != p_ctrl);
    SSP_ASSERT(NULL != p_ctrl->p_reg);
#endif

    /** Cleanup. Stop counter and disable output. */
    GPT_ERROR_RETURN(GPT_OPEN == p_ctrl->open, SSP_ERR_NOT_OPEN);
    GPT_BASE_PTR p_gpt_reg = (GPT_BASE_PTR) p_ctrl->p_reg;
    HW_GPT_CounterStartStop(p_gpt_reg, GPT_STOP);
    if (SSP_INVALID_VECTOR != p_ctrl->irq)
    {
        NVIC_DisableIRQ(p_ctrl->irq);
    }

    gtior_t * p_reg = HW_GPT_GTIOCRegLookup(p_gpt_reg, GPT_GTIOCA);
    HW_GPT_GTIOCPinOutputEnable(p_reg, false);
    p_reg = HW_GPT_GTIOCRegLookup(p_gpt_reg, GPT_GTIOCB);
    HW_GPT_GTIOCPinOutputEnable(p_reg, false);

    /** Unlock channel */
    ssp_feature_t ssp_feature = {{(ssp_ip_t) 0}};
    ssp_feature.channel = p_ctrl->channel;
    ssp_feature.unit = 0U;
    ssp_feature.id = SSP_IP_GPT;
    R_BSP_HardwareUnlock(&ssp_feature);

    /** Clear stored internal driver data */
    p_ctrl->one_shot = false;
    ssp_vector_info_t * p_vector_info;
    if (SSP_INVALID_VECTOR != p_ctrl->irq)
    {
        R_SSP_VectorInfoGet(p_ctrl->irq, &p_vector_info);
        (*p_vector_info->pp_ctrl) = NULL;
    }
    p_ctrl->open              = 0U;

    return err;
} /* End of function R_GPT_Close */

/*******************************************************************************************************************//**
 * @brief      Sets driver version based on compile time macros.
 *
 * @retval     SSP_SUCCESS          Successful close.
 * @retval     SSP_ERR_ASSERTION    The parameter p_version is NULL.
 **********************************************************************************************************************/
ssp_err_t R_GPT_VersionGet (ssp_version_t * const p_version)
{
#if GPT_CFG_PARAM_CHECKING_ENABLE
    /** Verify parameters are valid */
    SSP_ASSERT(NULL != p_version);
#endif

    p_version->version_id = g_gpt_version.version_id;

    return SSP_SUCCESS;
} /* End of function R_GPT_VersionGet */

/** @} (end addtogroup GPT) */

/*********************************************************************************************************************//**
 * Private Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * Performs calculations required to set configure GPT.
 *
 * @param[in]  p_ctrl          Instance control block.
 * @param[in]  p_cfg           Pointer to timer configuration.
 * @param[in]  p_ssp_feature   Pointer to ssp_feature_t structure for this channel.
 * @param[out] p_pclk_divisor  Divisor applied to GPT clock set here.
 * @param[out] p_pclk_counts   Counts before timer expires set here.
 * @param[out] p_variant       Variant data set here.
 *
 * @retval     SSP_SUCCESS                 Calculation of parameters required for configuration of timer is successful
 * @retval     SSP_ERR_IRQ_BSP_DISABLED    IRQ not enabled in BSP
 * @return                       See @ref Common_Error_Codes or functions called by this function for other possible
 *                               return codes. This function calls:
 *                                   * fmi_api_t::productFeatureGet
 **********************************************************************************************************************/
static ssp_err_t gpt_common_open (gpt_instance_ctrl_t * const p_ctrl,
                                  timer_cfg_t   const * const p_cfg,
                                  ssp_feature_t const * const p_ssp_feature,
                                  gpt_pclk_div_t      * const p_pclk_divisor,
                                  timer_size_t        * const p_pclk_counts,
                                  uint16_t            * const p_variant)
{
    fmi_feature_info_t info = {0U};
    ssp_err_t     err          = SSP_SUCCESS;
    err = g_fmi_on_fmi.productFeatureGet(p_ssp_feature, &info);
    GPT_ERROR_RETURN(SSP_SUCCESS == err, err);
    p_ctrl->p_reg = info.ptr;
    *p_variant = (uint16_t) info.variant_data;

    fmi_event_info_t event_info = {(IRQn_Type) 0U};
    g_fmi_on_fmi.eventInfoGet(p_ssp_feature, SSP_SIGNAL_GPT_COUNTER_OVERFLOW, &event_info);
    p_ctrl->irq = event_info.irq;

    if (0U == (info.variant_data & GPT_VARIANT_TIMER_SIZE_MASK))
    {
        p_ctrl->variant = TIMER_VARIANT_16_BIT;
    }
    else
    {
        p_ctrl->variant = TIMER_VARIANT_32_BIT;
    }
    
    /** Convert the period into PCLK counts and clock divisor */
    err = gpt_period_to_pclk(p_ctrl, p_cfg->period, p_cfg->unit, p_pclk_counts, p_pclk_divisor);
    GPT_ERROR_RETURN(SSP_SUCCESS == err, err);

    /** If callback is not null or timer mode is one shot, make sure the IRQ is enabled and store callback in the
     *  control block.
     *  @note The GPT hardware does not support one-shot mode natively.  To support one-shot mode, the timer will be
     *  stopped and cleared using software in the ISR. */
    if ((p_cfg->p_callback) || (TIMER_MODE_ONE_SHOT == p_cfg->mode))
    {
        GPT_ERROR_RETURN(SSP_INVALID_VECTOR != p_ctrl->irq, SSP_ERR_IRQ_BSP_DISABLED);

        p_ctrl->p_callback = p_cfg->p_callback;
        p_ctrl->p_context  = p_cfg->p_context;
    }

    /** Initialize channel and one_shot in control block. */
    p_ctrl->channel          = p_cfg->channel;
    if (TIMER_MODE_ONE_SHOT == p_cfg->mode)
    {
        p_ctrl->one_shot = true;
    }
    else
    {
        p_ctrl->one_shot = false;
    }

    return SSP_SUCCESS;
} /* End of function gpt_common_open */

/*******************************************************************************************************************//**
 * Performs hardware initialization of the GPT.
 *
 * @pre    All parameter checking and calculations should be done prior to calling this function.
 *
 * @param[in]  p_ctrl        Instance control block.
 * @param[in]  p_cfg         Pointer to timer configuration.
 * @param[in]  pclk_divisor  Divisor applied to GPT clock.
 * @param[in]  pclk_counts   Counts before timer expires.
 * @param[in]  duty_cycle    Duty cycle for this implementation.
 **********************************************************************************************************************/
static void gpt_hardware_initialize (gpt_instance_ctrl_t * const p_ctrl,
                                     timer_cfg_t   const * const p_cfg,
                                     gpt_pclk_div_t        const pclk_divisor,
                                     timer_size_t          const pclk_counts,
                                     timer_size_t          const duty_cycle)
{
    /** Initialization following flowchart in hardware manual (Figure 24.4 in NoSecurity_r01uh0488ej0040_sc32.pdf) */
    /** In one-shot mode, timer will be stopped and reset in the ISR. */
    GPT_BASE_PTR p_gpt_reg = (GPT_BASE_PTR) p_ctrl->p_reg;
    HW_GPT_ModeSet(p_gpt_reg, GPT_MODE_PERIODIC);      ///< In one-shot mode, ISR will stop and reset timer.
    HW_GPT_DirectionSet(p_gpt_reg, GPT_DIR_COUNT_UP);
    HW_GPT_DivisorSet(p_gpt_reg, pclk_divisor);
    HW_GPT_TimerCycleSet(p_gpt_reg, pclk_counts - 1);
    HW_GPT_CounterSet(p_gpt_reg, 0);

    /** Set output if requested */
    if (p_cfg->p_extend)
    {
        timer_on_gpt_cfg_t * p_ext = (timer_on_gpt_cfg_t *) p_cfg->p_extend;
        if (p_ext->gtioca.output_enabled)
        {
            gpt_set_output_pin(p_ctrl, GPT_GTIOCA, p_ext->gtioca.stop_level, p_cfg->mode, duty_cycle);
        }

        if (p_ext->gtiocb.output_enabled)
        {
            gpt_set_output_pin(p_ctrl, GPT_GTIOCB, p_ext->gtiocb.stop_level, p_cfg->mode, duty_cycle);
        }
    }

    /** Enable CPU interrupts if callback is not null.  Also enable interrupts for one shot mode.
     *  @note The GPT hardware does not support one-shot mode natively.  To support one-shot mode, the timer will be
     *  stopped and cleared using software in the ISR. */
    if ((p_cfg->p_callback) || (TIMER_MODE_ONE_SHOT == p_cfg->mode))
    {
        ssp_vector_info_t * p_vector_info;
        R_SSP_VectorInfoGet(p_ctrl->irq, &p_vector_info);
        NVIC_SetPriority(p_ctrl->irq, p_cfg->irq_ipl);
        *(p_vector_info->pp_ctrl) = p_ctrl;

        R_BSP_IrqStatusClear(p_ctrl->irq);
        NVIC_ClearPendingIRQ(p_ctrl->irq);
        NVIC_EnableIRQ(p_ctrl->irq);
    }

    /** Start the timer if requested by user */
    if (p_cfg->autostart)
    {
        HW_GPT_CounterStartStop(p_gpt_reg, GPT_START);
    }
} /* End of function gpt_hardware_initialize */

/*******************************************************************************************************************//**
 * Uses divisor to ensure period fits in 32 bits.
 *
 * @param[in]      p_ctrl           Instance control block
 * @param[in,out]  p_period_counts  GPT period in counts stored here
 * @param[out]     p_divisor        GPT divisor stored here
 *
 * @retval         SSP_SUCCESS                    Divisor is set successsfully
 * @retval         SSP_ERR_INVALID_ARGUMENT       Period counts is greater than the maximum number of clock counts
 **********************************************************************************************************************/
static ssp_err_t gpt_divisor_select(gpt_instance_ctrl_t * const p_ctrl,
                                    uint64_t            *       p_period_counts,
                                    gpt_pclk_div_t      * const p_divisor)
{
    /** temp_period now represents PCLK counts, but it could potentially overflow 32 bits.  If so, convert it here and
     *  set divisor appropriately.
     */
    gpt_pclk_div_t temp_div = GPT_PCLK_DIV_BY_1;
    uint64_t max_counts     = GPT_MAX_CLOCK_COUNTS_32;

    if (TIMER_VARIANT_16_BIT == p_ctrl->variant)
    {
        max_counts = GPT_MAX_CLOCK_COUNTS_16;
    }
    while ((*p_period_counts > max_counts) && (temp_div < GPT_PCLK_DIV_BY_1024))
    {
        *p_period_counts >>= 2;
        temp_div++;
    }

    /** If period is still too large, return error */
    GPT_ERROR_RETURN((*p_period_counts <= max_counts), SSP_ERR_INVALID_ARGUMENT);

    *p_divisor = temp_div;

    return SSP_SUCCESS;
}


/*******************************************************************************************************************//**
 * Converts period from input value to PCLK counts
 *
 * @param[in]  p_ctrl        Instance control block.
 * @param[in]  period        Period configured by user.
 * @param[in]  unit          Period unit configured by user.
 * @param[out] p_period_pclk Period in clock counts set here.
 * @param[out] p_divisor     Clock divisor set here.
 *
 * @retval     SSP_SUCCESS          Successful conversion
 * @retval     SSP_ERR_INVALID_ARGUMENT  One of the following is invalid:
 *                                    - p_period->unit: must be one of the options from timer_unit_t
 *                                    - p_period->value: must result in a period in the following range:
 *                                        - Lower bound: (1 / (PCLK frequency))
 *                                        - Upper bound: (0xFFFFFFFF * 1024 / (PCLK frequency))
 **********************************************************************************************************************/
static ssp_err_t gpt_period_to_pclk (gpt_instance_ctrl_t * const p_ctrl,
                                     timer_size_t          const period,
                                     timer_unit_t          const unit,
                                     timer_size_t        * const p_period_pclk,
                                     gpt_pclk_div_t      * const p_divisor)
{
    uint64_t temp_period = period;
    ssp_err_t err = SSP_SUCCESS;

    /** Read the current PCLK frequency from the clock module. */
    uint32_t pclk_freq_hz = 0;
    g_cgc_on_cgc.systemClockFreqGet(CGC_SYSTEM_CLOCKS_PCLKD, &pclk_freq_hz);

    /** Convert period to PCLK counts so it can be set in hardware. */
    switch (unit)
    {
        case TIMER_UNIT_PERIOD_RAW_COUNTS:
            temp_period = period;
            break;
        case TIMER_UNIT_FREQUENCY_KHZ:
            temp_period = (pclk_freq_hz * 1ULL) / (1000 * period);
            break;
        case TIMER_UNIT_FREQUENCY_HZ:
            temp_period = (pclk_freq_hz * 1ULL) / period;
            break;
        case TIMER_UNIT_PERIOD_NSEC:
            temp_period = (period * (pclk_freq_hz * 1ULL)) / 1000000000;
            break;
        case TIMER_UNIT_PERIOD_USEC:
            temp_period = (period * (pclk_freq_hz * 1ULL)) / 1000000;
            break;
        case TIMER_UNIT_PERIOD_MSEC:
            temp_period = (period * (pclk_freq_hz * 1ULL)) / 1000;
            break;
        case TIMER_UNIT_PERIOD_SEC:
            temp_period = period * (pclk_freq_hz * 1ULL);
            break;
        default:
            err = SSP_ERR_INVALID_ARGUMENT;
            break;
    }
    GPT_ERROR_RETURN(SSP_SUCCESS == err, err);

    err = gpt_divisor_select(p_ctrl, &temp_period, p_divisor);
    GPT_ERROR_RETURN(SSP_SUCCESS == err, err);

    /** If the period is valid, return to caller. */
    *p_period_pclk = (timer_size_t) temp_period;

    return SSP_SUCCESS;
} /* End of function gpt_period_to_pclk */

/*******************************************************************************************************************//**
 * Converts user input duty cycle to PCLK counts, then sets duty cycle value.
 *
 * @param[in]  p_ctrl  Instance Control
 * @param[in]  duty_cycle_counts  Duty cycle to set.
 * @param[in]  pin                Which pin to update (0 = A or 1 = B).
 *
 * @retval     SSP_SUCCESS          Delay and divisor were set successfully.
 * @retval     SSP_ERR_INVALID_ARGUMENT  One of the following is invalid:
 *                                    - p_period->unit: must be one of the options from timer_pwm_unit_t
 *                                    - p_period->value: must result in a period in the following range:
 *                                        - Lower bound: (1 / (PCLK frequency))
 *                                        - Upper bound: (0xFFFFFFFF * 1024 / (PCLK frequency))
 **********************************************************************************************************************/
static void gpt_set_duty_cycle (gpt_instance_ctrl_t * const p_ctrl,
                                timer_size_t          const duty_cycle_counts,
                                uint8_t               const pin)
{
    /* Pin is represented by gpt_gtioc_t internally for readability. */
    GPT_BASE_PTR p_gpt_reg = (GPT_BASE_PTR) p_ctrl->p_reg;
    if (0U == duty_cycle_counts)
    {
        HW_GPT_DutyCycleModeSet(p_gpt_reg, (gpt_gtioc_t) pin, GPT_DUTY_CYCLE_MODE_0_PERCENT);
    }
    else if (GPT_MAX_CLOCK_COUNTS_32 == duty_cycle_counts)
    {
        HW_GPT_DutyCycleModeSet(p_gpt_reg, (gpt_gtioc_t) pin, GPT_DUTY_CYCLE_MODE_100_PERCENT);
    }
    else
    {
        HW_GPT_DutyCycleModeSet(p_gpt_reg, (gpt_gtioc_t) pin, GPT_DUTY_CYCLE_MODE_REGISTER);
        HW_GPT_CompareMatchSet(p_gpt_reg, (gpt_gtioc_t) pin, duty_cycle_counts);
    }
} /* End of function gpt_set_duty_cycle */

/*******************************************************************************************************************//**
 * Lookup function for clock frequency of GPT counter.  Divides GPT clock by GPT clock divisor.
 *
 * @param[in]  p_ctrl     Instance control block
 *
 * @return     Clock frequency of the GPT counter.
 **********************************************************************************************************************/
static uint32_t gpt_clock_frequency_get(gpt_instance_ctrl_t * const p_ctrl)
{
    GPT_BASE_PTR p_gpt_reg = (GPT_BASE_PTR) p_ctrl->p_reg;
    uint32_t pclk_freq_hz  = 0;
    gpt_pclk_div_t pclk_divisor = HW_GPT_DivisorGet(p_gpt_reg);
    uint32_t divisor = 1U << (2 * (uint32_t) pclk_divisor);
    g_cgc_on_cgc.systemClockFreqGet(CGC_SYSTEM_CLOCKS_PCLKD, &pclk_freq_hz);
    return (pclk_freq_hz / divisor);
}

/*******************************************************************************************************************//**
 * Converts duty cycle from input value to PCLK counts
 *
 * @param[in]  shortest_pwm_signal  Shortest PWM signal to generate
 * @param[in]  duty_cycle         Duty cycle configured by user.
 * @param[in]  unit               Duty cycle unit configured by user.
 * @param[in]  period             Period in GPT clock counts.
 * @param[out] p_duty_cycle_pclk  Duty cycle in clock counts set here.
 *
 * @retval     SSP_SUCCESS          Successful conversion
 * @retval     SSP_ERR_INVALID_ARGUMENT  One of the following is invalid:
 *                                    - unit: must be one of the options from timer_pwm_unit_t
 *                                    - duty_cycle: must result in a period in the following range:
 *                                        - Lower bound: (1 / (PCLK frequency))
 *                                        - Upper bound: period
 **********************************************************************************************************************/
static ssp_err_t gpt_duty_cycle_to_pclk (gpt_shortest_level_t  shortest_pwm_signal,
                                         timer_size_t     const duty_cycle,
                                         timer_pwm_unit_t const unit,
                                         timer_size_t     const period,
                                         timer_size_t   * const p_duty_cycle_pclk)
{
    /** Initially set to an invalid value */
    uint64_t temp_duty_cycle = 0xFFFFFFFEULL;
    ssp_err_t err = SSP_SUCCESS;

    /** Convert duty cycle to PCLK counts so it can be set in hardware. */
    switch (unit)
    {
        case TIMER_PWM_UNIT_RAW_COUNTS:
            temp_duty_cycle = duty_cycle;
            break;
        case TIMER_PWM_UNIT_PERCENT:
            temp_duty_cycle = ((uint64_t) period * duty_cycle) / 100ULL;
            break;
        case TIMER_PWM_UNIT_PERCENT_X_1000:
            temp_duty_cycle = ((uint64_t) period * duty_cycle) / 100000ULL;
            break;
        default:
            err = SSP_ERR_INVALID_ARGUMENT;
            break;
    }
    GPT_ERROR_RETURN(SSP_SUCCESS == err, err);

    if(GPT_SHORTEST_LEVEL_ON == shortest_pwm_signal)
    {
        temp_duty_cycle = (uint64_t)period - temp_duty_cycle - 1ULL;
    }

    if (temp_duty_cycle == period)
    {
        /** If duty cycle and period are equal, set to maximum period to use 100% duty cycle setting. */
        temp_duty_cycle = GPT_MAX_CLOCK_COUNTS_32;
    }

    /** If duty_cycle is valid, set it.  Otherwise return error */
    GPT_ERROR_RETURN(((GPT_MAX_CLOCK_COUNTS_32 == temp_duty_cycle) || (temp_duty_cycle <= period)), SSP_ERR_INVALID_ARGUMENT);
    *p_duty_cycle_pclk = (timer_size_t) temp_duty_cycle;

    return SSP_SUCCESS;
} /* End of function gpt_duty_cycle_to_pclk */

/*******************************************************************************************************************//**
 * Sets output pin to toggle at cycle end.
 *
 * @param[in]  p_ctrl      Instance control block
 * @param[in]  gtio        Which pin to toggle
 * @param[in]  level       Output level after timer stops
 * @param[in]  mode        PWM or timer interface
 * @param[in]  duty_cycle  Initial duty cycle
 **********************************************************************************************************************/
static void gpt_set_output_pin (gpt_instance_ctrl_t * const p_ctrl,
                                gpt_gtioc_t           const gtio,
                                gpt_pin_level_t       const level,
                                timer_mode_t          const mode,
                                timer_size_t          const duty_cycle)
{
    /** When the counter starts, set the output pin to the default state */
    GPT_BASE_PTR p_gpt_reg = (GPT_BASE_PTR) p_ctrl->p_reg;
    gtior_t * p_reg = HW_GPT_GTIOCRegLookup(p_gpt_reg, gtio);
    HW_GPT_GTIOCInitialOutputSet(p_reg, level);

    /** Configure pin to toggle at each overflow */
    HW_GPT_GTIOCPinLevelStoppedSet(p_reg, level);
    HW_GPT_GTIOCCycleEndOutputSet(p_reg, GPT_OUTPUT_TOGGLED);

    if (TIMER_MODE_ONE_SHOT == mode)
    {
        HW_GPT_InitialCompareMatchSet(p_gpt_reg, gtio, 0);
        HW_GPT_SingleBufferEnable(p_gpt_reg, gtio);

        /** Compare match register must be loaded with a value that exceeds the timer cycle end value so that second compare match event
         * would never occur and hence there will be only a single pulse*/
        if (TIMER_VARIANT_16_BIT == p_ctrl->variant)
        {
            HW_GPT_CompareMatchSet(p_gpt_reg, gtio, GPT_MAX_CLOCK_COUNTS_16);
        }
        else
        {
            HW_GPT_CompareMatchSet(p_gpt_reg, gtio, GPT_MAX_CLOCK_COUNTS_32);
        }
        if (0 == level)
        {
            HW_GPT_GTIOCCompareMatchOutputSet(p_reg, GPT_OUTPUT_HIGH);
            HW_GPT_GTIOCCycleEndOutputSet(p_reg, GPT_OUTPUT_LOW);
        }
        else
        {
            HW_GPT_GTIOCCompareMatchOutputSet(p_reg, GPT_OUTPUT_LOW);
            HW_GPT_GTIOCCycleEndOutputSet(p_reg, GPT_OUTPUT_HIGH);
        }
    }
    else if (TIMER_MODE_PERIODIC == mode)
    {
        HW_GPT_GTIOCCompareMatchOutputSet(p_reg, GPT_OUTPUT_RETAINED);
    }
    else // (TIMER_MODE_PWM == mode)
    {
        if (GPT_SHORTEST_LEVEL_OFF == p_ctrl->shortest_pwm_signal)
        {
            HW_GPT_GTIOCCompareMatchOutputSet(p_reg, GPT_OUTPUT_LOW);
            HW_GPT_GTIOCCycleEndOutputSet(p_reg, GPT_OUTPUT_HIGH);
        }
        else
        {
            HW_GPT_GTIOCCompareMatchOutputSet(p_reg, GPT_OUTPUT_HIGH);
            HW_GPT_GTIOCCycleEndOutputSet(p_reg, GPT_OUTPUT_LOW);
        }
        HW_GPT_SingleBufferEnable(p_gpt_reg, gtio);
        gpt_set_duty_cycle(p_ctrl, duty_cycle, gtio);
    }

    HW_GPT_GTIOCPinOutputEnable(p_reg, true);
} /* End of function gpt_set_output_pin */

/*******************************************************************************************************************//**
 * Stops the timer if one-shot mode, clears interrupts, and calls callback if one was provided in the open function.
 **********************************************************************************************************************/
void gpt_counter_overflow_isr (void)
{
    /* Save context if RTOS is used */
    SF_CONTEXT_SAVE;

    ssp_vector_info_t * p_vector_info = NULL;
    R_SSP_VectorInfoGet(R_SSP_CurrentIrqGet(), &p_vector_info);
    gpt_instance_ctrl_t * p_ctrl = (gpt_instance_ctrl_t *) *(p_vector_info->pp_ctrl);

    /** Clear pending IRQ to make sure it doesn't fire again after exiting */
    R_BSP_IrqStatusClear(R_SSP_CurrentIrqGet());

    if (NULL != p_ctrl)
    {
        GPT_BASE_PTR p_gpt_reg = (GPT_BASE_PTR) p_ctrl->p_reg;

        /** If one-shot mode is selected, stop the timer since period has expired. */
        if (p_ctrl->one_shot)
        {
            HW_GPT_CounterStartStop(p_gpt_reg, GPT_STOP);

            /** Clear the GPT counter, compare match registers and the overflow flag after the one shot pulse has being generated */
            HW_GPT_CounterSet(p_gpt_reg, 0);

            if (p_ctrl->gtioca_output_enabled)
            {
                HW_GPT_InitialCompareMatchSet(p_gpt_reg, GPT_GTIOCA, 0);
            }
            if (p_ctrl->gtiocb_output_enabled)
            {
                HW_GPT_InitialCompareMatchSet(p_gpt_reg, GPT_GTIOCB, 0);
            }

            HW_GPT_InterruptClear(p_gpt_reg);
        }

        if (NULL != p_ctrl->p_callback)
        {
            /** Set data to identify callback to user, then call user callback. */
            timer_callback_args_t cb_data;
            cb_data.p_context = p_ctrl->p_context;
            cb_data.event     = TIMER_EVENT_EXPIRED;
            p_ctrl->p_callback(&cb_data);
        }
    }

    /* Restore context if RTOS is used */
    SF_CONTEXT_RESTORE;
} /* End of function gpt_counter_overflow_isr */
