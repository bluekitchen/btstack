#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdio.h>
#define mbedtls_printf     printf
#endif

#include "mbedtls/ecp.h"

typedef uint8_t sm_key24_t[3];
typedef uint8_t sm_key56_t[7];
typedef uint8_t sm_key_t[16];
typedef uint8_t sm_key256_t[32];

// P256 Set 1
static const char * set1_private_a_string = "3f49f6d4a3c55f3874c9b3e3d2103f504aff607beb40b7995899b8a6cd3c1abd";
static const char * set1_private_b_string = "55188b3d32f6bb9a900afcfbeed4e72a59cb9ac2f19d7cfb6b4fdd49f47fc5fd";
static const char * set1_public_ax_string = "20b003d2f297be2c5e2c83a7e9f9a5b9eff49111acf4fddbcc0301480e359de6";
static const char * set1_public_ay_string = "dc809c49652aeb6d63329abf5a52155c766345c28fed3024741c8ed01589d28b";
static const char * set1_public_bx_string = "1ea1f0f01faf1d9609592284f19e4c0047b58afd8615a69f559077b22faaa190";
static const char * set1_public_by_string = "4c55f33e429dad377356703a9ab85160472d1130e28e36765f89aff915b1214a";
static const char * set1_dh_key_string    = "ec0234a357c8ad05341010a60a397d9b99796b13b4f866f1868d34f373bfa698";

// P256 Set 1
static const char * set2_private_a_string = "06a516693c9aa31a6084545d0c5db641b48572b97203ddffb7ac73f7d0457663";
static const char * set2_private_b_string = "529aa0670d72cd6497502ed473502b037e8803b5c60829a5a3caa219505530ba";
static const char * set2_public_ax_string = "2c31a47b5779809ef44cb5eaaf5c3e43d5f8faad4a8794cb987e9b03745c78dd";
static const char * set2_public_ay_string = "919512183898dfbecd52e2408e43871fd021109117bd3ed4eaf8437743715d4f";
static const char * set2_public_bx_string = "f465e43ff23d3f1b9dc7dfc04da8758184dbc966204796eccf0d6cf5e16500cc";
static const char * set2_public_by_string = "0201d048bcbbd899eeefc424164e33c201c2b010ca6b4d43a8a155cad8ecb279";
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
		*buffer++ = (high_nibble << 4) | low_nibble;
		len++;
	}
	return len;
}

static void hexdump2(void *data, int size){
    if (size <= 0) return;
    int i;
    for (i=0; i<size;i++){
        printf("%02X ", ((uint8_t *)data)[i]);
    }
    printf("\n");
}

int test_set1(void){
    mbedtls_ecp_group grp;
    mbedtls_ecp_point R1, R2;
    mbedtls_ecp_point Pa, Pb;
    mbedtls_mpi da, db, dh_key;

    mbedtls_ecp_group_init( &grp );
    mbedtls_ecp_point_init( &R1 );
    mbedtls_ecp_point_init( &R2 );
    mbedtls_ecp_point_init( &Pa );
    mbedtls_ecp_point_init( &Pb );
    mbedtls_mpi_init(&da);
    mbedtls_mpi_init(&db);
    mbedtls_mpi_init(&dh_key);

    int res = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
    if (res) {
        printf("set1: error loading curve %d\n", res);
        return 0;
    }

    // da * Pb
    mbedtls_mpi_read_string( &da,   16, set1_private_a_string );
    mbedtls_mpi_read_string( &Pb.X, 16, set1_public_bx_string );
    mbedtls_mpi_read_string( &Pb.Y, 16, set1_public_by_string );
    mbedtls_mpi_read_string( &Pb.Z, 16, "1" );
    mbedtls_ecp_mul(&grp, &R1, &da, &Pb, NULL, NULL);

    // db * Pa
    mbedtls_mpi_read_string( &Pa.X, 16, set1_public_ax_string );
    mbedtls_mpi_read_string( &Pa.Y, 16, set1_public_ay_string );
    mbedtls_mpi_read_string( &Pa.Z, 16, "1" );
    mbedtls_mpi_read_string( &db,   16, set1_private_b_string );
    mbedtls_ecp_mul(&grp, &R2, &db, &Pa, NULL, NULL);

    // checks
    mbedtls_mpi_read_string( &dh_key, 16, set1_dh_key_string);
    if (mbedtls_mpi_cmp_mpi(&R1.X, &dh_key)){
        printf( "set1: da * Pb incorrect\n");
        return 0;
    }  else {
        printf( "set1: da * Pb correct\n");
    }
    if (mbedtls_mpi_cmp_mpi(&R2.X, &dh_key)){
        printf( "set1: db * Pa incorrect\n");
        return 0;
    }  else {
        printf( "set1: db * Pa correct\n");
    }
    return 1;
}

int test_set2(void){
    mbedtls_ecp_group grp;
    mbedtls_ecp_point R1, R2;
    mbedtls_ecp_point Pa, Pb;
    mbedtls_mpi da, db, dh_key;

    mbedtls_ecp_group_init( &grp );
    mbedtls_ecp_point_init( &R1 );
    mbedtls_ecp_point_init( &R2 );
    mbedtls_ecp_point_init( &Pa );
    mbedtls_ecp_point_init( &Pb );
    mbedtls_mpi_init(&da);
    mbedtls_mpi_init(&db);

    int res = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
    if (res) {
        printf("set 2: error loading curve %d\n", res);
        return 0;
    }

    // da * Pb
    mbedtls_mpi_read_string( &da,   16, set2_private_a_string );
    mbedtls_mpi_read_string( &Pb.X, 16, set2_public_bx_string );
    mbedtls_mpi_read_string( &Pb.Y, 16, set2_public_by_string );
    mbedtls_mpi_read_string( &Pb.Z, 16, "1" );
    mbedtls_ecp_mul(&grp, &R1, &da, &Pb, NULL, NULL);

    // db * Pa
    mbedtls_mpi_read_string( &Pa.X, 16, set2_public_ax_string );
    mbedtls_mpi_read_string( &Pa.Y, 16, set2_public_ay_string );
    mbedtls_mpi_read_string( &Pa.Z, 16, "1" );
    mbedtls_mpi_read_string( &db,   16, set2_private_b_string );
    mbedtls_ecp_mul(&grp, &R2, &db, &Pa, NULL, NULL);

    // checks
    mbedtls_mpi_read_string( &dh_key, 16, set2_dh_key_string);
    if (mbedtls_mpi_cmp_mpi(&R1.X, &dh_key)){
        printf( "set2: da * Pb incorrect\n");
        return 0;
    }  else {
        printf( "set2: da * Pb correct\n");
    }
    if (mbedtls_mpi_cmp_mpi(&R2.X, &dh_key)){
        printf( "set2: db * Pa incorrect\n");
        return 0;
    }  else {
        printf( "set2: db * Pa correct\n");
    }
    return 1;
}

int test_generate_f_rng(void * context, unsigned char * buffer, size_t size){
    printf("test_generate_f_rng: size %u\n", (int)size);
    while (size) {
        *buffer++ = rand() & 0xff;
        size--;
    }
    return 0;
}

int test_generate(void){
    // use stdlib rand with fixed seed for testing
    srand(0);

    mbedtls_ecp_keypair keypair;
    mbedtls_ecp_keypair_init(&keypair);
    int res = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, &keypair, &test_generate_f_rng, NULL);
    if (res){
        printf("generate keypair failed %u\n", res);
        return 0;
    }
    // print keypair
    char buffer[100];
    size_t len;
    mbedtls_mpi_write_string( &keypair.d, 16, buffer, sizeof(buffer), &len);
    printf("d: %s\n", buffer);
    mbedtls_mpi_write_string( &keypair.Q.X, 16, buffer, sizeof(buffer), &len);
    printf("X: %s\n", buffer);
    mbedtls_mpi_write_string( &keypair.Q.Y, 16, buffer, sizeof(buffer), &len);
    printf("Y: %s\n", buffer);

    // verify
    res = mbedtls_ecp_check_pubkey(&keypair.grp, &keypair.Q);
    if (res){
        printf("public key invalid\n");
        return 0;
    }
    res = mbedtls_ecp_check_privkey(&keypair.grp, &keypair.d);
    if (res){
        printf("private key invalid\n");
        return 0;
    }
    return 0;
}

int main(void){
    test_set1();
    test_set2();
    test_generate();
    return 0;
}
