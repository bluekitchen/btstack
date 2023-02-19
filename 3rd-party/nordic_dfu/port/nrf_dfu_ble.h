/**
 * @title NRF DFU Service Server
 * 
 */

#ifndef __NRF_DFU_BLE_H__
#define __NRF_DFU_BLE_H__

#include <stdint.h>
#include "bluetooth.h"
#include "btstack_defines.h"
#include "nrf_dfu_types.h"
#if defined __cplusplus
extern "C" {
#endif

#define GATT_MAX_MTU_SIZE          247
#define GATT_HEADER_LEN            3                                                       /**< GATT header length. */
#define GATT_PAYLOAD(mtu)          ((mtu) - GATT_HEADER_LEN)                               /**< Length of the ATT payload for a given ATT MTU. */
#define MAX_DFU_PKT_LEN            (GATT_MAX_MTU_SIZE - GATT_HEADER_LEN)                   /**< Maximum length (in bytes) of the DFU Packet characteristic (3 bytes are used for the GATT opcode and handle). */
#define MAX_RESPONSE_LEN           17                                                      /**< Maximum length (in bytes) of the response to a Control Point command. */
#define RESPONSE_HEADER_LEN        3                                                       /**< The length of the header of a response. I.E. the index of the opcode-specific payload. */

#define NRF_DFU_BUTTONLESS_RSP     0x20

typedef enum {
    NRF_DFU_BUT_CMD_ENTR_BOOTLOADER = 0x01,
    NRF_DFU_BUT_CMD_CHANGE_BOOTLOADER_NAME = 0x02
} nrf_dfu_buttonless_cmd_code_t;

typedef struct {
    uint8_t rsp_code;
    uint8_t cmd_code;
    uint8_t result;
} __attribute__((packed)) nrf_dfu_buttonless_rsp_t;

typedef struct {
  uint16_t value_handle;       /**< Handle to the characteristic value. */
  uint16_t user_desc_handle;   /**< Handle to the User Description descriptor. */
  uint16_t cccd_handle;        /**< Handle to the Client Characteristic Configuration Descriptor. */
  uint16_t sccd_handle;        /**< Handle to the Server Characteristic Configuration Descriptor. */    
} nrf_dfu_cha_handles_t;

typedef struct {
    hci_con_handle_t con_handle;
    nrf_dfu_cha_handles_t dfu_ctrl_pt_handles;
    nrf_dfu_cha_handles_t dfu_pkt_handles;
    nrf_dfu_cha_handles_t dfu_buttonless_handles;
} nrf_dfu_ble_t;

/* API_START */

/**
 * @text The NRF DFU Service is implementation of the Nordic DFU profile.
 *
 * To use with your application, add `#import <nordic_dfu_service.gatt` to your .gatt file
 * and call all functions below. All strings and blobs need to stay valid after calling the functions.
 */

/**
 * @brief Init NRF DFU Service Server with ATT DB
 * @param packet_handler for events and data from dfu controller
 */
uint32_t nrf_dfu_ble_init(nrf_dfu_observer_t observer);

/* API_END */

#if defined __cplusplus
}
#endif

#endif

