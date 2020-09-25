/* Copyright 2014, Kenneth MacKay. Licensed under the BSD 2-clause license. */

#include "uECC.h"

#include <stdio.h>
#include <string.h>

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "btstack_debug.h"

typedef uint8_t sm_key24_t[3];
typedef uint8_t sm_key56_t[7];
typedef uint8_t sm_key_t[16];
typedef uint8_t sm_key256_t[32];

// P256 Set 1
static const char * set1_private_a_string = "3f49f6d4a3c55f3874c9b3e3d2103f504aff607beb40b7995899b8a6cd3c1abd";
static const char * set1_private_b_string = "55188b3d32f6bb9a900afcfbeed4e72a59cb9ac2f19d7cfb6b4fdd49f47fc5fd";
static const char * set1_public_a_string = \
    "20b003d2f297be2c5e2c83a7e9f9a5b9eff49111acf4fddbcc0301480e359de6" \
    "dc809c49652aeb6d63329abf5a52155c766345c28fed3024741c8ed01589d28b";
static const char * set1_public_b_string = \
    "1ea1f0f01faf1d9609592284f19e4c0047b58afd8615a69f559077b22faaa190" \
    "4c55f33e429dad377356703a9ab85160472d1130e28e36765f89aff915b1214a";
static const char * set1_dh_key_string    = "ec0234a357c8ad05341010a60a397d9b99796b13b4f866f1868d34f373bfa698";

// P256 Set 1
static const char * set2_private_a_string = "06a516693c9aa31a6084545d0c5db641b48572b97203ddffb7ac73f7d0457663";
static const char * set2_private_b_string = "529aa0670d72cd6497502ed473502b037e8803b5c60829a5a3caa219505530ba";
static const char * set2_public_a_string = \
    "2c31a47b5779809ef44cb5eaaf5c3e43d5f8faad4a8794cb987e9b03745c78dd" \
    "919512183898dfbecd52e2408e43871fd021109117bd3ed4eaf8437743715d4f";
static const char * set2_public_b_string = \
    "f465e43ff23d3f1b9dc7dfc04da8758184dbc966204796eccf0d6cf5e16500cc" \
    "0201d048bcbbd899eeefc424164e33c201c2b010ca6b4d43a8a155cad8ecb279";
static const char * set2_dh_key_string    = "ab85843a2f6d883f62e5684b38e307335fe6e1945ecd19604105c6f23221eb69";

uint32_t big_endian_read_32( const uint8_t * buffer, int pos) {
    return ((uint32_t) buffer[(pos)+3]) | (((uint32_t)buffer[(pos)+2]) << 8) | (((uint32_t)buffer[(pos)+1]) << 16) | (((uint32_t) buffer[pos]) << 24);
}

void big_endian_store_32(uint8_t *buffer, uint16_t pos, uint32_t value){
    buffer[pos++] = value >> 24;
    buffer[pos++] = value >> 16;
    buffer[pos++] = value >> 8;
    buffer[pos++] = value;
}

static void hexdump_key(void *data, int size){
    if (size <= 0) return;
    int i;
    for (i=0; i<size;i++){
        printf("%02X", ((uint8_t *)data)[i]);
    }
    printf("\n");
}

static int nibble_for_char(char c){
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'F' + 10;
    return -1;
}

static int parse_hex(uint8_t * buffer, const char * hex_string){
    int len = 0;
    while (*hex_string){
        if (*hex_string == ' '){
            hex_string++;
            continue;
        }
        int high_nibble = nibble_for_char(*hex_string++);
        int low_nibble = nibble_for_char(*hex_string++);
        int value =  (high_nibble << 4) | low_nibble;
        buffer[len++] = value;
    }
    return len;
}


static int test_generate_f_rng(uint8_t * buffer, unsigned size){
    // printf("test_generate_f_rng: size %u\n", (int)size);
    while (size) {
        *buffer++ = rand() & 0xff;
        size--;
    }
    return 1;
}

int test_set1(void){
    uint8_t private1[uECC_BYTES];
    uint8_t private2[uECC_BYTES];
    uint8_t public1[uECC_BYTES * 2];
    uint8_t public1_computed[uECC_BYTES * 2];
    uint8_t public2[uECC_BYTES * 2];
    uint8_t secret1[uECC_BYTES];
    uint8_t secret2[uECC_BYTES];
    uint8_t secret[uECC_BYTES];

    parse_hex(private1, set1_private_a_string);
    parse_hex(public1,  set1_public_a_string);
    parse_hex(private2, set1_private_b_string);
    parse_hex(public2,  set1_public_b_string);
    parse_hex(secret,   set1_dh_key_string);

    if (!uECC_compute_public_key(private1, public1_computed)){
        printf("uECC_compute_public_key() failed\n");
    }
    if (memcmp(public1, public1_computed, sizeof(public1_computed)) != 0) {
        printf("Computed public key differs from test data!\n");
        printf("Computed key = ");
        hexdump_key(public1_computed, uECC_BYTES * 2);
        printf("Expected ke = ");
        hexdump_key(public1, uECC_BYTES * 2);
        return 0;
    }

    if (!uECC_shared_secret(public2, private1, secret1)) {
        printf("shared_secret() failed (1)\n");
        return 0;
    }

    if (!uECC_shared_secret(public1, private2, secret2)) {
        printf("shared_secret() failed (2)\n");
        return 0;
    }
    
    if (memcmp(secret1, secret2, sizeof(secret1)) != 0) {
        printf("Shared secrets are not identical!\n");
        printf("Shared secret 1 = ");
        hexdump_key(secret1, uECC_BYTES);
        printf("Shared secret 2 = ");
        hexdump_key(secret2, uECC_BYTES);
        printf("Expected secret = "); hexdump_key(secret1, uECC_BYTES);
        return 0;
    }
    // printf("Shared secret = "); hexdump_key(secret1, uECC_BYTES);
    return 1;
}

int test_set2(void){
    uint8_t private1[uECC_BYTES];
    uint8_t private2[uECC_BYTES];
    uint8_t public1[uECC_BYTES * 2];
    uint8_t public1_computed[uECC_BYTES * 2];
    uint8_t public2[uECC_BYTES * 2];
    uint8_t secret1[uECC_BYTES];
    uint8_t secret2[uECC_BYTES];
    uint8_t secret[uECC_BYTES];

    parse_hex(private1, set2_private_a_string);
    parse_hex(public1,  set2_public_a_string);
    parse_hex(private2, set2_private_b_string);
    parse_hex(public2,  set2_public_b_string);
    parse_hex(secret,   set2_dh_key_string);

    if (!uECC_compute_public_key(private1, public1_computed)){
        printf("uECC_compute_public_key() failed\n");
    }
    
    if (memcmp(public1, public1_computed, sizeof(public1_computed)) != 0) {
        printf("Computed public key differs from test data!\n");
        printf("Computed key = ");
        hexdump_key(public1_computed, uECC_BYTES * 2);
        printf("Expected key = ");
        hexdump_key(public1, uECC_BYTES * 2);
        return 0;
    }

    if (!uECC_shared_secret(public2, private1, secret1)) {
        printf("shared_secret() failed (1)\n");
        return 0;
    }

    if (!uECC_shared_secret(public1, private2, secret2)) {
        printf("shared_secret() failed (2)\n");
        return 0;
    }
    
    if (memcmp(secret1, secret2, sizeof(secret1)) != 0) {
        printf("Shared secrets are not identical!\n");
        printf("Shared secret 1 = ");
        hexdump_key(secret1, uECC_BYTES);
        printf("Shared secret 2 = ");
        hexdump_key(secret2, uECC_BYTES);
        printf("Expected secret = "); hexdump_key(secret1, uECC_BYTES);
        return 0;
    }
    // printf("Shared secret = "); hexdump_key(secret1, uECC_BYTES);
    return 1;
}

int test_generate(void){
    // use stdlib rand with fixed seed for testing
    srand(0);

    uint8_t d[uECC_BYTES];
    uint8_t q[uECC_BYTES * 2];

    if (!uECC_make_key(q, d)) {
        printf("uECC_make_key() failed\n");
        return 1;
    }

    // print keypair
    printf("d: "); hexdump_key(d, uECC_BYTES);
    printf("X: "); hexdump_key(&q[00], uECC_BYTES);
    printf("Y: "); hexdump_key(&q[16], uECC_BYTES);

    // verify public key
    if (!uECC_valid_public_key(q)){
        printf("uECC_valid_public_key() == 0 -> generated public key invalid\n");
        return 1;
    }

    // verify private key?
    // TODO:

    return 0;
}

int main(void){
	// check configuration
	btstack_assert(uECC_curve() == uECC_secp256r1);
	btstack_assert(uECC_bytes() == 32);

	// check zero key is invalid
	uint8_t q[uECC_BYTES * 2];
	memset(q, 0, sizeof(q));
	btstack_assert(uECC_valid_public_key(q) == 0);

	// check key genration without rng
	uint8_t d[uECC_BYTES];
	btstack_assert(uECC_make_key(q, d) == 0);

	uECC_set_rng(&test_generate_f_rng);
    test_set1();
    test_set2();
    test_generate();
    return 0;
}
