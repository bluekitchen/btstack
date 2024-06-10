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

#define BTSTACK_FILE__ "btstack_stdin_windows.c"

#include <Windows.h>
#include <errno.h>
#include <stdio.h>

#include "btstack_run_loop.h"
#include "btstack_defines.h"
#include <stdlib.h>

#include "btstack_stdin.h"
#include "btstack_stdin_windows.h"

// From MSDN:
// __WIN32 Defined as 1 when the compilation target is 32-bit ARM, 64-bit ARM, x86, or x64.
//         Otherwise, undefined.

#include <conio.h>  //provides non standard getch() function
#include <signal.h>

static btstack_data_source_t stdin_source;
static int activated = 0;

static HANDLE stdin_reader_thread_handle;
static char   key_read_buffer;
static HANDLE key_processed_handle;
static void (*stdin_handler)(char c);
static void (*ctrl_c_handler)(void);

static DWORD WINAPI stdin_reader_thread_process(void * p){
    while (true){
        key_read_buffer = _getch();
        SignalObjectAndWait(stdin_source.source.handle, key_processed_handle, INFINITE, FALSE);
    }
    return 0;
}

static void stdin_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type){

    // handle CTRL-C
    if (key_read_buffer == 0x03){
        if (ctrl_c_handler != NULL){
            (*ctrl_c_handler)();
        } else {
            printf("Ignore CTRL-c\n");
        }
    } else {
        if (stdin_handler){
            (*stdin_handler)(key_read_buffer);
        }
    }

    SetEvent(key_processed_handle);
}

void btstack_stdin_windows_init(void){
    if (activated) return;

    // asynchronous io on stdin via OVERLAPPED seems to be problematic.

    // Use separate thread and event objects instead
    stdin_source.source.handle  = CreateEvent(NULL, FALSE, FALSE, NULL);
    key_processed_handle = CreateEvent(NULL, FALSE, FALSE, NULL);
    // default attributes, default stack size, proc, args, start immediately, don't care for thread id
    stdin_reader_thread_handle = CreateThread(NULL, 0, &stdin_reader_thread_process, NULL, 0, NULL);

    btstack_run_loop_enable_data_source_callbacks(&stdin_source, DATA_SOURCE_CALLBACK_READ);
    btstack_run_loop_set_data_source_handler(&stdin_source, stdin_process);
    btstack_run_loop_add_data_source(&stdin_source);

    activated = 1;
}

void btstack_stdin_window_register_ctrl_c_callback(void (*callback)(void)){
    ctrl_c_handler = callback;
}

void btstack_stdin_setup(void (*handler)(char c)){
    btstack_stdin_windows_init();
    stdin_handler = handler;
}

void btstack_stdin_reset(void){
    if (!activated) return;
    activated = 0;
    stdin_handler = NULL;

    btstack_run_loop_remove_data_source(&stdin_source);

    // shutdown thread
    TerminateThread(stdin_reader_thread_handle, 0);
    WaitForSingleObject(stdin_reader_thread_handle, INFINITE);
    CloseHandle(stdin_reader_thread_handle);

    // free events
    CloseHandle(stdin_source.source.handle);
    CloseHandle(key_processed_handle);
}

