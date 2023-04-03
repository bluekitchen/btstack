/*
 * Copyright (C) 2017 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "btstack_stdin_esp32.c"

/*
 *  btstack_stdin_esp32.c
 *
 *  Uses blocking thread to wait for console input
 *  Busy waits until character has been processed
 */

#include "btstack_stdin.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_freertos.h"
#include "btstack_defines.h"
#include "btstack_debug.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <unistd.h>
#include <errno.h>
#include <reent.h>

#include "sdkconfig.h"
#include "driver/uart.h"

#include "esp_vfs_dev.h"
#include "esp_log.h"

static const char *TAG = "btstack_uart_console";

// handle esp-idf change from CONFIG_CONSOLE_UART_NUM to CONFIG_ESP_CONSOLE_UART_NUM
#ifndef CONFIG_ESP_CONSOLE_UART_NUM
#define CONFIG_ESP_CONSOLE_UART_NUM CONFIG_CONSOLE_UART_NUM
#endif

#define BUF_SIZE (UART_FIFO_LEN)

volatile int stdin_character_received;
volatile char stdin_character;
static void (*stdin_handler)(char c);
static btstack_data_source_t stdin_data_source;

// after the capacity of the TX buffer is exceeded the output will block, so
// in order to prevent that the TX buffer needs to be big enough or
// the baudrate needs to be increased.
// Usually the maximum baudrate for the target is preferred
#define RX_BUF_SIZE (1024)
#define TX_BUF_SIZE (4096)
static QueueHandle_t uart_queue = NULL;

static void btstack_stdin_process(struct btstack_data_source *ds, btstack_data_source_callback_type_t callback_type){
    uart_event_t event;
    if( xQueueReceive(uart_queue, (void*)&event, 0)) {
        switch( event.type ) {
        case UART_DATA: {
            uint8_t stdin_character;
            if(!stdin_handler) {
                uart_flush_input(CONFIG_ESP_CONSOLE_UART_NUM);
                break;
            }

            for( int i=0; i<event.size; ++i ) {
                int ret = uart_read_bytes( CONFIG_ESP_CONSOLE_UART_NUM, &stdin_character, 1, 0 );
                btstack_assert( ret == 1 );
                (*stdin_handler)(stdin_character);
            }

            break;
        }
        case UART_FIFO_OVF:
            ESP_LOGI(TAG, "hw fifo overflow");
            uart_flush_input(CONFIG_ESP_CONSOLE_UART_NUM);
            xQueueReset(uart_queue);
            break;
        case UART_BUFFER_FULL:
            ESP_LOGI(TAG, "ring buffer full");
            uart_flush_input(CONFIG_ESP_CONSOLE_UART_NUM);
            xQueueReset(uart_queue);
            break;
        default:
            break;
        }
    }
}

static void btstack_esp32_uart_init() {
    /* Drain stdout before reconfiguring it */
    fflush(stdout);
    fsync(fileno(stdout));

    int baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE;
#if SOC_UART_SUPPORT_REF_TICK
    uart_sclk_t clk_source = UART_SCLK_REF_TICK;
    // REF_TICK clock can't provide a high baudrate
    if (baud_rate > 1 * 1000 * 1000) {
#ifdef UART_SCLK_DEFAULT
        // defined in esp-idf v5
        clk_source = UART_SCLK_DEFAULT;
#else
        // backport defines from esp-idf v5 for ESP32, ESP32-C3, ESP32-S3
        clk_source = UART_SCLK_APB;
#endif
        ESP_LOGW(TAG, "light sleep UART wakeup might not work at the configured baud rate");
    }
#elif SOC_UART_SUPPORT_XTAL_CLK
    uart_sclk_t clk_source = UART_SCLK_XTAL;
#else
    #error "No UART clock source is aware of DFS"
#endif // SOC_UART_SUPPORT_xxx

    uart_config_t uart_config = {
        .baud_rate = baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = clk_source
    };
    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(CONFIG_ESP_CONSOLE_UART_NUM, &uart_config));
#ifdef CONFIG_ESP_UART_CUSTOM
    ESP_ERROR_CHECK(uart_set_pin(
            CONFIG_ESP_CONSOLE_UART_NUM,
            CONFIG_ESP_CONSOLE_UART_TX_GPIO,
            CONFIG_ESP_CONSOLE_UART_RX_GPIO,
            UART_PIN_NO_CHANGE,
            UART_PIN_NO_CHANGE));
#endif
    // Setup UART buffered IO with event queue
    // Install UART driver using an event queue here
    ESP_ERROR_CHECK(uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, RX_BUF_SIZE,
            TX_BUF_SIZE, 10, &uart_queue, 0));

    /* Tell VFS to use UART driver */
    esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
}

static void btstack_stdin_task(void *arg){
    UNUSED(arg);

    uart_event_t event;
    do {
        if(xQueuePeek(uart_queue, (void*)&event, portMAX_DELAY) ) {
            // request poll
            btstack_run_loop_freertos_trigger();
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    } while(1);
}

void btstack_stdin_setup(void (*handler)(char c)){
    // set handler
    stdin_handler = handler;

    btstack_esp32_uart_init();

    // set up polling data_source
    btstack_run_loop_set_data_source_handler(&stdin_data_source, &btstack_stdin_process);
    btstack_run_loop_enable_data_source_callbacks(&stdin_data_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_add_data_source(&stdin_data_source);

    //Create a task to block on UART RX
    xTaskCreate(btstack_stdin_task, "btstack_stdin", 2048, NULL, 12, NULL);
}
