// config.h created by configure for BTstack  Tue Jun 4 23:10:20 CEST 2013

#ifndef __BTSTACK_CONFIG
#define __BTSTACK_CONFIG

#define HAVE_TRANSPORT_USB
#define HAVE_BLE
#define USE_POSIX_RUN_LOOP
#define HAVE_SDP
#define HAVE_RFCOMM
#define REMOTE_DEVICE_DB remote_device_db_iphone
#define HAVE_SO_NOSIGPIPE
#define HAVE_TIME
#define HAVE_MALLOC
#define HAVE_BZERO
#define SDP_DES_DUMP
#define ENABLE_LOG_INFO 
#define ENABLE_LOG_ERROR
#define HCI_INCOMING_PRE_BUFFER_SIZE 14 // sizeof benep heade, avoid memcpy
#define HCI_ACL_PAYLOAD_SIZE (1691 + 4)
#define HAVE_HCI_DUMP
#define SDP_DES_DUMP

#define HAVE_SCO
#define HAVE_SCO_OVER_HCI

#endif
