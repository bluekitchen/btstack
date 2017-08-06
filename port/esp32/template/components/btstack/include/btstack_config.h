//
// btstack_config.h for esp32 port
//

#ifndef __BTSTACK_CONFIG
#define __BTSTACK_CONFIG

// Port related features
#define HAVE_EMBEDDED_TIME_MS
#define HAVE_MALLOC
#define HAVE_BTSTACK_STDIN

// BTstack features that can be enabled
#define ENABLE_BLE
#define ENABLE_CLASSIC
#define ENABLE_LE_PERIPHERAL
#define ENABLE_LE_CENTRAL
#define ENABLE_LOG_ERROR
#define ENABLE_LOG_INFO
// #define ENABLE_LOG_DEBUG

// BTstack configuration. buffers, sizes, ...
#define HCI_ACL_PAYLOAD_SIZE (1691 + 4)
#define MAX_NR_LE_DEVICE_DB_ENTRIES 4

// HCI Controller to Host Flow Control
//
// Needed on the ESP32, but not working yet
// see https://github.com/espressif/esp-idf/issues/480
//
// #define ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL

// Interal ring buffer
#define HCI_HOST_ACL_PACKET_NUM 10
#define HCI_HOST_ACL_PACKET_LEN 1024
#define HCI_HOST_SCO_PACKET_NUM 10
#define HCI_HOST_SCO_PACKET_LEN 60
#endif

