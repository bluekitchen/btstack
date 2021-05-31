//
// btstack_config.h for most tests
//

#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H

// Port related features
#define HAVE_BTSTACK_STDIN
#define HAVE_POSIX_FILE_IO
#define HAVE_POSIX_TIME
#define HAVE_MALLOC

// BTstack features that can be enabled
#define ENABLE_BLE
#define ENABLE_CLASSIC
#define ENABLE_LOG_ERROR
#define ENABLE_LOG_INFO
#define ENABLE_PRINTF_HEXDUMP

// BTstack configuration. buffers, sizes, ...
#define HCI_ACL_PAYLOAD_SIZE 1024
#define HCI_INCOMING_PRE_BUFFER_SIZE 6

// test hook to mock malloc
#define ENABLE_MALLOC_TEST

#endif
