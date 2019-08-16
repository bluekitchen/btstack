//
// btstack_config.h for esp32 port
//

#ifndef __BTSTACK_CONFIG
#define __BTSTACK_CONFIG

// Port related features
#define HAVE_EMBEDDED_TIME_MS
#define HAVE_MALLOC
#define HAVE_BTSTACK_STDIN
#define HAVE_FREERTOS_TASK_NOTIFICATIONS
#define HAVE_FREERTOS_INCLUDE_PREFIX

// BTstack features that can be enabled
#define ENABLE_BLE
#define ENABLE_CLASSIC
#define ENABLE_LE_PERIPHERAL
#define ENABLE_LE_CENTRAL
#define ENABLE_LE_DATA_CHANNELS
#define ENABLE_LE_DATA_LENGTH_EXTENSION
#define ENABLE_LE_SECURE_CONNECTIONS
#define ENABLE_ATT_DELAYED_RESPONSE
// ESP32 supports ECDH HCI Commands, but micro-ecc lib is already provided anyway
#define ENABLE_MICRO_ECC_FOR_LE_SECURE_CONNECTIONS
#define ENABLE_SCO_OVER_HCI
#define ENABLE_LOG_ERROR
#define ENABLE_LOG_INFO
// #define ENABLE_LOG_DEBUG

// BTstack configuration. buffers, sizes, ...
#define HCI_ACL_PAYLOAD_SIZE (1691 + 4)

// HCI Controller to Host Flow Control
#define ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL

// Interal ring buffer: 21 kB
#define HCI_HOST_ACL_PACKET_NUM 20
#define HCI_HOST_ACL_PACKET_LEN 1024
#define HCI_HOST_SCO_PACKET_NUM 10
#define HCI_HOST_SCO_PACKET_LEN 60

// Link Key DB and LE Device DB using TLV on top of Flash Sector interface
#define NVM_NUM_LINK_KEYS 16
#define NVM_NUM_DEVICE_DB_ENTRIES 16


// Mesh Configuration
#define ENABLE_MESH
#define ENABLE_MESH_ADV_BEARER
#define ENABLE_MESH_GATT_BEARER
#define ENABLE_MESH_PB_ADV
#define ENABLE_MESH_PB_GATT
#define ENABLE_MESH_PROXY_SERVER
#define ENABLE_MESH_PROVISIONER

#define MAX_NR_MESH_TRANSPORT_KEYS    16
#define MAX_NR_MESH_VIRTUAL_ADDRESSES 16
#define MAX_NR_MESH_SUBNETS            2

// allow for one NetKey update
#define MAX_NR_MESH_NETWORK_KEYS      (MAX_NR_MESH_SUBNETS+1)

#endif
