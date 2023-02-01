//
// btstack_config.h
//
//  Made for BlueKitchen by OneWave with <3
//      Author: ftrefou@onewave.io
//

#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H

// Port related features
#define HAVE_EMBEDDED_TIME_MS
#define HAVE_FREERTOS_TASK_NOTIFICATIONS

// BTstack features that can be enabled
#define ENABLE_BLE
#define ENABLE_LE_CENTRAL
#define ENABLE_LE_PERIPHERAL
#define ENABLE_LE_SECURE_CONNECTIONS
#define ENABLE_LOG_ERROR
#define ENABLE_LOG_INFO
#define ENABLE_MICRO_ECC_P256
#define ENABLE_PRINTF_HEXDUMP
#define ENABLE_SOFTWARE_AES128
#define ENABLE_TLV_FLASH_EXPLICIT_DELETE_FIELD
#define ENABLE_LE_SET_ADV_PARAMS_ON_RANDOM_ADDRESS_CHANGE

// BTstack configuration. buffers, sizes, ...
#define HCI_ACL_PAYLOAD_SIZE (512 + 4) //Max official att size + l2cap header size
#define MAX_NR_GATT_CLIENTS 1
#define MAX_NR_HCI_CONNECTIONS 1
#define MAX_NR_HIDS_CLIENTS 1
#define MAX_NR_L2CAP_CHANNELS  3
#define MAX_NR_L2CAP_SERVICES  2
#define MAX_NR_LE_DEVICE_DB_ENTRIES 1
#define MAX_NR_SM_LOOKUP_ENTRIES 3
#define MAX_NR_WHITELIST_ENTRIES 1

#define MAX_ATT_DB_SIZE 350

// LE Device DB using TLV
#define NVM_NUM_DEVICE_DB_ENTRIES 16

#endif
