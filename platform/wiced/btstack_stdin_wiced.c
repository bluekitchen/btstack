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

#define BTSTACK_FILE__ "btstack_stdin_wiced.c"

#include <stdio.h>

#include "btstack_run_loop_wiced.h"
#include "btstack_defines.h"
#include <stdlib.h>

#include "btstack_stdin.h"
#include "wiced.h"

static int activated = 0;
static wiced_thread_t stdin_reader_thread;
static void (*stdin_handler)(char c);

static wiced_result_t stdin_reader_notify(void * p){
    char c = (char)(uintptr_t) p;
    (*stdin_handler)(c);
    return WICED_SUCCESS;
}

static btstack_context_callback_registration_t callback_registration;
static void stdin_reader_thread_process(wiced_thread_arg_t p){
    UNUSED(p);
    callback_registration.callback = &callback_registration;
    while (true){
        uint8_t c = getchar();
        callback_registration.context =  (void *)(uintptr_t) c;
        btstack_run_loop_execute_code_on_main_thread(&callback_registration);
    }
}

void btstack_stdin_setup(void (*handler)(char c)){
    if (activated) return;
    activated = 1;
    stdin_handler = handler;

    /* Remove buffering from all std streams */
    setvbuf( stdin,  NULL, _IONBF, 0 );
    setvbuf( stdout, NULL, _IONBF, 0 );
    setvbuf( stderr, NULL, _IONBF, 0 );

    wiced_rtos_create_thread(&stdin_reader_thread, WICED_APPLICATION_PRIORITY, "STDIN", &stdin_reader_thread_process, 512, NULL);
}

void btstack_stdin_reset(void){
    if (!activated) return;
    // TODO: implement
}
