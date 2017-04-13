/* Copyright 2014, Kenneth MacKay. Licensed under the BSD 2-clause license. */

#include "uECC.h"

#include <stdio.h>
#include <string.h>

#ifndef uECC_TEST_NUMBER_OF_ITERATIONS
#define uECC_TEST_NUMBER_OF_ITERATIONS   256
#endif

#if LPC11XX

#include "/Projects/lpc11xx/peripherals/uart.h"
#include "/Projects/lpc11xx/peripherals/time.h"

static uint64_t g_rand = 88172645463325252ull;
int fake_rng(uint8_t *dest, unsigned size) {
    while(size) {
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

void vli_print(char *str, uint8_t *vli, unsigned int size) {
    printf("%s ", str);
    while (size) {
        printf("%02X ", (unsigned)vli[size - 1]);
        --size;
    }
    printf("\n");
}

int main() {
#if LPC11XX
    uartInit(BAUD_115200);
        initTime();
        
    uECC_set_rng(&fake_rng);
#endif

    uint8_t public[uECC_BYTES * 2];
    uint8_t private[uECC_BYTES];
    uint8_t compressed_point[uECC_BYTES + 1];
    uint8_t decompressed_point[uECC_BYTES * 2];

    int i;
    printf("Testing compression and decompression of %d random EC points\n", uECC_TEST_NUMBER_OF_ITERATIONS);

    for (i = 0; i < uECC_TEST_NUMBER_OF_ITERATIONS; ++i) {
        printf(".");
    #if !LPC11XX
        fflush(stdout);
    #endif

        /* Generate arbitrary EC point (public) on Curve */
        if (!uECC_make_key(public, private)) {
            printf("uECC_make_key() failed\n");
            continue;
        }

        /* compress and decompress point */
        uECC_compress(public, compressed_point);
        uECC_decompress(compressed_point, decompressed_point);

        if (memcmp(public, decompressed_point, 2 * uECC_BYTES) != 0) {
            printf("Original and decompressed points are not identical!\n");
            vli_print("Original point =     ", public, 2 * uECC_BYTES);
            vli_print("Compressed point =   ", compressed_point, uECC_BYTES + 1);
            vli_print("Decompressed point = ", decompressed_point, 2 * uECC_BYTES);
        }
    }
    printf("\n");

    return 0;
}
