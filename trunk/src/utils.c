/*
 * Copyright (C) 2014 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

/*
 *  utils.c
 *
 *  General utility functions
 *
 *  Created by Matthias Ringwald on 7/23/09.
 */

#include "btstack-config.h"
#include <btstack/utils.h>
#include <stdio.h>
#include <string.h>
#include "debug.h"

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

void net_store_16(uint8_t *buffer, uint16_t pos, uint16_t value){
    buffer[pos++] = value >> 8;
    buffer[pos++] = value;
}

void net_store_32(uint8_t *buffer, uint16_t pos, uint32_t value){
    buffer[pos++] = value >> 24;
    buffer[pos++] = value >> 16;
    buffer[pos++] = value >> 8;
    buffer[pos++] = value;
}

void bt_flip_addr(bd_addr_t dest, bd_addr_t src){
    dest[0] = src[5];
    dest[1] = src[4];
    dest[2] = src[3];
    dest[3] = src[2];
    dest[4] = src[1];
    dest[5] = src[0];
}

// general swap/endianess utils
void swapX(const uint8_t *src, uint8_t *dst, int len){
    int i;
    for (i = 0; i < len; i++)
        dst[len - 1 - i] = src[i];
}
void swap24(const uint8_t src[3], uint8_t dst[3]){
    swapX(src, dst, 3);
}
void swap56(const uint8_t src[7], uint8_t dst[7]){
    swapX(src, dst, 7);
}
void swap64(const uint8_t src[8], uint8_t dst[8]){
    swapX(src, dst, 8);
}
void swap128(const uint8_t src[16], uint8_t dst[16]){
    swapX(src, dst, 16);
}

char char_for_nibble(int nibble){
    if (nibble < 10) return '0' + nibble;
    nibble -= 10;
    if (nibble < 6) return 'A' + nibble;
    return '?';
}

void printf_hexdump(const void *data, int size){
    if (size <= 0) return;
    int i;
    for (i=0; i<size;i++){
        printf("%02X ", ((uint8_t *)data)[i]);
    }
    printf("\n");
}

void hexdump(const void *data, int size){
    char buffer[6*16+1];
    int i, j;

    uint8_t low = 0x0F;
    uint8_t high = 0xF0;
    j = 0;
    for (i=0; i<size;i++){
        uint8_t byte = ((uint8_t *)data)[i];
        buffer[j++] = '0';
        buffer[j++] = 'x';
        buffer[j++] = char_for_nibble((byte & high) >> 4);
        buffer[j++] = char_for_nibble(byte & low);
        buffer[j++] = ',';
        buffer[j++] = ' ';     
        if (j >= 6*16 ){
            buffer[j] = 0;
            log_info("%s", buffer);
            j = 0;
        }
    }
    if (j != 0){
        buffer[j] = 0;
        log_info("%s", buffer);
    }
}

void hexdumpf(const void *data, int size){
    char buffer[6*16+1];
    int i, j;

    uint8_t low = 0x0F;
    uint8_t high = 0xF0;
    j = 0;
    for (i=0; i<size;i++){
        uint8_t byte = ((uint8_t *)data)[i];
        buffer[j++] = '0';
        buffer[j++] = 'x';
        buffer[j++] = char_for_nibble((byte & high) >> 4);
        buffer[j++] = char_for_nibble(byte & low);
        buffer[j++] = ',';
        buffer[j++] = ' ';     
        if (j >= 6*16 ){
            buffer[j] = 0;
            printf("%s\n", buffer);
            j = 0;
        }
    }
    if (j != 0){
        buffer[j] = 0;
        printf("%s\n", buffer);
    }
}

void log_key(const char * name, sm_key_t key){
    log_info("%-6s ", name);
    hexdump(key, 16);
}

void printUUID128(uint8_t *uuid) {
    printf("%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
           uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
           uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
}

static char bd_addr_to_str_buffer[6*3];  // 12:45:78:01:34:67\0
char * bd_addr_to_str(bd_addr_t addr){
    // orig code
    // sprintf(bd_addr_to_str_buffer, "%02x:%02x:%02x:%02x:%02x:%02x", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    // sprintf-free code
    char * p = bd_addr_to_str_buffer;
    int i;
    for (i = 0; i < 6 ; i++) {
        *p++ = char_for_nibble((addr[i] >> 4) & 0x0F);
        *p++ = char_for_nibble((addr[i] >> 0) & 0x0F);
        *p++ = ':';
    }
    *--p = 0;
    return (char *) bd_addr_to_str_buffer;
}

static char link_key_to_str_buffer[LINK_KEY_STR_LEN+1];  // 11223344556677889900112233445566\0
char *link_key_to_str(link_key_t link_key){
    char * p = link_key_to_str_buffer;
    int i;
    for (i = 0; i < LINK_KEY_LEN ; i++) {
        *p++ = char_for_nibble((link_key[i] >> 4) & 0x0F);
        *p++ = char_for_nibble((link_key[i] >> 0) & 0x0F);
    }
    *p = 0;
    return (char *) link_key_to_str_buffer;
}

static char link_key_type_to_str_buffer[2];
char *link_key_type_to_str(link_key_type_t link_key){
    snprintf(link_key_type_to_str_buffer, sizeof(link_key_type_to_str_buffer), "%d", link_key);
    return (char *) link_key_type_to_str_buffer;
}

void print_bd_addr( bd_addr_t addr){
    log_info("%s", bd_addr_to_str(addr));
}

#ifndef EMBEDDED
int sscan_bd_addr(uint8_t * addr_string, bd_addr_t addr){
	unsigned int bd_addr_buffer[BD_ADDR_LEN];  //for sscanf, integer needed
	// reset result buffer
    memset(bd_addr_buffer, 0, sizeof(bd_addr_buffer));
    
	// parse
    int result = sscanf( (char *) addr_string, "%2x:%2x:%2x:%2x:%2x:%2x", &bd_addr_buffer[0], &bd_addr_buffer[1], &bd_addr_buffer[2],
						&bd_addr_buffer[3], &bd_addr_buffer[4], &bd_addr_buffer[5]);

    if (result != BD_ADDR_LEN) return 0;

	// store
    int i;
	for (i = 0; i < BD_ADDR_LEN; i++) {
		addr[i] = (uint8_t) bd_addr_buffer[i];
	}
	return 1;
}

int sscan_link_key(char * addr_string, link_key_t link_key){
    unsigned int buffer[LINK_KEY_LEN];

    // reset result buffer
    memset(&buffer, 0, sizeof(buffer));

    // parse
    int result = sscanf( (char *) addr_string, "%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x",
                                    &buffer[0], &buffer[1], &buffer[2], &buffer[3],
                                    &buffer[4], &buffer[5], &buffer[6], &buffer[7],
                                    &buffer[8], &buffer[9], &buffer[10], &buffer[11],
                                    &buffer[12], &buffer[13], &buffer[14], &buffer[15] );

    if (result != LINK_KEY_LEN) return 0;

    // store
    int i;
    uint8_t *p = (uint8_t *) link_key;
    for (i=0; i<LINK_KEY_LEN; i++ ) {
        *p++ = (uint8_t) buffer[i];
    }
    return 1;
}

#endif


// treat standard pairing as Authenticated as it uses a PIN
int is_authenticated_link_key(link_key_type_t link_key_type){
    switch (link_key_type){
        case COMBINATION_KEY:
        case AUTHENTICATED_COMBINATION_KEY_GENERATED_FROM_P192:
        case AUTHENTICATED_COMBINATION_KEY_GENERATED_FROM_P256:
            return 1;
        default:
            return 0;
    }
}

/*  
 * CRC (reversed crc) lookup table as calculated by the table generator in ETSI TS 101 369 V6.3.0.
 */
static const uint8_t crc8table[256] = {    /* reversed, 8-bit, poly=0x07 */
    0x00, 0x91, 0xE3, 0x72, 0x07, 0x96, 0xE4, 0x75, 0x0E, 0x9F, 0xED, 0x7C, 0x09, 0x98, 0xEA, 0x7B,
    0x1C, 0x8D, 0xFF, 0x6E, 0x1B, 0x8A, 0xF8, 0x69, 0x12, 0x83, 0xF1, 0x60, 0x15, 0x84, 0xF6, 0x67,
    0x38, 0xA9, 0xDB, 0x4A, 0x3F, 0xAE, 0xDC, 0x4D, 0x36, 0xA7, 0xD5, 0x44, 0x31, 0xA0, 0xD2, 0x43,
    0x24, 0xB5, 0xC7, 0x56, 0x23, 0xB2, 0xC0, 0x51, 0x2A, 0xBB, 0xC9, 0x58, 0x2D, 0xBC, 0xCE, 0x5F,
    0x70, 0xE1, 0x93, 0x02, 0x77, 0xE6, 0x94, 0x05, 0x7E, 0xEF, 0x9D, 0x0C, 0x79, 0xE8, 0x9A, 0x0B,
    0x6C, 0xFD, 0x8F, 0x1E, 0x6B, 0xFA, 0x88, 0x19, 0x62, 0xF3, 0x81, 0x10, 0x65, 0xF4, 0x86, 0x17,
    0x48, 0xD9, 0xAB, 0x3A, 0x4F, 0xDE, 0xAC, 0x3D, 0x46, 0xD7, 0xA5, 0x34, 0x41, 0xD0, 0xA2, 0x33,
    0x54, 0xC5, 0xB7, 0x26, 0x53, 0xC2, 0xB0, 0x21, 0x5A, 0xCB, 0xB9, 0x28, 0x5D, 0xCC, 0xBE, 0x2F,
    0xE0, 0x71, 0x03, 0x92, 0xE7, 0x76, 0x04, 0x95, 0xEE, 0x7F, 0x0D, 0x9C, 0xE9, 0x78, 0x0A, 0x9B,
    0xFC, 0x6D, 0x1F, 0x8E, 0xFB, 0x6A, 0x18, 0x89, 0xF2, 0x63, 0x11, 0x80, 0xF5, 0x64, 0x16, 0x87,
    0xD8, 0x49, 0x3B, 0xAA, 0xDF, 0x4E, 0x3C, 0xAD, 0xD6, 0x47, 0x35, 0xA4, 0xD1, 0x40, 0x32, 0xA3,
    0xC4, 0x55, 0x27, 0xB6, 0xC3, 0x52, 0x20, 0xB1, 0xCA, 0x5B, 0x29, 0xB8, 0xCD, 0x5C, 0x2E, 0xBF,
    0x90, 0x01, 0x73, 0xE2, 0x97, 0x06, 0x74, 0xE5, 0x9E, 0x0F, 0x7D, 0xEC, 0x99, 0x08, 0x7A, 0xEB,
    0x8C, 0x1D, 0x6F, 0xFE, 0x8B, 0x1A, 0x68, 0xF9, 0x82, 0x13, 0x61, 0xF0, 0x85, 0x14, 0x66, 0xF7,
    0xA8, 0x39, 0x4B, 0xDA, 0xAF, 0x3E, 0x4C, 0xDD, 0xA6, 0x37, 0x45, 0xD4, 0xA1, 0x30, 0x42, 0xD3,
    0xB4, 0x25, 0x57, 0xC6, 0xB3, 0x22, 0x50, 0xC1, 0xBA, 0x2B, 0x59, 0xC8, 0xBD, 0x2C, 0x5E, 0xCF
};

#define CRC8_INIT  0xFF          // Initial FCS value 
#define CRC8_OK    0xCF          // Good final FCS value 
/*-----------------------------------------------------------------------------------*/
uint8_t crc8(uint8_t *data, uint16_t len)
{
    uint16_t count;
    uint8_t crc = CRC8_INIT;
    for (count = 0; count < len; count++)
        crc = crc8table[crc ^ data[count]];
    return crc;
}

/*-----------------------------------------------------------------------------------*/
uint8_t crc8_check(uint8_t *data, uint16_t len, uint8_t check_sum)
{
    uint8_t crc;
    
    crc = crc8(data, len);
    
    crc = crc8table[crc ^ check_sum];
    if (crc == CRC8_OK) 
        return 0;               /* Valid */
    else 
        return 1;               /* Failed */
    
}

/*-----------------------------------------------------------------------------------*/
uint8_t crc8_calc(uint8_t *data, uint16_t len)
{
    /* Ones complement */
    return 0xFF - crc8(data, len);
}

