
#include "rijndael.h"
#include <stdio.h>
#include <strings.h>

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
	bzero(key, 16);
	bzero(plaintext, 16);
	uint8_t cyphertext[16];
	aes128_calc_cyphertext(key, plaintext, cyphertext);
	hexdump2(cyphertext, 16);
}
