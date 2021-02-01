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

#define BTSTACK_FILE__ "btstack_util.c"

/*
 *  btstack_util.c
 *
 *  General utility functions
 *
 *  Created by Matthias Ringwald on 7/23/09.
 */

#include "btstack_config.h"
#include "btstack_debug.h"
#include "btstack_util.h"

#ifdef ENABLE_PRINTF_HEXDUMP
#include <stdio.h>
#endif

#include <string.h>

/**
 * @brief Compare two Bluetooth addresses
 * @param a
 * @param b
 * @return 0 if equal
 */
int bd_addr_cmp(const bd_addr_t a, const bd_addr_t b){
    return memcmp(a,b, BD_ADDR_LEN);
}

/**
 * @brief Copy Bluetooth address
 * @param dest
 * @param src
 */
void bd_addr_copy(bd_addr_t dest, const bd_addr_t src){
    (void)memcpy(dest, src, BD_ADDR_LEN);
}

uint16_t little_endian_read_16(const uint8_t * buffer, int pos){
    return (uint16_t)(((uint16_t) buffer[pos]) | (((uint16_t)buffer[(pos)+1]) << 8));
}
uint32_t little_endian_read_24(const uint8_t * buffer, int pos){
    return ((uint32_t) buffer[pos]) | (((uint32_t)buffer[(pos)+1]) << 8) | (((uint32_t)buffer[(pos)+2]) << 16);
}
uint32_t little_endian_read_32(const uint8_t * buffer, int pos){
    return ((uint32_t) buffer[pos]) | (((uint32_t)buffer[(pos)+1]) << 8) | (((uint32_t)buffer[(pos)+2]) << 16) | (((uint32_t) buffer[(pos)+3]) << 24);
}

void little_endian_store_16(uint8_t *buffer, uint16_t pos, uint16_t value){
    buffer[pos++] = (uint8_t)value;
    buffer[pos++] = (uint8_t)(value >> 8);
}

void little_endian_store_24(uint8_t *buffer, uint16_t pos, uint32_t value){
    buffer[pos++] = (uint8_t)(value);
    buffer[pos++] = (uint8_t)(value >> 8);
    buffer[pos++] = (uint8_t)(value >> 16);
}

void little_endian_store_32(uint8_t *buffer, uint16_t pos, uint32_t value){
    buffer[pos++] = (uint8_t)(value);
    buffer[pos++] = (uint8_t)(value >> 8);
    buffer[pos++] = (uint8_t)(value >> 16);
    buffer[pos++] = (uint8_t)(value >> 24);
}

uint32_t big_endian_read_16( const uint8_t * buffer, int pos) {
    return (uint16_t)(((uint16_t) buffer[(pos)+1]) | (((uint16_t)buffer[ pos   ]) << 8));
}

uint32_t big_endian_read_24( const uint8_t * buffer, int pos) {
    return ( ((uint32_t)buffer[(pos)+2]) | (((uint32_t)buffer[(pos)+1]) << 8) | (((uint32_t) buffer[pos]) << 16));
}

uint32_t big_endian_read_32( const uint8_t * buffer, int pos) {
    return ((uint32_t) buffer[(pos)+3]) | (((uint32_t)buffer[(pos)+2]) << 8) | (((uint32_t)buffer[(pos)+1]) << 16) | (((uint32_t) buffer[pos]) << 24);
}

void big_endian_store_16(uint8_t *buffer, uint16_t pos, uint16_t value){
    buffer[pos++] = (uint8_t)(value >> 8);
    buffer[pos++] = (uint8_t)(value);
}

void big_endian_store_24(uint8_t *buffer, uint16_t pos, uint32_t value){
    buffer[pos++] = (uint8_t)(value >> 16);
    buffer[pos++] = (uint8_t)(value >> 8);
    buffer[pos++] = (uint8_t)(value);
}

void big_endian_store_32(uint8_t *buffer, uint16_t pos, uint32_t value){
    buffer[pos++] = (uint8_t)(value >> 24);
    buffer[pos++] = (uint8_t)(value >> 16);
    buffer[pos++] = (uint8_t)(value >> 8);
    buffer[pos++] = (uint8_t)(value);
}

// general swap/endianess utils
void reverse_bytes(const uint8_t *src, uint8_t *dst, int len){
    int i;
    for (i = 0; i < len; i++)
        dst[len - 1 - i] = src[i];
}
void reverse_24(const uint8_t * src, uint8_t * dst){
    reverse_bytes(src, dst, 3);
}
void reverse_48(const uint8_t * src, uint8_t * dst){
    reverse_bytes(src, dst, 6);
}
void reverse_56(const uint8_t * src, uint8_t * dst){
    reverse_bytes(src, dst, 7);
}
void reverse_64(const uint8_t * src, uint8_t * dst){
    reverse_bytes(src, dst, 8);
}
void reverse_128(const uint8_t * src, uint8_t * dst){
    reverse_bytes(src, dst, 16);
}
void reverse_256(const uint8_t * src, uint8_t * dst){
    reverse_bytes(src, dst, 32);
}

void reverse_bd_addr(const bd_addr_t src, bd_addr_t dest){
    reverse_bytes(src, dest, 6);
}

uint32_t btstack_min(uint32_t a, uint32_t b){
    return (a < b) ? a : b;
}

uint32_t btstack_max(uint32_t a, uint32_t b){
    return (a > b) ? a : b;
}

/**
 * @brief Calculate delta between two points in time
 * @returns time_a - time_b - result > 0 if time_a is newer than time_b
 */
int32_t btstack_time_delta(uint32_t time_a, uint32_t time_b){
    return (int32_t)(time_a - time_b);
}


char char_for_nibble(int nibble){

    static const char * char_to_nibble = "0123456789ABCDEF";

    if (nibble < 16){
        return char_to_nibble[nibble];
    } else {
        return '?';
    }
}

static inline char char_for_high_nibble(int value){
    return char_for_nibble((value >> 4) & 0x0f);
}

static inline char char_for_low_nibble(int value){
    return char_for_nibble(value & 0x0f);
}


int nibble_for_char(char c){
    if ((c >= '0') && (c <= '9')) return c - '0';
    if ((c >= 'a') && (c <= 'f')) return c - 'a' + 10;
    if ((c >= 'A') && (c <= 'F')) return c - 'A' + 10;
    return -1;
}

#ifdef ENABLE_PRINTF_HEXDUMP
void printf_hexdump(const void *data, int size){
    char buffer[4];
    buffer[2] = ' ';
    buffer[3] =  0;
    const uint8_t * ptr = (const uint8_t *) data;
    while (size > 0){
        uint8_t byte = *ptr++;
        buffer[0] = char_for_high_nibble(byte);
        buffer[1] = char_for_low_nibble(byte);
        printf("%s", buffer);
        size--;
    }
    printf("\n");
}
#endif

#if defined(ENABLE_LOG_INFO) || defined(ENABLE_LOG_DEBUG)
static void log_hexdump(int level, const void * data, int size){
#define ITEMS_PER_LINE 16
// template '0x12, '
#define BYTES_PER_BYTE  6
    char buffer[BYTES_PER_BYTE*ITEMS_PER_LINE+1];
    int i, j;
    j = 0;
    for (i=0; i<size;i++){

        // help static analyzer proof that j stays within bounds
        if (j > (BYTES_PER_BYTE * (ITEMS_PER_LINE-1))){
            j = 0;
        }

        uint8_t byte = ((uint8_t *)data)[i];
        buffer[j++] = '0';
        buffer[j++] = 'x';
        buffer[j++] = char_for_high_nibble(byte);
        buffer[j++] = char_for_low_nibble(byte);
        buffer[j++] = ',';
        buffer[j++] = ' ';     

        if (j >= (BYTES_PER_BYTE * ITEMS_PER_LINE) ){
            buffer[j] = 0;
            HCI_DUMP_LOG(level, "%s", buffer);
            j = 0;
        }
    }
    if (j != 0){
        buffer[j] = 0;
        HCI_DUMP_LOG(level, "%s", buffer);
    }
}
#endif

void log_debug_hexdump(const void *data, int size){
#ifdef ENABLE_LOG_DEBUG
    log_hexdump(HCI_DUMP_LOG_LEVEL_DEBUG, data, size);
#else
    UNUSED(data);   // ok: no code
    UNUSED(size);   // ok: no code
#endif
}

void log_info_hexdump(const void *data, int size){
#ifdef ENABLE_LOG_INFO
    log_hexdump(HCI_DUMP_LOG_LEVEL_INFO, data, size);
#else
    UNUSED(data);   // ok: no code
    UNUSED(size);   // ok: no code
#endif
}

void log_info_key(const char * name, sm_key_t key){
#ifdef ENABLE_LOG_INFO
    char buffer[16*2+1];
    int i;
    int j = 0;
    for (i=0; i<16;i++){
        uint8_t byte = key[i];
        buffer[j++] = char_for_high_nibble(byte);
        buffer[j++] = char_for_low_nibble(byte);
    }
    buffer[j] = 0;
    log_info("%-6s %s", name, buffer);
#else
    UNUSED(name);
    (void)key;
#endif
}

// UUIDs are stored in big endian, similar to bd_addr_t

// Bluetooth Base UUID: 00000000-0000-1000-8000- 00805F9B34FB
const uint8_t bluetooth_base_uuid[] = { 0x00, 0x00, 0x00, 0x00, /* - */ 0x00, 0x00, /* - */ 0x10, 0x00, /* - */
    0x80, 0x00, /* - */ 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB };

void uuid_add_bluetooth_prefix(uint8_t *uuid, uint32_t shortUUID){
    (void)memcpy(uuid, bluetooth_base_uuid, 16);
    big_endian_store_32(uuid, 0, shortUUID);
}

int uuid_has_bluetooth_prefix(const uint8_t * uuid128){
    return memcmp(&uuid128[4], &bluetooth_base_uuid[4], 12) == 0;
}

static char uuid128_to_str_buffer[32+4+1];
char * uuid128_to_str(const uint8_t * uuid){
    int i;
    int j = 0;
    // after 4, 6, 8, and 10 bytes = XYXYXYXY-XYXY-XYXY-XYXY-XYXYXYXYXYXY, there's a dash
    const int dash_locations = (1<<3) | (1<<5) | (1<<7) | (1<<9);
    for (i=0;i<16;i++){
        uint8_t byte = uuid[i];
        uuid128_to_str_buffer[j++] = char_for_high_nibble(byte);
        uuid128_to_str_buffer[j++] = char_for_low_nibble(byte);
        if (dash_locations & (1<<i)){
            uuid128_to_str_buffer[j++] = '-';
        }
    }
    return uuid128_to_str_buffer;
}

static char bd_addr_to_str_buffer[6*3];  // 12:45:78:01:34:67\0
char * bd_addr_to_str(const bd_addr_t addr){
    char * p = bd_addr_to_str_buffer;
    int i;
    for (i = 0; i < 6 ; i++) {
        uint8_t byte = addr[i];
        *p++ = char_for_high_nibble(byte);
        *p++ = char_for_low_nibble(byte);
        *p++ = ':';
    }
    *--p = 0;
    return (char *) bd_addr_to_str_buffer;
}

void btstack_replace_bd_addr_placeholder(uint8_t * buffer, uint16_t size, const bd_addr_t address){
    const int bd_addr_string_len = 17;
    uint16_t i = 0;
    while ((i + bd_addr_string_len) <= size){
        if (memcmp(&buffer[i], "00:00:00:00:00:00", bd_addr_string_len)) {
            i++;
            continue;
        }
        // set address
        (void)memcpy(&buffer[i], bd_addr_to_str(address), bd_addr_string_len);
        i += bd_addr_string_len;
    }
}

static int scan_hex_byte(const char * byte_string){
    int upper_nibble = nibble_for_char(*byte_string++);
    if (upper_nibble < 0) return -1;
    int lower_nibble = nibble_for_char(*byte_string);
    if (lower_nibble < 0) return -1;
    return (upper_nibble << 4) | lower_nibble;
}

int sscanf_bd_addr(const char * addr_string, bd_addr_t addr){
    uint8_t buffer[BD_ADDR_LEN];
    int result = 0;
    int i;
    for (i = 0; i < BD_ADDR_LEN; i++) {
        int single_byte = scan_hex_byte(addr_string);
        if (single_byte < 0) break;
        addr_string += 2;
        buffer[i] = (uint8_t)single_byte;
        // don't check seperator after last byte
        if (i == (BD_ADDR_LEN - 1)) {
            result = 1;
            break;
        }
        // skip supported separators
        char next_char = *addr_string;
        if ((next_char == ':') || (next_char == '-') || (next_char == ' ')) {
            addr_string++;
        }
    }

    if (result != 0){
        bd_addr_copy(addr, buffer);
    }
	return result;
}

uint32_t btstack_atoi(const char *str){
    uint32_t val = 0;
    while (true){
        char chr = *str;
        if (!chr || (chr < '0') || (chr > '9'))
            return val;
        val = (val * 10u) + (uint8_t)(chr - '0');
        str++;
    }
}

int string_len_for_uint32(uint32_t i){
    if (i <         10) return 1;
    if (i <        100) return 2;
    if (i <       1000) return 3;
    if (i <      10000) return 4;
    if (i <     100000) return 5;
    if (i <    1000000) return 6;      
    if (i <   10000000) return 7;
    if (i <  100000000) return 8;
    if (i < 1000000000) return 9;
    return 10;
}

int count_set_bits_uint32(uint32_t x){
    x = (x & 0x55555555) + ((x >> 1) & 0x55555555);
    x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
    x = (x & 0x0F0F0F0F) + ((x >> 4) & 0x0F0F0F0F);
    x = (x & 0x00FF00FF) + ((x >> 8) & 0x00FF00FF);
    x = (x & 0x0000FFFF) + ((x >> 16) & 0x0000FFFF);
    return x;
}

/*  
 * CRC (reversed crc) lookup table as calculated by the table generator in ETSI TS 101 369 V6.3.0.
 */

#define CRC8_INIT  0xFF          // Initial FCS value 
#define CRC8_OK    0xCF          // Good final FCS value 

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

/*-----------------------------------------------------------------------------------*/
static uint8_t crc8(uint8_t *data, uint16_t len){
    uint16_t count;
    uint8_t crc = CRC8_INIT;
    for (count = 0; count < len; count++){
        crc = crc8table[crc ^ data[count]];
    }
    return crc;
}

/*-----------------------------------------------------------------------------------*/
uint8_t btstack_crc8_check(uint8_t *data, uint16_t len, uint8_t check_sum){
    uint8_t crc;
    crc = crc8(data, len);
    crc = crc8table[crc ^ check_sum];
    if (crc == CRC8_OK){
        return 0;               /* Valid */
    } else {
        return 1;               /* Failed */
    } 
}

/*-----------------------------------------------------------------------------------*/
uint8_t btstack_crc8_calc(uint8_t *data, uint16_t len){
    /* Ones complement */
    return 0xFFu - crc8(data, len);
}
