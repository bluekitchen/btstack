/*
 *  utils.c
 *
 *  General utility functions
 *
 *  Created by Matthias Ringwald on 7/23/09.
 */

#include "utils.h"
#include <stdio.h>

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
