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

void vli_print(uint8_t *vli, unsigned int size) {
    while (size) {
        printf("%02X ", (unsigned)vli[size - 1]);
        --size;
    }
}

int main() {
#if LPC11XX
    uartInit(BAUD_115200);
	initTime();
	
    uECC_set_rng(&fake_rng);
#endif
	
    int i;
    uint8_t private1[uECC_BYTES];
    uint8_t private2[uECC_BYTES];
    uint8_t public1[uECC_BYTES * 2];
    uint8_t public2[uECC_BYTES * 2];
    uint8_t secret1[uECC_BYTES];
    uint8_t secret2[uECC_BYTES];
    
    printf("Testing 256 random private key pairs\n");

    for (i = 0; i < 256; ++i) {
        printf(".");
    #if !LPC11XX
        fflush(stdout);
    #endif

        if (!uECC_make_key(public1, private1) || !uECC_make_key(public2, private2)) {
            printf("uECC_make_key() failed\n");
            return 1;
        }

        if (!uECC_shared_secret(public2, private1, secret1)) {
            printf("shared_secret() failed (1)\n");
            return 1;
        }

        if (!uECC_shared_secret(public1, private2, secret2)) {
            printf("shared_secret() failed (2)\n");
            return 1;
        }
        
        if (memcmp(secret1, secret2, sizeof(secret1)) != 0) {
            printf("Shared secrets are not identical!\n");
            printf("Shared secret 1 = ");
            vli_print(secret1, uECC_BYTES);
            printf("\n");
            printf("Shared secret 2 = ");
            vli_print(secret2, uECC_BYTES);
            printf("\n");
            printf("Private key 1 = ");
            vli_print(private1, uECC_BYTES);
            printf("\n");
            printf("Private key 2 = ");
            vli_print(private2, uECC_BYTES);
            printf("\n");
        }
    }
    printf("\n");
    
    return 0;
}
