//
// btstack_config.h for generic POSIX H4 port
//

#ifndef __BTSTACK_CONFIG
#define __BTSTACK_CONFIG

// Port related features
#define HAVE_MALLOC
#define HAVE_POSIX_FILE_IO
#define HAVE_BTSTACK_STDIN
#define HAVE_POSIX_TIME

// BTstack features that can be enabled
#define ENABLE_BLE
#define ENABLE_CLASSIC
#define ENABLE_HFP_WIDE_BAND_SPEECH
#define ENABLE_LE_CENTRAL
#define ENABLE_LE_PERIPHERAL
#define ENABLE_LE_SECURE_CONNECTIONS
#define ENABLE_MICRO_ECC_FOR_LE_SECURE_CONNECTIONS
#define ENABLE_LE_DATA_CHANNELS
#define ENABLE_LE_DATA_LENGTH_EXTENSION
#define ENABLE_LOG_ERROR
#define ENABLE_LOG_INFO 
#define ENABLE_SCO_OVER_HCI
#define ENABLE_SDP_DES_DUMP
// #define ENABLE_EHCILL

// BTstack configuration. buffers, sizes, ...
#define HCI_INCOMING_PRE_BUFFER_SIZE 14 // sizeof benep heade, avoid memcpy
#define HCI_ACL_PAYLOAD_SIZE (1691 + 4)

#endif

