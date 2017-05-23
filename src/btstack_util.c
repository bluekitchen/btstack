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

#define __BTSTACK_FILE__ "btstack_util.c"

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

#include <stdio.h>
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
    memcpy(dest,src,BD_ADDR_LEN);
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
    return a < b ? a : b;
}

uint32_t btstack_max(uint32_t a, uint32_t b){
    return a > b ? a : b;
}

char char_for_nibble(int nibble){
    if (nibble < 10) return (char)('0' + nibble);
    nibble -= 10;
    if (nibble < 6) return (char)('A' + nibble);
    return '?';
}

static inline char char_for_high_nibble(int value){
    return char_for_nibble((value >> 4) & 0x0f);
}

static inline char char_for_low_nibble(int value){
    return char_for_nibble(value & 0x0f);
}

int nibble_for_char(char c){
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void printf_hexdump(const void *data, int size){
    if (size <= 0) return;
    int i;
    for (i=0; i<size;i++){
        printf("%02X ", ((uint8_t *)data)[i]);
    }
    printf("\n");
}

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
        if (j > BYTES_PER_BYTE * (ITEMS_PER_LINE-1)){
            j = 0;
        }

        uint8_t byte = ((uint8_t *)data)[i];
        buffer[j++] = '0';
        buffer[j++] = 'x';
        buffer[j++] = char_for_high_nibble(byte);
        buffer[j++] = char_for_low_nibble(byte);
        buffer[j++] = ',';
        buffer[j++] = ' ';     

        if (j >= BYTES_PER_BYTE * ITEMS_PER_LINE ){
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
    log_hexdump(LOG_LEVEL_DEBUG, data, size);
#else
    UNUSED(data);
    UNUSED(size);
#endif
}

void log_info_hexdump(const void *data, int size){
#ifdef ENABLE_LOG_INFO
    log_hexdump(LOG_LEVEL_INFO, data, size);
#else
    UNUSED(data);
    UNUSED(size);
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
    memcpy(uuid, bluetooth_base_uuid, 16);
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
    // orig code
    // sprintf(bd_addr_to_str_buffer, "%02x:%02x:%02x:%02x:%02x:%02x", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    // sprintf-free code
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
        if (i == BD_ADDR_LEN - 1) {
            result = 1;
            break;
        }
        char separator = *addr_string++;
        if (separator != ':' && separator != '-' && separator != ' ') break;
    }

    if (result){
        bd_addr_copy(addr, buffer);
    }
	return result;
}

uint32_t btstack_atoi(const char *str){
    uint32_t val = 0;
    while (1){
        char chr = *str;
        if (!chr || chr < '0' || chr > '9')
            return val;
        val = (val * 10) + (uint8_t)(chr - '0');
        str++;
    }
}