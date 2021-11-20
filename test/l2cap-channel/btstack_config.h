//
// btstack_config.h for most tests
//

#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H

// Port related features
#define HAVE_BTSTACK_STDIN
#define HAVE_MALLOC
#define HAVE_POSIX_FILE_IO
#define HAVE_POSIX_TIME


// BTstack features that can be enabled
#define ENABLE_CLASSIC
#define ENABLE_BLE
#define ENABLE_LOG_ERROR
#define ENABLE_LOG_INFO
#define ENABLE_PRINTF_HEXDUMP

#define ENABLE_LE_CENTRAL
#define ENABLE_LE_PERIPHERAL
#define ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
#define ENABLE_L2CAP_ENHANCED_CREDIT_BASED_FLOW_CONTROL_MODE

// for ready-to-use hci channels
#define FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION

// BTstack configuration. buffers, sizes, ...
#define HCI_ACL_PAYLOAD_SIZE 52
#define HCI_INCOMING_PRE_BUFFER_SIZE 4

#endif
