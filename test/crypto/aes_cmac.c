#include "aes_cmac.h"
#include "rijndael.h"

#include <string.h>

static void sm_shift_left_by_one_bit_inplace(int len, uint8_t * data){
    int i;
    int carry = 0;
    for (i=len-1; i >= 0 ; i--){
        int new_carry = data[i] >> 7;
        data[i] = data[i] << 1 | carry;
        carry = new_carry;
    }
}

void aes128_calc_cyphertext(const uint8_t key[16], const uint8_t plaintext[16], uint8_t cyphertext[16]){
	uint32_t rk[RKLENGTH(KEYBITS)];
	int nrounds = rijndaelSetupEncrypt(rk, &key[0], KEYBITS);
	rijndaelEncrypt(rk, nrounds, plaintext, cyphertext);
}

void aes_cmac_calc_subkeys(sm_key_t k0, sm_key_t k1, sm_key_t k2){
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

void aes_cmac(sm_key_t aes_cmac, const sm_key_t key, const uint8_t * data, int sm_cmac_message_len){
    unsigned int i;
	sm_key_t k0, k1, k2, zero;
	memset(zero, 0, 16);

	aes128_calc_cyphertext(key, zero, k0);
	aes_cmac_calc_subkeys(k0, k1, k2);

    int sm_cmac_block_count = (sm_cmac_message_len + 15) / 16;

    // step 3: ..
    if (sm_cmac_block_count==0){
        sm_cmac_block_count = 1;
    }

    // printf("sm_cmac_start: len %u, block count %u\n", sm_cmac_message_len, sm_cmac_block_count);
    // LOG_KEY(sm_cmac_m_last);

    // Step 5
    sm_key_t sm_cmac_x;
    memset(sm_cmac_x, 0, 16);

    // Step 6
    sm_key_t sm_cmac_y;
    int block;
    for (block = 0 ; block < sm_cmac_block_count-1 ; block++){
        for (i=0;i<16;i++){
        	sm_cmac_y[i] = sm_cmac_x[i] ^ data[block * 16 + i];
        }
        aes128_calc_cyphertext(key, sm_cmac_y, sm_cmac_x);
    }

    // step 4: set m_last
    sm_key_t sm_cmac_m_last;
    int sm_cmac_last_block_complete = sm_cmac_message_len != 0 && (sm_cmac_message_len & 0x0f) == 0;
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

    for (i=0;i<16;i++){
		sm_cmac_y[i] = sm_cmac_x[i] ^ sm_cmac_m_last[i];
    }

	// Step 7
    aes128_calc_cyphertext(key, sm_cmac_y, aes_cmac);
}