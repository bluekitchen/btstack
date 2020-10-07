//
// btstack_config.h for most tests
//

#ifndef __BTSTACK_CONFIG
#define __BTSTACK_CONFIG

// Port related features
#define HAVE_MALLOC
#define HAVE_ASSERT
#define HAVE_POSIX_TIME
#define HAVE_POSIX_FILE_IO
#define HAVE_BTSTACK_STDIN
#define HAVE_ASSERT


// BTstack features that can be enabled
#define ENABLE_BLE
// #define ENABLE_LOG_DEBUG
#define ENABLE_GATT_CLIENT_PAIRING
#define ENABLE_LOG_ERROR
#define ENABLE_LOG_INFO 
#define ENABLE_LE_DATA_CHANNELS
#define ENABLE_LE_SIGNED_WRITE
#define ENABLE_LE_PERIPHERAL
#define ENABLE_LE_CENTRAL
#define ENABLE_LE_SECURE_CONNECTIONS
#define ENABLE_MICRO_ECC_FOR_LE_SECURE_CONNECTIONS
#define ENABLE_BTP

// BTstack configuration. buffers, sizes, ...
#define HCI_ACL_PAYLOAD_SIZE 1024
#define HCI_INCOMING_PRE_BUFFER_SIZE 6
#define NVM_NUM_LINK_KEYS 2
#define NVM_NUM_DEVICE_DB_ENTRIES 4

#endif
