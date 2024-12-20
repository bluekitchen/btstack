/*
 * Copyright (C) 2023 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "btstack_stdio_esp32.c"

/*
 *  btstack_stdio_esp32.c
 *
 *  Uses blocking thread to wait for console input
 *  Busy waits until character has been processed
 */

#include "sdkconfig.h"

#ifdef CONFIG_ESP_CONSOLE_UART

#include "btstack_stdin.h"
#include "btstack_run_loop.h"
#include "btstack_defines.h"
#include "btstack_debug.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <unistd.h>
#include <errno.h>
#include <reent.h>

#include "driver/uart.h"

#include "esp_idf_version.h"
#include "esp_log.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
#include "driver/uart_vfs.h"
#else
#include "esp_vfs_dev.h"
#endif

static const char *TAG = "btstack_stdio";

// handle esp-idf change from CONFIG_CONSOLE_UART_NUM to CONFIG_ESP_CONSOLE_UART_NUM
#ifndef CONFIG_ESP_CONSOLE_UART_NUM
#define CONFIG_ESP_CONSOLE_UART_NUM CONFIG_CONSOLE_UART_NUM
#endif

static void (*stdin_handler)(char c);

// after the capacity of the TX buffer is exceeded the output will block, so
// in order to prevent that the TX buffer needs to be big enough or
// the baudrate needs to be increased.
// Usually the maximum baudrate for the target is preferred
#define RX_BUF_SIZE (1024)
#define TX_BUF_SIZE (4096)
static QueueHandle_t uart_queue = NULL;

static bool btstack_stdio_initialized;

static void btstack_stdio_process(void *context);

static btstack_context_callback_registration_t stdio_callback_context = {
        .callback = btstack_stdio_process,
        .context = NULL,
};

static void btstack_stdio_process(void *context){
    UNUSED(context);
    uart_event_t event;
    while( xQueueReceive(uart_queue, (void*)&event, 0) ) {
        switch( event.type ) {
        case UART_DATA: {
            uint8_t stdin_character;
            if(!stdin_handler) {
                ESP_ERROR_CHECK(uart_flush_input(CONFIG_ESP_CONSOLE_UART_NUM));
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
            ESP_ERROR_CHECK(uart_flush_input(CONFIG_ESP_CONSOLE_UART_NUM));
            xQueueReset(uart_queue);
            break;
        case UART_BUFFER_FULL:
            ESP_LOGI(TAG, "ring buffer full");
            ESP_ERROR_CHECK(uart_flush_input(CONFIG_ESP_CONSOLE_UART_NUM));
            xQueueReset(uart_queue);
            break;
        default:
            break;
        }
    }
}

static void btstack_stdio_task(void *arg){
    UNUSED(arg);

    uart_event_t event;
    do {
        if(xQueuePeek(uart_queue, (void*)&event, portMAX_DELAY) ) {
            // request poll
            btstack_run_loop_execute_on_main_thread(&stdio_callback_context);
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    } while(1);
}

// UART_SCLK_DEFAULT is not available on esp-idf version lower than v5
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
#define UART_SCLK_DEFAULT UART_SCLK_APB
#endif

void btstack_stdio_init(void) {
    /* Drain stdout before reconfiguring it */
    fflush(stdout);
    fsync(fileno(stdout));

    int baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE;
#if SOC_UART_SUPPORT_REF_TICK
    uart_sclk_t clk_source = UART_SCLK_REF_TICK;
    // REF_TICK clock can't provide a high baudrate
    if (baud_rate > 1 * 1000 * 1000) {
        clk_source = UART_SCLK_DEFAULT;
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
    ESP_ERROR_CHECK(uart_driver_install(
            CONFIG_ESP_CONSOLE_UART_NUM,
            RX_BUF_SIZE,
            TX_BUF_SIZE,
            10, &uart_queue,
            0));

    /* Tell VFS to use UART driver */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    uart_vfs_dev_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
#else
    esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
#endif

    //Create a task to block on UART RX
    xTaskCreate(btstack_stdio_task, "btstack_stdio", 2048, NULL, 12, NULL);

    btstack_stdio_initialized = true;
}

void btstack_stdin_setup(void (*handler)(char c)){
    if (btstack_stdio_initialized == false){
        ESP_LOGE(TAG, "to enable support for console input, call btstack_stdio_init first");
    }
    // set handler
    stdin_handler = handler;
}

#else
// Empty functions for backwards-compatiblitity
void btstack_stdio_init(void) {}
void btstack_stdin_setup(void (*handler)(char c)){
    (void) handler;
}

#endif /* CONFIG_ESP_CONSOLE_UART */
