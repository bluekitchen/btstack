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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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
 *  General utility functions
 */

#ifdef _MSC_VER
#include <Windows.h>
#include <intrin.h>
#endif

#include "btstack_config.h"
#include "btstack_debug.h"
#include "btstack_util.h"

#include <stdio.h>  // vsnprintf
#include <string.h> // memcpy



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

uint8_t little_endian_read_08(const uint8_t* buffer, int position) {
    return (uint8_t)buffer[position];
}
uint16_t little_endian_read_16(const uint8_t * buffer, int position){
    return (uint16_t)(((uint16_t) buffer[position]) | (((uint16_t)buffer[position+1]) << 8));
}
uint32_t little_endian_read_24(const uint8_t * buffer, int position){
    return ((uint32_t) buffer[position]) | (((uint32_t)buffer[position+1]) << 8) | (((uint32_t)buffer[position+2]) << 16);
}
uint32_t little_endian_read_32(const uint8_t * buffer, int position){
    return ((uint32_t) buffer[position]) | (((uint32_t)buffer[position+1]) << 8) | (((uint32_t)buffer[position+2]) << 16) | (((uint32_t) buffer[position+3]) << 24);
}

void little_endian_store_08(uint8_t* buffer, uint16_t position, uint8_t value) {
    uint16_t pos = position;
    buffer[pos] = value;
}

void little_endian_store_16(uint8_t * buffer, uint16_t position, uint16_t value){
    uint16_t pos = position;
    buffer[pos++] = (uint8_t)value;
    buffer[pos++] = (uint8_t)(value >> 8);
}

void little_endian_store_24(uint8_t * buffer, uint16_t position, uint32_t value){
    uint16_t pos = position;
    buffer[pos++] = (uint8_t)(value);
    buffer[pos++] = (uint8_t)(value >> 8);
    buffer[pos++] = (uint8_t)(value >> 16);
}

void little_endian_store_32(uint8_t * buffer, uint16_t position, uint32_t value){
    uint16_t pos = position;
    buffer[pos++] = (uint8_t)(value);
    buffer[pos++] = (uint8_t)(value >> 8);
    buffer[pos++] = (uint8_t)(value >> 16);
    buffer[pos++] = (uint8_t)(value >> 24);
}

uint32_t big_endian_read_08(const uint8_t* buffer, int position) {
    return buffer[position];
}

uint32_t big_endian_read_16(const uint8_t * buffer, int position) {
    return (uint16_t)(((uint16_t) buffer[position+1]) | (((uint16_t)buffer[position]) << 8));
}

uint32_t big_endian_read_24(const uint8_t * buffer, int position) {
    return ( ((uint32_t)buffer[position+2]) | (((uint32_t)buffer[position+1]) << 8) | (((uint32_t) buffer[position]) << 16));
}

uint32_t big_endian_read_32(const uint8_t * buffer, int position) {
    return ((uint32_t) buffer[position+3]) | (((uint32_t)buffer[position+2]) << 8) | (((uint32_t)buffer[position+1]) << 16) | (((uint32_t) buffer[position]) << 24);
}

void big_endian_store_08(uint8_t* buffer, uint16_t position, uint8_t value) {
    uint16_t pos = position;
    buffer[pos++] = (uint8_t)(value);
}

void big_endian_store_16(uint8_t * buffer, uint16_t position, uint16_t value){
    uint16_t pos = position;
    buffer[pos++] = (uint8_t)(value >> 8);
    buffer[pos++] = (uint8_t)(value);
}

void big_endian_store_24(uint8_t * buffer, uint16_t position, uint32_t value){
    uint16_t pos = position;
    buffer[pos++] = (uint8_t)(value >> 16);
    buffer[pos++] = (uint8_t)(value >> 8);
    buffer[pos++] = (uint8_t)(value);
}

void big_endian_store_32(uint8_t * buffer, uint16_t position, uint32_t value){
    uint16_t pos = position;
    buffer[pos++] = (uint8_t)(value >> 24);
    buffer[pos++] = (uint8_t)(value >> 16);
    buffer[pos++] = (uint8_t)(value >> 8);
    buffer[pos++] = (uint8_t)(value);
}

// general swap/endianess utils
void reverse_bytes(const uint8_t * src, uint8_t * dest, int len){
    int i;
    for (i = 0; i < len; i++)
        dest[len - 1 - i] = src[i];
}
void reverse_24(const uint8_t * src, uint8_t * dest){
    reverse_bytes(src, dest, 3);
}
void reverse_48(const uint8_t * src, uint8_t * dest){
    reverse_bytes(src, dest, 6);
}
void reverse_56(const uint8_t * src, uint8_t * dest){
    reverse_bytes(src, dest, 7);
}
void reverse_64(const uint8_t * src, uint8_t * dest){
    reverse_bytes(src, dest, 8);
}
void reverse_128(const uint8_t * src, uint8_t * dest){
    reverse_bytes(src, dest, 16);
}
void reverse_256(const uint8_t * src, uint8_t * dest){
    reverse_bytes(src, dest, 32);
}

void reverse_bd_addr(const bd_addr_t src, bd_addr_t dest){
    reverse_bytes(src, dest, 6);
}

bool btstack_is_null(const uint8_t * buffer, uint16_t size){
    uint16_t i;
    for (i=0; i < size ; i++){
        if (buffer[i] != 0) {
            return false;
        }
    }
    return true;
}

bool btstack_is_null_bd_addr( const bd_addr_t addr ){
    return btstack_is_null( addr, sizeof(bd_addr_t) );
}

uint32_t btstack_min(uint32_t a, uint32_t b){
    return (a < b) ? a : b;
}

uint32_t btstack_max(uint32_t a, uint32_t b){
    return (a > b) ? a : b;
}

/**
 * @brief Calculate delta between two uint32_t points in time
 * @return time_a - time_b - result > 0 if time_a is newer than time_b
 */
int32_t btstack_time_delta(uint32_t time_a, uint32_t time_b){
    return (int32_t)(time_a - time_b);
}

/**
 * @brief Calculate delta between two uint16_t points in time
 * @return time_a - time_b - result > 0 if time_a is newer than time_b
 */
int16_t btstack_time16_delta(uint16_t time_a, uint16_t time_b){
    return (int16_t)(time_a - time_b);
}

char char_for_nibble(uint8_t nibble){

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
void printf_hexdump(const void * data, int size){
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
            hci_dump_log(level, "%s", buffer);
            j = 0;
        }
    }
    if (j != 0){
        buffer[j] = 0;
        hci_dump_log(level, "%s", buffer);
    }
}
#endif

void log_debug_hexdump(const void * data, int size){
#ifdef ENABLE_LOG_DEBUG
    log_hexdump(HCI_DUMP_LOG_LEVEL_DEBUG, data, size);
#else
    UNUSED(data);   // ok: no code
    UNUSED(size);   // ok: no code
#endif
}

void log_info_hexdump(const void * data, int size){
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

void uuid_add_bluetooth_prefix(uint8_t * uuid128, uint32_t short_uuid){
    (void)memcpy(uuid128, bluetooth_base_uuid, 16);
    big_endian_store_32(uuid128, 0, short_uuid);
}

bool uuid_has_bluetooth_prefix(const uint8_t * uuid128){
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
char * bd_addr_to_str_with_delimiter(const bd_addr_t addr, char delimiter){
    char * p = bd_addr_to_str_buffer;
    int i;
    for (i = 0; i < 6 ; i++) {
        uint8_t byte = addr[i];
        *p++ = char_for_high_nibble(byte);
        *p++ = char_for_low_nibble(byte);
        *p++ = delimiter;
    }
    *--p = 0;
    return (char *) bd_addr_to_str_buffer;
}

char * bd_addr_to_str(const bd_addr_t addr){
    return bd_addr_to_str_with_delimiter(addr, ':');
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
    int upper_nibble = nibble_for_char(byte_string[0]);
    if (upper_nibble < 0) return -1;
    int lower_nibble = nibble_for_char(byte_string[1]);
    if (lower_nibble < 0) return -1;
    return (upper_nibble << 4) | lower_nibble;
}

int sscanf_bd_addr(const char * addr_string, bd_addr_t addr){
    const char * the_string = addr_string;
    uint8_t buffer[BD_ADDR_LEN];
    int result = 0;
    int i;
    for (i = 0; i < BD_ADDR_LEN; i++) {
        int single_byte = scan_hex_byte(the_string);
        if (single_byte < 0) break;
        the_string += 2;
        buffer[i] = (uint8_t)single_byte;
        // don't check separator after last byte
        if (i == (BD_ADDR_LEN - 1)) {
            result = 1;
            break;
        }
        // skip supported separators
        char next_char = *the_string;
        if ((next_char == ':') || (next_char == '-') || (next_char == ' ')) {
            the_string++;
        }
    }

    if (result != 0){
        bd_addr_copy(addr, buffer);
    }
	return result;
}

uint32_t btstack_atoi(const char * str){
    const char * the_string = str;
    uint32_t val = 0;
    while (true){
        char chr = *the_string++;
        // skip whitespace
        if (((chr >= 0x09) && (chr <= 0x0d)) || (chr == ' ')) {
            continue;
        }
        if (!chr || (chr < '0') || (chr > '9')){
            return val;
        }
        val = (val * 10u) + (uint8_t)(chr - '0');
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
    uint32_t v = x;
    v = (v & 0x55555555) + ((v >> 1)  & 0x55555555U);
    v = (v & 0x33333333) + ((v >> 2)  & 0x33333333U);
    v = (v & 0x0F0F0F0F) + ((v >> 4)  & 0x0F0F0F0FU);
    v = (v & 0x00FF00FF) + ((v >> 8)  & 0x00FF00FFU);
    v = (v & 0x0000FFFF) + ((v >> 16) & 0x0000FFFFU);
    return v;
}

uint8_t btstack_clz(uint32_t value) {
    btstack_assert( value != 0 );
#if defined(__GNUC__) || defined (__clang__)
    // use gcc/clang intrinsic
    return (uint8_t) __builtin_clz(value);
#elif defined(_MSC_VER)
    // use MSVC intrinsic
    DWORD leading_zero = 0;
    _BitScanReverse( &leading_zero, value );
	return (uint8_t)(31 - leading_zero);
#else
    // divide-and-conquer implementation for 32-bit integers
    uint32_t x = value;
    uint8_t r = 0;
    if ((x & 0xffff0000u) == 0) {
        x <<= 16;
        r += 16;
    }
    if ((x & 0xff000000u) == 0) {
        x <<= 8;
        r += 8;
    }
    if ((x & 0xf0000000u) == 0) {
        x <<= 4;
        r += 4;
    }
    if ((x & 0xc0000000u) == 0) {
        x <<= 2;
        r += 2;
    }
    if ((x & 0x80000000u) == 0) {
        x <<= 1;
        r += 1;
    }
    return r;
#endif
}

/*  
 * CRC-8 (reversed crc) lookup table as calculated by the table generator in ETSI TS 101 369 V6.3.0.
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

/*  
 * CRC-32 lookup table as calculated by the table generator. Polynomial (normal) 0x04c11db7, ISO 3309 (HDLC)
 *
 * Created using pycrc tool (https://pycrc.org) with "crc-32" as model, and "table-driven" algorithm
 *  python3 pycrc.py --model crc-32 --algorithm table-driven --generate h -o crc32.h
 *  python3 pycrc.py --model crc-32 --algorithm table-driven --generate c -o crc32.c
 */

/**
 * Static table used for the table_driven implementation.
 */
static const uint32_t crc_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
    0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
    0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
    0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
    0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
    0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
    0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
    0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
    0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
    0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
    0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
    0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
    0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
    0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
    0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
    0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

uint32_t btstack_crc32_init(void){
    return 0xffffffff;
}

uint32_t btstack_crc32_update(uint32_t crc, const uint8_t * data, uint32_t data_len){
    const uint8_t *d = data;
    uint32_t tbl_idx;

    while (data_len--) {
        tbl_idx = (crc ^ *d) & 0xff;
        crc = (crc_table[tbl_idx] ^ (crc >> 8)) & 0xffffffff;
        d++;
    }
    return crc & 0xffffffff;
}

uint32_t btstack_crc32_finalize(uint32_t crc){
    return crc ^ 0xffffffff;
}

/*-----------------------------------------------------------------------------------*/

uint16_t btstack_next_cid_ignoring_zero(uint16_t current_cid){
    uint16_t next_cid;
    if (current_cid == 0xffff) {
        next_cid = 1;
    } else {
        next_cid = current_cid + 1;
    }
    return next_cid;
}

uint16_t btstack_strcpy(char * dst, uint16_t dst_size, const char * src){
    uint16_t bytes_to_copy = (uint16_t) btstack_min( dst_size - 1, (uint16_t) strlen(src));
    (void) memcpy(dst, src, bytes_to_copy);
    dst[bytes_to_copy] = 0;
    return bytes_to_copy + 1;
}

void btstack_strcat(char * dst, uint16_t dst_size, const char * src){
    uint16_t src_len = (uint16_t) strlen(src);
    uint16_t dst_len = (uint16_t) strlen(dst);
    uint16_t bytes_to_copy = btstack_min( src_len, dst_size - dst_len - 1);
    (void) memcpy( &dst[dst_len], src, bytes_to_copy);
    dst[dst_len + bytes_to_copy] = 0;
}

int btstack_printf_strlen(const char * format, ...){
    va_list argptr;
    va_start(argptr, format);
    char dummy_buffer[1];
    int len = vsnprintf(dummy_buffer, sizeof(dummy_buffer), format, argptr);
    va_end(argptr);
    return len;
}

uint16_t btstack_snprintf_assert_complete(char * buffer, size_t size, const char * format, ...){
    va_list argptr;
    va_start(argptr, format);
    int len = vsnprintf(buffer, size, format, argptr);
    va_end(argptr);

    // check for no error and no truncation
    btstack_assert(len >= 0);
    btstack_assert((unsigned int) len < size);
    return (uint16_t) len;
}

uint16_t btstack_snprintf_best_effort(char * buffer, size_t size, const char * format, ...){
    btstack_assert(size > 0);
    va_list argptr;
    va_start(argptr, format);
    int len = vsnprintf(buffer, size, format, argptr);
    va_end(argptr);
    if (len < 0) {
        // error -> len = 0
        return 0;
    } else {
        // min of total string len and buffer size
        return (uint16_t) btstack_min((uint32_t) len, (uint32_t) size - 1);
    }
}

uint16_t btstack_virtual_memcpy(
    const uint8_t * field_data, uint16_t field_len, uint16_t field_offset, // position of field in complete data block
    uint8_t * buffer, uint16_t buffer_size, uint16_t buffer_offset){

    uint16_t after_buffer = buffer_offset + buffer_size ;
    // bail before buffer
    if ((field_offset + field_len) < buffer_offset){
        return 0;
    }
    // bail after buffer
    if (field_offset >= after_buffer){
        return 0;
    }
    // calc overlap
    uint16_t bytes_to_copy = field_len;
    
    uint16_t skip_at_start = 0;
    if (field_offset < buffer_offset){
        skip_at_start = buffer_offset - field_offset;
        bytes_to_copy -= skip_at_start;
    }

    uint16_t skip_at_end = 0;
    if ((field_offset + field_len) > after_buffer){
        skip_at_end = (field_offset + field_len) - after_buffer;
        bytes_to_copy -= skip_at_end;
    }
    
    btstack_assert((skip_at_end + skip_at_start) <= field_len);
    btstack_assert(bytes_to_copy <= field_len);

    memcpy(&buffer[(field_offset + skip_at_start) - buffer_offset], &field_data[skip_at_start], bytes_to_copy);
    return bytes_to_copy;
}
