/***********************************************************************************************************************
 * Copyright [2020-2022] Renesas Electronics Corporation and/or its affiliates.  All Rights Reserved.
 *
 * This software and documentation are supplied by Renesas Electronics America Inc. and may only be used with products
 * of Renesas Electronics Corp. and its affiliates ("Renesas").  No other uses are authorized.  Renesas products are
 * sold pursuant to Renesas terms and conditions of sale.  Purchasers are solely responsible for the selection and use
 * of Renesas products and Renesas assumes no liability.  No license, express or implied, to any intellectual property
 * right is granted by Renesas. This software is protected under all applicable laws, including copyright laws. Renesas
 * reserves the right to change or discontinue this software and/or this documentation. THE SOFTWARE AND DOCUMENTATION
 * IS DELIVERED TO YOU "AS IS," AND RENESAS MAKES NO REPRESENTATIONS OR WARRANTIES, AND TO THE FULLEST EXTENT
 * PERMISSIBLE UNDER APPLICABLE LAW, DISCLAIMS ALL WARRANTIES, WHETHER EXPLICITLY OR IMPLICITLY, INCLUDING WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NONINFRINGEMENT, WITH RESPECT TO THE SOFTWARE OR
 * DOCUMENTATION.  RENESAS SHALL HAVE NO LIABILITY ARISING OUT OF ANY SECURITY VULNERABILITY OR BREACH.  TO THE MAXIMUM
 * EXTENT PERMITTED BY LAW, IN NO EVENT WILL RENESAS BE LIABLE TO YOU IN CONNECTION WITH THE SOFTWARE OR DOCUMENTATION
 * (OR ANY PERSON OR ENTITY CLAIMING RIGHTS DERIVED FROM YOU) FOR ANY LOSS, DAMAGES, OR CLAIMS WHATSOEVER, INCLUDING,
 * WITHOUT LIMITATION, ANY DIRECT, CONSEQUENTIAL, SPECIAL, INDIRECT, PUNITIVE, OR INCIDENTAL DAMAGES; ANY LOST PROFITS,
 * OTHER ECONOMIC DAMAGE, PROPERTY DAMAGE, OR PERSONAL INJURY; AND EVEN IF RENESAS HAS BEEN ADVISED OF THE POSSIBILITY
 * OF SUCH LOSS, DAMAGES, CLAIMS OR COSTS.
 **********************************************************************************************************************/

#ifndef R_CGC_API_H
#define R_CGC_API_H

/*******************************************************************************************************************//**
 * @ingroup RENESAS_INTERFACES
 * @defgroup CGC_API CGC Interface
 * @brief Interface for clock generation.
 *
 * @section CGC_API_SUMMARY Summary
 *
 * The CGC interface provides the ability to configure and use all of the CGC module's capabilities. Among the
 * capabilities is the selection of several clock sources to use as the system clock source. Additionally, the
 * system clocks can be divided down to provide a wide range of frequencies for various system and peripheral needs.
 *
 * Clock stability can be checked and clocks may also be stopped to save power when not needed. The API has a function
 * to return the frequency of the system and system peripheral clocks at run time. There is also a feature to detect
 * when the main oscillator has stopped, with the option of calling a user provided callback function.
 *
 * The CGC interface is implemented by:
 * - @ref CGC
 *
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

/* Includes board and MCU related header files. */
#include "bsp_api.h"

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/** Events that can trigger a callback function */
typedef enum e_cgc_event
{
    CGC_EVENT_OSC_STOP_DETECT          ///< Oscillator stop detection has caused the event
} cgc_event_t;

/** Callback function parameter data */
typedef struct st_cgc_callback_args
{
    cgc_event_t  event;                ///< The event can be used to identify what caused the callback
    void const * p_context;            ///< Placeholder for user data
} cgc_callback_args_t;

/** System clock source identifiers -  The source of ICLK, BCLK, FCLK, PCLKS A-D and UCLK prior to the system clock
 * divider */
typedef enum e_cgc_clock
{
    CGC_CLOCK_HOCO     = 0,            ///< The high speed on chip oscillator
    CGC_CLOCK_MOCO     = 1,            ///< The middle speed on chip oscillator
    CGC_CLOCK_LOCO     = 2,            ///< The low speed on chip oscillator
    CGC_CLOCK_MAIN_OSC = 3,            ///< The main oscillator
    CGC_CLOCK_SUBCLOCK = 4,            ///< The subclock oscillator
    CGC_CLOCK_PLL      = 5,            ///< The PLL oscillator
    CGC_CLOCK_PLL2     = 6,            ///< The PLL2 oscillator
} cgc_clock_t;

/** PLL divider values */
typedef enum e_cgc_pll_div
{
    CGC_PLL_DIV_1 = 0,                 ///< PLL divider of 1
    CGC_PLL_DIV_2 = 1,                 ///< PLL divider of 2
    CGC_PLL_DIV_3 = 2,                 ///< PLL divider of 3 (S7, S5 only)
    CGC_PLL_DIV_4 = 3,                 ///< PLL divider of 4 (S3 only)
} cgc_pll_div_t;

/** PLL multiplier values */
typedef enum e_cgc_pll_mul
{
    CGC_PLL_MUL_8_0  = 0xF,            ///< PLL multiplier of 8.0
    CGC_PLL_MUL_9_0  = 0x11,           ///< PLL multiplier of 9.0
    CGC_PLL_MUL_10_0 = 0x13,           ///< PLL multiplier of 10.0
    CGC_PLL_MUL_10_5 = 0x14,           ///< PLL multiplier of 10.5
    CGC_PLL_MUL_11_0 = 0x15,           ///< PLL multiplier of 11.0
    CGC_PLL_MUL_11_5 = 0x16,           ///< PLL multiplier of 11.5
    CGC_PLL_MUL_12_0 = 0x17,           ///< PLL multiplier of 12.0
    CGC_PLL_MUL_12_5 = 0x18,           ///< PLL multiplier of 12.5
    CGC_PLL_MUL_13_0 = 0x19,           ///< PLL multiplier of 13.0
    CGC_PLL_MUL_13_5 = 0x1A,           ///< PLL multiplier of 13.5
    CGC_PLL_MUL_14_0 = 0x1B,           ///< PLL multiplier of 14.0
    CGC_PLL_MUL_14_5 = 0x1D,           ///< PLL multiplier of 14.5
    CGC_PLL_MUL_15_0 = 0x1D,           ///< PLL multiplier of 15.0
    CGC_PLL_MUL_15_5 = 0x1E,           ///< PLL multiplier of 15.5
    CGC_PLL_MUL_16_0 = 0x1F,           ///< PLL multiplier of 16.0
    CGC_PLL_MUL_16_5 = 0x20,           ///< PLL multiplier of 16.5
    CGC_PLL_MUL_17_0 = 0x21,           ///< PLL multiplier of 17.0
    CGC_PLL_MUL_17_5 = 0x22,           ///< PLL multiplier of 17.5
    CGC_PLL_MUL_18_0 = 0x23,           ///< PLL multiplier of 18.0
    CGC_PLL_MUL_18_5 = 0x24,           ///< PLL multiplier of 18.5
    CGC_PLL_MUL_19_0 = 0x25,           ///< PLL multiplier of 19.0
    CGC_PLL_MUL_19_5 = 0x26,           ///< PLL multiplier of 19.5
    CGC_PLL_MUL_20_0 = 0x27,           ///< PLL multiplier of 20.0
    CGC_PLL_MUL_20_5 = 0x28,           ///< PLL multiplier of 20.5
    CGC_PLL_MUL_21_0 = 0x29,           ///< PLL multiplier of 21.0
    CGC_PLL_MUL_21_5 = 0x2A,           ///< PLL multiplier of 21.5
    CGC_PLL_MUL_22_0 = 0x2B,           ///< PLL multiplier of 22.0
    CGC_PLL_MUL_22_5 = 0x2C,           ///< PLL multiplier of 22.5
    CGC_PLL_MUL_23_0 = 0x2D,           ///< PLL multiplier of 23.0
    CGC_PLL_MUL_23_5 = 0x2E,           ///< PLL multiplier of 23.5
    CGC_PLL_MUL_24_0 = 0x2F,           ///< PLL multiplier of 24.0
    CGC_PLL_MUL_24_5 = 0x30,           ///< PLL multiplier of 24.5
    CGC_PLL_MUL_25_0 = 0x31,           ///< PLL multiplier of 25.0
    CGC_PLL_MUL_25_5 = 0x32,           ///< PLL multiplier of 25.5
    CGC_PLL_MUL_26_0 = 0x33,           ///< PLL multiplier of 26.0
    CGC_PLL_MUL_26_5 = 0x34,           ///< PLL multiplier of 26.5
    CGC_PLL_MUL_27_0 = 0x35,           ///< PLL multiplier of 27.0
    CGC_PLL_MUL_27_5 = 0x36,           ///< PLL multiplier of 27.5
    CGC_PLL_MUL_28_0 = 0x37,           ///< PLL multiplier of 28.0
    CGC_PLL_MUL_28_5 = 0x38,           ///< PLL multiplier of 28.5
    CGC_PLL_MUL_29_0 = 0x39,           ///< PLL multiplier of 29.0
    CGC_PLL_MUL_29_5 = 0x3A,           ///< PLL multiplier of 29.5
    CGC_PLL_MUL_30_0 = 0x3B,           ///< PLL multiplier of 30.0
    CGC_PLL_MUL_31_0 = 0x3D,           ///< PLL multiplier of 31.0
} cgc_pll_mul_t;

/** System clock divider vlues - The individually selectable divider of each of the system clocks, ICLK, BCLK, FCLK,
 * PCLKS A-D.  */
typedef enum e_cgc_sys_clock_div
{
    CGC_SYS_CLOCK_DIV_1  = 0,          ///< System clock divided by 1
    CGC_SYS_CLOCK_DIV_2  = 1,          ///< System clock divided by 2
    CGC_SYS_CLOCK_DIV_4  = 2,          ///< System clock divided by 4
    CGC_SYS_CLOCK_DIV_8  = 3,          ///< System clock divided by 8
    CGC_SYS_CLOCK_DIV_16 = 4,          ///< System clock divided by 16
    CGC_SYS_CLOCK_DIV_32 = 5,          ///< System clock divided by 32
    CGC_SYS_CLOCK_DIV_64 = 6,          ///< System clock divided by 64
} cgc_sys_clock_div_t;

/** Clock configuration structure - Used as an input parameter to the @ref cgc_api_t::clockStart function for the PLL clock. */
typedef struct st_cgc_pll_cfg
{
    cgc_clock_t   source_clock;        ///< PLL source clock (main oscillator or HOCO)
    cgc_pll_div_t divider;             ///< PLL divider
    cgc_pll_mul_t multiplier;          ///< PLL multiplier
} cgc_pll_cfg_t;

/** Clock configuration structure - Used as an input parameter to the @ref cgc_api_t::systemClockSet and @ref cgc_api_t::systemClockGet
 * functions. */
typedef union u_cgc_divider_cfg
{
    uint32_t sckdivcr_w;               ///< (@ 0x4001E020) System clock Division control register

    /* DEPRECATED: Anonymous structure. */
    struct
    {
        cgc_sys_clock_div_t pclkd_div : 3; ///< Divider value for PCLKD
        uint32_t                      : 1;
        cgc_sys_clock_div_t pclkc_div : 3; ///< Divider value for PCLKC
        uint32_t                      : 1;
        cgc_sys_clock_div_t pclkb_div : 3; ///< Divider value for PCLKB
        uint32_t                      : 1;
        cgc_sys_clock_div_t pclka_div : 3; ///< Divider value for PCLKA
        uint32_t                      : 1;
        cgc_sys_clock_div_t bclk_div  : 3; ///< Divider value for BCLK
        uint32_t                      : 5;
        cgc_sys_clock_div_t iclk_div  : 3; ///< Divider value for ICLK
        uint32_t                      : 1;
        cgc_sys_clock_div_t fclk_div  : 3; ///< Divider value for FCLK
        uint32_t                      : 1;
    };

    struct
    {
        cgc_sys_clock_div_t pclkd_div : 3; ///< Divider value for PCLKD
        uint32_t                      : 1;
        cgc_sys_clock_div_t pclkc_div : 3; ///< Divider value for PCLKC
        uint32_t                      : 1;
        cgc_sys_clock_div_t pclkb_div : 3; ///< Divider value for PCLKB
        uint32_t                      : 1;
        cgc_sys_clock_div_t pclka_div : 3; ///< Divider value for PCLKA
        uint32_t                      : 1;
        cgc_sys_clock_div_t bclk_div  : 3; ///< Divider value for BCLK
        uint32_t                      : 5;
        cgc_sys_clock_div_t iclk_div  : 3; ///< Divider value for ICLK
        uint32_t                      : 1;
        cgc_sys_clock_div_t fclk_div  : 3; ///< Divider value for FCLK
        uint32_t                      : 1;
    } sckdivcr_b;
} cgc_divider_cfg_t;

/** USB clock divider values */
typedef enum e_cgc_usb_clock_div
{
    CGC_USB_CLOCK_DIV_3 = 2,           ///< Divide USB source clock by 3
    CGC_USB_CLOCK_DIV_4 = 3,           ///< Divide USB source clock by 4
    CGC_USB_CLOCK_DIV_5 = 4,           ///< Divide USB source clock by 5
} cgc_usb_clock_div_t;

/** Clock options */
typedef enum e_cgc_clock_change
{
    CGC_CLOCK_CHANGE_START = 0,        ///< Start the clock
    CGC_CLOCK_CHANGE_STOP  = 1,        ///< Stop the clock
    CGC_CLOCK_CHANGE_NONE  = 2,        ///< No change to the clock
} cgc_clock_change_t;

/** CGC control block.  Allocate an instance specific control block to pass into the CGC API calls.
 * @par Implemented as
 * - cgc_instance_ctrl_t
 */
typedef void cgc_ctrl_t;

/** Configuration options. */
typedef struct s_cgc_cfg
{
    void (* p_callback)(cgc_callback_args_t * p_args);
    void const * p_context;
} cgc_cfg_t;

/** Clock configuration */
typedef struct st_cgc_clocks_cfg
{
    cgc_clock_t        system_clock;   ///< System clock source enumeration
    cgc_pll_cfg_t      pll_cfg;        ///< PLL configuration structure
    cgc_pll_cfg_t      pll2_cfg;       ///< PLL2 configuration structure
    cgc_divider_cfg_t  divider_cfg;    ///< Clock dividers structure
    cgc_clock_change_t loco_state;     ///< State of LOCO
    cgc_clock_change_t moco_state;     ///< State of MOCO
    cgc_clock_change_t hoco_state;     ///< State of HOCO
    cgc_clock_change_t mainosc_state;  ///< State of Main oscillator
    cgc_clock_change_t pll_state;      ///< State of PLL
    cgc_clock_change_t pll2_state;     ///< State of PLL2
} cgc_clocks_cfg_t;

/** CGC functions implemented at the HAL layer follow this API. */
typedef struct
{
    /** Initial configuration
     * @par Implemented as
     * - @ref R_CGC_Open()
     * @param[in]   p_ctrl         Pointer to instance control block
     * @param[in]   p_cfg          Pointer to configuration
     */
    fsp_err_t (* open)(cgc_ctrl_t * const p_ctrl, cgc_cfg_t const * const p_cfg);

    /** Configure all system clocks.
     * @par Implemented as
     * - @ref R_CGC_ClocksCfg()
     * @param[in]   p_ctrl         Pointer to instance control block
     * @param[in]   p_clock_cfg    Pointer to desired configuration of system clocks
     */
    fsp_err_t (* clocksCfg)(cgc_ctrl_t * const p_ctrl, cgc_clocks_cfg_t const * const p_clock_cfg);

    /** Start a clock.
     * @par Implemented as
     * - @ref R_CGC_ClockStart()
     * @param[in]   p_ctrl         Pointer to instance control block
     * @param[in]   clock_source   Clock source to start
     * @param[in]   p_pll_cfg      Pointer to PLL configuration, can be NULL if clock_source is not CGC_CLOCK_PLL or
     *                             CGC_CLOCK_PLL2
     */
    fsp_err_t (* clockStart)(cgc_ctrl_t * const p_ctrl, cgc_clock_t clock_source,
                             cgc_pll_cfg_t const * const p_pll_cfg);

    /** Stop a clock.
     * @par Implemented as
     * - @ref R_CGC_ClockStop()
     * @param[in]   p_ctrl         Pointer to instance control block
     * @param[in]   clock_source   The clock source to stop
     */
    fsp_err_t (* clockStop)(cgc_ctrl_t * const p_ctrl, cgc_clock_t clock_source);

    /** Check the stability of the selected clock.
     * @par Implemented as
     * - @ref R_CGC_ClockCheck()
     * @param[in]   p_ctrl         Pointer to instance control block
     * @param[in]   clock_source   Which clock source to check for stability
     */
    fsp_err_t (* clockCheck)(cgc_ctrl_t * const p_ctrl, cgc_clock_t clock_source);

    /** Set the system clock.
     * @par Implemented as
     * - @ref R_CGC_SystemClockSet()
     * @param[in]   p_ctrl         Pointer to instance control block
     * @param[in]   clock_source   Clock source to set as system clock
     * @param[in]   p_divider_cfg  Pointer to the clock divider configuration
     */
    fsp_err_t (* systemClockSet)(cgc_ctrl_t * const p_ctrl, cgc_clock_t clock_source,
                                 cgc_divider_cfg_t const * const p_divider_cfg);

    /** Get the system clock information.
     * @par Implemented as
     * - @ref R_CGC_SystemClockGet()
     * @param[in]   p_ctrl          Pointer to instance control block
     * @param[out]  p_clock_source  Returns the current system clock
     * @param[out]  p_divider_cfg   Returns the current system clock dividers
     */
    fsp_err_t (* systemClockGet)(cgc_ctrl_t * const p_ctrl, cgc_clock_t * const p_clock_source,
                                 cgc_divider_cfg_t * const p_divider_cfg);

    /** Enable and optionally register a callback for Main Oscillator stop detection.
     * @par Implemented as
     * - @ref R_CGC_OscStopDetectEnable()
     * @param[in]   p_ctrl       Pointer to instance control block
     * @param[in]   p_callback   Callback function that will be called by the NMI interrupt when an oscillation stop is
     *                           detected. If the second argument is "false", then this argument can be NULL.
     * @param[in]   enable       Enable/disable Oscillation Stop Detection
     */
    fsp_err_t (* oscStopDetectEnable)(cgc_ctrl_t * const p_ctrl);

    /** Disable Main Oscillator stop detection.
     * @par Implemented as
     * - @ref R_CGC_OscStopDetectDisable()
     * @param[in]   p_ctrl          Pointer to instance control block
     */
    fsp_err_t (* oscStopDetectDisable)(cgc_ctrl_t * const p_ctrl);

    /** Clear the oscillator stop detection flag.
     * @par Implemented as
     * - @ref R_CGC_OscStopStatusClear()
     * @param[in]   p_ctrl          Pointer to instance control block
     */
    fsp_err_t (* oscStopStatusClear)(cgc_ctrl_t * const p_ctrl);

    /**
     * Specify callback function and optional context pointer and working memory pointer.
     * @par Implemented as
     * - R_CGC_CallbackSet()
     *
     * @param[in]   p_ctrl                   Pointer to the CGC control block.
     * @param[in]   p_callback               Callback function
     * @param[in]   p_context                Pointer to send to callback function
     * @param[in]   p_working_memory         Pointer to volatile memory where callback structure can be allocated.
     *                                       Callback arguments allocated here are only valid during the callback.
     */
    fsp_err_t (* callbackSet)(cgc_ctrl_t * const p_api_ctrl, void (* p_callback)(cgc_callback_args_t *),
                              void const * const p_context, cgc_callback_args_t * const p_callback_memory);

    /** Close the CGC driver.
     * @par Implemented as
     * - @ref R_CGC_Close()
     * @param[in]   p_ctrl          Pointer to instance control block
     */
    fsp_err_t (* close)(cgc_ctrl_t * const p_ctrl);
} cgc_api_t;

/** This structure encompasses everything that is needed to use an instance of this interface. */
typedef struct st_cgc_instance
{
    cgc_ctrl_t      * p_ctrl;          ///< Pointer to the control structure for this instance
    cgc_cfg_t const * p_cfg;           ///< Pointer to the configuration structure for this instance
    cgc_api_t const * p_api;           ///< Pointer to the API structure for this instance
} cgc_instance_t;

/*******************************************************************************************************************//**
 * @} (end defgroup CGC_API)
 **********************************************************************************************************************/

/* Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER

#endif                                 // R_CGC_API_H
