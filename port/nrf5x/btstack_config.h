//
// btstack_config.h for WICED port
//

#ifndef __BTSTACK_CONFIG
#define __BTSTACK_CONFIG

// Port related features
#define HAVE_EMBEDDED_TIME_MS

// BTstack features that can be enabled
#define ENABLE_BLE
#define ENABLE_CLASSIC
#define ENABLE_LE_PERIPHERAL
#define ENABLE_LE_CENTRAL
#define ENABLE_LOG_INFO
#define ENABLE_LOG_ERROR

// BTstack configuration. buffers, sizes, ...
#define HCI_ACL_PAYLOAD_SIZE 52
#define MAX_NR_HCI_CONNECTIONS 1
#define MAX_NR_L2CAP_SERVICES  12
#define MAX_NR_L2CAP_CHANNELS  1
#define MAX_NR_GATT_CLIENTS 1
#define MAX_NR_WHITELIST_ENTRIES 1
#define MAX_NR_SM_LOOKUP_ENTRIES 3
#define MAX_NR_LE_DEVICE_DB_ENTRIES 1

#endif
