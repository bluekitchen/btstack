/*
 * Copyright (C) 2016 BlueKitchen GmbH
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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

/*
 *  btstack_uart_block.h
 *
 *  Common code to access serial port via asynchronous block read/write commands
 *
 */

#ifndef BTSTACK_UART_BLOCK_H
#define BTSTACK_UART_BLOCK_H

#include <stdint.h>

typedef struct {
    uint32_t   baudrate;
    int        flowcontrol;
    const char *device_name;
} btstack_uart_config_t;

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

    // support for sleep modes in TI's H4 eHCILL and H5

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

} btstack_uart_block_t;

// common implementations
const btstack_uart_block_t * btstack_uart_block_posix_instance(void);
const btstack_uart_block_t * btstack_uart_block_windows_instance(void);
const btstack_uart_block_t * btstack_uart_block_embedded_instance(void);
const btstack_uart_block_t * btstack_uart_block_freertos_instance(void);

#endif
