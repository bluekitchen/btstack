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
#include <unistd.h>

#include "btstack_run_loop.h"
#include <stdlib.h>

#include "stdin_support.h"

// From MSDN:
// __WIN32 Defined as 1 when the compilation target is 32-bit ARM, 64-bit ARM, x86, or x64.
//         Otherwise, undefined.

#ifdef _WIN32
#include <Windows.h>
#include <conio.h>  //provides non standard getch() function
#include <signal.h>
#else
#include <termios.h>
#endif

static btstack_data_source_t stdin_source;
static int activated = 0;

#ifdef _WIN32
static HANDLE stdin_reader_thread_handle;
static char   key_read_buffer;
static HANDLE key_processed_handle;

static WINAPI DWORD stdin_reader_thread_process(void * p){
    while (1){
        key_read_buffer = getch();
        SignalObjectAndWait(stdin_source.handle , key_processed_handle, INFINITE, FALSE);        
    }
    return 0;
}
#endif

#if 0
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
#endif

void btstack_stdin_setup(void (*stdin_process)(btstack_data_source_t *_ds, btstack_data_source_callback_type_t callback_type)){

    if (activated) return;

#ifdef _WIN32

    // asynchronous io on stdin via OVERLAPPED seems to be problematic. 

    // Use separate thread and event objects instead
    stdin_source.handle  = CreateEvent(NULL, FALSE, FALSE, NULL);
    key_processed_handle = CreateEvent(NULL, FALSE, FALSE, NULL);
    // default attributes, default stack size, proc, args, start immediately, don't care for thread id
    stdin_reader_thread_handle = CreateThread(NULL, 0, &stdin_reader_thread_process, NULL, 0, NULL);

#else
    // disable line buffering
    struct termios term = {0};
    if (tcgetattr(0, &term) < 0)
            perror("tcsetattr()");
    term.c_lflag &= ~ICANON;
    term.c_lflag &= ~ECHO;
    if (tcsetattr(0, TCSANOW, &term) < 0) {
        perror("tcsetattr ICANON");
    }

    btstack_run_loop_set_data_source_fd(&stdin_source, 0); // stdin
#endif

    btstack_run_loop_enable_data_source_callbacks(&stdin_source, DATA_SOURCE_CALLBACK_READ);
    btstack_run_loop_set_data_source_handler(&stdin_source, stdin_process);
    btstack_run_loop_add_data_source(&stdin_source);

    activated = 1;
}

void btstack_stdin_reset(void){
    if (!activated) return;
    activated = 0;
    
    btstack_run_loop_remove_data_source(&stdin_source);    

#ifdef _WIN32
    // shutdown thread
    TerminateThread(stdin_reader_thread_handle, 0);
    WaitForSingleObject(stdin_reader_thread_handle, INFINITE);
    CloseHandle(stdin_reader_thread_handle);

    // free events
    CloseHandle(stdin_source.handle);
    CloseHandle(key_processed_handle);
#else
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

// read single byte after data source callback was triggered
char btstack_stdin_read(void){
    char data;
#ifdef _WIN32

    // raise SIGINT for CTRL-c on main thread
    if (key_read_buffer == 0x03){
        raise(SIGINT);
        return 0;
    }

    data = key_read_buffer;
    SetEvent(key_processed_handle);

#else
    read(stdin_source.fd, &data, 1);
#endif

    return data;
}

