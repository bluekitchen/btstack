/*
 * Copyright (C) 2022 BlueKitchen GmbH
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
 */

/*
 *  hal_uart_dma_chibios.c
 * 
 *  hal_uart_dma implementation on top of ChibiOS/HAL
 */

#define BTSTACK_FILE__ "hal_uart_dma_chibios.c"


#include "btstack_config.h"

#ifdef HAL_CONTROLLER_UART

#include "btstack_debug.h"
#include "btstack_util.h"

#include "hal_uart_dma.h"
#include "hal.h"

#include <inttypes.h>
#include <string.h> // memcpy


static void dummy_handler(void);
static void tx_complete(UARTDriver *uartp);
static void rx_complete(UARTDriver *uartp);

// handlers
static void (*rx_done_handler)(void) = &dummy_handler;
static void (*tx_done_handler)(void) = &dummy_handler;

static UARTConfig uartConf = {
    NULL,               /* UART transmission buffer callback.           */
    &tx_complete,       /* UART physical end of transmission callback.  */
    &rx_complete,       /* UART Receiver receiver filled callback.      */
    NULL,               /* UART caracter received callback.             */
    NULL,               /* UART received error callback.                */
    NULL,               /* Receiver timeout callback */
    115200,             /* UART baudrate.                               */
    0,                  /* CR 1 */
    0,                  /* CR 2 */
    (1<<8) | (1<<9) ,   /* CR 3 - enable RTS/CTS */
};

static void dummy_handler(void){};

// reset Bluetooth using n_shutdown
static void bluetooth_power_cycle(void){
#ifdef HAL_CONTROLLER_RESET_PIN
    log_info("Bluetooth power cycle");
    palClearPad( HAL_CONTROLLER_RESET_PORT, HAL_CONTROLLER_RESET_PIN);
    chThdSleepMilliseconds( 250 );
    palSetPad( HAL_CONTROLLER_RESET_PORT, HAL_CONTROLLER_RESET_PIN);
    chThdSleepMilliseconds( 500 );
#else
    log_info("Bluetooth power cycle skipped, HAL_CONTROLLER_RESET_PIN not defined");
#endif
}

static void rx_complete(UARTDriver *uartp){
    if (uartp == &HAL_CONTROLLER_UART){
        (*rx_done_handler)();
    }
}

static void tx_complete(UARTDriver *uartp){
    UNUSED(uartp);
    if (uartp == &HAL_CONTROLLER_UART){
        (*tx_done_handler)();
    }
}

void hal_uart_dma_init(void){
    bluetooth_power_cycle();
    uartStart(&HAL_CONTROLLER_UART, &uartConf);
}

void hal_uart_dma_set_block_received( void (*the_block_handler)(void)){
    rx_done_handler = the_block_handler;
}

void hal_uart_dma_set_block_sent( void (*the_block_handler)(void)){
    tx_done_handler = the_block_handler;
}

void hal_uart_dma_set_sleep(uint8_t sleep){
    UNUSED(sleep);
}

void hal_uart_dma_set_csr_irq_handler( void (*the_irq_handler)(void)){
    UNUSED(the_irq_handler);
}

int hal_uart_dma_set_baud(uint32_t baud){
    // driver supports 'hot restart'
    uartConf.speed = baud;
    uartStart(&HAL_CONTROLLER_UART, &uartConf);
    log_info("UART baud %" PRIu32, baud);
    return 0;
}

void hal_uart_dma_send_block(const uint8_t *data, uint16_t size){
    uartStartSend(&HAL_CONTROLLER_UART, size, data);
}

void hal_uart_dma_receive_block(uint8_t *data, uint16_t size){
    uartStartReceive(&HAL_CONTROLLER_UART, size, data);
}

#endif
