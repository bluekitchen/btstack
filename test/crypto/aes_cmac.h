#ifndef AES_CMAC_H
#define AES_CMAC_H

#include <stdint.h>
typedef uint8_t sm_key_t[16];
void aes128_calc_cyphertext(const uint8_t key[16], const uint8_t plaintext[16], uint8_t cyphertext[16]);
void aes_cmac_calc_subkeys(sm_key_t k0, sm_key_t k1, sm_key_t k2);
void aes_cmac(sm_key_t aes_cmac, const sm_key_t key, const uint8_t * data, int sm_cmac_message_len);

#endif
