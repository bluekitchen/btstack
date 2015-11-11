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

#include <errno.h>
#include <stdio.h>
#include <btstack/run_loop.h>
#include <stdlib.h>

#include "stdin_support.h"

#ifndef _WIN32
#include <termios.h>
#endif

static data_source_t stdin_source;
static int activated = 0;

void btstack_stdin_setup(int (*stdin_process)(data_source_t *_ds)){

#ifndef _WIN32
    struct termios term = {0};
    if (tcgetattr(0, &term) < 0)
            perror("tcsetattr()");
    term.c_lflag &= ~ICANON;
    term.c_lflag &= ~ECHO;
    if (tcsetattr(0, TCSANOW, &term) < 0) {
        perror("tcsetattr ICANON");
    }
#endif

    stdin_source.fd = 0; // stdin
    stdin_source.process = stdin_process;
    run_loop_add_data_source(&stdin_source);

    activated = 1;
}

void btstack_stdin_reset(void){
    if (!activated) return;
    activated = 0;
    
    run_loop_remove_data_source(&stdin_source);    

#ifndef _WIN32
    struct termios term = {0};
    if (tcgetattr(0, &term) < 0){
        perror("tcsetattr()");
    }
    term.c_lflag |= ICANON;
    term.c_lflag |= ECHO;
    if (tcsetattr(0, TCSANOW, &term) < 0){
        perror("tcsetattr ICANON");
    }
#endif
}

static int getstring(char *line, int size)
{
    int i = 0;
    while (1){
        int ch = getchar();
        if ( ch == '\n' || ch == EOF ) {
            printf("\n");
            break;
        }
        if ( ch == '\b' || ch == 0x7f){
            printf("\b \b");
            i--;
            continue;
        }
        putchar(ch);
        line[i++] = ch;
    }
    line[i] = 0;
    return i;
}

uint32_t btstack_stdin_query_int(const char * fieldName){
    printf("Please enter new int value for %s:\n", fieldName);
    char buffer[80];
    getstring(buffer, sizeof(buffer));
    return atoi(buffer);
}

uint32_t btstack_stdin_query_hex(const char * fieldName){
    printf("Please enter new hex value for %s:\n", fieldName);
    char buffer[80];
    getstring(buffer, sizeof(buffer));
    uint32_t value;
    sscanf(buffer, "%x", &value);
    return value;
}

