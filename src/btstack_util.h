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

/**
 * @title General Utility Functions
 *
 */
 
#ifndef BTSTACK_UTIL_H
#define BTSTACK_UTIL_H


#if defined __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bluetooth.h"
#include "btstack_defines.h"
#include "btstack_linked_list.h"
#include "btstack_debug.h"

#ifdef ENABLE_LOG_APP_MESSAGING
	#define log_app_messaging log_debug
	#define LOG_APP_CODE(...) __VA_ARGS__
#else
	#define log_app_messaging(...)
#define LOG_APP_CODE(...)
#endif
	
// hack: compilation with the android ndk causes an error as there's a reverse_64 macro
#ifdef reverse_64
#undef reverse_64
#endif

#ifdef _WINDOWS
	#define strcasecmp _stricmp
#endif 

// will be moved to daemon/btstack_device_name_db.h

/**
 * @brief TestNon-zero if all bits in flag_mask are set in value
 * @parm value this is tested for all flags set in the mask
 * @parm flag_mask this mask is tested against value
 * @return 0 if false
 */
#define IS_BIT_SET(value, flag_mask) (((value) & flag_mask) == flag_mask)

 /**
  * @brief returns a value with number of lowest bits set to <1>
  */
#define N_BITS(value) ((1<<value) - 1)

/**
* @brief type independed min-macro for 3 values
*/
#define min3(a, b, c) (min(min(a, b), c))

/**
 * @brief The device name type
 */
#define DEVICE_NAME_LEN 248
typedef uint8_t device_name_t[DEVICE_NAME_LEN+1];
//typedef enum { read = 0, write = 1 } subevent_value_read_write_t;

/* API_START */

/**
 * @brief Minimum function for uint32_t
 * @param a
 * @param b
 * @return value
 */
uint32_t btstack_min(uint32_t a, uint32_t b);

/**
 * @brief Maximum function for uint32_t
 * @param a
 * @param b
 * @return value
 */
uint32_t btstack_max(uint32_t a, uint32_t b);

/**
 * @brief Calculate delta between two uint32_t points in time
 * @return time_a - time_b - result > 0 if time_a is newer than time_b
 */
int32_t btstack_time_delta(uint32_t time_a, uint32_t time_b);

/**
 * @brief Calculate delta between two uint16_t points in time
 * @return time_a - time_b - result > 0 if time_a is newer than time_b
 */
int16_t btstack_time16_delta(uint16_t time_a, uint16_t time_b);

/** 
 * @brief Read 08/16/24/32 bit little endian value from buffer
 * @param buffer
 * @param position in buffer
 * @return value
 */
uint8_t  little_endian_read_08(const uint8_t * buffer, int position);
uint16_t little_endian_read_16(const uint8_t * buffer, int position);
uint32_t little_endian_read_24(const uint8_t * buffer, int position);
uint32_t little_endian_read_32(const uint8_t * buffer, int position);


/** 
 * @brief Write 16/32 bit little endian value into buffer
 * @param buffer
 * @param position in buffer
 * @param value
 */
void little_endian_store_08(uint8_t * buffer, uint16_t position, uint8_t value);
void little_endian_store_16(uint8_t * buffer, uint16_t position, uint16_t value);
void little_endian_store_24(uint8_t * buffer, uint16_t position, uint32_t value);
void little_endian_store_32(uint8_t * buffer, uint16_t position, uint32_t value);

/** 
 * @brief BTstack internal stack<>application messaging
 */
void app_write_08(uint8_t* buffer, uint16_t* pos, uint8_t  value);
void app_write_16(uint8_t* buffer, uint16_t* pos, uint16_t value);
void app_write_24(uint8_t* buffer, uint16_t* pos, uint32_t value);
void app_write_32(uint8_t* buffer, uint16_t* pos, uint32_t value);
uint16_t app_write_str(uint8_t* buffer, uint16_t* pos, uint16_t dest_size, char* str);
																 
void app_read_08(const uint8_t* buffer, uint16_t* pos, uint8_t  *value);
void app_read_16(const uint8_t* buffer, uint16_t* pos, uint16_t *value);
void app_read_24(const uint8_t* buffer, uint16_t* pos, uint32_t *value);
void app_read_32(const uint8_t* buffer, uint16_t* pos, uint32_t *value);
uint16_t app_read_str(uint8_t* buffer, uint16_t* pos, uint16_t dest_size, char* str);


static inline uint16_t LITTLE_ENDIAN_READ_08(const uint8_t* buffer, uint16_t position) { uint8_t  value; uint16_t tmp_pos = (uint16_t)position; app_read_08(buffer, &tmp_pos, &value); log_app_messaging("LITTLE_ENDIAN_READ_08: pos:%u value:%u (0x%04X)", tmp_pos, value, value); return value; }
static inline uint16_t LITTLE_ENDIAN_READ_16(const uint8_t* buffer, uint16_t position) { uint16_t value; uint16_t tmp_pos = (uint16_t)position; app_read_16(buffer, &tmp_pos, &value); log_app_messaging("LITTLE_ENDIAN_READ_16: pos:%u value:%u (0x%04X)", tmp_pos, value, value); return value; }
static inline uint16_t LITTLE_ENDIAN_READ_24(const uint8_t* buffer, uint16_t position) { uint32_t value; uint16_t tmp_pos = (uint16_t)position; app_read_24(buffer, &tmp_pos, &value); log_app_messaging("LITTLE_ENDIAN_READ_24: pos:%u value:%u (0x%04X)", tmp_pos, value, value); return value; }
static inline uint16_t LITTLE_ENDIAN_READ_32(const uint8_t* buffer, uint16_t position) { uint32_t value; uint16_t tmp_pos = (uint16_t)position; app_read_32(buffer, &tmp_pos, &value); log_app_messaging("LITTLE_ENDIAN_READ_32: pos:%u value:%u (0x%04X)", tmp_pos, value, value); return value; }



#define APP_WRITE_08(buffer, ppos, value)  { uint8_t  tmp_val = value;  log_app_messaging("APP_WRITE_08: pos:%u %s:%u", (unsigned int)*ppos, #value, (unsigned int)tmp_val); app_write_08(buffer, ppos, tmp_val); } 
#define APP_WRITE_16(buffer, ppos, value)  { uint16_t tmp_val = value;  log_app_messaging("APP_WRITE_16: pos:%u %s:%u", (unsigned int)*ppos, #value, (unsigned int)tmp_val); app_write_16(buffer, ppos, tmp_val); } 
#define APP_WRITE_24(buffer, ppos, value)  { uint32_t tmp_val = value;  log_app_messaging("APP_WRITE_24: pos:%u %s:%u", (unsigned int)*ppos, #value, (unsigned int)tmp_val); app_write_24(buffer, ppos, tmp_val); } 
#define APP_WRITE_32(buffer, ppos, value)  { uint32_t tmp_val = value;  log_app_messaging("APP_WRITE_32: pos:%u %s:%u", (unsigned int)*ppos, #value, (unsigned int)tmp_val); app_write_32(buffer, ppos, tmp_val); } 
#define APP_WRITE_LEN(buffer, pos)         { btstack_assert(pos <= 255);log_app_messaging("APP_WRITE_LEN: buf[1]=%u", pos); buffer[1] = (uint8_t)pos; }


#define APP_READ_08(buffer, ppos, pvalue)  { LOG_APP_CODE(uint16_t tmp_pos = *ppos;) app_read_08(buffer, ppos, pvalue); log_app_messaging("APP_READ_08: pos:%u %s:%u (0x%04X)" , tmp_pos, #pvalue, (unsigned int)*pvalue, (unsigned int)*pvalue); } 
#define APP_READ_16(buffer, ppos, pvalue)  { LOG_APP_CODE(uint16_t tmp_pos = *ppos;) app_read_16(buffer, ppos, pvalue); log_app_messaging("APP_READ_16: pos:%u %s:%u (0x%04X)" , tmp_pos, #pvalue, (unsigned int)*pvalue, (unsigned int)*pvalue); } 
#define APP_READ_24(buffer, ppos, pvalue)  { LOG_APP_CODE(uint16_t tmp_pos = *ppos;) app_read_24(buffer, ppos, pvalue); log_app_messaging("APP_READ_24: pos:%u %s:%u (0x%04X)" , tmp_pos, #pvalue, (unsigned int)*pvalue, (unsigned int)*pvalue); } 
#define APP_READ_32(buffer, ppos, pvalue)  { LOG_APP_CODE(uint16_t tmp_pos = *ppos;) app_read_32(buffer, ppos, pvalue); log_app_messaging("APP_READ_32: pos:%u %s:%u (0x%04X)" , tmp_pos, #pvalue, (unsigned int)*pvalue, (unsigned int)*pvalue); } 

#define APP_WRITE_STR(buffer, ppos, dest_size, str) { LOG_APP_CODE(uint16_t tmp_pos = *ppos, bytes =) app_write_str(buffer, ppos, dest_size, str); log_app_messaging("APP_WRITE_STR: pos:%u dest_size:%u bytes:%u str(%s):%s", (unsigned int)tmp_pos, (unsigned int)dest_size, bytes, #str, str); }
#define APP_READ_STR(buffer, ppos, dest_size, str)  { LOG_APP_CODE(uint16_t tmp_pos = *ppos, bytes =) app_read_str(buffer, ppos, dest_size, str); log_app_messaging("APP_READ_STR: pos:%u dest_size:%u bytes:%u str(%s):%s", (unsigned int)tmp_pos, (unsigned int)dest_size, bytes, #str, str); }

/** 
 * @brief Read 16/24/32 bit big endian value from buffer
 * @param buffer
 * @param position in buffer
 * @return value
 */
uint32_t big_endian_read_08(const uint8_t* buffer, int position);
uint32_t big_endian_read_16(const uint8_t * buffer, int position);
uint32_t big_endian_read_24(const uint8_t * buffer, int position);
uint32_t big_endian_read_32(const uint8_t * buffer, int position);
static inline void read_arr(uint8_t* buffer, int position, uint8_t* arr, size_t size) { memcpy(arr, &buffer[position], size); }

/** 
 * @brief Write 16/32 bit big endian value into buffer
 * @param buffer
 * @param position in buffer
 * @param value
 */
void big_endian_store_08(uint8_t * buffer, uint16_t position, uint8_t value);
void big_endian_store_16(uint8_t * buffer, uint16_t position, uint16_t value);
void big_endian_store_24(uint8_t * buffer, uint16_t position, uint32_t value);
void big_endian_store_32(uint8_t * buffer, uint16_t position, uint32_t value);
static inline void store_arr(uint8_t* buffer, uint16_t position, uint8_t* arr, size_t size) { memcpy(&buffer[position], arr, size); }

/**
 * @brief Bluetooth SIG Central<>Peripheral APP-PARAMETER messaging (Big Endian)1
 */
#define BT_APP_PARAM_WRITE_08( buffer, ppos, value,  size)	{ big_endian_store_08(buffer, (uint16_t)*ppos, value); log_app_messaging("BT_APP_PARAM_STORE_08: pos:%u %s:%u (0x%04X)"         ,*ppos, #value, (unsigned int)value, (unsigned int)value);      *ppos += 1; }
#define BT_APP_PARAM_WRITE_16( buffer, ppos, value,  size)	{ big_endian_store_16(buffer, (uint16_t)*ppos, value); log_app_messaging("BT_APP_PARAM_STORE_16: pos:%u %s:%u (0x%04X)"         ,*ppos, #value, (unsigned int)value, (unsigned int)value);      *ppos += 2; }
#define BT_APP_PARAM_WRITE_24( buffer, ppos, value,  size)	{ big_endian_store_24(buffer, (uint16_t)*ppos, value); log_app_messaging("BT_APP_PARAM_STORE_24: pos:%u %s:%u (0x%04X)"         ,*ppos, #value, (unsigned int)value, (unsigned int)value);      *ppos += 3; }
#define BT_APP_PARAM_WRITE_32( buffer, ppos, value,  size)	{ big_endian_store_32(buffer, (uint16_t)*ppos, value); log_app_messaging("BT_APP_PARAM_STORE_32: pos:%u %s:%u (0x%04X)"         ,*ppos, #value, (unsigned int)value, (unsigned int)value);      *ppos += 4; }
#define BT_APP_PARAM_WRITE_ARR(buffer, ppos, pvalue, size)  { store_arr(buffer, *ppos, pvalue, size);              log_app_messaging("BT_APP_PARAM_STORE_ARR: pos:%u size:%u %s:<%s>"        , *ppos, size, #pvalue, pvalue, pvalue);                                *ppos += size; }

#define BT_APP_PARAM_READ_08( buffer, ppos, pvalue, size)	{ uint16_t tpos = *ppos; *pvalue = big_endian_read_08(buffer, tpos);              log_app_messaging("BT_APP_PARAM_READ_08: pos:%u %s:%u (0x%04X)"          ,tpos, #pvalue, (unsigned int)*pvalue, (unsigned int)*pvalue); *ppos += 1; }
#define BT_APP_PARAM_READ_16( buffer, ppos, pvalue, size)	{ uint16_t tpos = *ppos; *pvalue = big_endian_read_16(buffer, tpos);              log_app_messaging("BT_APP_PARAM_READ_16: pos:%u %s:%u (0x%04X)"          ,tpos, #pvalue, (unsigned int)*pvalue, (unsigned int)*pvalue); *ppos += 2; }
#define BT_APP_PARAM_READ_24( buffer, ppos, pvalue, size)	{ uint16_t tpos = *ppos; *pvalue = big_endian_read_24(buffer, tpos);              log_app_messaging("BT_APP_PARAM_READ_24: pos:%u %s:%u (0x%04X)"          ,tpos, #pvalue, (unsigned int)*pvalue, (unsigned int)*pvalue); *ppos += 3; }
#define BT_APP_PARAM_READ_32( buffer, ppos, pvalue, size)	{ uint16_t tpos = *ppos; *pvalue = big_endian_read_32(buffer, tpos);              log_app_messaging("BT_APP_PARAM_READ_32: pos:%u %s:%u (0x%04X)"          ,tpos, #pvalue, (unsigned int)*pvalue, (unsigned int)*pvalue); *ppos += 4; }
#define BT_APP_PARAM_READ_ARR(buffer, ppos, pvalue, size)   { uint16_t tpos = *ppos;          read_arr(buffer, tpos, (uint8_t*)pvalue, size); log_app_messaging("BT_APP_PARAM_READ_ARR: pos:%u %s:<%s>"                ,tpos, #pvalue, pvalue);                                       *ppos += size; } 

/**
 * @brief Swap bytes in 16 bit integer
 */
static inline uint16_t btstack_flip_16(uint16_t value){
    return (uint16_t)((value & 0xffu) << 8) | (value >> 8);
}

/** 
 * @brief Check for big endian system
 * @return 1 if on big endian
 */
static inline int btstack_is_big_endian(void){
	uint16_t sample = 0x0100;
	return (int) *(uint8_t*) &sample;
}

/** 
 * @brief Check for little endian system
 * @return 1 if on little endian
 */
static inline int btstack_is_little_endian(void){
	uint16_t sample = 0x0001;
	return (int) *(uint8_t*) &sample;
}

/**
 * @brief Copy from source to destination and reverse byte order
 * @param src
 * @param dest
 * @param len
 */
void reverse_bytes(const uint8_t * src, uint8_t * dest, int len);

/**
 * @brief Wrapper around reverse_bytes for common buffer sizes
 * @param src
 * @param dest
 */
void reverse_24 (const uint8_t * src, uint8_t * dest);
void reverse_48 (const uint8_t * src, uint8_t * dest);
void reverse_56 (const uint8_t * src, uint8_t * dest);
void reverse_64 (const uint8_t * src, uint8_t * dest);
void reverse_128(const uint8_t * src, uint8_t * dest);
void reverse_256(const uint8_t * src, uint8_t * dest);

void reverse_bd_addr(const bd_addr_t src, bd_addr_t dest);

/**
 * @brief Check if all bytes in buffer are zero
 * @param buffer
 * @param size
 * @return true if all bytes is buffer are zero
 */
bool btstack_is_null(const uint8_t * buffer, uint16_t size);

/**
 * @brief Check if all bytes in a bd_addr_t are zero
 * @param addr
 * @return true if all bytes in addr are zero
 */
bool btstack_is_null_bd_addr( const bd_addr_t addr );

/** 
 * @brief ASCII character for 4-bit nibble
 * @return character
 */
char char_for_nibble(uint8_t nibble);

/**
 * @brif 4-bit nibble from ASCII character
 * @return value
 */
int nibble_for_char(char c);

/**
 * @brief Compare two Bluetooth addresses
 * @param a
 * @param b
 * @return 0 if equal
 */
int bd_addr_cmp(const bd_addr_t a, const bd_addr_t b);

/**
 * @brief Copy Bluetooth address
 * @param dest
 * @param src
 */
void bd_addr_copy(bd_addr_t dest, const bd_addr_t src);

/**
 * @brief Use printf to write hexdump as single line of data
 */
void printf_hexdump(const void * data, int size);

/**
 * @brief Create human readable representation for UUID128
 * @note uses fixed global buffer
 * @return pointer to UUID128 string
 */
char * uuid128_to_str(const uint8_t * uuid);

/**
 * @brief Create human readable represenationt of Bluetooth address
 * @note uses fixed global buffer
 * @param delimiter
 * @return pointer to Bluetooth address string
 */
char * bd_addr_to_str_with_delimiter(const bd_addr_t addr, char delimiter);

/**
 * @brief Create human readable represenationt of Bluetooth address
 * @note uses fixed global buffer
 * @return pointer to Bluetooth address string
 */
char * bd_addr_to_str(const bd_addr_t addr);

/**
 * @brief Replace address placeholder '00:00:00:00:00:00' with Bluetooth address
 * @param buffer
 * @param size
 * @param address
 */
void btstack_replace_bd_addr_placeholder(uint8_t * buffer, uint16_t size, const bd_addr_t address);

/** 
 * @brief Parse Bluetooth address
 * @param address_string
 * @param buffer for parsed address
 * @return 1 if string was parsed successfully
 */
int sscanf_bd_addr(const char * addr_string, bd_addr_t addr);

/**
 * @brief Constructs UUID128 from 16 or 32 bit UUID using Bluetooth base UUID
 * @param uuid128 output buffer
 * @param short_uuid
 */
void uuid_add_bluetooth_prefix(uint8_t * uuid128, uint32_t short_uuid);

/**
 * @brief Checks if UUID128 has Bluetooth base UUID prefix
 * @param uui128 to test
 * @return true if it can be expressed as UUID32
 */
bool uuid_has_bluetooth_prefix(const uint8_t * uuid128);

/**
 * @brief Parse unsigned number 
 * @param str to parse
 * @return value
 */
uint32_t btstack_atoi(const char * str);

/**
 * @brief Return number of digits of a uint32 number
 * @param uint32_number
 * @return num_digits
 */
int string_len_for_uint32(uint32_t i);

/**
 * @brief Return number of set bits in a uint32 number
 * @param uint32_number
 * @return num_set_bits
 */
int count_set_bits_uint32(uint32_t x);

/**
 * @brief Check CRC8 using ETSI TS 101 369 V6.3.0.
 * @note Only used by RFCOMM
 * @param data
 * @param len
 * @param check_sum
 */
uint8_t btstack_crc8_check(uint8_t * data, uint16_t len, uint8_t check_sum);

/**
 * @brief Calculate CRC8 using ETSI TS 101 369 V6.3.0. 
 * @note Only used by RFCOMM
 * @param data
 * @param len
 */
uint8_t btstack_crc8_calc(uint8_t * data, uint16_t len);


/**
 * @brief Calculate the initial CRC32 value using ISO 3309 (HDLC), polynomial (normal) 0x04c11db7
 * @note Used by OTS Service. 
 * 
 * @return  The initial crc value.
 */
uint32_t btstack_crc32_init(void);

/**
 * @brief Update the CRC32 value with new data.
 *
 * @param crc      The current crc value.
 * @param data     Pointer to a buffer of \a data_len bytes.
 * @param data_len Number of bytes in the \a data buffer.
 * @return             The updated crc value.
 */
uint32_t btstack_crc32_update(uint32_t crc, const uint8_t * data, uint32_t data_len);

/**
 * @brief Calculate the final CRC32 value.
 *
 * @param crc  The current crc value.
 * @return     The final crc value.
 */
uint32_t btstack_crc32_finalize(uint32_t crc);

/**
 * @brief Get next cid
 * @param current_cid
 * @return next cid skiping 0
 */
uint16_t btstack_next_cid_ignoring_zero(uint16_t current_cid);

/**
 * @brief Copy string (up to dst_size-1 characters) from src into dst buffer with terminating '\0'
 * @note replaces strncpy + dst[dst_size-1] = '\0'
 * @param dst
 * @param dst_size
 * @param src
 * @retun bytes_copied including trailing 0
 */
uint16_t btstack_strcpy(char * dst, uint16_t dst_size, const char * src);

/**
 * @brief Append src string to string in dst buffer with terminating '\0'
 * @note max total string length will be dst_size-1 characters
 * @param dst
 * @param dst_size
 * @param src
 */
void btstack_strcat(char * dst, uint16_t dst_size, const char * src);

/**
 * Returns the number of leading 0-bits in x, starting at the most significant bit position.
 * If x is 0, the result is undefined.
 * @note maps to __builtin_clz for gcc and clang
 * @param value
 * @return number of leading 0-bits
 */
uint8_t btstack_clz(uint32_t value);

/**
 * @brief Copy chunk of data into a virtual buffer backed by a physical buffer.
 * Used to provide chunk of data of larger buffer that is constructed on the fly, e.g. serializing data struct
 * 
 * For example, copy field2 to buffer, with buffer_offset = 11
 * 
 *           field1  field2  field3     field4        field5  filed6
 * struct:  -------|-------|----------|-------------|-------|--------------
 * buffer:             ------------------       
 * result:             ----|         
 * 
 * When also copying field3 and field4 to buffer, with buffer_offset = 11
 * 
 *           field1  field2  field3     field4        field5  filed6
 * struct:  -------|-------|----------|-------------|-------|--------------
 * buffer:             ------------------       
 * result:             ----|----------|--         
 *
 * @param field_data
 * @param field_len
 * @param field_offset  position of field in complete data block
 * @param buffer_data
 * @param buffer_len
 * @param buffer_offset position of buffer in complete data block
 * @return bytes_copied number of bytes actually stored in buffer
 */
uint16_t btstack_virtual_memcpy(
    const uint8_t * field_data, uint16_t field_len, uint16_t field_offset, 
    uint8_t * buffer, uint16_t buffer_size, uint16_t buffer_offset);


/* API_END */

#if defined __cplusplus
}
#endif
		
#endif // BTSTACK_UTIL_H
