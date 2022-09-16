/*
 * Copyright (C) 2014 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "btstack_stdin_posix.c"

#include <stdio.h>
#include <unistd.h>

#include "btstack_debug.h"
#include "btstack_run_loop.h"

#include "btstack_stdin.h"
#include <termios.h>

static btstack_data_source_t stdin_source;
static int activated = 0;
static void (*stdin_handler)(char c);

// read single byte after data source callback was triggered
static void stdin_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type){
    UNUSED(ds);
    UNUSED(callback_type);

    char data;
    int result = read(stdin_source.source.fd, &data, 1);
    if (result < 1) return;
    if (stdin_handler == NULL) return;

#ifdef ENABLE_BTSTACK_STDIN_LOGGING
    log_info("stdin: %c", data);
#endif

    (*stdin_handler)(data);
}

void btstack_stdin_setup(void (*handler)(char c)){

    if (activated) return;

    // disable line buffering
    struct termios term = {0};
    if (tcgetattr(0, &term) < 0)
            perror("tcsetattr()");
    term.c_lflag &= ~ICANON;
    term.c_lflag &= ~ECHO;
    if (tcsetattr(0, TCSANOW, &term) < 0) {
        perror("tcsetattr ICANON");
    }

    stdin_handler = handler;

    btstack_run_loop_set_data_source_fd(&stdin_source, 0); // stdin

    btstack_run_loop_enable_data_source_callbacks(&stdin_source, DATA_SOURCE_CALLBACK_READ);
    btstack_run_loop_set_data_source_handler(&stdin_source, stdin_process);
    btstack_run_loop_add_data_source(&stdin_source);

    activated = 1;
}

void btstack_stdin_reset(void){
    if (!activated) return;
    activated = 0;
    stdin_handler = NULL;

    btstack_run_loop_remove_data_source(&stdin_source);    

    struct termios term = {0};
    if (tcgetattr(0, &term) < 0){
        perror("tcsetattr()");
    }
    term.c_lflag |= ICANON;
    term.c_lflag |= ECHO;
    if (tcsetattr(0, TCSANOW, &term) < 0){
        perror("tcsetattr ICANON");
    }
}

