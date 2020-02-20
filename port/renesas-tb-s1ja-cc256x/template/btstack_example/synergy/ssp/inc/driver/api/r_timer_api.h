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
 * File Name    : r_timer_api.h
 * Description  : General timer API.  Allows for periodic interrupt creation with a callback function.
 **********************************************************************************************************************/

#ifndef DRV_TIMER_API_H
#define DRV_TIMER_API_H

/*******************************************************************************************************************//**
 * @ingroup Interface_Library
 * @defgroup TIMER_API Timer Interface
 * @brief Interface for timer functions.
 *
 * @section TIMER_API_SUMMARY Summary
 * The general timer interface provides standard timer functionality including periodic mode, one-shot mode, and
 * free-running timer mode.  After each timer cycle (overflow or underflow), an interrupt can be triggered.
 *
 * If an instance supports output compare mode, it is provided in the extension configuration
 * timer_on_<instance>_cfg_t defined in r_<instance>.h.
 *
 * Implemented by:
 * - @ref GPT
 * - @ref AGT
 *
 * See also:
 * @ref INPUT_CAPTURE_API
 *
 * Related SSP architecture topics:
 *  - @ref ssp-interfaces
 *  - @ref ssp-predefined-layers
 *  - @ref using-ssp-modules
 *
 * Timer Interface description: @ref HALAGTInterface and @ref HALGPTInterface
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
/* Includes board and MCU related header files. */
#include "bsp_api.h"

/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER

/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/* Leading zeroes removed to avoid coding standard violation. */
#define TIMER_API_VERSION_MAJOR (1U)
#define TIMER_API_VERSION_MINOR (5U)

/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/** Largest supported timer size. Currently up to 32-bit timers are supported.  If a 16-bit timer is used, only the
 *  bottom 16 bits of any timer_size_t parameter can be used.  Passing in values larger than 16 bits would result in an
 *  error. */
typedef uint32_t timer_size_t;

/** Events that can trigger a callback function */
typedef enum e_timer_event
{
    TIMER_EVENT_EXPIRED          ///< Requested timer delay has expired or timer has wrapped around.
} timer_event_t;

/** Timer variant types. */
typedef enum e_timer_variant
{
    TIMER_VARIANT_32_BIT,        ///< 32-bit timer
    TIMER_VARIANT_16_BIT         ///< 16-bit timer
} timer_variant_t;

/** Callback function parameter data */
typedef struct st_timer_callback_args
{
    /** Placeholder for user data.  Set in timer_api_t::open function in ::timer_cfg_t. */
    void const   * p_context;
    timer_event_t  event;        ///< The event can be used to identify what caused the callback (overflow or error).
} timer_callback_args_t;

/** Timer control block.  Allocate an instance specific control block to pass into the timer API calls.
 * @par Implemented as
 * - gpt_instance_ctrl_t
 * - agt_instance_ctrl_t
 */
typedef void timer_ctrl_t;

/** Possible status values returned by timer_api_t::infoGet. */
typedef enum e_timer_status
{
    TIMER_STATUS_COUNTING,   ///< Timer is running
    TIMER_STATUS_STOPPED     ///< Timer is stopped
} timer_status_t;

/** Timer operational modes */
typedef enum e_timer_mode
{
    TIMER_MODE_PERIODIC,   ///< Timer will restart after delay periods.
    TIMER_MODE_ONE_SHOT,   ///< Timer will stop after delay periods.
    TIMER_MODE_PWM         ///< Timer generate PWM output.
} timer_mode_t;

/** Units of timer period value. */
typedef enum e_timer_unit
{
    TIMER_UNIT_PERIOD_RAW_COUNTS,  ///< Period in clock counts
    TIMER_UNIT_PERIOD_NSEC,        ///< Period in nanoseconds
    TIMER_UNIT_PERIOD_USEC,        ///< Period in microseconds
    TIMER_UNIT_PERIOD_MSEC,        ///< Period in milliseconds
    TIMER_UNIT_PERIOD_SEC,         ///< Period in seconds
    TIMER_UNIT_FREQUENCY_HZ,       ///< Frequency in Hz
    TIMER_UNIT_FREQUENCY_KHZ       ///< Frequency in kHz
} timer_unit_t;

/** Units of timer duty cycle value. */
typedef enum e_timer_pwm_unit
{
    TIMER_PWM_UNIT_RAW_COUNTS,     ///< Period in clock counts
    TIMER_PWM_UNIT_PERCENT,        ///< Percent unit used for duty cycle
    TIMER_PWM_UNIT_PERCENT_X_1000, ///< Percent multiplied by 1000 for extra resolution
} timer_pwm_unit_t;

/** Direction of timer count */
typedef enum e_timer_direction
{
    TIMER_DIRECTION_DOWN = 0,      ///< Timer count goes up
    TIMER_DIRECTION_UP   = 1       ///< Timer count goes down
} timer_direction_t;

/** DEPRECATED: Recommend using timer_size_t for period. */
typedef timer_size_t timer_period_t;

/** Timer information structure to store various information for a timer resource */
typedef struct st_timer_info
{
    timer_direction_t    count_direction;   ///< Clock counting direction of the timer resource.
    uint32_t             clock_frequency;   ///< Clock frequency of the timer resource.
    timer_size_t         period_counts;     ///< Time in clock counts until timer will expire.
    timer_status_t       status;
    elc_event_t          elc_event;         ///< ELC event associated with the count operation of the timer resource.
} timer_info_t;

/** User configuration structure, used in open function */
typedef struct st_timer_cfg
{
    timer_mode_t     mode;                   ///< Select enumerated value from ::timer_mode_t

    /** Defines when the timer should expire. For a free running counter, set to TIMER_MAX_CLOCK with unit
     *::TIMER_UNIT_PERIOD_RAW_COUNTS */
    uint32_t         period;
    timer_unit_t     unit;                   ///< Units of timer_cfg_t::period
    timer_size_t     duty_cycle;             ///< Duty cycle in units timer_cfg_t::duty_cycle_unit
    timer_pwm_unit_t duty_cycle_unit;        ///< Units of timer_cfg_t::duty_cycle

    /** Select a channel corresponding to the channel number of the hardware. */
    uint8_t  channel;
    uint8_t  irq_ipl;                        ///< Timer interrupt priority

    /** Whether to start during Open call or not.  True: Start during open. False: Don't start during open. */
    bool     autostart;

    /** Callback provided when a timer ISR occurs.  Set to NULL for no CPU interrupt. */
    void  (* p_callback)(timer_callback_args_t * p_args);

    /** Placeholder for user data.  Passed to the user callback in ::timer_callback_args_t. */
    void const  * p_context;
    void const  * p_extend;                               ///< Extension parameter for hardware specific settings.
} timer_cfg_t;

/** Timer API structure. General timer functions implemented at the HAL layer follow this API. */
typedef struct st_timer_api
{
    /** Initial configuration.
     * @par Implemented as
     * - R_GPT_TimerOpen()
     * - R_AGT_TimerOpen()
     *
     * @note To reconfigure after calling this function, call timer_api_t::close first.
     * @param[in]   p_ctrl     Pointer to control block. Must be declared by user. Elements set here.
     * @param[in]   p_cfg      Pointer to configuration structure. All elements of this structure must be set by user.
     */
    ssp_err_t (* open)(timer_ctrl_t      * const p_ctrl,
                       timer_cfg_t const * const p_cfg);

    /** Stop the counter.
     * @par Implemented as
     * - R_GPT_Stop()
     * - R_AGT_Stop()
     *
     * @param[in]   p_ctrl     Control block set in timer_api_t::open call for this timer.
     */
    ssp_err_t (* stop)(timer_ctrl_t      * const p_ctrl);

    /** Start the counter.
     * @par Implemented as
     * - R_GPT_Start()
     * - R_AGT_Start()
     *
     * @note The counter can also be started in the timer_api_t::open function if timer_cfg_t::autostart is true.
     * @param[in]   p_ctrl     Control block set in timer_api_t::open call for this timer.
     */
    ssp_err_t (* start)(timer_ctrl_t      * const p_ctrl);

    /** Reset the counter to the initial value.
     * @par Implemented as
     * - R_GPT_Reset()
     * - R_AGT_Reset()
     *
     * @param[in]   p_ctrl     Control block set in timer_api_t::open call for this timer.
     */
    ssp_err_t (* reset)(timer_ctrl_t      * const p_ctrl);

    /** Get current counter value and store it in provided pointer p_value.
     * @par Implemented as
     * - R_GPT_CounterGet()
     * - R_AGT_CounterGet()
     *
     * @param[in]   p_ctrl     Control block set in timer_api_t::open call for this timer.
     * @param[out]  p_value    Pointer to store current counter value.
     */
    ssp_err_t (* counterGet)(timer_ctrl_t      * const p_ctrl,
                             timer_size_t      * const p_value);

    /** Set the time until the timer expires.
     * @par Implemented as
     * - R_GPT_PeriodSet()
     * - R_AGT_PeriodSet()
     *
     * @note Timer expiration may or may not generate a CPU interrupt based on how the timer is configured in
     * timer_api_t::open.
     * @param[in]   p_ctrl     Control block set in timer_api_t::open call for this timer.
     * @param[in]   period     Time until timer should expire.
     * @param[in]   unit       Units of period parameter.
     */
    ssp_err_t (* periodSet)(timer_ctrl_t * const p_ctrl,
                            timer_size_t   const period,
                            timer_unit_t   const unit);

    /** Sets the time until the duty cycle expires.
     *
     * @param[in]   p_ctrl           Control block set in timer_api_t::open call for this timer.
     * @param[in]   duty_cycle       Time until duty cycle should expire.
     * @param[in]   duty_cycle_unit  Units of duty_cycle parameter.
     * @param[in]   pin              Which output pin to update.  Enter the pin number or if pins are identified by
     *                               letters, enter 0 for A, 1 for B, 2 for C, etc.
     */
    ssp_err_t (* dutyCycleSet)(timer_ctrl_t   * const p_ctrl,
                               timer_size_t     const duty_cycle,
                               timer_pwm_unit_t const duty_cycle_unit,
                               uint8_t          const pin);

    /** Get the time until the timer expires in clock counts and store it in provided pointer p_period_counts.
     * @par Implemented as
     * - R_GPT_InfoGet()
     * - R_AGT_InfoGet()
     *
     * @param[in]   p_ctrl     Control block set in timer_api_t::open call for this timer.
     * @param[out]  p_info     Collection of information for this timer.
     */
    ssp_err_t (* infoGet)(timer_ctrl_t    * const p_ctrl,
                          timer_info_t    * const p_info);

    /** Allows driver to be reconfigured and may reduce power consumption.
     * @par Implemented as
     * - R_GPT_Close()
     * - R_AGT_Close()
     *
     * @param[in]   p_ctrl     Control block set in timer_api_t::open call for this timer.
     */
    ssp_err_t (* close)(timer_ctrl_t      * const p_ctrl);

    /** Get version and store it in provided pointer p_version.
     * @par Implemented as
     * - R_GPT_VersionGet()
     * - R_AGT_VersionGet()
     *
     * @param[out]  p_version  Code and API version used.
     */
    ssp_err_t (* versionGet)(ssp_version_t     * const p_version);
} timer_api_t;

/** This structure encompasses everything that is needed to use an instance of this interface. */
typedef struct st_timer_instance
{
    timer_ctrl_t       * p_ctrl;    ///< Pointer to the control structure for this instance
    timer_cfg_t  const * p_cfg;     ///< Pointer to the configuration structure for this instance
    timer_api_t  const * p_api;     ///< Pointer to the API structure for this instance
} timer_instance_t;


/*******************************************************************************************************************//**
 * @} (end defgroup TIMER_API)
 **********************************************************************************************************************/

/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER

#endif /* DRV_TIMER_API_H */
