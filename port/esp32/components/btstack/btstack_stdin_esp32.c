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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"

// handle esp-idf change from CONFIG_CONSOLE_UART_NUM to CONFIG_ESP_CONSOLE_UART_NUM
#ifndef CONFIG_ESP_CONSOLE_UART_NUM
#define CONFIG_ESP_CONSOLE_UART_NUM CONFIG_CONSOLE_UART_NUM
#endif

#define BUF_SIZE (UART_FIFO_LEN)

volatile int stdin_character_received;
volatile char stdin_character;
static void (*stdin_handler)(char c);
static btstack_data_source_t stdin_data_source;

static void btstack_stdin_process(struct btstack_data_source *ds, btstack_data_source_callback_type_t callback_type){
	if (!stdin_character_received) return;
	if (stdin_handler){
		(*stdin_handler)(stdin_character);
	}
	stdin_character_received = 0;
}

static void btstack_stdin_task(void *arg){
	UNUSED(arg);

    //Install UART driver, and get the queue.
    uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, UART_FIFO_LEN * 2, UART_FIFO_LEN * 2, 0, NULL, 0);

    do {
    	// read single byte
        uart_read_bytes(CONFIG_ESP_CONSOLE_UART_NUM, (uint8_t*) &stdin_character, 1, portMAX_DELAY);
		stdin_character_received = 1;

		// request poll
		btstack_run_loop_freertos_trigger();

		// busy wait for input processed
		while (stdin_character_received){
			vTaskDelay(10 / portTICK_PERIOD_MS);
		}
    } while(1);
}

void btstack_stdin_setup(void (*handler)(char c)){
	// set handler
	stdin_handler = handler;

	// set up polling data_source
	btstack_run_loop_set_data_source_handler(&stdin_data_source, &btstack_stdin_process);
	btstack_run_loop_enable_data_source_callbacks(&stdin_data_source, DATA_SOURCE_CALLBACK_POLL);
	btstack_run_loop_add_data_source(&stdin_data_source);

	//Create a task to block on UART RX
	xTaskCreate(btstack_stdin_task, "btstack_stdin", 2048, NULL, 12, NULL);
}
