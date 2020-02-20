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
 * File Name    : r_transfer_api.h
 * Description  : Provides an API to configure background data transfers.
 **********************************************************************************************************************/

#ifndef DRV_TRANSFER_API_H
#define DRV_TRANSFER_API_H

/*******************************************************************************************************************//**
 * @ingroup Interface_Library
 * @defgroup TRANSFER_API Transfer Interface
 *
 * @brief Interface for data transfer functions.
 *
 * @section TRANSFER_API_SUMMARY Summary
 * The transfer interface supports background data transfer (no CPU intervention).
 *
 * The transfer interface can be implemented by:
 * - @ref DTC
 * - @ref DMAC
 *
 * Related SSP architecture topics:
 *  - @ref ssp-interfaces
 *  - @ref ssp-predefined-layers
 *  - @ref using-ssp-modules
 *
 * Transfer Interface description: @ref HALDTCInterface and @ref HALDMACInterface
 *
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
/* Common error codes and definitions. */
#include "bsp_api.h"
#include "r_elc_api.h"

/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER

/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define TRANSFER_API_VERSION_MAJOR (1U)
#define TRANSFER_API_VERSION_MINOR (3U)

/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
/** Transfer control block.  Allocate an instance specific control block to pass into the transfer API calls.
 * @par Implemented as
 * - dtc_instance_ctrl_t
 * - dmac_instance_ctrl_t
 */
typedef void transfer_ctrl_t;

/** Transfer mode describes what will happen when a transfer request occurs. */
typedef enum e_transfer_mode
{
    /** In normal mode, each transfer request causes a transfer of ::transfer_size_t from the source pointer to
     *  the destination pointer.  The transfer length is decremented and the source and address pointers are
     *  updated according to ::transfer_addr_mode_t.  After the transfer length reaches 0, transfer requests
     *  will not cause any further transfers. */
    TRANSFER_MODE_NORMAL = 0,

    /** Repeat mode is like normal mode, except that when the transfer length reaches 0, the pointer to the
     *  repeat area and the transfer length will be reset to their initial values.  If DMAC is used, the
     *  transfer repeats only transfer_info_t::num_blocks times.  After the transfer repeats 
     *  transfer_info_t::num_blocks times, transfer requests will not cause any further transfers.  If DTC is 
     *  used, the transfer repeats continuously (no limit to the number of repeat transfers). */
    TRANSFER_MODE_REPEAT = 1,

    /** In block mode, each transfer request causes transfer_info_t::length transfers of ::transfer_size_t.
     *  After each individual transfer, the source and destination pointers are updated according to
     *  ::transfer_addr_mode_t.  After the block transfer is complete, transfer_info_t::num_blocks is
     *  decremented.  After the transfer_info_t::num_blocks reaches 0, transfer requests will not cause any
     *  further transfers. */
    TRANSFER_MODE_BLOCK  = 2
} transfer_mode_t;

/** Transfer size specifies the size of each individual transfer.
 *  Total transfer length = transfer_size_t * transfer_length_t
 */
typedef enum e_transfer_size
{
    TRANSFER_SIZE_1_BYTE = 0,              ///< Each transfer transfers an 8-bit value
    TRANSFER_SIZE_2_BYTE = 1,              ///< Address pointer is incremented after each transfer
    TRANSFER_SIZE_4_BYTE = 2               ///< Address pointer is incremented after each transfer
} transfer_size_t;

/** Address mode specifies whether to modify (increment or decrement) pointer after each transfer. */
typedef enum e_transfer_addr_mode
{
    /** Address pointer remains fixed after each transfer. */
    TRANSFER_ADDR_MODE_FIXED       = 0,

    /** Address pointer changes as per the configured value of offset_byte. */
    TRANSFER_ADDR_MODE_OFFSET       = 1,

    /** Address pointer is incremented by associated ::transfer_size_t after each transfer. */
    TRANSFER_ADDR_MODE_INCREMENTED = 2,

    /** Address pointer is decremented by associated ::transfer_size_t after each transfer. */
    TRANSFER_ADDR_MODE_DECREMENTED = 3
} transfer_addr_mode_t;

/** Repeat area options (source or destination).  In ::TRANSFER_MODE_REPEAT, the selected pointer returns to its
 *  original value after transfer_info_t::length transfers.  In ::TRANSFER_MODE_BLOCK, the selected pointer
 *  returns to its original value after each transfer. */
typedef enum e_transfer_repeat_area
{
    /** Destination area repeated in ::TRANSFER_MODE_REPEAT or ::TRANSFER_MODE_BLOCK. */
    TRANSFER_REPEAT_AREA_DESTINATION = 0,

    /** Source area repeated in ::TRANSFER_MODE_REPEAT or ::TRANSFER_MODE_BLOCK. */
    TRANSFER_REPEAT_AREA_SOURCE      = 1
} transfer_repeat_area_t;

/** Chain transfer mode options.
 *  @note Only applies for DTC. */
typedef enum e_transfer_chain_mode
{
    /** Chain mode not used. */
    TRANSFER_CHAIN_MODE_DISABLED = 0,

    /** Switch to next transfer after a single transfer from this ::transfer_info_t. */
    TRANSFER_CHAIN_MODE_EACH     = 2,

    /** Complete the entire transfer defined in this ::transfer_info_t before chaining to next transfer. */
    TRANSFER_CHAIN_MODE_END      = 3
} transfer_chain_mode_t;

/** Interrupt options. */
typedef enum e_transfer_irq
{
    /** Interrupt occurs only after last transfer. If this transfer is chained to a subsequent transfer,
     *  the interrupt will occur only after subsequent chained transfer(s) are complete.
     *  @warning  DTC triggers the interrupt of the activation source.  Choosing TRANSFER_IRQ_END with DTC will
     *            prevent activation source interrupts until the transfer is complete. */
    TRANSFER_IRQ_END  = 0,

    /** Interrupt occurs after each transfer.
     *  @note     Not available in all HAL drivers.  See HAL driver for details.
     *  @warning  This will prevent chained transfers that would have happened after this one until the
     *            next activation source. */
    TRANSFER_IRQ_EACH = 1
} transfer_irq_t;

/** Driver specific information. */
typedef struct st_transfer_properties
{
    uint32_t transfer_length_max;  ///< Maximum number of transfers
    uint16_t transfer_length_remaining;  ///< Number of transfers remaining
    bool     in_progress;          ///< Whether or not this transfer is in progress
} transfer_properties_t;


/** This structure specifies the properties of the transfer.
 *  @warning  When using DTC, this structure corresponds to the descriptor block registers required by the DTC.
 *            The following components may be modified by the driver: p_src, p_dest, num_blocks, and length.
 *  @warning  When using DTC, do NOT reuse this structure to configure multiple transfers.  Each transfer must
 *            have a unique transfer_info_t.
 *  @warning  When using DTC, this structure must not be allocated in a temporary location.  Any instance of this
 *            structure must remain in scope until the transfer it is used for is closed.
 *  @note     When using DTC, consider placing instances of this structure in a protected section of memory. */
typedef struct st_transfer_info
{
    uint32_t : 16;
    uint32_t : 2;

    /** Select what happens to destination pointer after each transfer. */
    transfer_addr_mode_t  dest_addr_mode : 2;

    /** Select to repeat source or destination area, unused in ::TRANSFER_MODE_NORMAL. */
    transfer_repeat_area_t  repeat_area : 1;

    /** Select if interrupts should occur after each individual transfer or after the completion of all planned
     *  transfers. */
    transfer_irq_t  irq               : 1;

    /** Select when the chain transfer ends. */
    transfer_chain_mode_t  chain_mode : 2;

    uint32_t                          : 2;

    /** Select what happens to source pointer after each transfer. */
    transfer_addr_mode_t  src_addr_mode : 2;

    /** Select number of bytes to transfer at once. @see transfer_info_t::length. */
    transfer_size_t  size : 2;

    /** Select mode from ::transfer_mode_t. */
    transfer_mode_t  mode : 2;

    void const * volatile  p_src;   ///< Source pointer
    void * volatile        p_dest;  ///< Destination pointer

    /** Number of blocks to transfer when using ::TRANSFER_MODE_BLOCK (both DTC an DMAC) and
     * ::TRANSFER_MODE_REPEAT (DMAC only), unused in other modes. */
    volatile uint16_t  num_blocks;

    /** Length of each transfer.  Range limited for ::TRANSFER_MODE_BLOCK and ::TRANSFER_MODE_REPEAT,
     *  see HAL driver for details. */
    volatile uint16_t  length;
} transfer_info_t;

/** Callback function parameter data. */
typedef struct st_transfer_callback_args_t
{
    void const * p_context;  ///< Placeholder for user data.  Set in r_transfer_t::open function in ::transfer_cfg_t.
} transfer_callback_args_t;

/** Driver configuration set in transfer_api_t::open. All elements except p_extend are required and must be
 *  initialized. */
typedef struct st_transfer_cfg
{
    /** Pointer to transfer configuration options. If using chain transfer (DTC only), this can be a pointer to
     *  an array of chained transfers that will be completed in order. */
    transfer_info_t  * p_info;

    /** Select which event will trigger the transfer.
     *  @note Select ELC_EVENT_ELC_SOFTWARE_EVENT_0 or ELC_EVENT_ELC_SOFTWARE_EVENT_0 for software activation.  When
     *  using DTC, these events may only be used once each.  DMAC uses internal software start when either of these
     *  events are selected. */
    elc_event_t  activation_source;

    /** Select whether the transfer should be enabled after open. */
    bool         auto_enable;

    /** Interrupt priority level.
     *  @warning Unsupported for DTC except when ELC software events are used.  DTC transfers trigger the
     *  interrupt associated with the activation source. */
    uint8_t irq_ipl;

    /** Callback for transfer end interrupt. Set to NULL for no CPU interrupt.
     *  @warning Unsupported for DTC except when ELC software events are used.  DTC transfers trigger the
     *  interrupt associated with the activation source. */
    void (* p_callback)(transfer_callback_args_t * p_args);

    /** Placeholder for user data.  Passed to the user p_callback in ::transfer_callback_args_t. */
    void const  * p_context;
    void const  * p_extend;                               ///< Extension parameter for hardware specific settings.
} transfer_cfg_t;

/** Select whether to start single or repeated transfer with software start. */
typedef enum e_transfer_start_mode
{
    TRANSFER_START_MODE_SINGLE = 0,        ///< Software start triggers single transfer.
    TRANSFER_START_MODE_REPEAT = 1         ///< Software start transfer continues until transfer is complete.
} transfer_start_mode_t;

/** Transfer functions implemented at the HAL layer will follow this API. */
typedef struct st_transfer_api
{
    /** Initial configuration. Enables the transfer if auto_enable is true and p_src, p_dest, and length are valid.
     * Transfers can also be enabled using transfer_api_t::enable or transfer_api_t::reset.
     * @par Implemented as
     * - R_DTC_Open()
     * - R_DMAC_Open()
     *
     * @param[in,out] p_ctrl   Pointer to control block. Must be declared by user. Elements set here.
     * @param[in]     p_cfg    Pointer to configuration structure. All elements of this structure
     *                                               must be set by user.
     */
    ssp_err_t (* open)(transfer_ctrl_t      * const p_ctrl,
                       transfer_cfg_t const * const p_cfg);

    /** Reset source address pointer, destination address pointer, and/or length, keeping all other settings the same.
     * Enable the transfer if p_src, p_dest, and length are valid.
     * @par Implemented as
     * - R_DTC_Reset()
     * - R_DMAC_Reset()
     *
     * @param[in]     p_ctrl         Control block set in transfer_api_t::open call for this transfer.
     * @param[in]     p_src          Pointer to source. Set to NULL if source pointer should not change.
     * @param[in]     p_dest         Pointer to destination. Set to NULL if destination pointer should not change.
     * @param[in]     num_transfers  Transfer length in normal mode or number of blocks in block mode.  In DMAC only,
     *                               resets number of repeats (initially stored in transfer_info_t::num_blocks) in
     *                               repeat mode.  Not used in repeat mode for DTC.
     */
    ssp_err_t (* reset)(transfer_ctrl_t       * const p_ctrl,
                        void const                  * p_src,
                        void                        * p_dest,
                        uint16_t const                num_transfers);

    /** Enable transfer. Transfers occur after the activation source event (or when transfer_api_t::start is called
     * if ELC_EVENT_ELC_SOFTWARE_EVENT_0 or ELC_EVENT_ELC_SOFTWARE_EVENT_0 is chosen as activation source).
     * @par Implemented as
     * - R_DMAC_Enable()
     * - R_DTC_Enable()
     *
     * @param[in]     p_ctrl   Control block set in transfer_api_t::open call for this transfer.
     */
    ssp_err_t (* enable) (transfer_ctrl_t       * const p_ctrl);

    /** Disable transfer. Transfers do not occur after the transfer_info_t::activation source event (or when
     * transfer_api_t::start is called if ELC_EVENT_ELC_SOFTWARE_EVENT_0 or ELC_EVENT_ELC_SOFTWARE_EVENT_0 is chosen as
     * transfer_info_t::activation_source).
     * @note If a transfer is in progress, it will be completed.  Subsequent transfer requests do not cause a
     * transfer.
     * @par Implemented as
     * - R_DMAC_Disable()
     * - R_DTC_Disable()
     *
     * @param[in]     p_ctrl   Control block set in transfer_api_t::open call for this transfer.
     */
    ssp_err_t (* disable)(transfer_ctrl_t       * const p_ctrl);

    /** Start transfer in software.
     * @warning Only works if ELC_EVENT_ELC_SOFTWARE_EVENT_0 or ELC_EVENT_ELC_SOFTWARE_EVENT_0 is chosen as
     * transfer_info_t::activation_source.
     * @note DTC only supports TRANSFER_START_MODE_SINGLE.  DTC does not support TRANSFER_START_MODE_REPEAT.
     * @par Implemented as
     * - R_DMAC_Start()
     * - R_DTC_Start()
     *
     * @param[in]     p_ctrl   Control block set in transfer_api_t::open call for this transfer.
     * @param[in]     mode     Select mode from ::transfer_start_mode_t.
     */
    ssp_err_t (* start)(transfer_ctrl_t       * const p_ctrl,
                        transfer_start_mode_t         mode);

    /** Stop transfer in software. The transfer will stop after completion of the current transfer.
     * @note Not supported for DTC.
     * @note Only applies for transfers started with TRANSFER_START_MODE_REPEAT.
     * @warning Only works if ELC_EVENT_ELC_SOFTWARE_EVENT_0 or ELC_EVENT_ELC_SOFTWARE_EVENT_0 is chosen as
     * transfer_info_t::activation_source.
     * @par Implemented as
     * - R_DMAC_Stop()
     *
     * @param[in]     p_ctrl   Control block set in transfer_api_t::open call for this transfer.
     */
    ssp_err_t (* stop)(transfer_ctrl_t       * const p_ctrl);

    /** Provides information about this transfer.
     * @par Implemented as
     * - R_DTC_InfoGet()
     * - R_DMAC_InfoGet()
     *
     * @param[in]     p_ctrl         Control block set in transfer_api_t::open call for this transfer.
     * @param[out]    p_info         Driver specific information.
     */
    ssp_err_t (* infoGet)(transfer_ctrl_t       * const p_ctrl,
                          transfer_properties_t * const p_info);

    /** Releases hardware lock.  This allows a transfer to be reconfigured using transfer_api_t::open.
     * @par Implemented as
     * - R_DTC_Close()
     * - R_DMAC_Close()
     * @param[in]     p_ctrl    Control block set in transfer_api_t::open call for this transfer.
     */
    ssp_err_t (* close)(transfer_ctrl_t       * const p_ctrl);

    /** Gets version and stores it in provided pointer p_version.
     * @par Implemented as
     * - R_DTC_VersionGet()
     * - R_DMAC_VersionGet()
     * @param[out]    p_version  Code and API version used.
     */
    ssp_err_t (* versionGet)(ssp_version_t         * const p_version);

    /** Reset source address pointer, destination address pointer, and/or length, for block transfer keeping all other settings the same.
         * Enable the transfer if p_src, p_dest, and length are valid.
         * @par Implemented as
         * - R_DMAC_BlockReset()
         * - R_DTC_BlockReset()
         *
         *
         * @param[in]     p_ctrl         Control block set in transfer_api_t::open call for this transfer.
         * @param[in]     p_src          Pointer to source. Set to NULL if source pointer should not change.
         * @param[in]     p_dest         Pointer to destination. Set to NULL if destination pointer should not change.
         * @param[in]     length         Transfer length in block mode.In DMAC only.
         * @param[in]     size           Transfer size in block mode. In DMAC only.
         * @param[in]     num_transfers  number of blocks in block mode.  In DMAC only.
         *
         */
    ssp_err_t (* blockReset)(transfer_ctrl_t  * const p_ctrl,
                void const                    * p_src,
                void                          * p_dest,
                uint16_t const                length,
                transfer_size_t               size,
                uint16_t const                num_transfers);
} transfer_api_t;

/** This structure encompasses everything that is needed to use an instance of this interface. */
typedef struct st_transfer_instance
{
    transfer_ctrl_t      * p_ctrl;    ///< Pointer to the control structure for this instance
    transfer_cfg_t const * p_cfg;     ///< Pointer to the configuration structure for this instance
    transfer_api_t const * p_api;     ///< Pointer to the API structure for this instance
} transfer_instance_t;

/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER

#endif /* DRV_TRANSFER_API_H */

/*******************************************************************************************************************//**
 * @} (end defgroup TRANSFER_API)
 **********************************************************************************************************************/
