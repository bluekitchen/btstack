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
 *  btstack_util.h
 *
 *  General utility functions
 *
 *  Created by Matthias Ringwald on 7/23/09.
 */

#ifndef __BTSTACK_UTIL_H
#define __BTSTACK_UTIL_H


#if defined __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "bluetooth.h"
#include "btstack_defines.h"
#include "btstack_linked_list.h"
	
// will be moved to daemon/btstack_device_name_db.h

/**
 * @brief The device name type
 */
#define DEVICE_NAME_LEN 248
typedef uint8_t device_name_t[DEVICE_NAME_LEN+1]; 
	

// helper for little endian format
#define little_endian_read_16( buffer, pos) ( ((uint16_t) buffer[pos]) | (((uint16_t)buffer[(pos)+1]) << 8))
#define little_endian_read_24( buffer, pos) ( ((uint32_t) buffer[pos]) | (((uint32_t)buffer[(pos)+1]) << 8) | (((uint32_t)buffer[(pos)+2]) << 16))
#define little_endian_read_32( buffer, pos) ( ((uint32_t) buffer[pos]) | (((uint32_t)buffer[(pos)+1]) << 8) | (((uint32_t)buffer[(pos)+2]) << 16) | (((uint32_t) buffer[(pos)+3])) << 24)

void little_endian_store_16(uint8_t *buffer, uint16_t pos, uint16_t value);
void little_endian_store_32(uint8_t *buffer, uint16_t pos, uint32_t value);

// helper for big endian format
#define big_endian_read_16( buffer, pos) ( ((uint16_t) buffer[(pos)+1]) | (((uint16_t)buffer[ pos   ]) << 8))
#define bit_endian_read_32( buffer, pos) ( ((uint32_t) buffer[(pos)+3]) | (((uint32_t)buffer[(pos)+2]) << 8) | (((uint32_t)buffer[(pos)+1]) << 16) | (((uint32_t) buffer[pos])) << 24)

void big_endian_store_16(uint8_t *buffer, uint16_t pos, uint16_t value);
void big_endian_store_32(uint8_t *buffer, uint16_t pos, uint32_t value);

// hack: compilation with the android ndk causes an error as there's a swap64 macro
#ifdef swap64
#undef swap64
#endif

/**
 * @brief Copy from source to destination and reverse byte order
 */
void swapX  (const uint8_t *src, uint8_t * dst, int len);
void swap24 (const uint8_t *src, uint8_t * dst);
void swap48 (const uint8_t *src, uint8_t * dst);
void swap56 (const uint8_t *src, uint8_t * dst);
void swap64 (const uint8_t *src, uint8_t * dst);
void swap128(const uint8_t *src, uint8_t * dst);

void bt_flip_addr(bd_addr_t dest, bd_addr_t src);

/** 
 * @brief 4-bit nibble
 * @return ASCII character for 4-bit nibble
 */
char char_for_nibble(int nibble);

/**
 * @brief Compare two Bluetooth addresses
 * @param a
 * @param b
 * @return true if equal
 */
#define BD_ADDR_CMP(a,b) memcmp(a,b, BD_ADDR_LEN)

/**
 * @brief Copy Bluetooth address
 * @param dest
 * @param src
 */
#define BD_ADDR_COPY(dest,src) memcpy(dest,src,BD_ADDR_LEN)

/**
 * @brief Use printf to write hexdump as single line of data
 */
void printf_hexdump(const void *data, int size);

// move to btstack_debug.h
// void log_info_hexdump(..) either log or hci_dump or off
void log_key(const char * name, sm_key_t key);

//
void hexdump(const void *data, int size);
void hexdumpf(const void *data, int size);

/**
 * @brief Create human readable representation for UUID128
 * @note uses fixed global buffer
 * @return pointer to UUID128 string
 */
char * uuid128_to_str(uint8_t * uuid);

/**
 * @brief Print UUID128
 * @note uses fixed global buffer
 */
void printUUID128(uint8_t *uuid);

/**
 * @brief Create human readable represenationt of Bluetooth address
 * @note uses fixed global buffer
 * @return pointer to Bluetooth address string
 */
char * bd_addr_to_str(bd_addr_t addr);

/** 
 * @brief Parse Bluetooth address
 * @param address_string
 * @param buffer for parsed address
 * @return 1 if string was parsed successfully
 */
int sscan_bd_addr(uint8_t * addr_string, bd_addr_t addr);


void sdp_normalize_uuid(uint8_t *uuid, uint32_t shortUUID);
int  sdp_has_blueooth_base_uuid(uint8_t * uuid128);

#if defined __cplusplus
}
#endif
		
#endif // __BTSTACK_UTIL_H
