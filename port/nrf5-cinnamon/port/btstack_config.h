//
// btstack_config.h for nrf5-cinnamon port
//
// Documentation: https://bluekitchen-gmbh.com/btstack/#how_to/
//
//

#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H

// Port related features
#define HAVE_EM9304_PATCH_CONTAINER
#define HAVE_EMBEDDED_TIME_MS

// BTstack features that can be enabled
#define ENABLE_BLE
#define ENABLE_LE_CENTRAL
#define ENABLE_LE_DATA_LENGTH_EXTENSION
#define ENABLE_LE_PERIPHERAL
#define ENABLE_LOG_ERROR
#define ENABLE_LOG_INFO
#define ENABLE_PRINTF_HEXDUMP
#define ENABLE_SEGGER_RTT
#define ENABLE_BTSTACK_ASSERT

// BTstack configuration. buffers, sizes, ...
#define HCI_ACL_PAYLOAD_SIZE 100
#define MAX_NR_GATT_CLIENTS 1
#define MAX_NR_HCI_CONNECTIONS 1
#define MAX_NR_L2CAP_CHANNELS 1
#define MAX_NR_L2CAP_SERVICES 1
#define MAX_NR_SM_LOOKUP_ENTRIES 3
#define MAX_NR_WHITELIST_ENTRIES 1

// LE Device DB using TLV on top of Flash Sector interface
#define NVM_NUM_DEVICE_DB_ENTRIES 16

// GPIO debugging on nRF52832 DK, P6: 17-20 are LEDs 1-4, 13-16 Butttons 1-4
#define DEBUG_PIN_HF_CLOCK  0
#define DEBUG_PIN_ADDRESS   1
#define DEBUG_PIN_RX       21
#define DEBUG_PIN_TX        5
#define DEBUG_PIN_RADIO_IRQ 6

#endif
