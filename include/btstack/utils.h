/*
 *  utils.h
 *
 *  General utility functions
 *
 *  Created by Matthias Ringwald on 7/23/09.
 */

#pragma once

#include <stdint.h>

/**
 * @brief hci connection handle type
 */
typedef uint16_t hci_con_handle_t;

/**
 * @brief Length of a bluetooth device address.
 */
#define BD_ADDR_LEN 6
typedef uint8_t bd_addr_t[BD_ADDR_LEN];

/**
 * @brief The link key type
 */
#define LINK_KEY_LEN 16
typedef uint8_t link_key_t[LINK_KEY_LEN]; 

// helper for BT little endian format
#define READ_BT_16( buffer, pos) ( ((uint16_t) buffer[pos]) | (((uint16_t)buffer[pos+1]) << 8))
#define READ_BT_24( buffer, pos) ( ((uint32_t) buffer[pos]) | (((uint32_t)buffer[pos+1]) << 8) | (((uint32_t)buffer[pos+2]) << 16))
#define READ_BT_32( buffer, pos) ( ((uint32_t) buffer[pos]) | (((uint32_t)buffer[pos+1]) << 8) | (((uint32_t)buffer[pos+2]) << 16) | (((uint32_t) buffer[pos+3])) << 24)

// ACL Packet
#define READ_ACL_CONNECTION_HANDLE( buffer ) ( READ_BT_16(buffer,0) & 0x0fff)
#define READ_ACL_FLAGS( buffer )      ( buffer[1] >> 4 )
#define READ_ACL_LENGTH( buffer )     (READ_BT_16(buffer, 2)

// L2CAP Packet
#define READ_L2CAP_LENGTH(buffer)     ( READ_BT_16(buffer, 4))
#define READ_L2CAP_CHANNEL_ID(buffer) ( READ_BT_16(buffer, 6))

// L2CAP Signaling Packet
#define READ_L2CAP_SIGNALING_CODE( buffer )       (buffer[ 8])
#define READ_L2CAP_SIGNALING_IDENTIFIER( buffer ) (buffer[ 9])
#define READ_L2CAP_SIGNALING_LENGTH( buffer )     (READ_BT_16(buffer, 10))

#define L2CAP_SIGNALING_DATA_OFFSET 12

void bt_store_16(uint8_t *buffer, uint16_t pos, uint16_t value);
void bt_store_32(uint8_t *buffer, uint16_t pos, uint32_t value);
void bt_flip_addr(bd_addr_t dest, bd_addr_t src);

void hexdump(void *data, int size);
void print_bd_addr(bd_addr_t addr);
int sscan_bd_addr(uint8_t * addr_string, bd_addr_t addr);

uint8_t crc8_check(uint8_t *data, uint16_t len, uint8_t check_sum);
uint8_t crc8_calc(uint8_t *data, uint16_t len);

#define BD_ADDR_CMP(a,b) memcmp(a,b, BD_ADDR_LEN)
#define BD_ADDR_COPY(dest,src) memcpy(dest,src,BD_ADDR_LEN)
