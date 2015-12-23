/*
 * Copyright (C) 2015 BlueKitchen GmbH
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

#include "wiced.h"
#include "platform_bluetooth.h"
#include "btstack.h"


#if 0
static void dummy_handler(void);

// handlers
static void (*rx_done_handler)(void) = dummy_handler;
static void (*tx_done_handler)(void) = dummy_handler;
static void (*cts_irq_handler)(void) = dummy_handler;
static int hal_uart_needed_during_sleep = 1;

// static wiced_worker_thread_t tick_worker;

// worker thread to do blocking write
static wiced_worker_thread_t rx_worker;
static uint8_t * rx_worker_data_buffer;
static uint16_t  rx_worker_data_size;

// worker thread to do blocking read
static wiced_worker_thread_t tx_worker;
static const uint8_t * tx_worker_data_buffer;
static uint16_t  tx_worker_data_size;

static void dummy_handler(void){};

void hal_uart_dma_init(void){

    wiced_uart_config_t uart_config =
    {
        .baud_rate    = 115200,
        .data_width   = DATA_WIDTH_8BIT,
        .parity       = NO_PARITY,
        .stop_bits    = STOP_BITS_1,
        .flow_control = FLOW_CONTROL_CTS_RTS,
    };

    // test RTS first
    platform_gpio_init( wiced_bt_control_pins[WICED_BT_PIN_UART_RTS], OUTPUT_PUSH_PULL);
    platform_gpio_init( wiced_bt_control_pins[WICED_BT_PIN_UART_TX],  OUTPUT_PUSH_PULL);
    while (1) {
        platform_gpio_output_low( wiced_bt_control_pins[WICED_BT_PIN_UART_RTS]);
        platform_gpio_output_low( wiced_bt_control_pins[WICED_BT_PIN_UART_TX]);
        platform_gpio_output_high(wiced_bt_control_pins[WICED_BT_PIN_UART_RTS]);
        platform_gpio_output_high(wiced_bt_control_pins[WICED_BT_PIN_UART_TX]);
    }

    // configure HOST and DEVICE WAKE PINs
    platform_gpio_init( wiced_bt_control_pins[WICED_BT_PIN_HOST_WAKE], INPUT_HIGH_IMPEDANCE);
    platform_gpio_init( wiced_bt_control_pins[WICED_BT_PIN_DEVICE_WAKE], OUTPUT_PUSH_PULL);
    platform_gpio_output_low( wiced_bt_control_pins[WICED_BT_PIN_DEVICE_WAKE]);
    wiced_rtos_delay_milliseconds( 100 );

    // power cycle Bluetooth
    platform_gpio_init( wiced_bt_control_pins[ WICED_BT_PIN_POWER ], OUTPUT_PUSH_PULL );
    platform_gpio_output_low( wiced_bt_control_pins[ WICED_BT_PIN_POWER ] );
    wiced_rtos_delay_milliseconds( 100 );
    platform_gpio_output_high( wiced_bt_control_pins[ WICED_BT_PIN_POWER ] );

    wiced_rtos_delay_milliseconds( 500 );

    // init UART
    platform_uart_init( wiced_bt_uart_driver, wiced_bt_uart_peripheral, &uart_config, NULL );

    // create worker threads for rx/tx
    wiced_rtos_create_worker_thread(&tx_worker, WICED_NETWORK_WORKER_PRIORITY, 1000, 1);
    wiced_rtos_create_worker_thread(&rx_worker, WICED_NETWORK_WORKER_PRIORITY, 1000, 5);
}

void hal_uart_dma_set_sleep(uint8_t sleep){
    hal_uart_needed_during_sleep = !sleep;
}

void hal_uart_dma_set_block_received( void (*the_block_handler)(void)){
    rx_done_handler = the_block_handler;
}

void hal_uart_dma_set_block_sent( void (*the_block_handler)(void)){
    tx_done_handler = the_block_handler;
}

void hal_uart_dma_set_csr_irq_handler( void (*the_irq_handler)(void)){
    cts_irq_handler = the_irq_handler;
}

int  hal_uart_dma_set_baud(uint32_t baud){
    return 0;
}

static wiced_result_t tx_worker_task(void * arg){
    // printf("tx_worker_task len %u\n", tx_worker_data_size);
    platform_uart_transmit_bytes(wiced_bt_uart_driver, tx_worker_data_buffer, tx_worker_data_size);
    // printf("tx_worker_task done\n");
    tx_done_handler();
    return WICED_SUCCESS;
}

static wiced_result_t rx_worker_task(void * arg){
    // printf("rx_worker_task len %u\n", rx_worker_data_size);
    // printf("/%u\\\n", rx_worker_data_size);
    platform_uart_receive_bytes(wiced_bt_uart_driver, rx_worker_data_buffer, rx_worker_data_size, WICED_NEVER_TIMEOUT);
    // printf("rx_worker_task done\n");
    rx_done_handler();
    return WICED_SUCCESS;
}

void hal_uart_dma_send_block(const uint8_t *data, uint16_t size){
    // printf("hal_uart_dma_send_block len %u\n", size);
    tx_worker_data_buffer = data;
    tx_worker_data_size = size;
#if 0
    wiced_rtos_send_asynchronous_event(&tx_worker, &tx_worker_task, NULL);
    printf("hal_uart_dma_send_block B\n");
    wiced_rtos_delay_milliseconds(100);
#endif
    platform_uart_transmit_bytes(wiced_bt_uart_driver, tx_worker_data_buffer, tx_worker_data_size);
    // printf("hal_uart_dma_send_block done\n");
    tx_done_handler();
 }

void hal_uart_dma_receive_block(uint8_t *data, uint16_t size){
    // printf("hal_uart_dma_receive_block len %u\n", size);
    rx_worker_data_buffer = data;
    rx_worker_data_size = size;

    if ( wiced_rtos_is_current_thread( &rx_worker.thread ) == WICED_SUCCESS ){
        // re-entrant call .. don't post to queue, just call it directlyh
        // printf("/%u\\\n", rx_worker_data_size);
        rx_worker_task(NULL);
    } else {
        // printf("\\%u/\n", rx_worker_data_size);
        wiced_rtos_send_asynchronous_event(&rx_worker, &rx_worker_task, NULL);
        wiced_rtos_delay_milliseconds(100);
    }
    // printf("hal_uart_dma_receive_block done\n");
}
#endif
#if 0
    while (1) {
        // send HCI Reset
        uint8_t hci_reset[] = { 0x01, 0x03, 0x0c, 0x00};
        wiced_result_t res;
        res = platform_uart_transmit_bytes(wiced_bt_uart_driver, hci_reset, sizeof(hci_reset) );
        printf("Send reset.. res %u\n", res);

        // receive event
        uint8_t buffer[6];
        res = platform_uart_receive_bytes(wiced_bt_uart_driver, buffer, 6, 5000);
        int i;
        for (i=0;i<sizeof(buffer);i++){
            printf("0x%02x ", buffer[i]);
        }
        printf("\n");

        wiced_rtos_delay_milliseconds(2000);
    }
}
#endif

static const hci_uart_config_t hci_uart_config = {
    NULL,
    115200,
    0,
    0
};

extern int btstack_main(void);

void application_start(void)
{
    /* Initialise the WICED device */
    wiced_init();

    printf("BTstack on WICED\n");

    // start with BTstack init - especially configure HCI Transport
    btstack_memory_init();
    run_loop_init(RUN_LOOP_WICED);
    
    // enable full log output while porting
    hci_dump_open(NULL, HCI_DUMP_STDOUT);

    // init HCI
    hci_transport_t    * transport = hci_transport_h4_wiced_instance();
    bt_control_t       * control   = NULL; //  bt_control_cc256x_instance();
    remote_device_db_t * remote_db = (remote_device_db_t *) &remote_device_db_memory;
    hci_init(transport, (void*) &hci_uart_config, control, remote_db);

    // hand over to btstack embedded code 
    btstack_main();

    // go
    run_loop_execute();

    while (1){};
}
