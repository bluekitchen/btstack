/*
 *  hci_cmds.h
 *
 *  Created by Matthias Ringwald on 7/23/09.
 */

#pragma once

#include <stdint.h>
#include <stdarg.h>
#include <string.h>

// calculate combined ogf/ocf value
#define OPCODE(ogf, ocf) (ocf | ogf << 10)

// get HCI CMD OGF
#define READ_CMD_OGF(buffer) (buffer[1] >> 2)
#define READ_CMD_OCF(buffer) ((buffer[1] & 0x03) << 8 | buffer[0])

// OGFs
#define OGF_LINK_CONTROL 0x01
#define OGF_CONTROLLER_BASEBAND 0x03
#define OGF_INFORMATIONAL_PARAMETERS 0x04
#define OGF_BTSTACK 0x3d
#define OGF_VENDOR  0x3f

// cmds for BTstack
#define HCI_BTSTACK_GET_STATE                              0x01


/** 
 * compact HCI Command packet description
 */
 typedef struct {
    uint16_t    opcode;
    const char *format;
} hci_cmd_t;

// create and send hci command packets based on a template and a list of parameters
uint16_t hci_create_cmd(uint8_t *hci_cmd_buffer, hci_cmd_t *cmd, ...);
uint16_t hci_create_cmd_internal(uint8_t *hci_cmd_buffer, hci_cmd_t *cmd, va_list argptr);

// HCI Commands - see hci_cmds.c for info on parameters
extern hci_cmd_t hci_inquiry;
extern hci_cmd_t hci_inquiry_cancel;
extern hci_cmd_t hci_link_key_request_negative_reply;
extern hci_cmd_t hci_pin_code_request_reply;
extern hci_cmd_t hci_set_event_mask;
extern hci_cmd_t hci_reset;
extern hci_cmd_t hci_create_connection;
extern hci_cmd_t hci_host_buffer_size;
extern hci_cmd_t hci_write_authentication_enable;
extern hci_cmd_t hci_write_local_name;
extern hci_cmd_t hci_write_page_timeout;
extern hci_cmd_t hci_write_class_of_device;
extern hci_cmd_t hci_remote_name_request;
extern hci_cmd_t hci_remote_name_request_cancel;
extern hci_cmd_t hci_read_bd_addr;
extern hci_cmd_t hci_delete_stored_link_key;
extern hci_cmd_t hci_write_scan_enable;
extern hci_cmd_t hci_accept_connection_request;
extern hci_cmd_t hci_write_inquiry_mode;
extern hci_cmd_t hci_write_extended_inquiry_response;
extern hci_cmd_t hci_write_simple_pairing_mode;

// BTSTACK client/server commands - see hci.c for info on parameters
extern hci_cmd_t hci_get_btstack_state;