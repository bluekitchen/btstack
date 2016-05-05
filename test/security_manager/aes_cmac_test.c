
#include "rijndael.h"
#include <stdio.h>
#include <string.h>

typedef uint8_t sm_key_t[16];
typedef uint8_t sm_key256_t[32];

static const char * key_string      = "2b7e1516 28aed2a6 abf71588 09cf4f3c";
static const char * k0_string       = "7df76b0c 1ab899b3 3e42f047 b91b546f";
static const char * k1_string       = "fbeed618 35713366 7c85e08f 7236a8de";
static const char * k2_string       = "f7ddac30 6ae266cc f90bc11e e46d513b";

static const char * m0_string       = "";
static const char * cmac_m0_string  = "bb1d6929 e9593728 7fa37d12 9b756746";
static const char * m16_string       = "6bc1bee2 2e409f96 e93d7e11 7393172a";
static const char * cmac_m16_string  = "070a16b4 6b4d4144 f79bdd9d d04a287c";
static const char * m40_string       = "6bc1bee2 2e409f96 e93d7e11 7393172a ae2d8a57 1e03ac9c 9eb76fac 45af8e51 30c81c46 a35ce411";
static const char * cmac_m40_string  = "dfa66747 de9ae630 30ca3261 1497c827";
static const char * m64_string       = "6bc1bee2 2e409f96 e93d7e11 7393172a ae2d8a57 1e03ac9c 9eb76fac 45af8e51 30c81c46 a35ce411 e5fbc119 1a0a52ef f69f2445 df4f9b17 ad2b417b e66c3710";
static const char * cmac_m64_string  = "51f0bebf 7e3b9d92 fc497417 79363cfe";

// f4 
static const char * f4_u_string 	= "20b003d2 f297be2c 5e2c83a7 e9f9a5b9 eff49111 acf4fddb cc030148 0e359de6";
static const char * f4_v_string 	= "55188b3d 32f6bb9a 900afcfb eed4e72a 59cb9ac2 f19d7cfb 6b4fdd49 f47fc5fd";
static const char * f4_x_string 	= "d5cb8454 d177733e ffffb2ec 712baeab";
static const char * f4_z_string 	= "00";
static const char * f4_cmac_string 	= "f2c916f1 07a9bd1c f1eda1be a974872d";

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

static void sm_shift_left_by_one_bit_inplace(int len, uint8_t * data){
    int i;
    int carry = 0;
    for (i=len-1; i >= 0 ; i--){
        int new_carry = data[i] >> 7;
        data[i] = data[i] << 1 | carry;
        carry = new_carry;
    }
}

void aes128_calc_cyphertext(uint8_t key[16], uint8_t plaintext[16], uint8_t cyphertext[16]){
	uint32_t rk[RKLENGTH(KEYBITS)];
	int nrounds = rijndaelSetupEncrypt(rk, &key[0], KEYBITS);
	rijndaelEncrypt(rk, nrounds, plaintext, cyphertext);
}

static void hexdump2(void *data, int size){
    if (size <= 0) return;
    int i;
    for (i=0; i<size;i++){
        printf("%02X ", ((uint8_t *)data)[i]);
    }
    printf("\n");
}

static void calc_subkeys(sm_key_t k0, sm_key_t k1, sm_key_t k2){
    memcpy(k1, k0, 16);
    sm_shift_left_by_one_bit_inplace(16, k1);
    if (k0[0] & 0x80){
        k1[15] ^= 0x87;
    }
    memcpy(k2, k1, 16);
    sm_shift_left_by_one_bit_inplace(16, k2);
    if (k1[0] & 0x80){
        k2[15] ^= 0x87;
    } 
}

#define LOG_KEY(NAME) { printf("%16s: ", #NAME); hexdump2(NAME, 16); }
#define PARSE_KEY(NAME) { parse_hex(NAME, NAME##_string); LOG_KEY(NAME); }
#define VALIDATE_KEY(NAME) { LOG_KEY(NAME); sm_key_t test; parse_hex(test, NAME##_string); if (memcmp(NAME, test, 16)){ printf("Error calculating key\n"); } }
#define VALIDATE_MESSAGE(NAME) validate_message(#NAME, NAME##_string, cmac_##NAME##_string)

static void aes_cmac(sm_key_t aes_cmac, sm_key_t key, uint8_t * data, int sm_cmac_message_len){
	sm_key_t k0, k1, k2, zero;
	memset(zero, 0, 16);

	aes128_calc_cyphertext(key, zero, k0);
	calc_subkeys(k0, k1, k2);

    int sm_cmac_block_count = (sm_cmac_message_len + 15) / 16;

    // step 3: ..
    if (sm_cmac_block_count==0){
        sm_cmac_block_count = 1;
    }

    // step 4: set m_last
    sm_key_t sm_cmac_m_last;
    int sm_cmac_last_block_complete = sm_cmac_message_len != 0 && (sm_cmac_message_len & 0x0f) == 0;
    int i;
    if (sm_cmac_last_block_complete){
        for (i=0;i<16;i++){
            sm_cmac_m_last[i] = data[sm_cmac_message_len - 16 + i] ^ k1[i];
        }
    } else {
        int valid_octets_in_last_block = sm_cmac_message_len & 0x0f;
        for (i=0;i<16;i++){
            if (i < valid_octets_in_last_block){
                sm_cmac_m_last[i] = data[(sm_cmac_message_len & 0xfff0) + i] ^ k2[i];
                continue;
            }
            if (i == valid_octets_in_last_block){
                sm_cmac_m_last[i] = 0x80 ^ k2[i];
                continue;
            }
            sm_cmac_m_last[i] = k2[i];
        }
    }

    // printf("sm_cmac_start: len %u, block count %u\n", sm_cmac_message_len, sm_cmac_block_count);
    // LOG_KEY(sm_cmac_m_last);

    // Step 5
    sm_key_t sm_cmac_x;
    memset(sm_cmac_x, 0, 16);

    // Step 6
    sm_key_t sm_cmac_y;
    for (int block = 0 ; block < sm_cmac_block_count-1 ; block++){
        for (i=0;i<16;i++){
        	sm_cmac_y[i] = sm_cmac_x[i] ^ data[block * 16 + i];
        }
        aes128_calc_cyphertext(key, sm_cmac_y, sm_cmac_x);
    }
    for (i=0;i<16;i++){
		sm_cmac_y[i] = sm_cmac_x[i] ^ sm_cmac_m_last[i];
    }

	// Step 7
    aes128_calc_cyphertext(key, sm_cmac_y, aes_cmac);
}

static void f4(sm_key_t res, sm_key256_t u, sm_key256_t v, sm_key_t x, uint8_t z){
	uint8_t buffer[65];
	memcpy(buffer, u, 32);
	memcpy(buffer+32, v, 32);
	buffer[64] = z;
	hexdump2(buffer, sizeof(buffer));
	aes_cmac(res, x, buffer, sizeof(buffer));
}

static void validate_message(const char * name, const char * message_string, const char * cmac_string){

	uint8_t m[128];
	int len = parse_hex(m, message_string);

	sm_key_t cmac;
	parse_hex(cmac, cmac_string);

	printf("-- verify message %s, len %u:\nm:    %s\ncmac: %s\n", name, len, message_string, cmac_string);

	sm_key_t key;
	parse_hex(key, key_string);

	sm_key_t cmac_test;
	aes_cmac(cmac_test, key, m, len);

	LOG_KEY(cmac_test);

	if (memcmp(cmac_test, cmac, 16)){
		printf("CMAC incorrect!\n");
	} else {
		printf("CMAC correct!\n");
	}
}

int main(void){
	sm_key_t key, k0, k1, k2, zero;
	memset(zero, 0, 16);
	PARSE_KEY(key);

	// validate subkey k0,k1,k2 generation
	aes128_calc_cyphertext(key, zero, k0);
	VALIDATE_KEY(k0);
	calc_subkeys(k0, k1, k2);
	VALIDATE_KEY(k1);
	VALIDATE_KEY(k2);

	// validate AES_CMAC for some messages
	VALIDATE_MESSAGE(m0);
	VALIDATE_MESSAGE(m16);
	VALIDATE_MESSAGE(m40);
	VALIDATE_MESSAGE(m64);

	// validate f4
	printf("-- verify f4\n");
	sm_key_t f4_x, f4_cmac, f4_cmac_test;
	sm_key256_t f4_u, f4_v;
	uint8_t f4_z;
	parse_hex(f4_cmac, f4_cmac_string);
	parse_hex(f4_u, f4_u_string);
	parse_hex(f4_v, f4_v_string);
	parse_hex(f4_x, f4_x_string);
	parse_hex(&f4_z, f4_z_string);
	f4(f4_cmac_test, f4_u, f4_v, f4_x, f4_z);
	if (memcmp(f4_cmac_test, f4_cmac, 16)){
		printf("CMAC incorrect!\n");
	} else {
		printf("CMAC correct!\n");
	}
}
