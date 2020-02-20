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
 * File Name    : r_uart_api.h
 * Description  : UART Shared Interface definition
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @ingroup Interface_Library
 * @defgroup UART_API UART Interface
 * @brief Interface for UART communications.
 *
 * @section UART_INTERFACE_SUMMARY Summary
 * The UART interface provides common APIs for UART HAL drivers. The UART interface supports the following features:
 * - Full-duplex UART communication
 * - Generic UART parameter setting
 * - Interrupt driven transmit/receive processing
 * - Callback function with returned event code
 * - Runtime baud-rate change
 * - Hardware resource locking during a transaction
 * - CTS/RTS hardware flow control support (with an associated IOPORT pin)
 * - Circular buffer support
 * - Runtime Transmit/Receive circular buffer flushing
 *
 * Implemented by:
 * - @ref UARTonSCI
 *
 * Related SSP architecture topics:
 * - @ref ssp-interfaces
 * - @ref ssp-predefined-layers
 * - @ref using-ssp-modules
 *
 * UART Interface description: @ref HALUARTInterface
 * @{
 **********************************************************************************************************************/

#ifndef R_UART_API_H
#define R_UART_API_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
/* Includes board and MCU related header files. */
#include "bsp_api.h"
#include "r_transfer_api.h"

/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER

/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define UART_API_VERSION_MAJOR (1U)
#define UART_API_VERSION_MINOR (6U)

/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
/** UART Event codes */
typedef enum e_sf_event
{
    UART_EVENT_RX_COMPLETE        = (1UL << 0),         ///< Receive complete event
    UART_EVENT_TX_COMPLETE        = (1UL << 1),         ///< Transmit complete event
    UART_EVENT_ERR_PARITY         = (1UL << 2),         ///< Parity error event
    UART_EVENT_ERR_FRAMING        = (1UL << 3),         ///< Mode fault error event
    UART_EVENT_BREAK_DETECT       = (1UL << 4),         ///< Break detect error event
    UART_EVENT_ERR_OVERFLOW       = (1UL << 5),         ///< FIFO Overflow error event
    UART_EVENT_ERR_RXBUF_OVERFLOW = (1UL << 6),         ///< DEPRECATED: Receive buffer overflow error event
    UART_EVENT_RX_CHAR            = (1UL << 7),         ///< Character received
    UART_EVENT_TX_DATA_EMPTY      = (1UL << 8),         ///< Last byte is transmitting, ready for more data
} uart_event_t;

/** UART Data bit length definition */
typedef enum e_uart_data_bits
{
    UART_DATA_BITS_8,                       ///< Data bits 8-bit
    UART_DATA_BITS_7,                       ///< Data bits 7-bit
    UART_DATA_BITS_9                        ///< Data bits 9-bit
} uart_data_bits_t;

/** UART Parity definition */
typedef enum e_uart_parity
{
    UART_PARITY_OFF  = 0U,                  ///< No parity
    UART_PARITY_EVEN = 2U,                  ///< Even parity
    UART_PARITY_ODD  = 3U,                  ///< Odd parity
} uart_parity_t;

/** UART Stop bits definition */
typedef enum e_uart_stop_bits
{
    UART_STOP_BITS_1 = 0U,                  ///< Stop bit 1-bit
    UART_STOP_BITS_2 = 1U,                  ///< Stop bits 2-bit
} uart_stop_bits_t;

/** UART transaction definition */
typedef enum e_uart_dir
{
    UART_DIR_RX_TX = 0U,           ///< Both RX and TX
    UART_DIR_RX = 1U,              ///< Only RX
    UART_DIR_TX = 2U,              ///< Only TX
} uart_dir_t;

/** UART communication mode definition */
typedef enum e_uart_mode
{
    UART_MODE_RS232 = 0U,         ///< Enables RS232 communication mode
    UART_MODE_RS485 = 1U,         ///< Enables RS485 communication mode
}uart_mode_t;

/** UART RS485 communication channel type definition */
typedef enum e_uart_rs485_type
{
    UART_RS485_HD = 0U,         ///< Uses RS485 half duplex communication channel
    UART_RS485_FD = 1U,         ///< Uses RS485 full duplex communication channel
}uart_rs485_type_t;

/** UART driver specific information */
typedef struct st_uart_info
{
    /** Maximum bytes that can be written at this time.  Only applies if uart_cfg_t::p_transfer_tx is not NULL. */
    uint32_t      write_bytes_max;

    /** Maximum bytes that are available to read at one time.  Only applies if uart_cfg_t::p_transfer_rx is not NULL. */
    uint32_t      read_bytes_max;
} uart_info_t;

/** UART Callback parameter definition */
typedef struct st_uart_callback_arg
{
    uint32_t      channel;                  ///< Device channel number
    uart_event_t  event;                    ///< Event code

    /** Contains the next character received for the events UART_EVENT_RX_CHAR, UART_EVENT_ERR_PARITY,
     * UART_EVENT_ERR_FRAMING, or UART_EVENT_ERR_OVERFLOW.  Otherwise unused. */
    uint32_t      data;
    void const    * p_context;              ///< Context provided to user during callback
} uart_callback_args_t;

/** UART Configuration */
typedef struct st_uart_cfg
{
    /* UART generic configuration */
    uint8_t           channel;              ///< Select a channel corresponding to the channel number of the hardware.
    uint32_t          baud_rate;            ///< Baud rate, i.e. 9600, 19200, 115200
    uart_data_bits_t  data_bits;            ///< Data bit length (8 or 7 or 9)
    uart_parity_t     parity;               ///< Parity type (none or odd or even)
    uart_stop_bits_t  stop_bits;            ///< Stop bit length (1 or 2)
    bool              ctsrts_en;            ///< CTS/RTS hardware flow control enable
    uint8_t           rxi_ipl;              ///< Receive interrupt priority
    uint8_t           txi_ipl;              ///< Transmit interrupt priority
    uint8_t           tei_ipl;              ///< Transmit end interrupt priority
    uint8_t           eri_ipl;              ///< Error interrupt priority

    /** Optional transfer instance used to receive multiple bytes without interrupts.  Set to NULL if unused.
     * If NULL, the number of bytes allowed in the read API is limited to one byte at a time. */
    transfer_instance_t const * p_transfer_rx;

    /** Optional transfer instance used to send multiple bytes without interrupts.  Set to NULL if unused.
     * If NULL, the number of bytes allowed in the write APIs is limited to one byte at a time. */
    transfer_instance_t const * p_transfer_tx;

    /* Configuration for UART Event processing */
    void (* p_callback)(uart_callback_args_t * p_args); ///< Pointer to callback function
    void const * p_context;                             ///< User defined context passed into callback function

    /* Pointer to UART peripheral specific configuration */
    void const * p_extend;                  ///< UART hardware dependent configuration
} uart_cfg_t;

/** UART control block.  Allocate an instance specific control block to pass into the UART API calls.
 * @par Implemented as
 * - sci_uart_instance_ctrl_t
 */
typedef void uart_ctrl_t;

/** Shared Interface definition for UART */
typedef struct st_uart_api
{
    /** Open  UART device.
     * @par Implemented as
     * - R_SCI_UartOpen()
     *
     * @param[in,out]  p_ctrl     Pointer to the UART control block Must be declared by user. Value set here.
     * @param[in]      uart_cfg_t Pointer to UART configuration structure. All elements of this structure must be set by
     *                            user.
     */
	ssp_err_t (* open)(uart_ctrl_t      * const p_ctrl,
                       uart_cfg_t const * const p_cfg);

    /** Read from UART device.  If a transfer instance is used for reception, the received bytes are stored directly
     * in the read input buffer.  When a transfer is complete, the callback is called with event UART_EVENT_RX_COMPLETE.
     * Bytes received outside an active transfer are received in the callback function with event UART_EVENT_RX_CHAR.
     * The maximum transfer size is reported by infoGet().
     * @par Implemented as
     * - R_SCI_UartRead()
     *
     * @param[in]   p_ctrl     Pointer to the UART control block for the channel.
     * @param[in]   p_dest     Destination address to read data from.
     * @param[in]   bytes      Read data length.  Only applicable if uart_cfg_t::p_transfer_rx is not NULL.
     *                         Otherwise all read bytes will be provided through the callback set in
     *                         uart_cfg_t::p_callback.
     */
    ssp_err_t (* read)(uart_ctrl_t   * const p_ctrl,
                       uint8_t const * const p_dest,
                       uint32_t        const bytes);

    /** Write to UART device.  The write buffer is used until write is complete.  Do not overwrite write buffer
     * contents until the write is finished.  When the write is complete (all bytes are fully transmitted on the wire),
     * the callback called with event UART_EVENT_TX_COMPLETE.
     * The maximum transfer size is reported by infoGet().
     * @par Implemented as
     * - R_SCI_UartWrite()
     *
     * @param[in]   p_ctrl     Pointer to the UART control block.
     * @param[in]   p_src      Source address  to write data to.
     * @param[in]   bytes      Write data length.
     */
    ssp_err_t (* write)(uart_ctrl_t   * const p_ctrl,
                        uint8_t const * const p_src,
                        uint32_t        const bytes);

    /** Change baud rate.
     * @warning Calling this API aborts any in-progress transmission and disables reception until the new baud
     * settings have been applied.
     *
     * @par Implemented as
     * - R_SCI_UartBaudSet()
     *
     * @param[in]   p_ctrl     Pointer to the UART control block.
     * @param[in]   baudrate   Baud rate in bps.
     */
    ssp_err_t (* baudSet)(uart_ctrl_t * const p_ctrl,
                          uint32_t      const baudrate);

    /** Get the driver specific information.
     * @par Implemented as
     * - R_SCI_UartInfoGet()
     *
     * @param[in]   p_ctrl     Pointer to the UART control block.
     * @param[in]   baudrate   Baud rate in bps.
     */
    ssp_err_t (* infoGet)(uart_ctrl_t * const p_ctrl,
                          uart_info_t * const p_info);

    /** Close UART device.
     * @par Implemented as
     * - R_SCI_UartClose()
     *
     * @param[in]   p_ctrl     Pointer to the UART control block.
     */
    ssp_err_t (* close)(uart_ctrl_t * const p_ctrl);

    /** Get version.
     * @par Implemented as
     * - R_SCI_UartVersionGet()
     *
     * @param[in]   p_version  Pointer to the memory to store the version information.
     */
    ssp_err_t (* versionGet)(ssp_version_t * p_version);

    /**
     * Abort ongoing transfer.
     * @par Implemented as
     * - R_SCI_UartAbort()
     *
     * @param[in]   p_ctrl                   Pointer to the UART control block.
     * @param[in]   communication_to_abort   Type of abort request.
     */
    ssp_err_t (* communicationAbort)(uart_ctrl_t   * const p_ctrl, uart_dir_t communication_to_abort);
} uart_api_t;

/** This structure encompasses everything that is needed to use an instance of this interface. */
typedef struct st_uart_instance
{
    uart_ctrl_t       * p_ctrl;    ///< Pointer to the control structure for this instance
    uart_cfg_t  const * p_cfg;     ///< Pointer to the configuration structure for this instance
    uart_api_t  const * p_api;     ///< Pointer to the API structure for this instance
} uart_instance_t;


/** @} (end defgroup UART_API) */

/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER

#endif /* R_UART_API_H */
