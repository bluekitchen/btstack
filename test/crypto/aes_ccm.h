#ifndef AES_CCM_H
#define AES_CCM_H

#include <stdint.h>
#include <stdio.h>

// CCM Encrypt & Decrypt from Zephyr Project
int bt_mesh_ccm_decrypt(const uint8_t key[16], uint8_t nonce[13], const uint8_t *enc_msg, size_t msg_len,
			       const uint8_t *aad, size_t aad_len, uint8_t *out_msg, size_t mic_size);

int bt_mesh_ccm_encrypt(const uint8_t key[16], uint8_t nonce[13], const uint8_t *msg, size_t msg_len,
			       const uint8_t *aad, size_t aad_len, uint8_t *out_msg, size_t mic_size);

#endif
