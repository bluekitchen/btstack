/*
 *  utils.c
 *
 *  General utility functions
 *
 *  Created by Matthias Ringwald on 7/23/09.
 */

#include "utils.h"
#include <stdio.h>
#include <strings.h>

void bt_store_16(uint8_t *buffer, uint16_t pos, uint16_t value){
    buffer[pos++] = value;
    buffer[pos++] = value >> 8;
}

void bt_store_32(uint8_t *buffer, uint16_t pos, uint32_t value){
    buffer[pos++] = value;
    buffer[pos++] = value >> 8;
    buffer[pos++] = value >> 16;
    buffer[pos++] = value >> 24;
}

void bt_flip_addr(bd_addr_t dest, bd_addr_t src){
    dest[0] = src[5];
    dest[1] = src[4];
    dest[2] = src[3];
    dest[3] = src[2];
    dest[4] = src[1];
    dest[5] = src[0];
}

void hexdump(void *data, int size){
    int i;
    for (i=0; i<size;i++){
        printf("%02X ", ((uint8_t *)data)[i]);
    }
    printf("\n");
}

void print_bd_addr( bd_addr_t addr){
    int i;
    for (i=0; i<BD_ADDR_LEN-1;i++){
        printf("%02X-", ((uint8_t *)addr)[i]);
    }
    printf("%02X", ((uint8_t *)addr)[i]);
}

int sscan_bd_addr(uint8_t * addr_string, bd_addr_t addr){
	int bd_addr_buffer[BD_ADDR_LEN];  //for sscanf, integer needed
	// reset result buffer
	int i;
    bzero(bd_addr_buffer, sizeof(bd_addr_buffer));
	// parse
    int result = sscanf( (char *) addr_string, "%2x:%2x:%2x:%2x:%2x:%2x", &bd_addr_buffer[0], &bd_addr_buffer[1], &bd_addr_buffer[2],
						&bd_addr_buffer[3], &bd_addr_buffer[4], &bd_addr_buffer[5]);
	// store
	if (result == 6){
		for (i = 0; i < BD_ADDR_LEN; i++) {
			addr[i] = (uint8_t) bd_addr_buffer[i];
		}
	}
	return (result == 6);
}
