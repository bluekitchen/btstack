/*
 * Copyright (C) 2025 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "hal_uart_dma_rp2040.c"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "btstack_bool.h"
#include "btstack_config.h"
#include "btstack_debug.h"

#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/uart.h"

#include "pico/stdlib.h"

#include "board.h"

// Bluetooth UART Configuration
#define BAUD_RATE 115200
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY    UART_PARITY_NONE

static int tx_dma_channel;
static dma_channel_config tx_dma_config;

static int rx_dma_channel;
static dma_channel_config rx_dma_config;

static void dummy_handler(void);

// handlers
static void (*rx_done_handler)(void) = &dummy_handler;
static void (*tx_done_handler)(void) = &dummy_handler;

static void dummy_handler(void){}

static  void hal_uart_dma_complete(void) {
    uint32_t intr = dma_hw->intr;
    // TX IRQ
    if (intr & (1u << tx_dma_channel)) {
        // clear IRQ
        dma_hw->ints0 = 1u << tx_dma_channel;
        tx_done_handler();
    }
    // RX IRQ
    if (intr & (1u << rx_dma_channel)) {
        // clear IRQ
        dma_hw->ints0 = 1u << rx_dma_channel;
        rx_done_handler();
    }
}

static void flush_rx(void){
    log_info("flush_rx");
    while (uart_is_readable(UART_ID)) {
        (void) uart_getc(UART_ID);
    }
}

static void hal_uart_dma_configure_uart_pins(void) {
    // note: UART_FUNCSEL_NUM differs based on pin number
    gpio_set_function(BLUETOOTH_TX_PIN, UART_FUNCSEL_NUM(UART_ID, BLUETOOTH_TX_PIN));
    gpio_set_function(BLUETOOTH_RX_PIN, UART_FUNCSEL_NUM(UART_ID, BLUETOOTH_RX_PIN));
    gpio_set_function(BLUETOOTH_CTS_PIN, UART_FUNCSEL_NUM(UART_ID, BLUETOOTH_CTS_PIN));
    gpio_set_function(BLUETOOTH_RTS_PIN, UART_FUNCSEL_NUM(UART_ID, BLUETOOTH_RTS_PIN));
}

static void bluetooth_power_cycle(void) {
    log_info("nRST=0");

    // configure all UART lines as input with pull-ups
    uint8_t bluetooth_pins[] = {
        BLUETOOTH_TX_PIN,
        BLUETOOTH_RX_PIN,
        BLUETOOTH_CTS_PIN,
        BLUETOOTH_RTS_PIN
    };
    for (uint8_t i = 0; i < sizeof(bluetooth_pins); i++){
        uint8_t pin = bluetooth_pins[i];
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);
        gpio_set_function(pin, GPIO_FUNC_SIO);
    }

    // clear nReset
    gpio_put(BLUETOOTH_N_RESET_PIN, 0);
    sleep_ms(10);

    // set nReset
    gpio_put(BLUETOOTH_N_RESET_PIN, 1);

    // On Vela-IF820, a power control chip creates a 40 ms low pulse on the actual RST_N line
    // after nReset is released.
    sleep_ms(50);

    log_info("nRST=1");

    // signal ready
    gpio_set_dir(BLUETOOTH_RTS_PIN, GPIO_OUT);
    gpio_put(BLUETOOTH_RTS_PIN, 0);

    // wait for BLUETOOTH_CTS to get released
    while (gpio_get(BLUETOOTH_CTS_PIN)) {
        tight_loop_contents();
    }

    // re-activate UART control
    hal_uart_dma_configure_uart_pins();
}

void hal_uart_dma_init(void){

    // Setup nReset
    gpio_init(BLUETOOTH_N_RESET_PIN);
    gpio_set_dir(BLUETOOTH_N_RESET_PIN, GPIO_OUT);
    gpio_put(BLUETOOTH_N_RESET_PIN, 1);

    // power cycle Bluetooth Controller with UART in tri-state mode
    bluetooth_power_cycle();

    log_info("hal_uart_dma_init");

    // Set up our UART with a basic baud rate.
    uart_init(UART_ID, BAUD_RATE);

    // activate UART control
    hal_uart_dma_configure_uart_pins();

    // Set UART flow control CTS/RTS
    uart_set_hw_flow(UART_ID, true, true);

    // Set our data format
    uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);

    // Turn off FIFO's - we want to do this character by character
    uart_set_fifo_enabled(UART_ID, true);

    // clear UART RX buffer
    flush_rx();

    // Configure TX DMA Channel
    tx_dma_channel = dma_claim_unused_channel(true);
    tx_dma_config = dma_channel_get_default_config(tx_dma_channel);                  // Default configs
    channel_config_set_transfer_data_size(&tx_dma_config, DMA_SIZE_8);               // 8-bit txfers
    channel_config_set_read_increment(&tx_dma_config, true);                     // yes read incrementing
    channel_config_set_write_increment(&tx_dma_config, false);                   // no write incrementing
    channel_config_set_dreq(&tx_dma_config, uart_get_dreq_num(UART_ID, true));  // triggered by UART TX requests
    dma_channel_set_write_addr(tx_dma_channel, &uart_get_hw(UART_ID)->dr, false);
    dma_channel_set_config(tx_dma_channel, &tx_dma_config, false);

    // Configure RX DMA Channel
    rx_dma_channel = dma_claim_unused_channel(true);
    rx_dma_config = dma_channel_get_default_config(rx_dma_channel);                  // Default configs
    channel_config_set_transfer_data_size(&rx_dma_config, DMA_SIZE_8);               // 8-bit txfers
    channel_config_set_read_increment(&rx_dma_config, false);                    // no read incrementing
    channel_config_set_write_increment(&rx_dma_config, true);                    // yes write incrementing
    channel_config_set_dreq(&rx_dma_config, uart_get_dreq_num(UART_ID, false)); // triggered by UART RX requests
    dma_channel_set_read_addr(rx_dma_channel, &uart_get_hw(UART_ID)->dr, false);
    dma_channel_set_config(rx_dma_channel, &rx_dma_config, false);

    // use IRQ 0 for UART DMA
    dma_channel_set_irq0_enabled(tx_dma_channel, true);
    dma_channel_set_irq0_enabled(rx_dma_channel, true);

    irq_set_exclusive_handler(DMA_IRQ_0, hal_uart_dma_complete);
    irq_set_enabled(DMA_IRQ_0, true);
}

void hal_uart_dma_set_block_received( void (*the_block_handler)(void)){
    rx_done_handler = the_block_handler;
}

void hal_uart_dma_set_block_sent( void (*the_block_handler)(void)){
    tx_done_handler = the_block_handler;
}

int  hal_uart_dma_set_baud(uint32_t baud){
    log_info("UART DMA set baud %u", baud);
    (void) uart_set_baudrate(UART_ID, baud);
}

void hal_uart_dma_send_block(const uint8_t *buffer, uint16_t length){
    // set buffer and start
    dma_channel_set_read_addr(tx_dma_channel, buffer, false);
    dma_channel_set_trans_count(tx_dma_channel, length, true);
}

void hal_uart_dma_receive_block(uint8_t *buffer, uint16_t len){
    // set buffer and start
    dma_channel_set_write_addr(rx_dma_channel, buffer, false);
    dma_channel_set_trans_count(rx_dma_channel, len, true);
}

void hal_uart_dma_set_csr_irq_handler( void (*csr_irq_handler)(void)){
    // Not implemented
}

void hal_uart_dma_set_sleep(uint8_t sleep){
    // Not implemented
}
