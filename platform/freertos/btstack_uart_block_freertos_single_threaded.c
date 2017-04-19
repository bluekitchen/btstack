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

#define __BTSTACK_FILE__ "btstack_uart_block_freertos_single_threaded.c"

/*
 *  btstack_uart_block_freertos.c
 *
 *  Adapter to IRQ-driven hal_uart_dma.h with FreeRTOS BTstack Run Loop and 
 *  Callbacks are executed on main thread via btstack_run_loop_freertos_single_threaded_execute_code_on_main_thread()
 *
 */

#include "btstack_debug.h"
#include "btstack_uart_block.h"
#include "btstack_run_loop_freertos_single_threaded.h"
#include "hal_uart_dma.h"

// uart config
static const btstack_uart_config_t * uart_config;

// callbacks
static void (*block_sent)(void);
static void (*block_received)(void);


// called from main thread context
static void btstack_uart_block_received_thread(void * arg){
    UNUSED(arg);
    block_received();
}

static void btstack_uart_block_sent_thread(void * arg){
    UNUSED(arg);
    block_sent();
}

// called from ISR context
static void btstack_uart_block_received_isr(void){
    btstack_run_loop_freertos_single_threaded_execute_code_on_main_thread(&btstack_uart_block_received_thread, NULL);
}

static void btstack_uart_block_sent_isr(void){
    // notify upper stack that provided buffer can be used again
    btstack_run_loop_freertos_single_threaded_execute_code_on_main_thread(&btstack_uart_block_sent_thread, NULL);
}

//
static int btstack_uart_block_init(const btstack_uart_config_t * config){
    uart_config = config;
    hal_uart_dma_set_block_received(&btstack_uart_block_received_isr);
    hal_uart_dma_set_block_sent(&btstack_uart_block_sent_isr);
    return 0;
}

static int btstack_uart_block_open(void){
    hal_uart_dma_init();
    hal_uart_dma_set_baud(uart_config->baudrate);
    return 0;
} 

static int btstack_uart_block_close(void){

    // close device
    // ...
    return 0;
}

static void btstack_uart_block_set_block_received( void (*block_handler)(void)){
    block_received = block_handler;
}

static void btstack_uart_block_set_block_sent( void (*block_handler)(void)){
    block_sent = block_handler;
}

static int btstack_uart_block_set_parity(int parity){
    return 0;
}

// static void btstack_uart_block_set_sleep(uint8_t sleep){
// }

// static void btstack_uart_embedded_set_csr_irq_handler( void (*csr_irq_handler)(void)){
// }

static const btstack_uart_block_t btstack_uart_embedded = {
    /* int  (*init)(hci_transport_config_uart_t * config); */         &btstack_uart_block_init,
    /* int  (*open)(void); */                                         &btstack_uart_block_open,
    /* int  (*close)(void); */                                        &btstack_uart_block_close,
    /* void (*set_block_received)(void (*handler)(void)); */          &btstack_uart_block_set_block_received,
    /* void (*set_block_sent)(void (*handler)(void)); */              &btstack_uart_block_set_block_sent,
    /* int  (*set_baudrate)(uint32_t baudrate); */                    &hal_uart_dma_set_baud,
    /* int  (*set_parity)(int parity); */                             &btstack_uart_block_set_parity,
    /* void (*receive_block)(uint8_t *buffer, uint16_t len); */       &hal_uart_dma_receive_block,
    /* void (*send_block)(const uint8_t *buffer, uint16_t length); */ &hal_uart_dma_send_block,    
    /* int (*get_supported_sleep_modes); */                           NULL,
    /* void (*set_sleep)(btstack_uart_sleep_mode_t sleep_mode); */    NULL
};

const btstack_uart_block_t * btstack_uart_block_embedded_instance(void){
	return &btstack_uart_embedded;
}