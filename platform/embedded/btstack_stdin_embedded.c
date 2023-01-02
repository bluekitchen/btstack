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

#define BTSTACK_FILE__ "btstack_stdin_embedded.c"

/*
 *  btstack_stdin_embedded.c
 *
 *  Common code to provide console input using an IRQ-driven console UART in hal_stdin.h
 *
 */

#include "btstack_config.h"

#include "btstack_stdin.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_embedded.h"

#ifdef ENABLE_SEGGER_RTT
#include "SEGGER_RTT.h"
#else
#include "hal_stdin.h"
#endif

static void (*stdin_handler)(char c);

#ifdef ENABLE_SEGGER_RTT

#define RTT_POLL_INTERVAL_MS 50

static btstack_timer_source_t stdin_timer_source;

static void btstack_stdin_timer_handler(btstack_timer_source_t *ts){
	if (SEGGER_RTT_HasKey()){
		int stdin_character = SEGGER_RTT_GetKey();
		(*stdin_handler)((uint8_t)stdin_character);
	}
    btstack_run_loop_set_timer(ts, RTT_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(ts);
}

#else

static btstack_data_source_t stdin_data_source;

// callback from hal_stdin is from irq context

volatile int stdin_character_received;
volatile char stdin_character;

static void btstack_stdin_handler(char c){
	stdin_character = c;
	stdin_character_received = 1;
	btstack_run_loop_poll_data_sources_from_irq();
}

static void btstack_stdin_process(struct btstack_data_source *ds, btstack_data_source_callback_type_t callback_type){
    UNUSED(ds);
    UNUSED(callback_type);
	if (!stdin_character_received) {
        return;
    }
	(*stdin_handler)(stdin_character);
	stdin_character_received = 0;
}

#endif

void btstack_stdin_setup(void (*handler)(char c)){
	// set handler
	stdin_handler = handler;

#ifdef ENABLE_SEGGER_RTT
    // start polling with timer
    btstack_run_loop_set_timer_handler(&stdin_timer_source, &btstack_stdin_timer_handler);
    btstack_run_loop_set_timer(&stdin_timer_source, RTT_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(&stdin_timer_source);
#else
    // set up polling data_source
    btstack_run_loop_set_data_source_handler(&stdin_data_source, &btstack_stdin_process);
    btstack_run_loop_enable_data_source_callbacks(&stdin_data_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_add_data_source(&stdin_data_source);

	// start receiving via hal_stdin.h
	hal_stdin_setup(&btstack_stdin_handler);
#endif
}
