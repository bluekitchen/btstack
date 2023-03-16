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
#include "hal_cpu.h"
#include "sha256.h"

#ifdef HAVE_BTSTACK_STDIN
__attribute__((weak)) void hal_cpu_reset(void)
{
}

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
            hal_cpu_reset();
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

#ifdef HAVE_BTSTACK_STDIN
    btstack_stdin_setup(stdin_process);
#endif

#if 1
    printf("mbedtls port on stm32f407-discovery\r\n");

    /* sha256 test */
    char *source_cxt = "ye.he@amlogic.com";
    char digest_cxt[64] = {0};

    printf("source context is:%s\r\n", source_cxt);

    mbedtls_sha256_context sha256_ctx;
    mbedtls_sha256_init(&sha256_ctx);
    mbedtls_sha256_starts(&sha256_ctx, 0);
    mbedtls_sha256_update(&sha256_ctx, (unsigned char *)source_cxt, strlen(source_cxt));
    mbedtls_sha256_finish(&sha256_ctx, (unsigned char *)digest_cxt);
    mbedtls_sha256_free(&sha256_ctx);

    int i = 0;
    printf("sha256 digest context is:[");
    while (digest_cxt[i]) {
        printf("%02x", digest_cxt[i]);
        i++;
    }
    printf("]\r\n");
#endif

#if defined(MCUBOOT_IMG_BOOTLOADER)
    printf("I am mcuboot bootloader!\r\n");
    main_boot();
#endif

    return 0;
}

