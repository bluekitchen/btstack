//
// btstack_config.h for mRF5-Zephyr port
//
// Documentation: https://bluekitchen-gmbh.com/btstack/#how_to/
//

#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H

// Port related features

// BTstack features that can be enabled
#define ENABLE_BLE
#define ENABLE_LE_CENTRAL
#define ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
#define ENABLE_LE_DATA_LENGTH_EXTENSION
#define ENABLE_LE_EXTENDED_ADVERTISING
#define ENABLE_LE_ISOCHRONOUS_STREAMS
#define ENABLE_LE_PERIODIC_ADVERTISING
#define ENABLE_LE_PERIPHERAL
#define ENABLE_LOG_ERROR
#define ENABLE_LOG_INFO
#define ENABLE_PRINTF_HEXDUMP

// BTstack configuration. buffers, sizes, ...
#define HCI_ACL_PAYLOAD_SIZE 260
#define MAX_NR_GATT_CLIENTS 1
#define MAX_NR_HCI_CONNECTIONS 1
#define MAX_NR_L2CAP_CHANNELS 1
#define MAX_NR_L2CAP_SERVICES 1
#define MAX_NR_SM_LOOKUP_ENTRIES 3
#define MAX_NR_WHITELIST_ENTRIES 1
#define MAX_NR_HCI_ISO_STREAMS 10

#define MAX_NR_BNEP_CHANNELS 0
#define MAX_NR_BNEP_SERVICES 0
#define MAX_NR_BTSTACK_LINK_KEY_DB_MEMORY_ENTRIES 0
#define MAX_NR_HFP_CONNECTIONS 0
#define MAX_NR_LE_DEVICE_DB_ENTRIES 1
#define MAX_NR_RFCOMM_CHANNELS 0
#define MAX_NR_RFCOMM_MULTIPLEXERS 0
#define MAX_NR_RFCOMM_SERVICES 0
#define MAX_NR_SERVICE_RECORD_ITEMS 0

// hack to fix usage of hci_init in zephry
#define hci_init btstack_hci_init

// LE Device DB using TLV
#define NVM_NUM_DEVICE_DB_ENTRIES 16

#endif
