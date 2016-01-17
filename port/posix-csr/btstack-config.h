// config.h created by configure for BTstack  Tue Jun 4 23:10:20 CEST 2013

#ifndef __BTSTACK_CONFIG
#define __BTSTACK_CONFIG

#define ENABLE_LOG_ERROR
#define ENABLE_LOG_INFO 
#define HAVE_BLE
#define HAVE_BZERO
#define HAVE_HCI_DUMP
#define HAVE_MALLOC
#define HAVE_RFCOMM
#define HAVE_SDP
#define HAVE_SO_NOSIGPIPE
#define HAVE_TIME
#define HAVE_SCO
#define HAVE_SCO_OVER_HCI
#define HCI_ACL_PAYLOAD_SIZE (1691 + 4)
#define HCI_INCOMING_PRE_BUFFER_SIZE 14 // sizeof BNEP header, avoid memcpy
#define REMOTE_DEVICE_DB remote_device_db_iphone
#define SDP_DES_DUMP
#define SDP_DES_DUMP
#define USE_POSIX_RUN_LOOP

#endif
