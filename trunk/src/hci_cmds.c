/*
 * Copyright (C) 2009 by Matthias Ringwald
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 *  hci_cmds.c
 *
 *  Created by Matthias Ringwald on 7/23/09.
 */

#include <btstack/hci_cmds.h>
#include "hci.h"
#include <string.h>

// calculate combined ogf/ocf value
#define OPCODE(ogf, ocf) (ocf | ogf << 10)

/**
 * construct HCI Command based on template
 *
 * Format:
 *   1,2,3,4: one to four byte value
 *   H: HCI connection handle
 *   B: Bluetooth Baseband Address (BD_ADDR)
 *   P: 16 byte Pairing code
 *   N: Name up to 248 chars 
 *   E: Extended Inquiry Result
 */
uint16_t hci_create_cmd_internal(uint8_t *hci_cmd_buffer, hci_cmd_t *cmd, va_list argptr){
    
    hci_cmd_buffer[0] = cmd->opcode & 0xff;
    hci_cmd_buffer[1] = cmd->opcode >> 8;
    int pos = 3;
    
    const char *format = cmd->format;
    uint16_t word;
    uint32_t longword;
    uint8_t * ptr;
    while (*format) {
        switch(*format) {
            case '1': //  8 bit value
            case '2': // 16 bit value
            case 'H': // hci_handle
                word = va_arg(argptr, int);  // minimal va_arg is int: 2 bytes on 8+16 bit CPUs
                hci_cmd_buffer[pos++] = word & 0xff;
                if (*format == '2') {
                    hci_cmd_buffer[pos++] = word >> 8;
                } else if (*format == 'H') {
                    // TODO implement opaque client connection handles
                    //      pass module handle for now
                    hci_cmd_buffer[pos++] = word >> 8;
                } 
                break;
            case '3':
            case '4':
                longword = va_arg(argptr, uint32_t);
                // longword = va_arg(argptr, int);
                hci_cmd_buffer[pos++] = longword;
                hci_cmd_buffer[pos++] = longword >> 8;
                hci_cmd_buffer[pos++] = longword >> 16;
                if (*format == '4'){
                    hci_cmd_buffer[pos++] = longword >> 24;
                }
                break;
            case 'B': // bt-addr
                ptr = va_arg(argptr, uint8_t *);
                hci_cmd_buffer[pos++] = ptr[5];
                hci_cmd_buffer[pos++] = ptr[4];
                hci_cmd_buffer[pos++] = ptr[3];
                hci_cmd_buffer[pos++] = ptr[2];
                hci_cmd_buffer[pos++] = ptr[1];
                hci_cmd_buffer[pos++] = ptr[0];
                break;
            case 'P': // 16 byte PIN code or link key
                ptr = va_arg(argptr, uint8_t *);
                memcpy(&hci_cmd_buffer[pos], ptr, 16);
                pos += 16;
                break;
            case 'N': // UTF-8 string, null terminated
                ptr = va_arg(argptr, uint8_t *);
                memcpy(&hci_cmd_buffer[pos], ptr, 248);
                pos += 248;
                break;
            case 'E': // Extended Inquiry Information 240 octets
                ptr = va_arg(argptr, uint8_t *);
                memcpy(&hci_cmd_buffer[pos], ptr, 240);
                pos += 240;
                break;
            default:
                break;
        }
        format++;
    };
    hci_cmd_buffer[2] = pos - 3;
    return pos;
}

/**
 * construct HCI Command based on template
 *
 * mainly calls hci_create_cmd_internal
 */
uint16_t hci_create_cmd(uint8_t *hci_cmd_buffer, hci_cmd_t *cmd, ...){
    va_list argptr;
    va_start(argptr, cmd);
    uint16_t len = hci_create_cmd_internal(hci_cmd_buffer, cmd, argptr);
    va_end(argptr);
    return len;
}


/**
 *  Link Control Commands 
 */
hci_cmd_t hci_inquiry = {
OPCODE(OGF_LINK_CONTROL, 0x01), "311"
// LAP, Inquiry length, Num_responses
};
hci_cmd_t hci_inquiry_cancel = {
OPCODE(OGF_LINK_CONTROL, 0x02), ""
// no params
};
hci_cmd_t hci_create_connection = {
OPCODE(OGF_LINK_CONTROL, 0x05), "B21121"
// BD_ADDR, Packet_Type, Page_Scan_Repetition_Mode, Reserved, Clock_Offset, Allow_Role_Switch
};

hci_cmd_t hci_disconnect = {
OPCODE(OGF_LINK_CONTROL, 0x06), "H1"
// Handle, Reason: 0x05, 0x13-0x15, 0x1a, 0x29
// see Errors Codes in BT Spec Part D
};
hci_cmd_t hci_create_connection_cancel = {
OPCODE(OGF_LINK_CONTROL, 0x08), "B"
// BD_ADDR
};
hci_cmd_t hci_accept_connection_request = {
OPCODE(OGF_LINK_CONTROL, 0x09), "B1"
// BD_ADDR, Role: become master, stay slave
};
hci_cmd_t hci_link_key_request_reply = {
OPCODE(OGF_LINK_CONTROL, 0x0b), "BP"
// BD_ADDR, LINK_KEY
};
hci_cmd_t hci_link_key_request_negative_reply = {
OPCODE(OGF_LINK_CONTROL, 0x0c), "B"
// BD_ADDR
};
hci_cmd_t hci_pin_code_request_reply = {
OPCODE(OGF_LINK_CONTROL, 0x0d), "B1P"
// BD_ADDR, pin length, PIN: c-string
};
hci_cmd_t hci_authentication_requested = {
OPCODE(OGF_LINK_CONTROL, 0x11), "H"
// Handle
};
hci_cmd_t hci_remote_name_request = {
OPCODE(OGF_LINK_CONTROL, 0x19), "B112"
// BD_ADDR, Page_Scan_Repetition_Mode, Reserved, Clock_Offset
};
hci_cmd_t hci_remote_name_request_cancel = {
OPCODE(OGF_LINK_CONTROL, 0x1A), "B"
// BD_ADDR
};

/**
 *  Link Policy Commands 
 */
hci_cmd_t hci_qos_setup = {
    OPCODE(OGF_LINK_POLICY, 0x07), "H114444"
    // handle, flags, service_type, token rate (bytes/s), peak bandwith (bytes/s),
    // latency (us), delay_variation (us)
};
hci_cmd_t hci_read_link_policy_settings = {
    OPCODE(OGF_LINK_POLICY, 0x0c), "H"
    // handle 
};
hci_cmd_t hci_write_link_policy_settings = {
    OPCODE(OGF_LINK_POLICY, 0x0d), "H2"
    // handlee, settings
};

/**
 *  Controller & Baseband Commands 
 */
hci_cmd_t hci_set_event_mask = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x01), "44"
// event_mask lower 4 octets, higher 4 bytes
};
hci_cmd_t hci_reset = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x03), ""
// no params
};
hci_cmd_t hci_delete_stored_link_key = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x12), "B1"
// BD_ADDR, Delete_All_Flag
};
hci_cmd_t hci_write_local_name = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x13), "N"
// Local name (UTF-8, Null Terminated, max 248 octets)
};
hci_cmd_t hci_write_page_timeout = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x18), "2"
// Page_Timeout * 0.625 ms
};
hci_cmd_t hci_write_scan_enable = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x1A), "1"
// Scan_enable: no, inq, page, inq+page
};
hci_cmd_t hci_write_authentication_enable = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x20), "1"
// Authentication_Enable
};
hci_cmd_t hci_write_class_of_device = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x24), "3"
// Class of Device
};
hci_cmd_t hci_host_buffer_size = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x33), "2122"
// Host_ACL_Data_Packet_Length:, Host_Synchronous_Data_Packet_Length:, Host_Total_Num_ACL_Data_Packets:, Host_Total_Num_Synchronous_Data_Packets:
};
hci_cmd_t hci_read_link_supervision_timeout = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x36), "H"
// handle
};
hci_cmd_t hci_write_link_supervision_timeout = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x33), "H2"
// handle, Range for N: 0x0001 – 0xFFFF Time (Range: 0.625ms – 40.9 sec)
};
hci_cmd_t hci_write_inquiry_mode = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x45), "1"
// Inquiry mode: 0x00 = standard, 0x01 = with RSSI, 0x02 = extended
};
hci_cmd_t hci_write_extended_inquiry_response = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x52), "1E"
// FEC_Required, Exstended Inquiry Response
};

hci_cmd_t hci_write_simple_pairing_mode = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x56), "1"
// mode: 0 = off, 1 = on
};

hci_cmd_t hci_read_bd_addr = {
OPCODE(OGF_INFORMATIONAL_PARAMETERS, 0x09), ""
// no params
};

// BTstack commands

hci_cmd_t btstack_get_state = {
OPCODE(OGF_BTSTACK, BTSTACK_GET_STATE), ""
// no params -> 
};

hci_cmd_t btstack_set_power_mode = {
OPCODE(OGF_BTSTACK, BTSTACK_SET_POWER_MODE), "1"
// mode: 0 = off, 1 = on
};

hci_cmd_t btstack_set_acl_capture_mode = {
OPCODE(OGF_BTSTACK, BTSTACK_SET_ACL_CAPTURE_MODE), "1"
// mode: 0 = off, 1 = on
};

hci_cmd_t btstack_get_version = {
OPCODE(OGF_BTSTACK, BTSTACK_GET_VERSION), ""
};

hci_cmd_t btstack_get_system_bluetooth_enabled = {
OPCODE(OGF_BTSTACK, BTSTACK_GET_SYSTEM_BLUETOOTH_ENABLED), ""
};

hci_cmd_t btstack_set_system_bluetooth_enabled = {
OPCODE(OGF_BTSTACK, BTSTACK_SET_SYSTEM_BLUETOOTH_ENABLED), "1"
};

hci_cmd_t l2cap_create_channel = {
OPCODE(OGF_BTSTACK, L2CAP_CREATE_CHANNEL), "B2"
// @param bd_addr(48), psm (16)
};

hci_cmd_t l2cap_disconnect = {
OPCODE(OGF_BTSTACK, L2CAP_DISCONNECT), "21"
// @param channel(16), reason(8)
};


