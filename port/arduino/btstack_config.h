//
// btstack_config.h for Arduino port
//

#ifndef __BTSTACK_CONFIG
#define __BTSTACK_CONFIG

// Port related features
#define HAVE_BZERO
#define HAVE_TIME_MS

// BTstack features that can be enabled
#define ENABLE_BLE
#define ENABLE_CLASSIC
#define ENABLE_LOG_INTO_HCI_DUMP
#define ENABLE_LOG_INFO 
#define ENABLE_LOG_ERROR

// BTstack configuration. buffers, sizes, ...
#define HCI_ACL_PAYLOAD_SIZE 200
#define MAX_NO_BNEP_SERVICES 0
#define MAX_NO_BNEP_CHANNELS 0
#define MAX_NO_GATT_SUBCLIENTS 2
#define MAX_NO_HCI_CONNECTIONS 1
#define MAX_NO_L2CAP_SERVICES  0
#define MAX_NO_L2CAP_CHANNELS  0
#define MAX_NO_RFCOMM_MULTIPLEXERS 0
#define MAX_NO_RFCOMM_SERVICES 0
#define MAX_NO_RFCOMM_CHANNELS 0
#define MAX_NO_DB_MEM_DEVICE_LINK_KEYS  0
#define MAX_NO_DB_MEM_DEVICE_NAMES 0
#define MAX_NO_DB_MEM_SERVICES 0
#define MAX_NO_GATT_CLIENTS 1
#define MAX_ATT_DB_SIZE 200
#define MAX_NO_HFP_CONNECTIONS 0
#define MAX_NO_WHITELIST_ENTRIES 1
#define MAX_NO_SM_LOOKUP_ENTRIES 3
#define MAX_NO_SERVICE_RECORD_ITEMS 1
#endif