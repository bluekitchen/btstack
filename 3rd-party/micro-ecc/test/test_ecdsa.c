/* Copyright 2014, Kenneth MacKay. Licensed under the BSD 2-clause license. */

#include "uECC.h"

#include <stdio.h>
#include <string.h>

#if LPC11XX

#include "/Projects/lpc11xx/peripherals/uart.h"
#include "/Projects/lpc11xx/peripherals/time.h"

static uint64_t g_rand = 88172645463325252ull;
int fake_rng(uint8_t *dest, unsigned size) {
    while (size) {
        g_rand ^= (g_rand << 13);
        g_rand ^= (g_rand >> 7);
        g_rand ^= (g_rand << 17);

        unsigned amount = (size > 8 ? 8 : size);
        memcpy(dest, &g_rand, amount);
        dest += amount;
        size -= amount;
    }
    return 1;
}

#endif

int main() {
#if LPC11XX
    uartInit(BAUD_115200);
	initTime();

    uECC_set_rng(&fake_rng);
#endif

    uint8_t public[uECC_BYTES * 2];
    uint8_t private[uECC_BYTES];
    uint8_t hash[uECC_BYTES];
    uint8_t sig[uECC_BYTES * 2];
    
    int i;
    printf("Testing 256 signatures\n");
    for (i = 0; i < 256; ++i) {
        printf(".");
    #if !LPC11XX
        fflush(stdout);
    #endif
        
        if (!uECC_make_key(public, private)) {
            printf("uECC_make_key() failed\n");
            continue;
        }
        memcpy(hash, public, uECC_BYTES);
        
        if (!uECC_sign(private, hash, sig)) {
            printf("uECC_sign() failed\n");
            continue;
        }
        
        if (!uECC_verify(public, hash, sig)) {
            printf("uECC_verify() failed\n");
        }
    }
    printf("\n");
    
    return 0;
}
