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
 * File Name    : r_input_capture_api.h
 * Description  : Input Capture API. Allows sampling of input signals for pulse width.
 **********************************************************************************************************************/

#ifndef R_INPUT_CAPTURE_API_H
#define R_INPUT_CAPTURE_API_H

/*******************************************************************************************************************//**
 * @ingroup Interface_Library
 * @defgroup INPUT_CAPTURE_API Input Capture Interface
 *
 * @brief Interface for sampling input signals for pulse width.
 *
 * @section INPUT_CAPTURE_API_SUMMARY Summary
 * The input capture interface provides for sampling of input signals to determine the width of a pulse (from one edge
 * to the opposite edge). An interrupt can be triggered after each measurement is captured.
 *
 * Implemented by:
 *  - @ref GPT_INPUT_CAPTURE
 *  - @ref AGT_INPUT_CAPTURE
 *
 * See also: @ref TIMER_API
 *
 * Related SSP architecture topics:
 *  - @ref ssp-interfaces
 *  - @ref ssp-predefined-layers
 *  - @ref using-ssp-modules
 *
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
#define INPUT_CAPTURE_API_VERSION_MAJOR (1U)
#define INPUT_CAPTURE_API_VERSION_MINOR (7U)

/** Mapping of deprecated info_capture_info_t. */
#define info_capture_info_t input_capture_info_t

/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
/** Input capture operational modes */
typedef enum e_input_capture_mode
{
    INPUT_CAPTURE_MODE_PULSE_WIDTH,     ///< Measure a signal pulse width.
    INPUT_CAPTURE_MODE_PERIOD,          ///< Measure a signal Cycle period.
    INPUT_CAPTURE_MODE_PULSE_COUNT,     ///< Measure a signal event count.
} input_capture_mode_t;

/** Input capture signal edge trigger */
typedef enum e_input_capture_signal_edge
{
    INPUT_CAPTURE_SIGNAL_EDGE_RISING,   ///< The capture begins at the rising edge.
    INPUT_CAPTURE_SIGNAL_EDGE_FALLING,  ///< The capture begins at the falling edge.
} input_capture_signal_edge_t;

/** Input capture signal level, primarily used for the enable signal */
typedef enum e_input_capture_signal_level
{
    INPUT_CAPTURE_SIGNAL_LEVEL_NONE,    ///< Use this if signal_level is not applicable to a particular measurement.
    INPUT_CAPTURE_SIGNAL_LEVEL_LOW,     ///< The capture is enabled at the low level.
    INPUT_CAPTURE_SIGNAL_LEVEL_HIGH,    ///< The capture is enabled at the high level.
} input_capture_signal_level_t;

/** Specifies either a one-time or continuous measurements */
typedef enum e_input_capture_repetition
{
    INPUT_CAPTURE_REPETITION_PERIODIC,  ///< Capture continuous measurements, until explicitly stopped or disabled.
    INPUT_CAPTURE_REPETITION_ONE_SHOT,   ///< Capture a single measurement, then interrupts are disabled.
} input_capture_repetition_t;

/** Events that can trigger a callback function */
typedef enum e_input_capture_event
{
    INPUT_CAPTURE_EVENT_MEASUREMENT,    ///< A capture measurement has been captured.
    INPUT_CAPTURE_EVENT_OVERFLOW,       ///< A capture measurement overflowed the counter.
} input_capture_event_t;

/** Input capture status. */
typedef enum e_input_capture_status
{
    INPUT_CAPTURE_STATUS_IDLE,          ///< The input capture timer is idle.
    INPUT_CAPTURE_STATUS_CAPTURING,     ///< A capture measurement is in progress.
} input_capture_status_t;

/** Input capture timer variant types. */
typedef enum e_input_capture_variant
{
    INPUT_CAPTURE_VARIANT_32_BIT,        ///< 32-bit timer
	INPUT_CAPTURE_VARIANT_16_BIT         ///< 16-bit timer
} input_capture_variant_t;

/** Callback function parameter data */
typedef struct st_input_capture_callback_args
{
    uint8_t                channel;     ///< The channel being used.
    input_capture_event_t  event;       ///< The event that caused the interrupt and callback.
    uint32_t               counter;     ///< The value of the timer captured at the time of interrupt.
    uint32_t               overflows;   ///< The number of counter overflows that occurred during this measurement
    void const           * p_context;   ///< Placeholder for user data, set in input_capture_cfg_t::p_context.
} input_capture_callback_args_t;

/** Capture data */
typedef struct st_input_capture_capture
{
    uint32_t               counter;     ///< The value of the timer captured at the time of interrupt.
    uint32_t               overflows;   ///< The number of counter overflows that occurred during this measurement
} input_capture_capture_t;

/** Driver information */
typedef struct st_input_capture_info
{
    input_capture_status_t  status;      ///< Whether or not a capture is in progress
    input_capture_variant_t variant;     ///< Whether timer is 16 or 32 bits.
} input_capture_info_t;

/** Input capture control block.  Allocate an instance specific control block to pass into the input capture API calls.
 * @par Implemented as
 * - gpt_input_capture_instance_ctrl_t
 * - agt_input_capture_instance_ctrl_t
 */
typedef void input_capture_ctrl_t;

/** User configuration structure, passed to input_capture_api_t::open function */
typedef struct st_input_capture_cfg
{
    uint8_t                      channel;          ///< The channel in use.
    uint8_t                      capture_irq_ipl;  ///< Capture interrupt priority
    uint8_t                      overflow_irq_ipl; ///< Overflow interrupt priority
    input_capture_mode_t         mode;        ///< The mode of measurement to be performed.
    input_capture_signal_edge_t  edge;        ///< The triggering edge to start a measurement (rise or fall).
    input_capture_repetition_t   repetition;  ///< One-shot or periodic measurement.
    bool                         autostart;   ///< Specifies whether interrupts are enabled or not after open.

    /** REQUIRED. Pointer to peripheral-specific extension parameters.  See gpt_input_capture_extend_t for GPT. */
    void const                 * p_extend;

    /**  Pointer to user's callback function, or NULL if no interrupt desired. */
    void (*p_callback) (input_capture_callback_args_t * p_args);
    void const                 * p_context;   ///< Pointer to user's context data, to be passed to the callback
                                              // function.
} input_capture_cfg_t;

/** Input capture API structure. Functions implemented at the HAL layer will implement this API. */
typedef struct st_input_capture_api
{
    /** Initial configuration.
     * @par Implemented as
     * - R_GPT_InputCaptureOpen()
     * - R_AGT_InputCaptureOpen()
     *
     * @note To reconfigure after calling this function, call input_capture_api_t::close first.
     * @param[in]   p_ctrl   Pointer to control block: memory allocated by caller, contents filled in by open.
     * @param[in]   p_cfg    Pointer to configuration structure. All elements of this structure must be set by user.
     */
    ssp_err_t (* open)(input_capture_ctrl_t      * const p_ctrl,
                       input_capture_cfg_t const * const p_cfg);

    /** Disables input capture measurement.
     * @par Implemented as
     * - R_GPT_InputCaptureDisable()
     * - R_AGT_InputCaptureDisable()
     *
     * @param[in]  p_ctrl    Pointer to control block initialized by input_capture_api_t::open call.
     */
    ssp_err_t (* disable)(input_capture_ctrl_t const * const p_ctrl);

    /** Enables input capture measurement.
     * @par Implemented as
     * - R_GPT_InputCaptureEnable()
     * - R_AGT_InputCaptureEnable()
     *
     * @param[in]  p_ctrl    Pointer to control block initialized by input_capture_api_t::open call.
     * @note Interrupts may already be enabled if specified by input_capture_cfg_t::irq_enable.
     */
    ssp_err_t (* enable)(input_capture_ctrl_t const * const p_ctrl);

    /** Gets the status (running or not) of the measurement counter.
     * @par Implemented as
     * - R_GPT_InputCaptureInfoGet()
     * - R_AGT_InputCaptureInfoGet()
     *
     * @param[in]   p_ctrl    Pointer to control block initialized by input_capture_api_t::open call.
     * @param[out]  p_info    Pointer to returned status. Result will be one of input_capture_status_t.
     */
    ssp_err_t (* infoGet)(input_capture_ctrl_t const * const p_ctrl,
    		              input_capture_info_t       * const p_info);

    /** Gets the last captured timer/counter value
     * @par Implemented as
     * - R_GPT_InputCaptureLastCaptureGet()
     * - R_AGT_InputCaptureLastCaptureGet()
     *
     * @param[in]  p_ctrl     Pointer to control block initialized by input_capture_api_t::open call.
     * @param[out] p_counter  Pointer to location to store last captured counter value.
     */
    ssp_err_t (* lastCaptureGet)(input_capture_ctrl_t const * const p_ctrl,
                                 input_capture_capture_t    * const p_counter);

    /** Close the input capture operation. Allows driver to be reconfigured, and may reduce power consumption.
     * @par Implemented as
     * - R_GPT_InputCaptureClose()
     * - R_AGT_InputCaptureClose()
     *
     * @param[in]  p_ctrl    Pointer to control block initialized by input_capture_api_t::open call.
     */
    ssp_err_t (* close)(input_capture_ctrl_t     * const p_ctrl);

    /** Gets the version of this API and stores it in structure pointed to by p_version.
     * @par Implemented as
     * - R_GPT_InputCaptureVersionGet()
     * - R_AGT_InputCaptureVersionGet()
     *
     * @param[out]  p_version  Code and API version used.
     */
    ssp_err_t (* versionGet)(ssp_version_t   * const p_version);
} input_capture_api_t;

/** This structure encompasses everything that is needed to use an instance of this interface. */
typedef struct st_input_capture_instance
{
    input_capture_ctrl_t      * p_ctrl;    ///< Pointer to the control structure for this instance
    input_capture_cfg_t const * p_cfg;     ///< Pointer to the configuration structure for this instance
    input_capture_api_t const * p_api;     ///< Pointer to the API structure for this instance
} input_capture_instance_t;

/*******************************************************************************************************************//**
 * @} (end defgroup INPUT_CAPTURE_API)
 **********************************************************************************************************************/

/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER

#endif /* R_INPUT_CAPTURE_API_H */
