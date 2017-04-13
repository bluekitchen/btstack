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

#define SHA256_BLOCK_LENGTH  64
#define SHA256_DIGEST_LENGTH 32

typedef struct SHA256_CTX {
	uint32_t	state[8];
	uint64_t	bitcount;
	uint8_t	buffer[SHA256_BLOCK_LENGTH];
} SHA256_CTX;

extern void SHA256_Init(SHA256_CTX *ctx);
extern void SHA256_Update(SHA256_CTX *ctx, const uint8_t *message, size_t message_size);
extern void SHA256_Final(uint8_t digest[SHA256_DIGEST_LENGTH], SHA256_CTX *ctx);

typedef struct SHA256_HashContext {
    uECC_HashContext uECC;
    SHA256_CTX ctx;
} SHA256_HashContext;

static void init_SHA256(uECC_HashContext *base) {
    SHA256_HashContext *context = (SHA256_HashContext *)base;
    SHA256_Init(&context->ctx);
}

static void update_SHA256(uECC_HashContext *base,
                          const uint8_t *message,
                          unsigned message_size) {
    SHA256_HashContext *context = (SHA256_HashContext *)base;
    SHA256_Update(&context->ctx, message, message_size);
}

static void finish_SHA256(uECC_HashContext *base, uint8_t *hash_result) {
    SHA256_HashContext *context = (SHA256_HashContext *)base;
    SHA256_Final(hash_result, &context->ctx);
}

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
    uint8_t tmp[2 * SHA256_DIGEST_LENGTH + SHA256_BLOCK_LENGTH];
    SHA256_HashContext ctx = {{
        &init_SHA256,
        &update_SHA256,
        &finish_SHA256,
        SHA256_BLOCK_LENGTH,
        SHA256_DIGEST_LENGTH,
        tmp
    }};
    
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
        
        if (!uECC_sign_deterministic(private, hash, &ctx.uECC, sig)) {
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
