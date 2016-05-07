#include <stdio.h>
#include <string.h>
#include <stdint.h>

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
static const char * set2_private_a_string = "06a51669 3c9aa31a 6084545d 0c5db641 b48572b9 7203ddff b7ac73f7 d0457663";
static const char * set2_private_b_string = "529aa067 0d72cd64 97502ed4 73502b03 7e8803b5 c60829a5 a3caa219 505530ba";
static const char * set2_public_ax_string = "2c31a47b 5779809e f44cb5ea af5c3e43 d5f8faad 4a8794cb 987e9b03 745c78dd";
static const char * set2_public_ay_string = "91951218 3898dfbe cd52e240 8e43871f d0211091 17bd3ed4 eaf84377 43715d4f";
static const char * set2_publix_bx_string = "f465e43f f23d3f1b 9dc7dfc0 4da87581 84dbc966 204796ec cf0d6cf5 e16500cc";
static const char * set2_public_by_string = "0201d048 bcbbd899 eeefc424 164e33c2 01c2b010 ca6b4d43 a8a155ca d8ecb279";
static const char * set2_dh_key_string    = "ab85843a 2f6d883f 62e5684b 38e30733 5fe6e194 5ecd1960 4105c6f2 3221eb69";

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

int main(void){
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
	printf("loading curve %d\n", res);

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
        printf( "Set 1: da * Pb incorrect\n");
    }  else {
        printf( "Set 1: da * Pb correct\n");
    }
    if (mbedtls_mpi_cmp_mpi(&R2.X, &dh_key)){
        printf( "Set 1: db * Pa incorrect\n");
    }  else {
        printf( "Set 1: db * Pa correct\n");
    }

exit:
    mbedtls_ecp_group_free( &grp );
    mbedtls_ecp_point_free( &R1 );
    mbedtls_ecp_point_free( &R2 );
    mbedtls_ecp_point_free( &Pa );
    mbedtls_ecp_point_free( &Pb );
    mbedtls_mpi_free( &da );
    mbedtls_mpi_free( &db );
    mbedtls_mpi_free( &dh_key );
}
