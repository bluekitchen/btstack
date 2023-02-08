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

#define BTSTACK_FILE__ "mcuboot_bootloader_demo.c"


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "btstack.h"

#ifdef HAVE_BTSTACK_STDIN
static void show_usage(void) {
    printf("writ usage ---\n");
    printf("A: mcuboot do boot!\n");
    printf("B: mcuboot trigger image swap permanent!\n");
    printf("C: mcuboot trigger image swap temporary!\n");
    printf("D: mcuboot trigger image swap permanent from temporary!\n");
    printf("R: system reset!\n");
}

static void stdin_process(char cmd){
    switch (cmd){
        case 'A':
            printf("mcuboot trigger do boot!\r\n");
            main_boot();
            break;
        case 'B':
            printf("mcuboot trigger image swap permanent!\r\n");
            boot_set_pending(1);
            break;
        case 'C':
            printf("mcuboot trigger image swap temporary!\r\n");
            boot_set_pending(0);
            break;
        case 'D':
            printf("mcuboot trigger image swap permanent from temporary!\r\n");
            boot_set_confirmed();
            break;
        case 'R':
            printf("system reset!\r\n");
            volatile uint32_t *SCB_AIRCR = (uint32_t *)0xE000ED0C; //0xE000ED0C:SCB->AIRCR
            *SCB_AIRCR = 0x05FA0000|(uint32_t)0x04;
            break;
        default:
            show_usage();
            return;
    }
}
#endif

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]) {
    (void)argc;
    (void)argv;
#if defined(MCUBOOT_IMG_BOOTLOADER)
    printf("I am mcuboot bootloader!\r\n");
#endif

#ifdef HAVE_BTSTACK_STDIN
    btstack_stdin_setup(stdin_process);
#endif

    return 0;
}

