#ifndef __SERIAL_PORT_H__
#define __SERIAL_PORT_H__

#include <windows.h>
#include <stdio.h>
#include <direct.h>
#include "assert.h"
#include "type.h"

#define BT_HCI_LOG_CMD     0x01
#define BT_HCI_LOG_ACL_OUT 0x02
#define BT_HCI_LOG_ACL_IN  0x04
#define BT_HCI_LOG_EVT     0x08
typedef uint8_t bt_hci_log_type_t;

#define BT_HCI_CMD        0x01
#define BT_HCI_ACL        0x02
#define BT_HCI_EVT        0x08
typedef uint8_t bt_hci_data_type_t;

#define HOST_TO_CONTROLLER 0
#define CONTROLLER_TO_HOST 1
typedef uint8_t bt_hci_log_direction_t;

#pragma pack (1)
typedef struct {
    uint16_t header;
    bt_hci_log_type_t type;
    uint16_t log_length;
    uint8_t *log;
} hci_log_struct_t;
#pragma pack ()

typedef enum {
    HCI_LOG_PARSE_STATE_W4_HEADER1 = 0,
    HCI_LOG_PARSE_STATE_W4_HEADER2,
    HCI_LOG_PARSE_STATE_W4_TYPE,
    HCI_LOG_PARSE_STATE_W4_LENGTH,
    HCI_LOG_PARSE_STATE_W4_LOG,
    HCI_LOG_PARSE_STATE_W4_CHECK_SUM
} hci_log_parse_state_t;

#define GET_LOW_BYTE(x)  ((x) & 0xFF)
#define GET_HIGH_BYTE(x) (((x) >> 8) & 0xFF)

#define HCI_LOG_HEADER_LENGTH 5

int serial_port_open(LPCWSTR COMx, int BaudRate);
int serial_port_write(char lpOutBuffer[]);
int serial_port_read(char *ptr, unsigned int buffer_size, unsigned int *acture_read_length);
void serial_port_close(void);
extern BOOL mutex_lock();
extern BOOL mutex_unlock();
#endif
