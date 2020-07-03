#include <stdio.h>
#include <stdint.h>
#include "btstack_util.h"
#include "aes_ccm.h"
#include "btstack_crypto.h"

typedef uint8_t key_t[16];

#define LOG_KEY(NAME) { printf("%16s: ", #NAME); printf_hexdump(NAME, 16); }
#define PARSE_KEY(NAME) { parse_hex(NAME, NAME##_string); LOG_KEY(NAME); }
#define DEFINE_KEY(NAME, VALUE) key_t NAME; parse_hex(NAME, VALUE); LOG_KEY(NAME);

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

static void ccm_done(void * arg){

}

static void message_24_upper_transport_encrypt(void){
	printf("[+] Upper transport encrypt\n");
	DEFINE_KEY(label_uuid, "f4a002c7fb1e4ca0a469a021de0db875");
	DEFINE_KEY(app_key,    "63964771734fbd76e3b40519d1d94a48");

	uint8_t app_nonce[13];
	parse_hex(app_nonce, "018007080d1234973612345677");
	printf("%16s: ", "app_nonce"); printf_hexdump(app_nonce, 13);

	uint8_t plaintext[8];
	parse_hex(plaintext, "ea0a00576f726c64");
	printf("%16s: ", "plaintext"); printf_hexdump(plaintext, sizeof(plaintext));

	printf("Reference:\n");
	uint8_t ciphertext[8+8];
	bt_mesh_ccm_encrypt(app_key, app_nonce, plaintext, sizeof(plaintext), label_uuid, sizeof(label_uuid), ciphertext, 8);
	printf("%16s: ", "ciphertext"); printf_hexdump(ciphertext, 8);
	printf("%16s: ", "TransMIC");     printf_hexdump(&ciphertext[8], 8);

	// btstack_crypto 
	printf("btstack_crypto:\n");
	uint8_t trans_mic[8];
	btstack_crypto_init();
	btstack_crypto_ccm_t btstack_crypto_ccm;
	btstack_crypto_ccm_init(&btstack_crypto_ccm, app_key, app_nonce, sizeof(plaintext), sizeof(label_uuid), sizeof(trans_mic));
	btstack_crypto_ccm_digest(&btstack_crypto_ccm, label_uuid, 16,  &ccm_done, NULL);
	btstack_crypto_ccm_encrypt_block(&btstack_crypto_ccm, sizeof(plaintext), plaintext, ciphertext, &ccm_done, NULL);
	btstack_crypto_ccm_get_authentication_value(&btstack_crypto_ccm, trans_mic);
	printf("%16s: ", "ciphertext"); printf_hexdump(ciphertext, 8);
	printf("%16s: ", "TransMIC");     printf_hexdump(trans_mic, 8);
}

static void message_24_upper_transport_decrypt(void){
	printf("[+] Upper transport decrypt\n");
	DEFINE_KEY(label_uuid, "f4a002c7fb1e4ca0a469a021de0db875");
	DEFINE_KEY(app_key,    "63964771734fbd76e3b40519d1d94a48");

	uint8_t app_nonce[13];
	parse_hex(app_nonce, "018007080d1234973612345677");
	printf("%16s: ", "app_nonce"); printf_hexdump(app_nonce, 13);


	uint8_t ciphertext[8];
	parse_hex(ciphertext, "c3c51d8e476b28e3");
	printf("%16s: ", "ciphertext"); printf_hexdump(ciphertext, sizeof(ciphertext));

	printf("Reference:\n");
	uint8_t plaintext[8+8];
	bt_mesh_ccm_decrypt(app_key, app_nonce, ciphertext, sizeof(ciphertext), label_uuid, sizeof(label_uuid), plaintext, 8);
	printf("%16s: ", "plaintext"); printf_hexdump(plaintext, 8);
	printf("%16s: ", "TransMIC");    printf_hexdump(&plaintext[8], 8);

	// btstack_crypto 
	printf("btstack_crypto:\n");
	uint8_t trans_mic[8];
	btstack_crypto_init();
	btstack_crypto_ccm_t btstack_crypto_ccm;
	btstack_crypto_ccm_init(&btstack_crypto_ccm, app_key, app_nonce, sizeof(ciphertext), sizeof(label_uuid), sizeof(trans_mic));
	btstack_crypto_ccm_digest(&btstack_crypto_ccm, label_uuid, 16,  &ccm_done, NULL);
	btstack_crypto_ccm_decrypt_block(&btstack_crypto_ccm, sizeof(ciphertext), ciphertext, plaintext, &ccm_done, NULL);
	btstack_crypto_ccm_get_authentication_value(&btstack_crypto_ccm, trans_mic);
	printf("%16s: ", "plaintext"); printf_hexdump(plaintext, 8);
	printf("%16s: ", "TransMIC");     printf_hexdump(trans_mic, 8);
}

static void message_24_lower_transport_segment_0(void){
	printf("[+] Lower Transport Segment 0\n");
	DEFINE_KEY(encryption_key, "0953fa93e7caac9638f58820220a398e");

	uint8_t network_nonce[13];
	parse_hex(network_nonce, "000307080d1234000012345677");
	printf("%16s: ", "network_nonce"); printf_hexdump(network_nonce, 13);

	uint8_t plaintext[18];
	parse_hex(plaintext, "9736e6a03401de1547118463123e5f6a17b9");
	printf("%16s: ", "plaintext"); printf_hexdump(plaintext, sizeof(plaintext));

	printf("Reference:\n");
	uint8_t ciphertext[18+4];
	bt_mesh_ccm_encrypt(encryption_key, network_nonce, plaintext, sizeof(plaintext), NULL, 0, ciphertext, 4);
	printf("%16s: ", "ciphertext"); printf_hexdump(ciphertext, 18);
	printf("%16s: ", "NetMIC");     printf_hexdump(&ciphertext[18], 4);

	// btstack_crypto 
	printf("btstack_crypto:\n");
	uint8_t net_mic[4];
	btstack_crypto_init();
	btstack_crypto_ccm_t btstack_crypto_ccm;
	btstack_crypto_ccm_init(&btstack_crypto_ccm, encryption_key, network_nonce, sizeof(plaintext), 0, 4);
	btstack_crypto_ccm_encrypt_block(&btstack_crypto_ccm, sizeof(plaintext), plaintext, ciphertext, &ccm_done, NULL);
	btstack_crypto_ccm_get_authentication_value(&btstack_crypto_ccm, net_mic);
	printf("%16s: ", "ciphertext"); printf_hexdump(ciphertext, 18);
	printf("%16s: ", "NetMIC");     printf_hexdump(net_mic, 4);
}

int main(void){
	message_24_upper_transport_encrypt();
	message_24_upper_transport_decrypt();
	message_24_lower_transport_segment_0();
	return 0;
}
