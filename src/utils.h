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

void bt_store_16(uint8_t *buffer, uint16_t pos, uint16_t value);
void bt_store_32(uint8_t *buffer, uint16_t pos, uint32_t value);
void bt_flip_addr(bd_addr_t dest, bd_addr_t src);

void hexdump(void *data, int size);
