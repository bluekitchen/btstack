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

#define BTSTACK_FILE__ "btstack_uart_block_freertos.c"

/*
 *  btstack_uart_block_freertos.c
 *
 *  Adapter to IRQ-driven hal_uart_dma.h with FreeRTOS BTstack Run Loop and 
 *  Callbacks are executed on main thread via data source and btstack_run_loop_freertos_trigger_from_isr
 *
 */

#include "btstack_debug.h"
#include "btstack_uart_block.h"
#include "btstack_run_loop_freertos.h"
#include "hal_uart_dma.h"

#ifdef HAVE_FREERTOS_INCLUDE_PREFIX
#include "freertos/FreeRTOS.h"
#else
#include "FreeRTOS.h"
#endif

#if (INCLUDE_xEventGroupSetBitFromISR != 1) && !defined(HAVE_FREERTOS_TASK_NOTIFICATIONS)
#error "The BTstack HAL UART Run Loop integration (btstack_uart_block_freertos) needs to trigger Run Loop iterations from ISR context," \
"but neither 'INCLUDE_xEventGroupSetBitFromISR' is enabled in your FreeRTOS configuration nor HAVE_FREERTOS_TASK_NOTIFICATIONS is enabled in " \
"btstack_config.h. Please enable INCLUDE_xEventGroupSetBitFromISR or HAVE_FREERTOS_TASK_NOTIFICATIONS."
#endif

// uart config
static const btstack_uart_config_t * uart_config;

// data source for integration with BTstack Runloop
static btstack_data_source_t transport_data_source;

// callbacks
static void (*block_sent)(void);
static void (*block_received)(void);

static int send_complete;
static int receive_complete;

static void btstack_uart_block_freertos_received_isr(void){
    receive_complete = 1;
    btstack_run_loop_freertos_trigger_from_isr();
}

static void btstack_uart_block_freertos_sent_isr(void){
    send_complete = 1;
    btstack_run_loop_freertos_trigger_from_isr();
}

static void btstack_uart_block_freertos_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type) {
    switch (callback_type){
        case DATA_SOURCE_CALLBACK_POLL:
            if (send_complete){
                send_complete = 0;
                if (block_sent){
                    block_sent();
                }
            }
            if (receive_complete){
                receive_complete = 0;
                if (block_received){
                    block_received();
                }
            }
            break;
        default:
            break;
    }
}

//
static int btstack_uart_block_freertos_init(const btstack_uart_config_t * config){
    uart_config = config;
    hal_uart_dma_set_block_received(&btstack_uart_block_freertos_received_isr);
    hal_uart_dma_set_block_sent(&btstack_uart_block_freertos_sent_isr);

    // set up polling data_source
    btstack_run_loop_set_data_source_handler(&transport_data_source, &btstack_uart_block_freertos_process);
    btstack_run_loop_enable_data_source_callbacks(&transport_data_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_add_data_source(&transport_data_source);
    return 0;
}

static int btstack_uart_block_freertos_open(void){
    hal_uart_dma_init();
    hal_uart_dma_set_baud(uart_config->baudrate);
    return 0;
} 

static int btstack_uart_block_freertos_close(void){

    // close device
    // ...
    return 0;
}

static void btstack_uart_block_freertos_set_block_received( void (*block_handler)(void)){
    block_received = block_handler;
}

static void btstack_uart_block_freertos_set_block_sent( void (*block_handler)(void)){
    block_sent = block_handler;
}

static int btstack_uart_block_freertos_set_parity(int parity){
    return 0;
}

// static void btstack_uart_block_set_sleep(uint8_t sleep){
// }

// static void btstack_uart_block_freertos_set_csr_irq_handler( void (*csr_irq_handler)(void)){
// }

static const btstack_uart_block_t btstack_uart_block_freertos = {
    /* int  (*init)(hci_transport_config_uart_t * config); */         &btstack_uart_block_freertos_init,
    /* int  (*open)(void); */                                         &btstack_uart_block_freertos_open,
    /* int  (*close)(void); */                                        &btstack_uart_block_freertos_close,
    /* void (*set_block_received)(void (*handler)(void)); */          &btstack_uart_block_freertos_set_block_received,
    /* void (*set_block_sent)(void (*handler)(void)); */              &btstack_uart_block_freertos_set_block_sent,
    /* int  (*set_baudrate)(uint32_t baudrate); */                    &hal_uart_dma_set_baud,
    /* int  (*set_parity)(int parity); */                             &btstack_uart_block_freertos_set_parity,
#ifdef HAVE_UART_DMA_SET_FLOWCONTROL
    /* int  (*set_flowcontrol)(int flowcontrol); */                   &hal_uart_dma_set_flowcontrol,
#else
    /* int  (*set_flowcontrol)(int flowcontrol); */                   NULL,
#endif
    /* void (*receive_block)(uint8_t *buffer, uint16_t len); */       &hal_uart_dma_receive_block,
    /* void (*send_block)(const uint8_t *buffer, uint16_t length); */ &hal_uart_dma_send_block,    
    /* int (*get_supported_sleep_modes); */                           NULL,
    /* void (*set_sleep)(btstack_uart_sleep_mode_t sleep_mode); */    NULL,
    /* void (*set_wakeup_handler)(void (*wakeup_handler)(void)); */   NULL,   
};

const btstack_uart_block_t * btstack_uart_block_freertos_instance(void){
	return &btstack_uart_block_freertos;
}
