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
 * File Name    : r_sci_uart.h
 * Description  : UART on SCI HAL module public header file
 **********************************************************************************************************************/

#ifndef R_SCI_UART_H
#define R_SCI_UART_H

/*******************************************************************************************************************//**
 * @ingroup HAL_Library
 * @defgroup UARTonSCI UART on SCI
 * @brief Driver for the UART on SCI.
 *
 * @section UARTonSCI_SUMMARY Summary
 * This module supports the UART on SCI. It implements the UART interface and drives SCI as a full-duplex UART
 * communication port. This module can drive all SCI channels as UART ports.
 *
 * Extends @ref UART_API.
 *
 * @note This module can use either the 16-stage hardware FIFO or a DTC transfer implementation to write multiple bytes.
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "bsp_api.h"
#include "r_uart_api.h"
#include "r_sci_uart_cfg.h"
#include "r_ioport_api.h"

/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define SCI_UART_CODE_VERSION_MAJOR (1U)
#define SCI_UART_CODE_VERSION_MINOR (11U)

/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/** UART instance control block. */
typedef struct st_sci_uart_instance_ctrl
{
    /* Parameters to control UART peripheral device */
    uint8_t  channel;                     ///< Channel number
    uint8_t  fifo_depth;                  ///< FIFO depth of the UART channel
    uint8_t  rx_transfer_in_progress;     ///< Set to 1 if a receive transfer is in progress, 0 otherwise
    uint8_t  data_bytes          :2;      ///< 1 byte for 7 or 8 bit data, 2 bytes for 9 bit data
    uint8_t  bitrate_modulation  :1;      ///< 1 if bit rate modulation is enabled, 0 otherwise
    uint32_t open;                        ///< Used to determine if the channel is configured

    /** Optional transfer instance used to send or receive multiple bytes without interrupts. */
    transfer_instance_t const * p_transfer_rx;

    /** Optional transfer instance used to send or receive multiple bytes without interrupts. */
    transfer_instance_t const * p_transfer_tx;

    /** Source buffer pointer used to fill hardware FIFO from transmit ISR. */
    uint8_t const * p_tx_src;

    /** Size of source buffer pointer used to fill hardware FIFO from transmit ISR. */
    uint32_t tx_src_bytes;

    IRQn_Type rxi_irq;                ///< Receive IRQ number
    IRQn_Type txi_irq;                ///< Transmit IRQ number
    IRQn_Type tei_irq;                ///< Transmit end IRQ number
    IRQn_Type eri_irq;                ///< Error IRQ number

    /* Parameters to process UART Event */
    void (* p_callback)(uart_callback_args_t * p_args);       ///< Pointer to callback function
    void const * p_context;                                   ///< Pointer to user interrupt context data
    void       * p_reg;                                       ///< Base register for this channel
    void (* p_extpin_ctrl)(uint32_t channel, uint32_t level); ///< External pin control
    uint32_t baud_rate_error_x_1000;                          ///< Baud rate \<Maximum percent error\> x 1000. baud_rate_error must be greater than 0 and less than 15000
    uint8_t  * p_rx_dst;                                      ///< Destination buffer initialized by read() API
    uint32_t rx_dst_bytes;                                    ///< Number of bytes expected by the read() API
    volatile uint32_t rx_bytes_count;                         ///< Number of bytes received since the last read()
    uart_mode_t         uart_comm_mode;                       ///< UART communication mode selection
    uart_rs485_type_t   uart_rs485_mode;                      ///< UART RS485 communication channel type selection
    ioport_port_pin_t   rs485_de_pin;                         ///< UART Driver Enable pin
} sci_uart_instance_ctrl_t;

/** Enumeration for SCI clock source */
typedef enum e_sci_clk_src
{
    SCI_CLK_SRC_INT,        ///< Use internal clock for baud generation
    SCI_CLK_SRC_EXT,        ///< Use external clock for baud generation
    SCI_CLK_SRC_EXT8X,      ///< Use external clock 8x baud rate
    SCI_CLK_SRC_EXT16X      ///< Use external clock 16x baud rate
} sci_clk_src_t;

/** Receive FIFO trigger configuration. */
typedef enum e_sci_uart_rx_fifo_trigger
{
    SCI_UART_RX_FIFO_TRIGGER_1   = 0x1,  ///< Callback after each byte is received without buffering
    SCI_UART_RX_FIFO_TRIGGER_MAX = 0xF,  ///< Callback when FIFO is full or after 15 bit times with no data (fewer interrupts)
} sci_uart_rx_fifo_trigger_t;

/** UART on SCI device Configuration */
typedef struct st_uart_on_sci_cfg
{
    sci_clk_src_t       clk_src;                            ///< Use SCI_CLK_SRC_INT/EXT8X/EXT16X
    bool                baudclk_out;                        ///< Baud rate clock output enable
    bool                rx_edge_start;                      ///< Start reception on falling edge
    bool                noisecancel_en;                     ///< Noise cancel enable

    /** Receive FIFO trigger level, unused if channel has no FIFO or if DTC is used. */
    sci_uart_rx_fifo_trigger_t  rx_fifo_trigger;

    /** Pointer to the user callback function to control external GPIO pin control used as RTS signal.
     *
     * @param[in] channel The UART channel used.
     * @param[in] level   When level is 0, assert RTS.  When level is 1, deassert RTS.
     */
    void (* p_extpin_ctrl)(uint32_t channel, uint32_t level);

    bool                bitrate_modulation;                 ///< Bitrate Modulation Function enable or disable
    uint32_t            baud_rate_error_x_1000;             ///< Baud rate \<Maximum percent error\> x 1000. baud_rate_error must be greater than 0 and less than 15000
    uart_mode_t         uart_comm_mode;                     ///< UART communication mode selection
    uart_rs485_type_t   uart_rs485_mode;                    ///< UART RS485 communication channel type selection
    ioport_port_pin_t   rs485_de_pin;                       ///< UART Driver Enable pin
} uart_on_sci_cfg_t;

/**********************************************************************************************************************
 * Exported global variables
 **********************************************************************************************************************/
/** @cond INC_HEADER_DEFS_SEC */
/** Filled in Interface API structure for this Instance. */
extern const uart_api_t g_uart_on_sci;
/** @endcond */

/*******************************************************************************************************************//**
 * @} (end defgroup UARTonSCI)
 **********************************************************************************************************************/

/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER

#endif /* R_SCI_UART_H */
