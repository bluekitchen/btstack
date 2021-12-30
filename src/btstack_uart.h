/*
 * Copyright (C) 2021 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

/**
 * @title UART
 *
 * Common types for UART transports
 *
 */

#ifndef BTSTACK_UART_H
#define BTSTACK_UART_H

#include <stdint.h>
#include "btstack_config.h"

#define BTSTACK_UART_PARITY_OFF  0
#define BTSTACK_UART_PARITY_EVEN 1
#define BTSTACK_UART_PARITY_ODD  2

typedef enum {
    // UART active, sleep off
    BTSTACK_UART_SLEEP_OFF = 0,
    // used for eHCILL
    BTSTACK_UART_SLEEP_RTS_HIGH_WAKE_ON_CTS_PULSE,
    // used for H5 and for eHCILL without support for wake on CTS pulse
    BTSTACK_UART_SLEEP_RTS_LOW_WAKE_ON_RX_EDGE, 

} btstack_uart_sleep_mode_t;


typedef enum { 
    BTSTACK_UART_SLEEP_MASK_RTS_HIGH_WAKE_ON_CTS_PULSE  = 1 << BTSTACK_UART_SLEEP_RTS_HIGH_WAKE_ON_CTS_PULSE,
    BTSTACK_UART_SLEEP_MASK_RTS_LOW_WAKE_ON_RX_EDGE     = 1 << BTSTACK_UART_SLEEP_RTS_LOW_WAKE_ON_RX_EDGE
} btstack_uart_sleep_mode_mask_t;

// parity is mainly used with h5 mode.
typedef struct {
    uint32_t              baudrate;
    int                   flowcontrol;
    const char *          device_name;
    int                   parity;
} btstack_uart_config_t;

/* API_START */

typedef struct {
    /**
     * init transport
     * @param uart_config
     */
    int (*init)(const btstack_uart_config_t * uart_config);

    /**
     * open transport connection
     */
    int (*open)(void);

    /**
     * close transport connection
     */
    int (*close)(void);

    /**
     * set callback for block received. NULL disables callback
     */
    void (*set_block_received)(void (*block_handler)(void));

    /**
     * set callback for sent. NULL disables callback
     */
    void (*set_block_sent)(void (*block_handler)(void));

    /**
     * set baudrate
     */
    int (*set_baudrate)(uint32_t baudrate);

    /**
     * set parity
     */
    int  (*set_parity)(int parity);

    /**
     * set flowcontrol
     */
    int  (*set_flowcontrol)(int flowcontrol);

    /**
     * receive block
     */
    void (*receive_block)(uint8_t *buffer, uint16_t len);

    /**
     * send block
     */
    void (*send_block)(const uint8_t *buffer, uint16_t length);


    /** Support for different Sleep Modes in TI's H4 eHCILL and in H5 - can be set to NULL if not used */

    /**
     * query supported wakeup mechanisms
     * @return supported_sleep_modes mask
     */
     int (*get_supported_sleep_modes)(void);

    /**
     * set UART sleep mode - allows to turn off UART and it's clocks to save energy
     * Supported sleep modes:
     * - off: UART active, RTS low if receive_block was called and block not read yet
     * - RTS high, wake on CTS: RTS should be high. On CTS pulse, UART gets enabled again and RTS goes to low
     * - RTS low, wake on RX: data on RX will trigger UART enable, bytes might get lost
     */
    void (*set_sleep)(btstack_uart_sleep_mode_t sleep_mode);

    /** 
     * set wakeup handler - needed to notify hci transport of wakeup requests by Bluetooth controller
     * Called upon CTS pulse or RX data. See sleep modes.
     */
    void (*set_wakeup_handler)(void (*wakeup_handler)(void));


    /** Support for HCI H5 Transport Mode - can be set to NULL for H4 */

    /**
     * H5/SLIP only: set callback for frame received. NULL disables callback
     */
    void (*set_frame_received)(void (*frame_handler)(uint16_t frame_size));

    /**
     * H5/SLIP only: set callback for frame sent. NULL disables callback
     */
    void (*set_frame_sent)(void (*block_handler)(void));

    /**
     * H5/SLIP only: receive SLIP frame
     */
    void (*receive_frame)(uint8_t *buffer, uint16_t len);

    /**
     * H5/SLIP only: send SLIP frame
     */
    void (*send_frame)(const uint8_t *buffer, uint16_t length);

} btstack_uart_t;

/* API_END */

// common implementations
const btstack_uart_t * btstack_uart_posix_instance(void);

#endif
