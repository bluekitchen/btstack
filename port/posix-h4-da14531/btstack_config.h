//
// btstack_config.h for POSIX H4 + DA14585 port
//
// Documentation: https://bluekitchen-gmbh.com/btstack/#how_to/
//

#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H

// Port related features
#define HAVE_ASSERT
#define HAVE_BTSTACK_STDIN
#define HAVE_MALLOC
#define HAVE_POSIX_FILE_IO
#define HAVE_POSIX_TIME

// BTstack features that can be enabled
#define ENABLE_BLE
#define ENABLE_LE_CENTRAL
#define ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
#define ENABLE_LE_DATA_LENGTH_EXTENSION
#define ENABLE_LE_PERIPHERAL
#define ENABLE_LE_PRIVACY_ADDRESS_RESOLUTION
#define ENABLE_LE_SECURE_CONNECTIONS
#define ENABLE_LOG_ERROR
#define ENABLE_LOG_INFO
#define ENABLE_MICRO_ECC_FOR_LE_SECURE_CONNECTIONS
#define ENABLE_PRINTF_HEXDUMP

// work around bug in Controller ACL fragmentation
#define ENABLE_LE_LIMIT_ACL_FRAGMENT_BY_MAX_OCTETS

// work around bug in Controller
#define ENABLE_LE_WHITELIST_TOUCH_AFTER_RESOLVING_LIST_UPDATE


// BTstack configuration. buffers, sizes, ...
#define HCI_ACL_PAYLOAD_SIZE (1691 + 4)
#define HCI_INCOMING_PRE_BUFFER_SIZE 14 // sizeof benep heade, avoid memcpy

#define NVM_NUM_DEVICE_DB_ENTRIES      20

// Mesh Configuration
#define ENABLE_MESH
#define ENABLE_MESH_ADV_BEARER
#define ENABLE_MESH_GATT_BEARER
#define ENABLE_MESH_PB_ADV
#define ENABLE_MESH_PB_GATT
#define ENABLE_MESH_PROVISIONER
#define ENABLE_MESH_PROXY_SERVER

#define MAX_NR_MESH_SUBNETS            2
#define MAX_NR_MESH_TRANSPORT_KEYS    16
#define MAX_NR_MESH_VIRTUAL_ADDRESSES 16

// allow for one NetKey update
#define MAX_NR_MESH_NETWORK_KEYS      (MAX_NR_MESH_SUBNETS+1)

#endif

