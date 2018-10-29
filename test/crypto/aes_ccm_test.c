#include <stdio.h>
#include <stdint.h>
#include "btstack_util.h"
#include "aes_cmac.h"
#include <errno.h>
#include "aes_ccm.h"

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

static void message_24(void){
	DEFINE_KEY(encryption_key, "0953fa93e7caac9638f58820220a398e");

	uint8_t network_nonce[13];
	parse_hex(network_nonce, "000307080d1234000012345677");
	printf("%16s: ", "network_nonce"); printf_hexdump(network_nonce, 13);

	uint8_t plaintext[18];
	parse_hex(plaintext, "9736e6a03401de1547118463123e5f6a17b9");
	printf("%16s: ", "plaintext"); printf_hexdump(plaintext, sizeof(plaintext));

	uint8_t ciphertext[18+4];
	bt_mesh_ccm_encrypt(encryption_key, network_nonce, plaintext, sizeof(plaintext), NULL, 0, ciphertext, 4);
	printf("%16s: ", "ciphertext"); printf_hexdump(ciphertext, 18);
	printf("%16s: ", "NetMIC");     printf_hexdump(&ciphertext[18], 4);
}

int main(void){
	message_24();
	return 0;
}
