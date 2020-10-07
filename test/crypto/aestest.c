
#include "rijndael.h"
#include "btstack_debug.h"
#include <stdio.h>
#include <string.h>

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

int main(void){
	uint8_t key[16];
	uint8_t plaintext[16];
	memset(key, 0, 16);
	memset(plaintext, 0, 16);
	uint8_t cyphertext[16];
	aes128_calc_cyphertext(key, plaintext, cyphertext);
	hexdump2(cyphertext, 16);

	// test invalid key len
	uint32_t rk[RKLENGTH(KEYBITS)];
	btstack_assert(rijndaelSetupEncrypt(rk, &key[0], 0) == 0);
}
