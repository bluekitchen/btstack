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
 * File Name    : r_sci_uart.c
 * Description  : UART on SCI HAL driver
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "bsp_api.h"
#include "r_sci_uart.h"
#include "hw/hw_sci_uart_private.h"
#include "r_sci_uart_private_api.h"
#include "r_cgc.h"
#include "hw/hw_sci_common.h"
#include "r_ioport.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#ifndef SCI_UART_ERROR_RETURN
/*LDRA_INSPECTED 77 S This macro does not work when surrounded by parentheses. */
#define SCI_UART_ERROR_RETURN(a, err) SSP_ERROR_RETURN((a), (err), &g_module_name[0], &module_version)
#endif

/** SCI FIFO depth is defined by b2:3 of the variant data.  FIFO depth values defined in g_sci_uart_fifo_depth. */
#define SCI_UART_VARIANT_FIFO_DEPTH_MASK    (0x0CU)
#define SCI_UART_VARIANT_FIFO_DEPTH_SHIFT   (2U)

/** Number of divisors in the data table used for baud rate calculation. */
#define SCI_UART_NUM_DIVISORS_ASYNC      (13U)

/** Valid range of values for the modulation duty register is 128 - 256 (256 = modulation disabled). */
#define SCI_UART_MDDR_MIN                (128U)
#define SCI_UART_MDDR_MAX                (256U)

/** The bit rate register is 8-bits, so the maximum value is 255. */
#define SCI_UART_BRR_MAX                 (255U)

/** No limit to the number of bytes to read or write if DTC is not used. */
#define SCI_UART_MAX_READ_WRITE_NO_DTC   (0xFFFFFFFFU)

/** Size of each data to transmit or receive in 9-bit mode. */
#define SCI_UART_DATA_SIZE_2_BYTES       (2U)

/** Mask of invalid data bits in 9-bit mode. */
#define SCI_UART_ALIGN_2_BYTES           (0x1U)

/** "SCIU" in ASCII.  Used to determine if the control block is open. */
#define SCI_UART_OPEN           (0x53434955U)

/** Absolute maximum baud rate error. */
#define SCI_UART_MAX_BAUD_RATE_ERROR_X_1000  (15000U)

/***********************************************************************************************************************
 * Private constants
 **********************************************************************************************************************/
static const int32_t SCI_UART_100_PERCENT_X_1000 = 100000;
static const int32_t SCI_UART_MDDR_DIVISOR       = 256;

static const uint32_t   SCI_UART_DEFAULT_BAUD_RATE_ERROR_X_1000 = 10000;


/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/
#if (SCI_UART_CFG_PARAM_CHECKING_ENABLE)
static ssp_err_t r_sci_uart_open_param_check  (sci_uart_instance_ctrl_t const * const p_ctrl,
                                               uart_cfg_t               const * const p_cfg);

static ssp_err_t r_sci_read_write_param_check (sci_uart_instance_ctrl_t const * const p_ctrl,
                                               uint8_t const * const     addr,
                                               uint32_t const            bytes);
#endif /* #if (SCI_UART_CFG_PARAM_CHECKING_ENABLE) */

static void r_sci_uart_config_set (sci_uart_instance_ctrl_t * const p_ctrl,
                                   uart_cfg_t         const * const p_cfg);

static ssp_err_t r_sci_uart_transfer_configure (uart_cfg_t          const * const p_cfg,
                                                transfer_instance_t const *       p_transfer,
                                                ssp_signal_t                      signal,
                                                transfer_cfg_t            *       p_transfer_cfg);

static ssp_err_t r_sci_uart_transfer_open (sci_uart_instance_ctrl_t * const p_ctrl,
                                           uart_cfg_t         const * const p_cfg);

static int32_t r_sci_uart_brr_mddr_calculate(uint32_t                freq_hz,
                                             uint32_t                baudrate,
                                             uint8_t               * p_brr_value,
                                             baud_setting_t const ** pp_baud_setting,
                                             uint32_t              * p_mddr,
                                             bool                    select_16_base_clk_cycles);

static ssp_err_t r_sci_uart_baud_calculate(sci_clk_src_t           clk_src,
                                           uint32_t                baudrate,
                                           uint8_t               * p_brr_value,
                                           baud_setting_t const ** pp_baud_setting,
                                           uint32_t              * p_mddr,
                                           uint32_t                baud_rate_error_x_1000);

static void r_sci_uart_baud_set (R_SCI0_Type          *       p_sci_reg,
                                 sci_clk_src_t          const clk_src,
                                 uint8_t                const brr,
                                 baud_setting_t const * const p_baud_setting,
                                 uint32_t       const * const p_mddr);

static void r_sci_uart_fifo_cfg (sci_uart_instance_ctrl_t * const p_ctrl,
                                 uart_cfg_t         const * const p_cfg);

static ssp_err_t r_sci_irq_cfg(ssp_feature_t * const p_feature,
                               ssp_signal_t    const signal,
                               uint8_t         const ipl,
                               void          * const p_ctrl,
                               IRQn_Type     * const p_irq);

static ssp_err_t r_sci_irqs_cfg(ssp_feature_t            * const p_feature,
                                sci_uart_instance_ctrl_t * const p_ctrl,
                                uart_cfg_t         const * const p_cfg);

static void r_sci_uart_transfer_close (sci_uart_instance_ctrl_t * p_ctrl);

static void r_sci_uart_rs485_de_pin_cfg (sci_uart_instance_ctrl_t * const p_ctrl);

#if (SCI_UART_CFG_EXTERNAL_RTS_OPERATION)
static void r_sci_uart_external_rts_operation_enable (sci_uart_instance_ctrl_t * const p_ctrl,
                                                      uart_cfg_t         const * const p_cfg);
#endif

#if (SCI_UART_CFG_TX_ENABLE)
static ssp_err_t r_sci_uart_write_transfer_setup(sci_uart_instance_ctrl_t * const p_ctrl,
                                                 uint8_t const * const p_src,
                                                 uint32_t const bytes);
void r_sci_uart_write_no_transfer(sci_uart_instance_ctrl_t * const p_ctrl);

#endif /* if (SCI_UART_CFG_TX_ENABLE) */



#if (SCI_UART_CFG_RX_ENABLE)
void r_sci_uart_rxi_read_no_transfer_eventset(sci_uart_instance_ctrl_t * const p_ctrl,
                                              uint32_t                   const data);

void r_sci_uart_rxi_read_no_transfer(sci_uart_instance_ctrl_t * const p_ctrl);

void sci_uart_rxi_isr (void);

void r_sci_uart_read_data(sci_uart_instance_ctrl_t * const p_ctrl,
                          uint32_t                 * const p_data);

void sci_uart_eri_isr (void);

static ssp_err_t r_sci_uart_abort_rx(sci_uart_instance_ctrl_t * p_ctrl);
#endif /* if (SCI_UART_CFG_RX_ENABLE) */

#if (SCI_UART_CFG_TX_ENABLE)
static ssp_err_t r_sci_uart_abort_tx(sci_uart_instance_ctrl_t * p_ctrl);

void sci_uart_txi_isr (void);
void sci_uart_tei_isr (void);
#endif /* if (SCI_UART_CFG_TX_ENABLE) */



/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
/** Name of module used by error logger macro */
#if BSP_CFG_ERROR_LOG != 0
static const char g_module_name[] = "sci_uart";
#endif

/** Baud rate divisor information (UART mode) */
static const baud_setting_t async_baud[SCI_UART_NUM_DIVISORS_ASYNC] =
{
    {   6U,  0U,  0U,  1U,  0U }, /* divisor, BGDM, ABCS, ABCSE, n */
    {   8U,  1U,  1U,  0U,  0U },
    {  16U,  1U,  0U,  0U,  0U },
    {  24U,  0U,  0U,  1U,  1U },
    {  32U,  0U,  0U,  0U,  0U },
    {  64U,  1U,  0U,  0U,  1U },
    {  96U,  0U,  0U,  1U,  2U },
    { 128U,  0U,  0U,  0U,  1U },
    { 256U,  1U,  0U,  0U,  2U },
    { 384U,  0U,  0U,  1U,  3U },
    { 512U,  0U,  0U,  0U,  2U },
    { 1024U, 1U,  0U,  0U,  3U },
    { 2048U, 0U,  0U,  0U,  3U }
};

/** FIFO depth values, use variant data b2:3 as index. */
static const uint8_t g_sci_uart_fifo_depth[] =
{
    0U, 4U, 8U, 16U
};

#if defined(__GNUC__)
/* This structure is affected by warnings from a GCC compiler bug. This pragma suppresses the warnings in this
 * structure only.*/
/*LDRA_INSPECTED 69 S */
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
/** SCI UART HAL module version data structure */
static const ssp_version_t module_version =
{
    .api_version_minor  = UART_API_VERSION_MINOR,
    .api_version_major  = UART_API_VERSION_MAJOR,
    .code_version_major = SCI_UART_CODE_VERSION_MAJOR,
    .code_version_minor = SCI_UART_CODE_VERSION_MINOR
};
#if defined(__GNUC__)
/* Restore warning settings for 'missing-field-initializers' to as specified on command line. */
/*LDRA_INSPECTED 69 S */
#pragma GCC diagnostic pop
#endif

/** UART on SCI HAL API mapping for UART interface */
/*LDRA_INSPECTED 27 D This structure must be accessible in user code. It cannot be static. */
const uart_api_t  g_uart_on_sci =
{
    .open               = R_SCI_UartOpen,
    .close              = R_SCI_UartClose,
    .write              = R_SCI_UartWrite,
    .read               = R_SCI_UartRead,
    .infoGet            = R_SCI_UartInfoGet,
    .baudSet            = R_SCI_UartBaudSet,
    .versionGet         = R_SCI_UartVersionGet,
    .communicationAbort = R_SCI_UartAbort
};

/*******************************************************************************************************************//**
 * @addtogroup UARTonSCI
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * Configures the UART driver based on the input configurations.  If reception is enabled at compile time, reception is
 * enabled at the end of this function.
 *
 * @retval  SSP_SUCCESS                Channel opened successfully.
 * @retval  SSP_ERR_ASSERTION          Pointer to UART control block or configuration structure is NULL.
 * @retval  SSP_ERR_INVALID_ARGUMENT   Invalid parameter setting found in the configuration structure.
 * @retval  SSP_ERR_IN_USE             Control block has already been opened or channel is being used by another
 *                                     instance. Call close() then open() to reconfigure.
 * @retval  SSP_ERR_IRQ_BSP_DISABLED   A required interrupt does not exist in the vector table
 *
 * @return                       See @ref Common_Error_Codes or functions called by this function for other possible
 *                               return codes. This function calls:
 *                                   * fmi_api_t::productFeatureGet
 *                                   * fmi_api_t::eventInfoGet
 *                                   * cgc_api_t::systemClockFreqGet
 *                                   * transfer_api_t::open
 **********************************************************************************************************************/
ssp_err_t R_SCI_UartOpen (uart_ctrl_t      * const p_api_ctrl,
                          uart_cfg_t const * const p_cfg)
{
    ssp_err_t         err = SSP_SUCCESS;
    sci_uart_instance_ctrl_t * p_ctrl = (sci_uart_instance_ctrl_t *) p_api_ctrl;

#if (SCI_UART_CFG_PARAM_CHECKING_ENABLE)
    /** Check parameters. */
    err = r_sci_uart_open_param_check(p_ctrl, p_cfg);        /** check arguments */
    SCI_UART_ERROR_RETURN((SSP_SUCCESS == err), err);
#endif


    /** Make sure this channel exists. */
    ssp_feature_t ssp_feature = {{(ssp_ip_t) 0U}};
    ssp_feature.channel = p_cfg->channel;
    ssp_feature.unit = 0U;
    ssp_feature.id = SSP_IP_SCI;
    fmi_feature_info_t info = {0U};
    err = g_fmi_on_fmi.productFeatureGet(&ssp_feature, &info);
    SCI_UART_ERROR_RETURN(SSP_SUCCESS == err, err);
    p_ctrl->p_reg = info.ptr;
    R_SCI0_Type * p_sci_reg = (R_SCI0_Type *) p_ctrl->p_reg;

    /** Reserve the hardware lock. */
    err = R_BSP_HardwareLock(&ssp_feature);
    SCI_UART_ERROR_RETURN(SSP_SUCCESS == err, err);

    /** Determine if this channel has a FIFO. */
    uint32_t fifo_depth_index = (uint32_t) (info.variant_data & SCI_UART_VARIANT_FIFO_DEPTH_MASK)
            >> SCI_UART_VARIANT_FIFO_DEPTH_SHIFT;
    p_ctrl->fifo_depth = g_sci_uart_fifo_depth[fifo_depth_index];

    /** Calculate the baud rate register settings. */
    uart_on_sci_cfg_t * p_extend = (uart_on_sci_cfg_t *) p_cfg->p_extend;

    sci_clk_src_t     clk_src = SCI_CLK_SRC_INT;
    p_ctrl->baud_rate_error_x_1000 = SCI_UART_DEFAULT_BAUD_RATE_ERROR_X_1000;

    uint32_t mddr = 0U;
    uint32_t * p_mddr = NULL;
    if (NULL != p_extend)
    {
        clk_src = p_extend->clk_src;
        if (p_extend->bitrate_modulation)
        {
            p_mddr = &mddr;
            p_ctrl->bitrate_modulation = 1U;
        }
        else
        {
            p_ctrl->bitrate_modulation = 0U;
        }
        p_ctrl->baud_rate_error_x_1000 = p_extend->baud_rate_error_x_1000;
    }

    uint8_t brr = 0U;
    baud_setting_t const * p_baud_setting = NULL;
    err = r_sci_uart_baud_calculate(clk_src, p_cfg->baud_rate, &brr, &p_baud_setting, p_mddr, p_ctrl->baud_rate_error_x_1000);

    /** Configure the interrupts. */
    if (SSP_SUCCESS == err)
    {
        err = r_sci_irqs_cfg(&ssp_feature, p_ctrl, p_cfg);
    }

    /** Configure the transfer interface for transmission and reception if provided. */
    if (SSP_SUCCESS == err)
    {
        p_ctrl->p_transfer_rx = p_cfg->p_transfer_rx;
        p_ctrl->p_transfer_tx = p_cfg->p_transfer_tx;

        err = r_sci_uart_transfer_open(p_ctrl, p_cfg);
    }

    /* If any error occurred, close the transfer interfaces if provided and release the hardware lock. */
    if (err != SSP_SUCCESS)
    {
        r_sci_uart_transfer_close(p_ctrl);
        R_BSP_HardwareUnlock(&ssp_feature);
    }
    SCI_UART_ERROR_RETURN(SSP_SUCCESS == err, err);

    p_ctrl->uart_comm_mode                   = p_extend->uart_comm_mode;
    p_ctrl->uart_rs485_mode                  = p_extend->uart_rs485_mode;
    p_ctrl->rs485_de_pin                     = p_extend->rs485_de_pin;

    /** Configuration of driver enable pin for rs485 communication mode. */
    r_sci_uart_rs485_de_pin_cfg(p_ctrl);

    /** Enable the SCI channel and reset the registers to their initial state. */
    R_BSP_ModuleStart(&ssp_feature);
    HW_SCI_RegisterReset(p_sci_reg);

    /** Set the default level of the TX pin to 1. */
    HW_SCI_TransmitterLevelSet(p_sci_reg, 1U);

    /** Set the baud rate registers. */
    r_sci_uart_baud_set(p_sci_reg, clk_src, brr, p_baud_setting, p_mddr);

    /** Set the UART configuration settings provided in ::uart_cfg_t and ::uart_on_sci_cfg_t. */
    r_sci_uart_config_set(p_ctrl, p_cfg);

    p_ctrl->channel                          = p_cfg->channel;
    p_ctrl->p_context                        = p_cfg->p_context;
    p_ctrl->p_callback                       = p_cfg->p_callback;
    p_ctrl->p_tx_src                         = NULL;
    p_ctrl->tx_src_bytes                     = 0U;
    p_ctrl->rx_transfer_in_progress          = 0U;
    p_ctrl->rx_dst_bytes                     = 0U;
    p_ctrl->rx_bytes_count                   = 0U;

#if (SCI_UART_CFG_RX_ENABLE)
    /** If reception is enabled at build time, enable reception. */
    /* NOTE: Transmitter and its interrupt are enabled in R_SCI_UartWrite(). */
    HW_SCI_RXIeventSelect(p_sci_reg);
    HW_SCI_ReceiverEnable(p_sci_reg);
    HW_SCI_RxIrqEnable(p_sci_reg, p_ctrl);
#endif

#if (SCI_UART_CFG_TX_ENABLE)
    HW_SCI_TransmitterEnable(p_ctrl->p_reg);
#endif

#if (SCI_UART_CFG_EXTERNAL_RTS_OPERATION)
    /** If external RTS operation is enabled at build time, call user provided RTS function to set initial RTS value
     * to 0. */
    r_sci_uart_external_rts_operation_enable(p_ctrl, p_cfg);
#endif

    p_ctrl->open = SCI_UART_OPEN;

    return SSP_SUCCESS;
}  /* End of function R_SCI_UartOpen() */

/*******************************************************************************************************************//**
 * Disables interrupts, receiver, and transmitter.  Closes lower level transfer drivers if used.  Removes power and
 * releases hardware lock.
 *
 * @retval  SSP_SUCCESS              Channel successfully closed.
 * @retval  SSP_ERR_ASSERTION        Pointer to UART control block is NULL.
 * @retval  SSP_ERR_NOT_OPEN         The control block has not been opened
 **********************************************************************************************************************/
ssp_err_t R_SCI_UartClose (uart_ctrl_t * const p_api_ctrl)
{
    sci_uart_instance_ctrl_t * p_ctrl = (sci_uart_instance_ctrl_t *) p_api_ctrl;
#if (SCI_UART_CFG_PARAM_CHECKING_ENABLE)
    SSP_ASSERT(p_ctrl);
    SCI_UART_ERROR_RETURN(SCI_UART_OPEN == p_ctrl->open, SSP_ERR_NOT_OPEN);
#endif

    /** Mark the channel not open so other APIs cannot use it. */
    p_ctrl->open = 0U;

    /** Disable interrupts, receiver, and transmitter. */
    R_SCI0_Type * p_sci_reg = (R_SCI0_Type *) p_ctrl->p_reg;

#if (SCI_UART_CFG_RX_ENABLE | SCI_UART_CFG_TX_ENABLE)
    HW_SCI_UartTransmitterReceiverDisable(p_sci_reg);
#endif
#if (SCI_UART_CFG_RX_ENABLE)
    /** If reception is enabled at build time, disable reception irqs. */
    NVIC_DisableIRQ(p_ctrl->rxi_irq);
    if (SSP_INVALID_VECTOR != p_ctrl->eri_irq)
    {
        NVIC_DisableIRQ(p_ctrl->eri_irq);
    }
#endif
#if (SCI_UART_CFG_TX_ENABLE)
    /** If transmission is enabled at build time, disable transmission irqs. */
    NVIC_DisableIRQ(p_ctrl->txi_irq);
    NVIC_DisableIRQ(p_ctrl->tei_irq);
#endif

    /** Disable baud clock output. */
    HW_SCI_BaudClkOutputDisable (p_sci_reg);

    /** Close the lower level transfer instances. */
    r_sci_uart_transfer_close(p_ctrl);

    /** Clear control block parameters. */
    p_ctrl->p_callback   = NULL;
    p_ctrl->p_extpin_ctrl = NULL;

    /** Remove power to the channel. */
    ssp_feature_t ssp_feature = {{(ssp_ip_t) 0U}};
    ssp_feature.channel = p_ctrl->channel;
    ssp_feature.unit = 0U;
    ssp_feature.id = SSP_IP_SCI;
    R_BSP_ModuleStop(&ssp_feature);

    /** Unlock the SCI channel. */
    R_BSP_HardwareUnlock(&ssp_feature);

    return SSP_SUCCESS;
}  /* End of function R_SCI_UartClose() */

/*******************************************************************************************************************//**
 * Receives user specified number of bytes into destination buffer pointer.
 *
 * @retval  SSP_SUCCESS                  Data reception successfully ends.
 * @retval  SSP_ERR_ASSERTION            Pointer to UART control block is NULL.
 *                                       Number of transfers outside the max or min boundary when transfer instance used
 * @retval  SSP_ERR_INVALID_ARGUMENT     Destination address or data size is not valid for 9-bit mode.
 * @retval  SSP_ERR_NOT_OPEN             The control block has not been opened
 * @retval  SSP_ERR_IN_USE               A previous read operation is still in progress.
 * @retval  SSP_ERR_UNSUPPORTED          SCI_UART_CFG_RX_ENABLE is set to 0
 *
 * @return                       See @ref Common_Error_Codes or functions called by this function for other possible
 *                               return codes. This function calls:
 *                                   * transfer_api_t::reset
 *
 * @note This API is only valid when SCI_UART_CFG_RX_ENABLE is enabled.
 *       If 9-bit data length is specified at R_SCI_UartOpen call, p_dest must be aligned 16-bit boundary.
 **********************************************************************************************************************/
ssp_err_t R_SCI_UartRead (uart_ctrl_t   * const p_api_ctrl,
                          uint8_t const * const p_dest,
                          uint32_t        const bytes)
{
#if (SCI_UART_CFG_RX_ENABLE)
    sci_uart_instance_ctrl_t * p_ctrl = (sci_uart_instance_ctrl_t *) p_api_ctrl;
    ssp_err_t err        = SSP_SUCCESS;

#if (SCI_UART_CFG_PARAM_CHECKING_ENABLE)
    err = r_sci_read_write_param_check(p_ctrl, p_dest, bytes);
    SCI_UART_ERROR_RETURN(SSP_SUCCESS == err, err);
#endif

    uint32_t data_bytes = p_ctrl->data_bytes;

    /** Configure transfer instance to receive the requested number of bytes if transfer is used for reception. */
    if (NULL != p_ctrl->p_transfer_rx)
    {
        uint32_t size = bytes >> (data_bytes - 1);
#if (SCI_UART_CFG_PARAM_CHECKING_ENABLE)
        transfer_properties_t transfer_max = {0U};
        p_ctrl->p_transfer_rx->p_api->infoGet(p_ctrl->p_transfer_rx->p_ctrl, &transfer_max);
        SSP_ASSERT(size <= transfer_max.transfer_length_max);
#endif
        SCI_UART_ERROR_RETURN(0U == p_ctrl->rx_transfer_in_progress, SSP_ERR_IN_USE);
        p_ctrl->rx_transfer_in_progress = 1U;
        err = p_ctrl->p_transfer_rx->p_api->reset(p_ctrl->p_transfer_rx->p_ctrl, NULL, (void *) p_dest, (uint16_t) size);
        SCI_UART_ERROR_RETURN(SSP_SUCCESS == err, err);
    }
    else
    {
        SCI_UART_ERROR_RETURN(0U == p_ctrl->rx_dst_bytes, SSP_ERR_IN_USE);
        p_ctrl->p_rx_dst = (uint8_t *) p_dest;
        p_ctrl->rx_dst_bytes = bytes;
        p_ctrl->rx_bytes_count = 0U;
    }

    return err;
#else
    SSP_PARAMETER_NOT_USED(p_api_ctrl);
    SSP_PARAMETER_NOT_USED(p_dest);
    SSP_PARAMETER_NOT_USED(bytes);
    return SSP_ERR_UNSUPPORTED;
#endif /* if (SCI_UART_CFG_RX_ENABLE) */
}  /* End of function R_SCI_UartRead() */

/*******************************************************************************************************************//**
 * Transmits user specified number of bytes from the source buffer pointer.
 *
 * @retval  SSP_SUCCESS                  Data transmission finished successfully.
 * @retval  SSP_ERR_ASSERTION            Pointer to UART control block is NULL.
 *                                       Number of transfers outside the max or min boundary when transfer instance used
 * @retval  SSP_ERR_INVALID_ARGUMENT     Source address or data size is not valid for 9-bit mode.
 * @retval  SSP_ERR_NOT_OPEN             The control block has not been opened
 * @retval  SSP_ERR_IN_USE               A UART transmission is in progress
 * @retval  SSP_ERR_UNSUPPORTED          SCI_UART_CFG_TX_ENABLE is set to 0
 *
 * @return                       See @ref Common_Error_Codes or functions called by this function for other possible
 *                               return codes. This function calls:
 *                                   * transfer_api_t::reset
 *
 * @note  This API is only valid when SCI_UART_CFG_TX_ENABLE is enabled.
 *        If 9-bit data length is specified at R_SCI_UartOpen call, p_src must be aligned on a 16-bit boundary.
 **********************************************************************************************************************/
ssp_err_t R_SCI_UartWrite (uart_ctrl_t   * const p_api_ctrl,
                           uint8_t const * const p_src,
                           uint32_t        const bytes)
{
#if (SCI_UART_CFG_TX_ENABLE)
    sci_uart_instance_ctrl_t * p_ctrl = (sci_uart_instance_ctrl_t *) p_api_ctrl;
    ssp_err_t err        = SSP_SUCCESS;

#if (SCI_UART_CFG_PARAM_CHECKING_ENABLE)
    err = r_sci_read_write_param_check(p_ctrl, p_src, bytes);
    SCI_UART_ERROR_RETURN(SSP_SUCCESS == err, err);
    SCI_UART_ERROR_RETURN(NULL == p_ctrl->p_tx_src, SSP_ERR_IN_USE);
#endif

    /**  Set the Driver Enable pin in RS485 half duplex mode to enable transmission. */
    if ((UART_MODE_RS485 == p_ctrl->uart_comm_mode) && (UART_RS485_HD == p_ctrl->uart_rs485_mode))
    {
          g_ioport_on_ioport.pinWrite (p_ctrl->rs485_de_pin, IOPORT_LEVEL_HIGH);
    }

    R_SCI0_Type * p_sci_reg = (R_SCI0_Type *) p_ctrl->p_reg;
    /** Transmit interrupts must be disabled to start with. */
    HW_SCI_UartTransmitIrqDisable(p_sci_reg);

    /** Save data to transmit to the control block. It will be transmitted in the TXI ISR. */
    p_ctrl->p_tx_src = p_src;
    p_ctrl->tx_src_bytes = bytes;

    /** If a transfer instance is used for transmission, reset the transfer instance to transmit the requested
     * data. */
    if (NULL != p_ctrl->p_transfer_tx)
    {
        err = r_sci_uart_write_transfer_setup(p_ctrl, p_src, bytes);
        if (SSP_SUCCESS != err)
        {
            /** Clear the Driver Enable pin in RS485 half duplex mode to enable reception in error conditions*/
            r_sci_uart_rs485_de_pin_cfg(p_ctrl);
            SSP_ERROR_LOG((err), (&g_module_name[0]), (&module_version));
            return err;
        }
    }

    /** Trigger a TXI interrupt. This triggers the transfer instance or a TXI interrupt if the transfer instance is
     * not used. */
    R_BSP_IrqStatusClear(p_ctrl->txi_irq);
    NVIC_ClearPendingIRQ(p_ctrl->txi_irq);
    if (0U != p_ctrl->fifo_depth)
    {
        NVIC_EnableIRQ(p_ctrl->txi_irq);
    }
    HW_SCI_UartTransmitterEnable(p_sci_reg);
    if (0U == p_ctrl->fifo_depth)
    {
        /* On channels with no FIFO, the first byte is sent from this function to trigger the first TXI event.  This
         * method is used instead of setting TE and TIE at the same time as recommended in the hardware manual to avoid
         * the one frame delay that occurs when the TE bit is set. */
        r_sci_uart_write_no_transfer(p_ctrl);
        NVIC_EnableIRQ(p_ctrl->txi_irq);
    }

    return SSP_SUCCESS;
#else
    SSP_PARAMETER_NOT_USED(p_api_ctrl);
    SSP_PARAMETER_NOT_USED(p_src);
    SSP_PARAMETER_NOT_USED(bytes);
    return SSP_ERR_UNSUPPORTED;
#endif /* if (SCI_UART_CFG_TX_ENABLE) */
}  /* End of function R_SCI_UartWrite() */

/*******************************************************************************************************************//**
 * Updates the baud rate.
 *
 * @warning This terminates any in-progress transmission.
 *
 * @retval  SSP_SUCCESS                  Baud rate was successfully changed.
 * @retval  SSP_ERR_ASSERTION            Pointer to UART control block is NULL or the UART is not configured to use the
 *                                       internal clock.
 * @retval  SSP_ERR_INVALID_ARGUMENT     Illegal baud rate value is specified.
 * @retval  SSP_ERR_NOT_OPEN             The control block has not been opened
 **********************************************************************************************************************/
ssp_err_t R_SCI_UartBaudSet (uart_ctrl_t * const p_api_ctrl,
                             uint32_t      const baudrate)
{
    sci_uart_instance_ctrl_t * p_ctrl = (sci_uart_instance_ctrl_t *) p_api_ctrl;
    ssp_err_t     err = SSP_SUCCESS;
    sci_clk_src_t clk_src = SCI_CLK_SRC_INT;
    R_SCI0_Type * p_sci_reg;

#if (SCI_UART_CFG_PARAM_CHECKING_ENABLE)
    SSP_ASSERT(p_ctrl);
    SCI_UART_ERROR_RETURN(SCI_UART_OPEN == p_ctrl->open, SSP_ERR_NOT_OPEN);
#endif

    p_sci_reg = (R_SCI0_Type *) p_ctrl->p_reg;
    SSP_ASSERT(HW_SCI_IsBaudRateInternalClkSelected(p_sci_reg));

    /** Calculate new baud rate register settings. */
    uint8_t brr = 0U;
    baud_setting_t const * p_baud_setting = NULL;
    uint32_t mddr = 0U;
    uint32_t * p_mddr = NULL;
    if (1U == p_ctrl->bitrate_modulation)
    {
        p_mddr = &mddr;
    }
    err = r_sci_uart_baud_calculate(clk_src, baudrate, &brr, &p_baud_setting, p_mddr, p_ctrl->baud_rate_error_x_1000);
    SCI_UART_ERROR_RETURN(SSP_SUCCESS == err, err);

    /** Disables transmitter and receiver. This terminates any in-progress transmission. */
    HW_SCI_UartTransmitterReceiverDisable(p_sci_reg);
    p_ctrl->p_tx_src = NULL;

    /** Apply new baud rate register settings. */
    r_sci_uart_baud_set(p_sci_reg, clk_src, brr, p_baud_setting, p_mddr);

#if (SCI_UART_CFG_RX_ENABLE)
    /** Enable receiver. */
    HW_SCI_UartReceiverEnable(p_sci_reg);
#endif

    return err;
}  /* End of function R_SCI_UartBaudSet() */

/*******************************************************************************************************************//**
 * Provides the driver information, including the maximum number of bytes that can be received or transmitted at a time.
 *
 * @retval  SSP_SUCCESS                  Information stored in provided p_info.
 * @retval  SSP_ERR_ASSERTION            Pointer to UART control block is NULL.
 * @retval  SSP_ERR_NOT_OPEN             The control block has not been opened
 **********************************************************************************************************************/
ssp_err_t R_SCI_UartInfoGet (uart_ctrl_t * const p_api_ctrl,
                             uart_info_t * const p_info)
{
    sci_uart_instance_ctrl_t * p_ctrl = (sci_uart_instance_ctrl_t *) p_api_ctrl;

#if (SCI_UART_CFG_PARAM_CHECKING_ENABLE)
    SSP_ASSERT(p_ctrl);
    SSP_ASSERT(p_info);
    SCI_UART_ERROR_RETURN(SCI_UART_OPEN == p_ctrl->open, SSP_ERR_NOT_OPEN);
#endif

    p_info->read_bytes_max = 0U;
    p_info->write_bytes_max = 0U;

#if (SCI_UART_CFG_RX_ENABLE)
    /** Store number of bytes that can be read at a time. */
    if (NULL != p_ctrl->p_transfer_rx)
    {
        transfer_properties_t properties = {0U};
        p_ctrl->p_transfer_rx->p_api->infoGet(p_ctrl->p_transfer_rx->p_ctrl, &properties);
        p_info->read_bytes_max = properties.transfer_length_max * p_ctrl->data_bytes;
    }
    else
    {
        /* No limit to number of bytes to read. */
        p_info->read_bytes_max = SCI_UART_MAX_READ_WRITE_NO_DTC;
    }
#endif

#if (SCI_UART_CFG_TX_ENABLE)
    /** Store number of bytes that can be written at a time. */
    if (NULL != p_ctrl->p_transfer_tx)
    {
        transfer_properties_t properties = {0U};
        p_ctrl->p_transfer_tx->p_api->infoGet(p_ctrl->p_transfer_tx->p_ctrl, &properties);
        p_info->write_bytes_max = properties.transfer_length_max * p_ctrl->data_bytes;
    }
    else
    {
        /* No limit to number of bytes to write. */
        p_info->write_bytes_max = SCI_UART_MAX_READ_WRITE_NO_DTC;
    }
#endif

    return SSP_SUCCESS;
}  /* End of function R_SCI_UartInfoGet() */

/*******************************************************************************************************************//**
 * Provides API and code version in the user provided pointer.
 *
 * @param[in] p_version   Version number set here
 *
 * @retval  SSP_SUCCESS                  Version information stored in provided p_version.
 * @retval  SSP_ERR_ASSERTION            p_version is NULL.
 **********************************************************************************************************************/
ssp_err_t R_SCI_UartVersionGet (ssp_version_t * p_version)
{
#if (SCI_UART_CFG_PARAM_CHECKING_ENABLE)
    SSP_ASSERT(p_version);
#endif

    *p_version = module_version;

    return SSP_SUCCESS;
} /* End of function R_SCI_UartVersionGet() */

/*******************************************************************************************************************//**
 * Provides API to abort ongoing transfer. Transmission is aborted after the current character is transmitted.
 * Reception is still enabled after abort(). Any characters received after abort() and before the transfer
 * is reset in the next call to read(), will arrive via the callback function with event UART_EVENT_RX_CHAR.
 *
 * @retval  SSP_SUCCESS                  UART transaction aborted successfully.
 * @retval  SSP_ERR_ASSERTION            Pointer to UART control block is NULL.
 * @retval  SSP_ERR_NOT_OPEN             The control block has not been opened.
 * @retval  SSP_ERR_UNSUPPORTED          The requested Abort direction is unsupported.
 *
 * @return                       See @ref Common_Error_Codes or functions called by this function for other possible
 *                               return codes. This function calls:
 *                                   * transfer_api_t::disable
 **********************************************************************************************************************/
ssp_err_t R_SCI_UartAbort (uart_ctrl_t   * const p_api_ctrl, uart_dir_t communication_to_abort)
{
    sci_uart_instance_ctrl_t * p_ctrl = (sci_uart_instance_ctrl_t *) p_api_ctrl;
    ssp_err_t err        = SSP_ERR_UNSUPPORTED;

#if (SCI_UART_CFG_PARAM_CHECKING_ENABLE)
    SSP_ASSERT(p_ctrl);
    SCI_UART_ERROR_RETURN(SCI_UART_OPEN == p_ctrl->open, SSP_ERR_NOT_OPEN);
#endif

#if (SCI_UART_CFG_TX_ENABLE)
    if ((UART_DIR_RX_TX == communication_to_abort) || (UART_DIR_TX == communication_to_abort))
    {
        err = r_sci_uart_abort_tx(p_ctrl);
        SCI_UART_ERROR_RETURN(SSP_SUCCESS == err, err);
    }
#endif
#if (SCI_UART_CFG_RX_ENABLE)
    if ((UART_DIR_RX_TX == communication_to_abort) || (UART_DIR_RX == communication_to_abort))
    {
        err = r_sci_uart_abort_rx(p_ctrl);
        SCI_UART_ERROR_RETURN(SSP_SUCCESS == err, err);
    }
#endif
    return err;
}  /* End of function R_SCI_UartAbort() */

/*******************************************************************************************************************//**
 * @} (end addtogroup UARTonSCI)
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private Functions
 **********************************************************************************************************************/

#if (SCI_UART_CFG_PARAM_CHECKING_ENABLE)
/*******************************************************************************************************************//**
 * Parameter error check function for open.
 *
 * @param[in] p_ctrl   Pointer to the control block for the channel
 * @param[in] p_cfg    Pointer to the configuration structure specific to UART mode
 *
 * @retval  SSP_SUCCESS                  No parameter error found
 * @retval  SSP_ERR_ASSERTION            Pointer to UART control block or configuration structure is NULL
 * @retval  SSP_ERR_IN_USE               Control block has already been opened
 **********************************************************************************************************************/
static ssp_err_t r_sci_uart_open_param_check  (sci_uart_instance_ctrl_t const * const p_ctrl,
                                               uart_cfg_t               const * const p_cfg)
{
    SSP_ASSERT(p_ctrl);
    SSP_ASSERT(p_cfg);
    SCI_UART_ERROR_RETURN(SCI_UART_OPEN != p_ctrl->open, SSP_ERR_IN_USE);
    return SSP_SUCCESS;
}  /* End of function r_sci_uart_open_param_check() */

/*******************************************************************************************************************//**
 * Parameter error check function for read/write.
 *
 * @param[in] p_ctrl Pointer to the control block for the channel
 * @param[in] addr   Pointer to the buffer
 * @param[in] bytes  Number of bytes to read or write
 *
 * @retval  SSP_SUCCESS              No parameter error found
 * @retval  SSP_ERR_NOT_OPEN         The control block has not been opened
 * @retval  SSP_ERR_ASSERTION        Pointer to UART control block or configuration structure is NULL
 * @retval  SSP_ERR_INVALID_ARGUMENT Address is not aligned to 2-byte boundary or size is the odd number when the data
 *                                   length is 9-bit
 **********************************************************************************************************************/
static ssp_err_t r_sci_read_write_param_check (sci_uart_instance_ctrl_t const * const p_ctrl,
                                               uint8_t const * const     addr,
                                               uint32_t const            bytes)
{
    SSP_ASSERT(p_ctrl);
    SSP_ASSERT(addr);
    SSP_ASSERT(0U != bytes);
    SCI_UART_ERROR_RETURN(SCI_UART_OPEN == p_ctrl->open, SSP_ERR_NOT_OPEN);

    if (SCI_UART_DATA_SIZE_2_BYTES == p_ctrl->data_bytes)
    {
        /** Do not allow odd buffer address if data length is 9 bits. */
        SCI_UART_ERROR_RETURN((0U == ((uint32_t) addr & SCI_UART_ALIGN_2_BYTES)), SSP_ERR_INVALID_ARGUMENT);

        /** Do not allow odd number of data bytes if data length is 9 bits. */
        SCI_UART_ERROR_RETURN((0U == (bytes & SCI_UART_ALIGN_2_BYTES)), SSP_ERR_INVALID_ARGUMENT);
    }

    return SSP_SUCCESS;
}  /* End of function r_sci_read_write_param_check() */
#endif /* if (SCI_UART_CFG_PARAM_CHECKING_ENABLE) */

/*******************************************************************************************************************//**
 * Subroutine to apply common UART transfer settings.
 *
 * @param[in]     p_cfg           Pointer to UART specific configuration structure
 * @param[in]     p_transfer      Pointer to transfer instance to configure
 * @param[in]     signal          Signal to used to activate the transfer instance
 * @param[in]     p_transfer_cfg  Pointer to transfer configuration structure
 *
 * @retval        SSP_SUCCESS        UART transfer drivers successfully configured
 * @retval        SSP_ERR_ASSERTION  Invalid pointer or required interrupt not enabled in vector table
 **********************************************************************************************************************/
static ssp_err_t r_sci_uart_transfer_configure (uart_cfg_t          const * const p_cfg,
                                                transfer_instance_t const *       p_transfer,
                                                ssp_signal_t                      signal,
                                                transfer_cfg_t            *       p_transfer_cfg)
{
    /** Configure the transfer instance, if enabled. */
#if (SCI_UART_CFG_PARAM_CHECKING_ENABLE)
    SSP_ASSERT(NULL != p_transfer->p_api);
    SSP_ASSERT(NULL != p_transfer->p_ctrl);
    SSP_ASSERT(NULL != p_transfer->p_cfg);
    SSP_ASSERT(NULL != p_transfer->p_cfg->p_info);
#endif
    transfer_info_t * p_info = p_transfer->p_cfg->p_info;
    p_info->mode = TRANSFER_MODE_NORMAL;
    p_info->irq = TRANSFER_IRQ_END;
    if (UART_DATA_BITS_9 == p_cfg->data_bits)
    {
        p_info->size = TRANSFER_SIZE_2_BYTE;
    }
    else
    {
        p_info->size = TRANSFER_SIZE_1_BYTE;
    }
    fmi_event_info_t event_info = {(IRQn_Type) 0U};
    ssp_feature_t ssp_feature = {{(ssp_ip_t) 0U}};
    ssp_feature.channel = p_cfg->channel;
    ssp_feature.unit = 0U;
    ssp_feature.id = SSP_IP_SCI;
    g_fmi_on_fmi.eventInfoGet(&ssp_feature, signal, &event_info);
#if (SCI_UART_CFG_PARAM_CHECKING_ENABLE)
    /* Check to make sure the interrupt is enabled. */
    SSP_ASSERT(SSP_INVALID_VECTOR != event_info.irq);
#endif
    p_transfer_cfg->activation_source = event_info.event;
    p_transfer_cfg->auto_enable = false;

    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Configures UART related transfer drivers (if enabled).
 *
 * @param[in]     p_ctrl  Pointer to UART control structure
 * @param[in]     p_cfg   Pointer to UART specific configuration structure
 *
 * @retval        SSP_SUCCESS        UART transfer drivers successfully configured
 * @retval        SSP_ERR_ASSERTION  Invalid pointer or required interrupt not enabled in vector table
 *
 * @return                       See @ref Common_Error_Codes or functions called by this function for other possible
 *                               return codes. This function calls:
 *                                   * transfer_api_t::open
 **********************************************************************************************************************/
static ssp_err_t r_sci_uart_transfer_open (sci_uart_instance_ctrl_t * const p_ctrl,
                                           uart_cfg_t         const * const p_cfg)
{
    ssp_err_t err;
    p_ctrl->data_bytes = 1U;
    if (UART_DATA_BITS_9 == p_cfg->data_bits)
    {
        p_ctrl->data_bytes = SCI_UART_DATA_SIZE_2_BYTES;
    }
#if (SCI_UART_CFG_RX_ENABLE)
    /** If a transfer instance is used for reception, apply UART specific settings and open the transfer instance. */
    if (NULL != p_cfg->p_transfer_rx)
    {
        transfer_cfg_t cfg = *(p_cfg->p_transfer_rx->p_cfg);
        err = r_sci_uart_transfer_configure(p_cfg, p_cfg->p_transfer_rx, SSP_SIGNAL_SCI_RXI, &cfg);
        SCI_UART_ERROR_RETURN(SSP_SUCCESS == err, err);
        transfer_info_t * p_info = p_cfg->p_transfer_rx->p_cfg->p_info;
        if (p_ctrl->fifo_depth > 0U)
        {
            p_info->p_src = HW_SCI_ReadAddrGet(p_ctrl->p_reg, p_ctrl->data_bytes);
        }
        else
        {
            p_info->p_src = HW_SCI_Non_FIFO_ReadAddrGet(p_ctrl->p_reg, p_ctrl->data_bytes);
        }
        p_info->dest_addr_mode = TRANSFER_ADDR_MODE_INCREMENTED;
        p_info->src_addr_mode = TRANSFER_ADDR_MODE_FIXED;
        err = p_cfg->p_transfer_rx->p_api->open(p_cfg->p_transfer_rx->p_ctrl, &cfg);
        SCI_UART_ERROR_RETURN(SSP_SUCCESS == err, err);
    }
#endif
#if (SCI_UART_CFG_TX_ENABLE)
    /** If a transfer instance is used for transmission, apply UART specific settings and open the transfer instance. */
    if (NULL != p_cfg->p_transfer_tx)
    {
        transfer_cfg_t cfg = *(p_cfg->p_transfer_tx->p_cfg);
        err = r_sci_uart_transfer_configure(p_cfg, p_cfg->p_transfer_tx, SSP_SIGNAL_SCI_TXI, &cfg);
        SCI_UART_ERROR_RETURN(SSP_SUCCESS == err, err);
        transfer_info_t * p_info = p_cfg->p_transfer_tx->p_cfg->p_info;
        if (p_ctrl->fifo_depth > 0U)
        {
            p_info->p_dest = HW_SCI_WriteAddrGet(p_ctrl->p_reg, p_ctrl->data_bytes);
        }
        else
        {
            p_info->p_dest = HW_SCI_Non_FIFO_WriteAddrGet(p_ctrl->p_reg, p_ctrl->data_bytes);
        }
        p_info->dest_addr_mode = TRANSFER_ADDR_MODE_FIXED;
        p_info->src_addr_mode = TRANSFER_ADDR_MODE_INCREMENTED;
        err = p_cfg->p_transfer_tx->p_api->open(p_cfg->p_transfer_tx->p_ctrl, &cfg);
        SCI_UART_ERROR_RETURN(SSP_SUCCESS == err, err);
    }
#endif
    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Configures Driver enable pin based on user configuration of supported communication modes.
 *
 * @param[in,out]     p_ctrl  Pointer to UART control structure
 **********************************************************************************************************************/
static void r_sci_uart_rs485_de_pin_cfg (sci_uart_instance_ctrl_t * const p_ctrl)
{
    /** Configure the Driver Enable pin accordingly for rs485 communication modes */
    if (UART_MODE_RS485 == p_ctrl->uart_comm_mode)
    {
        if (UART_RS485_HD == p_ctrl->uart_rs485_mode)
        {
            /** For RS485 Half duplex mode, the Driver Enable should be low to be able to receive asynchronously */
            g_ioport_on_ioport.pinWrite (p_ctrl->rs485_de_pin, IOPORT_LEVEL_LOW);
        }
        else
        {
            g_ioport_on_ioport.pinWrite (p_ctrl->rs485_de_pin, IOPORT_LEVEL_HIGH);
        }
    }
}

/*******************************************************************************************************************//**
 * Configures UART related registers based on user configurations.
 *
 * @param[in]     p_ctrl  Pointer to UART control structure
 * @param[in]     p_cfg   Pointer to UART specific configuration structure
 **********************************************************************************************************************/
static void r_sci_uart_config_set (sci_uart_instance_ctrl_t * const p_ctrl,
                                   uart_cfg_t         const * const p_cfg)
{
    /** Configure FIFO related registers. */
    r_sci_uart_fifo_cfg(p_ctrl, p_cfg);

    /** Configure parity. */
    R_SCI0_Type * p_sci_reg = (R_SCI0_Type *) p_ctrl->p_reg;
    HW_SCI_ParitySet(p_sci_reg, p_cfg->parity);

    /** Configure data size. */
    if (UART_DATA_BITS_7 == p_cfg->data_bits)
    {
        HW_SCI_DataBits7bitsSelect(p_sci_reg);
    }
    else if (UART_DATA_BITS_9 == p_cfg->data_bits)
    {
        HW_SCI_DataBits9bitsSelect(p_sci_reg);
    }
    else
    {
        /* Do nothing.  Default is 8-bit mode. */
    }

    /** Configure stop bits. */
    HW_SCI_StopBitsSelect(p_sci_reg, p_cfg->stop_bits);

    /** Configure CTS flow control if CTS/RTS flow control is enabled. */
    if (p_cfg->ctsrts_en)
    {
        HW_SCI_CtsInEnable(p_sci_reg);
    }

    uart_on_sci_cfg_t * p_extend = (uart_on_sci_cfg_t *) p_cfg->p_extend;
    if (NULL != p_extend)
    {
        /** Starts reception on falling edge of RXD if enabled in extension (otherwise reception starts at low level
         * of RXD). */
        HW_SCI_StartBitSet(p_sci_reg, p_extend->rx_edge_start);

        /** Enables the noise cancellation, fixed to the minimum level, if enabled in the extension. */
        HW_SCI_NoiseFilterSet(p_sci_reg, p_extend->noisecancel_en);

        /** Enables Baud clock output if configured in the extension. */
        HW_SCI_BaudClkOutputEnable(p_sci_reg, p_extend->baudclk_out);
    }
}  /* End of function r_sci_uart_config_set() */

/*******************************************************************************************************************//**
 * This function enables external RTS operation (using a GPIO).
 *
 * @param[in] p_ctrl  Pointer to UART instance control
 * @param[in] p_cfg   Pointer to UART configuration structure
 **********************************************************************************************************************/
#if (SCI_UART_CFG_EXTERNAL_RTS_OPERATION)
static void r_sci_uart_external_rts_operation_enable (sci_uart_instance_ctrl_t * const p_ctrl,
                                                      uart_cfg_t         const * const p_cfg)
{
    uart_on_sci_cfg_t * p_extend = (uart_on_sci_cfg_t *) p_cfg->p_extend;
    if (NULL != p_extend)
    {
        p_ctrl->p_extpin_ctrl = p_extend->p_extpin_ctrl;

        /** If CTS/RTS flow control is enabled and an RTS flow control callback is provided, call the RTS flow control
         * callback to assert the user GPIO RTS pin. */
        if ((p_cfg->ctsrts_en) && (p_ctrl->p_extpin_ctrl))
        {
            p_ctrl->p_extpin_ctrl(p_cfg->channel, 0);
        }
    }
}  /* End of function r_sci_uart_external_rts_operation_enable () */
#endif /* if (SCI_UART_CFG_EXTERNAL_RTS_OPERATION) */

/*******************************************************************************************************************//**
 * Resets FIFO related registers.
 *
 * @param[in] p_ctrl  Pointer to UART instance control
 * @param[in] p_cfg   Pointer to UART configuration structure
 **********************************************************************************************************************/
static void r_sci_uart_fifo_cfg (sci_uart_instance_ctrl_t * const p_ctrl,
                                 uart_cfg_t         const * const p_cfg)
{
    R_SCI0_Type * p_sci_reg = (R_SCI0_Type *) p_ctrl->p_reg;

    if (0U != p_ctrl->fifo_depth)
    {
        HW_SCI_FifoEnable(p_sci_reg);
#if (SCI_UART_CFG_RX_ENABLE)
        /** Reset receive FIFO. */
        HW_SCI_ReceiveFifoReset(p_sci_reg);

        if (NULL != p_cfg->p_transfer_rx)
        {
            /** Set receive trigger number to 0 if DTC is used. */
            HW_SCI_RxTriggerNumberSet(p_sci_reg, 0);
        }
        else
        {
            /** Otherwise, set receive trigger number as configured by the user. */
            uint16_t fifo_trigger_mask = (uint16_t) (p_ctrl->fifo_depth - 1U);
            uart_on_sci_cfg_t const * p_extend = p_cfg->p_extend;
            if (NULL != p_extend)
            {
                uint16_t rx_fifo_setting = (uint16_t) (fifo_trigger_mask & (uint16_t) p_extend->rx_fifo_trigger);
                HW_SCI_RxTriggerNumberSet(p_sci_reg, rx_fifo_setting);
            }
            else
            {
                /** Default is largest setting to reduce number of interrupts. */
                HW_SCI_RxTriggerNumberSet(p_sci_reg, fifo_trigger_mask);
            }
        }
        HW_SCI_RTSTriggerNumberSet(p_sci_reg, (uint16_t) (p_ctrl->fifo_depth - 1U));
#else
        SSP_PARAMETER_NOT_USED(p_cfg);
#endif
#if (SCI_UART_CFG_TX_ENABLE)
        /** Resets transmit FIFO. */
        HW_SCI_TransmitFifoReset(p_sci_reg);

        /** Set FIFO trigger to trigger when the FIFO is empty. */
        HW_SCI_TxTriggerNumberSet(p_sci_reg, 0U);
#endif
    }
}  /* End of function r_sci_uart_fifo_cfg() */

/*******************************************************************************************************************//**
 * Sets interrupt priority and initializes vector info.
 *
 * @param[in]     p_feature  SSP feature
 * @param[in]     signal     SSP signal ID
 * @param[in]     ipl        Interrupt priority level
 * @param[in]     p_ctrl     Pointer to driver control block
 * @param[out]    p_irq      Pointer to IRQ for this signal, set here
 *
 * @retval        SSP_SUCCESS               Interrupt enabled
 * @retval        SSP_ERR_IRQ_BSP_DISABLED  Interrupt does not exist in the vector table
 *
 * @return                       See @ref Common_Error_Codes or functions called by this function for other possible
 *                               return codes. This function calls:
 *                                   * fmi_api_t::eventInfoGet
 **********************************************************************************************************************/
static ssp_err_t r_sci_irq_cfg(ssp_feature_t * const p_feature,
                               ssp_signal_t    const signal,
                               uint8_t         const ipl,
                               void          * const p_ctrl,
                               IRQn_Type     * const p_irq)
{
    /** Look up interrupt number. */
    fmi_event_info_t event_info = {(IRQn_Type) 0U};
    ssp_vector_info_t * p_vector_info;
    ssp_err_t err = g_fmi_on_fmi.eventInfoGet(p_feature, signal, &event_info);
    IRQn_Type irq = event_info.irq;
    *p_irq = irq;

    /** If interrupt is registered in the vector table, disable interrupts, set priority, and store control block in
     * the vector information so it can be accessed from the callback. */
    if (SSP_SUCCESS == err)
    {
        NVIC_DisableIRQ(irq);
        R_BSP_IrqStatusClear(irq);
        NVIC_SetPriority(irq, ipl);
        R_SSP_VectorInfoGet(irq, &p_vector_info);
        *(p_vector_info->pp_ctrl) = p_ctrl;
    }

    return err;
}

/*******************************************************************************************************************//**
 * Sets interrupt priority and initializes vector info for all interrupts.
 *
 * @param[in]     p_feature  SSP feature
 * @param[in]     p_ctrl     Pointer to UART instance control block
 * @param[in]     p_cfg      Pointer to UART specific configuration structure
 *
 * @retval        SSP_SUCCESS               Interrupt enabled
 * @retval        SSP_ERR_IRQ_BSP_DISABLED  Interrupt does not exist in the vector table
 *
 * @return                       See @ref Common_Error_Codes or functions called by this function for other possible
 *                               return codes. This function calls:
 *                                   * fmi_api_t::eventInfoGet
 **********************************************************************************************************************/
static ssp_err_t r_sci_irqs_cfg(ssp_feature_t            * const p_feature,
                                sci_uart_instance_ctrl_t * const p_ctrl,
                                uart_cfg_t         const * const p_cfg)
{
    ssp_err_t err;
#if (SCI_UART_CFG_RX_ENABLE)
    /* ERI is optional. */
    r_sci_irq_cfg(p_feature, SSP_SIGNAL_SCI_ERI, p_cfg->eri_ipl, p_ctrl, &p_ctrl->eri_irq);
    err = r_sci_irq_cfg(p_feature, SSP_SIGNAL_SCI_RXI, p_cfg->rxi_ipl, p_ctrl, &p_ctrl->rxi_irq);
    SCI_UART_ERROR_RETURN(SSP_SUCCESS == err, err);
#endif
#if (SCI_UART_CFG_TX_ENABLE)
    err = r_sci_irq_cfg(p_feature, SSP_SIGNAL_SCI_TXI, p_cfg->txi_ipl, p_ctrl, &p_ctrl->txi_irq);
    SCI_UART_ERROR_RETURN(SSP_SUCCESS == err, err);

    err = r_sci_irq_cfg(p_feature, SSP_SIGNAL_SCI_TEI, p_cfg->tei_ipl, p_ctrl, &p_ctrl->tei_irq);
    SCI_UART_ERROR_RETURN(SSP_SUCCESS == err, err);
#endif
    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Closes transfer interfaces.
 *
 * @param[in]     p_ctrl     Pointer to UART instance control block
 **********************************************************************************************************************/
static void r_sci_uart_transfer_close (sci_uart_instance_ctrl_t * p_ctrl)
{
#if (SCI_UART_CFG_RX_ENABLE)
    if ((NULL != p_ctrl->p_transfer_rx) && (NULL != p_ctrl->p_transfer_rx->p_api))
    {
        p_ctrl->p_transfer_rx->p_api->close(p_ctrl->p_transfer_rx->p_ctrl);
    }
#endif
#if (SCI_UART_CFG_TX_ENABLE)
    if ((NULL != p_ctrl->p_transfer_tx) && (NULL != p_ctrl->p_transfer_tx->p_api))
    {
        p_ctrl->p_transfer_tx->p_api->close(p_ctrl->p_transfer_tx->p_ctrl);
    }
#endif
}

/*******************************************************************************************************************//**
 * Calculates baud rate register settings based on internal clock frequency. Evaluates and determines the best possible
 * settings set to the baud rate related registers.  Parameter checking must be done before this function is called.
 *
 * @param[in]  freq_hz                             The source clock frequency for the SCI internal clock
 * @param[in]  baudrate                            Baud rate[bps] e.g. 19200, 57600, 115200, etc.
 * @param[out] p_brr_value                         Bit rate register value stored here if successful
 * @param[out] pp_baud_setting                     Baud setting information stored here if successful
 * @param[out] p_mddr                              MDDR register value stored here if successful
 * @param[out] select_16_base_clk_cycles           If True; Only find solutions for when abcs and abcse are equal to 0
 *
 * @return  bit error (percent * 1000, or percent to 3 decimal places)
 **********************************************************************************************************************/
static int32_t r_sci_uart_brr_mddr_calculate(uint32_t                freq_hz,
                                             uint32_t                baudrate,
                                             uint8_t               * p_brr_value,
                                             baud_setting_t const ** pp_baud_setting,
                                             uint32_t              * p_mddr, 
                                             bool                    select_16_base_clk_cycles)
{
    /** Find the best BRR (bit rate register) value.
     *  In table async_baud, divisor values are stored for BGDM, ABCS, ABCSE and N values.  Each set of divisors
     *  is tried, and the settings with the lowest bit rate error are stored. The formula to calculate BRR is as
     *  follows and it must be 255 or less:
     *  BRR = (PCLK / (div_coefficient * baud)) - 1
     */
    baud_setting_t const * p_baudinfo = &async_baud[0];
    int32_t  hit_bit_err = SCI_UART_100_PERCENT_X_1000;
    uint32_t hit_mddr = 0U;
    uint32_t divisor = 0U;
    for (uint32_t i = 0U; i < SCI_UART_NUM_DIVISORS_ASYNC; i++)
    {
        /** if select_16_base_clk_cycles == true:  Skip this calculation for divisors that are not acheivable with 16 base clk cycles per bit.
         *  if select_16_base_clk_cycles == false: Skip this calculation for divisors that are only acheivable without 16 base clk cycles per bit.
         */
        if ((!select_16_base_clk_cycles) ^ (p_baudinfo[i].abcs | p_baudinfo[i].abcse))
        {
            continue;
        }

        divisor = (uint32_t) p_baudinfo[i].div_coefficient * baudrate;
        uint32_t temp_brr = freq_hz / divisor;

        if (temp_brr <= (SCI_UART_BRR_MAX + 1U))
        {
            while (temp_brr > 0U)
            {
                temp_brr -= 1U;

                /** Calculate the bit rate error. The formula is as follows:
                 *  bit rate error[%] = {(PCLK / (baud * div_coefficient * (BRR + 1)) - 1} x 100
                 *  calculates bit rate error[%] to three decimal places
                 */
                int32_t err_divisor = (int32_t) (divisor * (temp_brr + 1U));

                /* Promoting to 64 bits for calculation, but the final value can never be more than 32 bits, as
                 * described below, so this cast is safe.
                 *    1. (temp_brr + 1) can be off by an upper limit of 1 due to rounding from the calculation:
                 *       freq_hz / divisor, or:
                 *       freq_hz / divisor <= (temp_brr + 1) < (freq_hz / divisor) + 1
                 *    2. Solving for err_divisor:
                 *       freq_hz <= err_divisor < freq_hz + divisor
                 *    3. Solving for bit_err:
                 *       0 >= bit_err >= (freq_hz * 100000 / (freq_hz + divisor)) - 100000
                 *    4. freq_hz >= divisor (or temp_brr would be -1 and we would never enter this while loop), so:
                 *       0 >= bit_err >= 100000 / freq_hz - 100000
                 *    5. Larger frequencies yield larger bit errors (absolute value).  As the frequency grows,
                 *       the bit_err approaches -100000, so:
                 *       0 >= bit_err >= -100000
                 *    6. bit_err is between -100000 and 0.  This entire range fits in an int32_t type, so the cast
                 *       to (int32_t) is safe.
                 */
                int32_t bit_err   = (int32_t) (((((int64_t) freq_hz) * SCI_UART_100_PERCENT_X_1000) /
                                    err_divisor) - SCI_UART_100_PERCENT_X_1000);

                uint32_t mddr = 0U;
                if (NULL != p_mddr)
                {
                    /** Calculate the MDDR (M) value if bit rate modulation is enabled,
                     * The formula to calculate MBBR (from the M and N relationship given in the hardware manual) is as follows
                     * and it must be between 128 and 256.
                     * MDDR = ((div_coefficient * baud * 256) * (BRR + 1)) / PCLK */
                    mddr = (uint32_t) err_divisor / (freq_hz / SCI_UART_MDDR_MAX);

                    /* The maximum value that could result from the calculation above is 256, which is a valid MDDR
                     * value, so only the lower bound is checked. */
                    if (mddr < SCI_UART_MDDR_MIN)
                    {
                        break;
                    }

                    /** Adjust bit rate error for bit rate modulation. The following formula is used:
                     *  bit rate error [%] = ((bit rate error [%, no modulation] + 100) * MDDR / 256) - 100
                     */
                    bit_err = (((bit_err + SCI_UART_100_PERCENT_X_1000) * (int32_t) mddr) /
                              SCI_UART_MDDR_DIVISOR) - SCI_UART_100_PERCENT_X_1000;
                }

                /** Take the absolute value of the bit rate error. */
                if (bit_err < 0)
                {
                    bit_err = -bit_err;
                }

                /** If the absolute value of the bit rate error is less than the previous lowest absolute value of
                 *  bit rate error, then store these settings as the best value.
                 */
                if (bit_err < hit_bit_err)
                {
                    *pp_baud_setting = &p_baudinfo[i];
                    *p_brr_value     = (uint8_t) temp_brr;
                    hit_bit_err      = bit_err;
                    hit_mddr         = mddr;
                }
                if (NULL == p_mddr)
                {
                    break;
                }
                else
                {
                    *p_mddr = hit_mddr;
                }
            }
        }
    }

    return hit_bit_err;
}

/*******************************************************************************************************************//**
 * Calculates baud rate register settings. Evaluates and determines the best possible settings set to the baud rate
 * related registers.
 *
 * @param[in]  clk_src               A clock source for SCI module
 * @param[in]  baudrate              Baud rate[bps] e.g. 19200, 57600, 115200, etc.
 * @param[out] p_brr_value           Bit rate register value stored here if successful
 * @param[out] pp_baud_setting       Baud setting information stored here if successful
 * @param[out] p_mddr                MDDR register value stored here if successful
 * @param[in]  baud_rate_error_x_1000 \<baud_rate_percent_error\> x 1000 required for module to function. Absolute max baud_rate_error is 15000(15%).
 *
 * @retval  SSP_SUCCESS                  Baud rate is set successfully
 * @retval  SSP_ERR_INVALID_ARGUMENT     Baud rate is '0', source clock frequency could not be read, or error in
 *                                       calculated baud rate is larger than 10%.
 *
 * @return                       See @ref Common_Error_Codes or functions called by this function for other possible
 *                               return codes. This function calls:
 *                                   * cgc_api_t::systemClockFreqGet
 **********************************************************************************************************************/
static ssp_err_t r_sci_uart_baud_calculate(sci_clk_src_t           clk_src,
                                           uint32_t                baudrate,
                                           uint8_t               * p_brr_value,
                                           baud_setting_t const ** pp_baud_setting,
                                           uint32_t              * p_mddr,
                                           uint32_t                baud_rate_error_x_1000)
{
    *p_brr_value     = SCI_UART_BRR_MAX;
    *pp_baud_setting = &async_baud[0];

    if (SCI_CLK_SRC_INT == clk_src)
    {
#if (SCI_UART_CFG_PARAM_CHECKING_ENABLE)
        SCI_UART_ERROR_RETURN(SCI_UART_MAX_BAUD_RATE_ERROR_X_1000 > baud_rate_error_x_1000, SSP_ERR_INVALID_ARGUMENT);
        SCI_UART_ERROR_RETURN((0U != baudrate), SSP_ERR_INVALID_ARGUMENT);
#endif

        bsp_feature_sci_t sci_feature = {0U};
        R_BSP_FeatureSciGet(&sci_feature);
        uint32_t freq_hz = 0U;
        ssp_err_t err = g_cgc_on_cgc.systemClockFreqGet((cgc_system_clocks_t) sci_feature.clock, &freq_hz);
        SCI_UART_ERROR_RETURN(SSP_SUCCESS == err, SSP_ERR_INVALID_ARGUMENT);

        /** Try to get accurate baudrate using 16 base clk cycles per bit */
        int32_t  hit_bit_err = r_sci_uart_brr_mddr_calculate(freq_hz, baudrate, p_brr_value, pp_baud_setting, p_mddr, true);

        /** If the clock is not accurate enough, try with different base clk cycles per bit */
        if (hit_bit_err > ((int32_t) baud_rate_error_x_1000))
        {
            hit_bit_err = r_sci_uart_brr_mddr_calculate(freq_hz, baudrate, p_brr_value, pp_baud_setting, p_mddr, false);
        }

        /** Return an error if the percent error is larger than the maximum percent error allowed for this instance */
        SCI_UART_ERROR_RETURN((hit_bit_err <= (int32_t) baud_rate_error_x_1000), SSP_ERR_INVALID_ARGUMENT);
    }

    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Changes baud rate based on predetermined register settings.
 *
 * @param[in] p_sci_reg       Base pointer for SCI registers
 * @param[in] clk_src         A clock source for SCI module
 * @param[in] brr             Bit rate register setting
 * @param[in] p_baud_setting  Pointer to other divisor related settings
 * @param[in] p_mddr          Pointer to bit rate modulation register setting, NULL if not used.
 *
 * @note   The transmitter and receiver (TE and RE bits in SCR) must be disabled prior to calling this function.
 **********************************************************************************************************************/
static void r_sci_uart_baud_set (R_SCI0_Type          *       p_sci_reg,
                                 sci_clk_src_t          const clk_src,
                                 uint8_t                const brr,
                                 baud_setting_t const * const p_baud_setting,
                                 uint32_t       const * const p_mddr)
{
    if (SCI_CLK_SRC_INT == clk_src)
    {
        HW_SCI_BaudRateGenInternalClkSelect(p_sci_reg);

        HW_SCI_UartBitRateSet(p_sci_reg, brr, p_baud_setting);

        /* Check Bitrate Modulation function is enabled or not.
         * If it is enabled,set the MDDR register to correct the bit rate generated by the on-chip baud rate generator */
        if (NULL != p_mddr)
        {
            /* Set MDDR register only for values between 128 and 256, do not set otherwise */
            uint32_t mddr = *p_mddr;
            if (mddr < SCI_UART_MDDR_MAX)
            {
                HW_SCI_BitRateModulationEnable(p_sci_reg, true);
                HW_SCI_UartBitRateModulationSet(p_sci_reg, (uint8_t) mddr);
            }
            else
            {
                HW_SCI_BitRateModulationEnable(p_sci_reg, false);
            }
        }
    }
    else
    {
        HW_SCI_BitRateDefaultSet(p_sci_reg);

        HW_SCI_BaudRateGenExternalClkSelect(p_sci_reg);

        if (SCI_CLK_SRC_EXT8X == clk_src)
        {
            HW_SCI_BaudRateGenExternalClkDivideBy8(p_sci_reg);
        }
    }
}  /* End of function r_sci_uart_baud_set() */

#if (SCI_UART_CFG_TX_ENABLE)
/*******************************************************************************************************************//**
 * Set up the Uart Write if DTC is selected.
 *
 * @param[in] p_ctrl   Pointer to the control block for the channel
 * @param[in] p_src    Source to transfer data
 * @param[in] bytes    Number of bytes to transfer
 **********************************************************************************************************************/
static ssp_err_t r_sci_uart_write_transfer_setup(sci_uart_instance_ctrl_t * const p_ctrl,
                                                 uint8_t             const * const p_src,
                                                 uint32_t                    const bytes)
{
    ssp_err_t err = SSP_SUCCESS;
    uint32_t data_bytes = p_ctrl->data_bytes;
    p_ctrl->tx_src_bytes = 0U;
    uint32_t num_transfers =  bytes >> (data_bytes - 1);
#if (SCI_UART_CFG_PARAM_CHECKING_ENABLE)
    transfer_properties_t transfer_max = {0U};
    p_ctrl->p_transfer_tx->p_api->infoGet(p_ctrl->p_transfer_tx->p_ctrl, &transfer_max);
    SSP_ASSERT(num_transfers <= transfer_max.transfer_length_max);
#endif
    void const * p_transfer_src = p_src;
    bool do_not_reset = false;
    if (0U == p_ctrl->fifo_depth)
    {
        /* If the channel has no FIFO, the first byte is sent later in this function to trigger the TXI event to
         * start the DTC, so skip the first byte when configuring the transfer. */
        p_ctrl->tx_src_bytes = data_bytes;
        p_transfer_src = (void const *) ((uint32_t) p_src + data_bytes);
        num_transfers--;
        if (0U == num_transfers)
        {
            do_not_reset = true;
        }
    }
    if (!do_not_reset)
    {
        err = p_ctrl->p_transfer_tx->p_api->reset(p_ctrl->p_transfer_tx->p_ctrl, p_transfer_src,
                                                  NULL, (uint16_t) num_transfers);
        SCI_UART_ERROR_RETURN(SSP_SUCCESS == err, err);
    }
    return err;
}

/*******************************************************************************************************************//**
 * Writes the next byte (or 2 bytes if 9-bit mode is selected) of data to the transmit register.
 *
 * @param[in] p_ctrl   Pointer to the control block for the channel
 **********************************************************************************************************************/
void r_sci_uart_write_no_transfer(sci_uart_instance_ctrl_t * const p_ctrl)
{
    if (0U == p_ctrl->tx_src_bytes)
    {
        /* Nothing to write. */
        return;
    }

    /** Locate next data to transmit (2 bytes for 9-bit mode, 1 byte otherwise). */
    uint16_t data = 0U;
    if (SCI_UART_DATA_SIZE_2_BYTES == p_ctrl->data_bytes)
    {
        data = (uint16_t) *((uint16_t *) p_ctrl->p_tx_src);
    }
    else
    {
        data = (uint16_t) *(p_ctrl->p_tx_src);
    }

    /** Write the data to the FIFO if the channel has a FIFO.  Otherwise write data based on size to the transmit
     * register.  Write to 16-bit TDRHL for 9-bit data, or 8-bit TDR otherwise. */
    R_SCI0_Type * p_sci_reg = (R_SCI0_Type *) p_ctrl->p_reg;
    if (p_ctrl->fifo_depth > 0U)
    {
        HW_SCI_WriteFIFO(p_sci_reg, data);
    }
    else
    {
        if (SCI_UART_DATA_SIZE_2_BYTES == p_ctrl->data_bytes)
        {
            HW_SCI_Write9bits (p_sci_reg, data);
        }
        else
        {
            HW_SCI_Write (p_sci_reg, (uint8_t) data);
        }
    }

    /** Update pointer to the next data and number of remaining bytes in the control block. */
    p_ctrl->tx_src_bytes -= p_ctrl->data_bytes;
    p_ctrl->p_tx_src += p_ctrl->data_bytes;
}
#endif /* if (SCI_UART_CFG_TX_ENABLE) */

#if (SCI_UART_CFG_RX_ENABLE)
/*******************************************************************************************************************//**
 * Reads the requested number of data (2 bytes for 9-bit mode, 1 byte otherwise) and calls the user callback with event
 * UART_EVENT_RX_CHAR for each.
 *
 * @param[in]  p_ctrl   Pointer to the control block for the channel
 * @param[out] p_data   Data read from SCI stored here
 **********************************************************************************************************************/
void r_sci_uart_read_data(sci_uart_instance_ctrl_t * const p_ctrl,
                          uint32_t                 * const p_data)
{
    R_SCI0_Type * p_sci_reg = (R_SCI0_Type *) p_ctrl->p_reg;
    if (p_ctrl->fifo_depth > 0U)
    {
        *p_data = HW_SCI_ReadFIFO(p_sci_reg);
    }
    else
    {
        if (SCI_UART_DATA_SIZE_2_BYTES == p_ctrl->data_bytes)
        {
            *p_data = HW_SCI_Read9bits (p_sci_reg);
        }
        else
        {
            *p_data = HW_SCI_Read (p_sci_reg);
        }
    }
}
#endif /* if (SCI_UART_CFG_RX_ENABLE) */

#if (SCI_UART_CFG_RX_ENABLE)
/*******************************************************************************************************************//**
 * Reads the requested number of data (2 bytes for 9-bit mode, 1 byte otherwise) and calls the user callback with event
 * UART_EVENT_RX_CHAR for each.
 *
 * @param[in] p_ctrl   Pointer to the control block for the channel
 **********************************************************************************************************************/
void r_sci_uart_rxi_read_no_transfer(sci_uart_instance_ctrl_t * const p_ctrl)
{
    /** Determine how much data to read based on how many slots of the read FIFO are filled. */
    R_SCI0_Type * p_sci_reg = (R_SCI0_Type *) p_ctrl->p_reg;
    uint32_t data = 0;

    if (p_ctrl->fifo_depth > 0U)
    {
        do{
            /** Clear Receive Data Ready Flag (DR) */
            HW_SCI_DRClear(p_sci_reg);

            /** Read all the FIFO data when callback is configured */
            if (NULL != p_ctrl->p_callback)
            {
                /** Read all data in the FIFO or receive register. */
                while (HW_SCI_FIFO_ReadCount(p_sci_reg) > 0U)
                {
                    data = HW_SCI_ReadFIFO(p_sci_reg);
                    r_sci_uart_rxi_read_no_transfer_eventset(p_ctrl, data);
                }
            }

            else
            {
                /** Dummy read to flush the FIFO when callback is not configured */
                while (HW_SCI_FIFO_ReadCount(p_sci_reg) > 0U)
                {
                    HW_SCI_ReadFIFO(p_sci_reg);
                }
            }
            /** Clear receive FIFO data full flag (RDF) */
            HW_SCI_RDFClear(p_sci_reg);
            /* Read receive data from FIFO, until DR flag become zero.*/
        }while(HW_SCI_DRBitGet(p_sci_reg));
    }
    else
    {
        r_sci_uart_read_data(p_ctrl, &data);
        /** Call user callback with the data. */
        if (NULL != p_ctrl->p_callback)
        {
            r_sci_uart_rxi_read_no_transfer_eventset(p_ctrl, data);
        }
    }
}
#endif /* if (SCI_UART_CFG_RX_ENABLE) */

#if (SCI_UART_CFG_RX_ENABLE)
/*******************************************************************************************************************//**
 * Calls the user callback with event.
 * When read API called --> invokes callback with UART_EVENT_RX_COMPLETE event after receiving the expected bytes.
 * When callback is used to receive data --> invokes callback with UART_EVENT_RX_CHAR event after each byte.
 * @param[in] p_ctrl   Pointer to the control block for the channel
 * @param[out] data   Data read from SCI stored here
 **********************************************************************************************************************/
void r_sci_uart_rxi_read_no_transfer_eventset(sci_uart_instance_ctrl_t * const p_ctrl,
                                              uint32_t                   const data)
{
    uart_callback_args_t args;

    /** Initialize the immutable data fields **/
    args.channel = p_ctrl->channel;
    args.p_context = p_ctrl->p_context;
    args.event = UART_EVENT_RX_CHAR;

    /* If read API is not called then invoke callback with UART_EVENT_RX_CHAR event for each byte */
    if(p_ctrl->rx_dst_bytes == 0U)
    {
        args.data = data;
        args.event = UART_EVENT_RX_CHAR;
        p_ctrl->p_callback(&args);
    }
    /* If read API is called then invoke callback with UART_EVENT_RX_COMPLETE event after receiving the expected bytes.*/
    else if(p_ctrl->rx_bytes_count < p_ctrl->rx_dst_bytes)
    {
        if (SCI_UART_DATA_SIZE_2_BYTES == p_ctrl->data_bytes)
        {   /* 9 bit data mode of operation */
            p_ctrl->rx_bytes_count += p_ctrl->data_bytes;
            *(p_ctrl->p_rx_dst) = data & 0xFF;
            p_ctrl->p_rx_dst += 1;
            *(p_ctrl->p_rx_dst) = (data >> 8) & 0xFF;
            p_ctrl->p_rx_dst += 1;
        }
        else
        {
            /* 7 or 8 bit data mode of operation */
            p_ctrl->rx_bytes_count += p_ctrl->data_bytes;
            *(p_ctrl->p_rx_dst) = data & 0xFF;
            p_ctrl->p_rx_dst += 1;
        }

        if(p_ctrl->rx_bytes_count == p_ctrl->rx_dst_bytes)
        {
            /* Invoke callback with UART_EVENT_RX_COMPLETE event once the expected bytes are received*/
            args.data = 0U;
            args.event = UART_EVENT_RX_COMPLETE;
            p_ctrl->rx_bytes_count = 0U;
            p_ctrl->rx_dst_bytes = 0U;
            p_ctrl->p_callback(&args);
        }
    }
}
#endif /* if (SCI_UART_CFG_RX_ENABLE) */

#if (SCI_UART_CFG_TX_ENABLE)
/*******************************************************************************************************************//**
 * Abort ongoing transmission by disabling irq and DTC, Reset the FIFO
 *
 * @param[in] p_ctrl              Pointer to UART instance control
 *
 * @retval  SSP_SUCCESS           Transmission aborted successfully
 *
 * @return                       See @ref Common_Error_Codes or functions called by this function for other possible
 *                               return codes. This function calls:
 *                                   * transfer_api_t::disable
 **********************************************************************************************************************/
static ssp_err_t r_sci_uart_abort_tx(sci_uart_instance_ctrl_t * p_ctrl)
{
    ssp_err_t err = SSP_SUCCESS;
    HW_SCI_UartTransmitIrqDisable(p_ctrl->p_reg);
    R_BSP_IrqStatusClear(p_ctrl->txi_irq);
    NVIC_DisableIRQ(p_ctrl->txi_irq);
    NVIC_ClearPendingIRQ(p_ctrl->txi_irq);
    R_BSP_IrqStatusClear(p_ctrl->tei_irq);
    NVIC_DisableIRQ(p_ctrl->tei_irq);
    NVIC_ClearPendingIRQ(p_ctrl->tei_irq);
    if (NULL != p_ctrl->p_transfer_tx)
    {
        err = p_ctrl->p_transfer_tx->p_api->disable(p_ctrl->p_transfer_tx->p_ctrl);
    }
    if (0U != p_ctrl->fifo_depth)
    {
        HW_SCI_TransmitFifoReset(p_ctrl->p_reg);
    }
    p_ctrl->p_tx_src = NULL;
    p_ctrl->tx_src_bytes = 0U;
    /** Clear the Driver Enable pin in RS485 half duplex mode to enable reception */
    r_sci_uart_rs485_de_pin_cfg(p_ctrl);
    return err;
}
#endif

#if (SCI_UART_CFG_RX_ENABLE)
/*******************************************************************************************************************//**
 * Abort ongoing reception by disabling DTC and Reset the FIFO
 *
 * @param[in] p_ctrl                 Pointer to UART instance control
 *
 * @retval  SSP_SUCCESS              Reception aborted successfully
 *
 * @return                       See @ref Common_Error_Codes or functions called by this function for other possible
 *                               return codes. This function calls:
 *                                   * transfer_api_t::disable
 **********************************************************************************************************************/
static ssp_err_t r_sci_uart_abort_rx(sci_uart_instance_ctrl_t * p_ctrl)
{
    ssp_err_t err = SSP_SUCCESS;
    if (NULL != p_ctrl->p_transfer_rx)
    {
        p_ctrl->rx_transfer_in_progress = 0U;
        err = p_ctrl->p_transfer_rx->p_api->disable(p_ctrl->p_transfer_rx->p_ctrl);
    }
    else
    {
        /* Clear byte count and bytes when ongoing UART reception is aborted */
        p_ctrl->rx_bytes_count = 0U;
        p_ctrl->rx_dst_bytes = 0U;

    }
    if (0U != p_ctrl->fifo_depth)
    {
        HW_SCI_ReceiveFifoReset(p_ctrl->p_reg);
    }
    return err;
}
#endif

#if (SCI_UART_CFG_TX_ENABLE)
/*******************************************************************************************************************//**
 * TXI interrupt processing for UART mode. TXI interrupt fires when the data in the data register or FIFO register has
 * been transferred to the data shift register, and the next data can be written.  This interrupt writes the next data.
 * After the last data byte is written, this interrupt disables the TXI interrupt and enables the TEI (transmit end)
 * interrupt.
 **********************************************************************************************************************/
void sci_uart_txi_isr (void)
{
    /* Save context if RTOS is used */
    SF_CONTEXT_SAVE;

    /** Clear pending IRQ to make sure it doesn't fire again after exiting */
    R_BSP_IrqStatusClear(R_SSP_CurrentIrqGet());

    ssp_vector_info_t * p_vector_info = NULL;
    R_SSP_VectorInfoGet(R_SSP_CurrentIrqGet(), &p_vector_info);
    sci_uart_instance_ctrl_t * p_ctrl = (sci_uart_instance_ctrl_t *) *(p_vector_info->pp_ctrl);

    if (NULL != p_ctrl)
    {
        R_SCI0_Type * p_sci_reg = (R_SCI0_Type *) p_ctrl->p_reg;

        if (NULL == p_ctrl->p_transfer_tx)
        {
            /** If transfer is not used, write data until FIFO is full. */
            if (0U == p_ctrl->fifo_depth)
            {
                r_sci_uart_write_no_transfer(p_ctrl);
            }
            else
            {
                uint32_t fifo_filled = HW_SCI_FIFO_WriteCount(p_sci_reg);
                int32_t fifo_open = (int32_t) p_ctrl->fifo_depth - (int32_t) fifo_filled;
                for (int32_t cnt = 0; cnt < fifo_open; cnt++)
                {
                    r_sci_uart_write_no_transfer(p_ctrl);
                }
                HW_SCI_TDFEClear(p_sci_reg);
            }
        }
        if (0U == p_ctrl->tx_src_bytes)
        {
            /** After all data has been transmitted, disable transmit interrupts and enable the transmit end interrupt. */
            HW_SCI_TeIrqEnable(p_sci_reg, p_ctrl);
            p_ctrl->p_tx_src = NULL;
            uart_callback_args_t args;
            if (NULL != p_ctrl->p_callback)
            {
                args.channel   = p_ctrl->channel;
                args.data      = 0U;
                args.event     = UART_EVENT_TX_DATA_EMPTY;
                args.p_context = p_ctrl->p_context;
                p_ctrl->p_callback(&args);
            }
        }
    }

    /* Restore context if RTOS is used */
    SF_CONTEXT_RESTORE;
}  /* End of function sci_uart_txi_isr() */
#endif /* if (SCI_UART_CFG_TX_ENABLE) */

#if (SCI_UART_CFG_RX_ENABLE)
/*******************************************************************************************************************//**
 * RXI interrupt processing for UART mode. RXI interrupt happens when data arrives to the data register or the FIFO
 * register.  This function calls callback function when it meets conditions below.
 *  - UART_EVENT_RX_COMPLETE: The number of data which has been read reaches to the number specified in R_SCI_UartRead()
 *    if a transfer instance is used for reception.
 *  - UART_EVENT_RX_CHAR: Data is received asynchronously (read has not been called) or the transfer interface is not
 *    being used for reception.
 *
 * This interrupt also calls the callback function for RTS pin control if it is registered in R_SCI_UartOpen(). This is
 * special functionality to expand SCI hardware capability and make RTS/CTS hardware flow control possible. If macro
 * 'SCI_UART_CFG_EXTERNAL_RTS_OPERATION' is set, it is called at the beginning in this function to set the RTS pin high,
 * then it is it is called again just before leaving this function to set the RTS pin low.
 * @retval    none
 **********************************************************************************************************************/
void sci_uart_rxi_isr (void)
{
    /* Save context if RTOS is used */
    SF_CONTEXT_SAVE;

    /** Clear pending IRQ to make sure it doesn't fire again after exiting */
    R_BSP_IrqStatusClear(R_SSP_CurrentIrqGet());

    ssp_vector_info_t * p_vector_info = NULL;
    R_SSP_VectorInfoGet(R_SSP_CurrentIrqGet(), &p_vector_info);
    sci_uart_instance_ctrl_t * p_ctrl = (sci_uart_instance_ctrl_t *) *(p_vector_info->pp_ctrl);

    if (NULL != p_ctrl)
    {
        uint8_t channel = p_ctrl->channel;

#if (SCI_UART_CFG_EXTERNAL_RTS_OPERATION)
        /** If an external RTS callback is provided, call it to deassert RTS GPIO pin. */
        if (p_ctrl->p_extpin_ctrl)
        {
            p_ctrl->p_extpin_ctrl(channel, 1);
        }
#endif

        if (1U == p_ctrl->rx_transfer_in_progress)
        {
            /** If a transfer has completed, call callback with event UART_EVENT_RX_COMPLETE. */
            p_ctrl->rx_transfer_in_progress = 0U;

            /* Do callback if available */
            if (NULL != p_ctrl->p_callback)
            {
                uart_callback_args_t args;
                args.channel        = channel;
                args.data           = 0U;
                args.p_context      = p_ctrl->p_context;
                args.event = UART_EVENT_RX_COMPLETE;
                p_ctrl->p_callback(&args);
            }
        }
        else
        {
            /** If a character is received outside a transfer, call callback with event UART_EVENT_RX_CHAR. */
            r_sci_uart_rxi_read_no_transfer(p_ctrl);
        }
#if (SCI_UART_CFG_EXTERNAL_RTS_OPERATION)
        /** If an external RTS callback is provided, call it to assert the RTS GPIO pin. */
        if (p_ctrl->p_extpin_ctrl)
        {
            p_ctrl->p_extpin_ctrl(channel, 0);         /** user definition function call to control GPIO */
        }
#endif
    }

    /* Restore context if RTOS is used */
    SF_CONTEXT_RESTORE;
}  /* End of function sci_uart_rxi_isr () */
#endif /* if (SCI_UART_CFG_RX_ENABLE) */

#if (SCI_UART_CFG_TX_ENABLE)
/*******************************************************************************************************************//**
 * TEI interrupt processing for UART mode. The TEI interrupt fires after the last byte is transmitted on the TX pin.
 * The user callback function is called with the UART_EVENT_TX_COMPLETE event code (if it is registered in
 * R_SCI_UartOpen()).
 **********************************************************************************************************************/
void sci_uart_tei_isr (void)
{
    /* Save context if RTOS is used */
    SF_CONTEXT_SAVE;

    ssp_vector_info_t * p_vector_info = NULL;
    R_SSP_VectorInfoGet(R_SSP_CurrentIrqGet(), &p_vector_info);
    sci_uart_instance_ctrl_t * p_ctrl = (sci_uart_instance_ctrl_t *) *(p_vector_info->pp_ctrl);

    if (NULL != p_ctrl)
    {
        uint8_t channel = p_ctrl->channel;
        R_SCI0_Type * p_sci_reg = (R_SCI0_Type *) p_ctrl->p_reg;
        uart_callback_args_t args;

        /** Receiving TEI(transmit end interrupt) means the completion of transmission, so call callback function here. */
        HW_SCI_UartTransmitIrqDisable(p_sci_reg);
        NVIC_DisableIRQ(p_ctrl->tei_irq);
        if (NULL != p_ctrl->p_callback)
        {
            args.channel   = channel;
            args.data      = 0U;
            args.event     = UART_EVENT_TX_COMPLETE;
            args.p_context = p_ctrl->p_context;
            p_ctrl->p_callback(&args);
        }
    }

    /** Clear pending IRQ to make sure it doesn't fire again after exiting */
    R_BSP_IrqStatusClear(R_SSP_CurrentIrqGet());

    /** Clear the Driver Enable pin in RS485 half duplex mode to enable reception */
    r_sci_uart_rs485_de_pin_cfg(p_ctrl);

    /* Restore context if RTOS is used */
    SF_CONTEXT_RESTORE;
}  /* End of function sci_uart_tei_isr () */
#endif /* if (SCI_UART_CFG_TX_ENABLE) */

#if (SCI_UART_CFG_RX_ENABLE)
/*******************************************************************************************************************//**
 * ERI interrupt processing for UART mode. When an ERI interrupt fires, the user callback function is called if it is
 * registered in R_SCI_UartOpen() with the event code that triggered the interrupt.
 **********************************************************************************************************************/
void sci_uart_eri_isr (void)
{
    /* Save context if RTOS is used */
    SF_CONTEXT_SAVE;

    ssp_vector_info_t * p_vector_info = NULL;
    R_SSP_VectorInfoGet(R_SSP_CurrentIrqGet(), &p_vector_info);
    sci_uart_instance_ctrl_t * p_ctrl = (sci_uart_instance_ctrl_t *) *(p_vector_info->pp_ctrl);

    if (NULL != p_ctrl)
    {
        uint8_t channel = p_ctrl->channel;
        R_SCI0_Type * p_sci_reg = (R_SCI0_Type *) p_ctrl->p_reg;
        uint32_t             data = 0U;
        uart_callback_args_t args = {0U};

        /** Read data. */
        r_sci_uart_read_data(p_ctrl, &data);

        /** Determine cause of error. */
        if (HW_SCI_OverRunErrorCheck(p_sci_reg))
        {
            args.event = UART_EVENT_ERR_OVERFLOW;
        }
        else if (HW_SCI_FramingErrorCheck(p_sci_reg))
        {
            if (HW_SCI_BreakDetectionCheck(p_sci_reg))
            {
                args.event = UART_EVENT_BREAK_DETECT;
            }
            else
            {
                args.event = UART_EVENT_ERR_FRAMING;
            }
        }
        else
        {
            args.event = UART_EVENT_ERR_PARITY;
        }

        /** Clear error condition. */
        HW_SCI_ErrorConditionClear (p_sci_reg);

        /** Call callback if available. */
        if (NULL != p_ctrl->p_callback)
        {
            args.channel   = channel;
            args.data      = data;
            args.p_context = p_ctrl->p_context;
            p_ctrl->p_callback(&args);
        }
    }

    /** Clear pending IRQ to make sure it doesn't fire again after exiting */
    R_BSP_IrqStatusClear(R_SSP_CurrentIrqGet());

    /* Restore context if RTOS is used */
    SF_CONTEXT_RESTORE;
}  /* End of function sci_uart_eri_isr () */
#endif /* if (SCI_UART_CFG_RX_ENABLE) */
