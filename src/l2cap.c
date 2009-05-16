/*
 *  l2cap.c
 *
 *  Logical Link Control and Adaption Protocl (L2CAP)
 *
 *  Created by Matthias Ringwald on 5/16/09.
 */

#include "l2cap.h"

#include <stdarg.h>
#include <string.h>

#include <stdio.h>

static char *l2cap_signaling_commands_format[] = {
    "D",    // 0x01 command reject: reason {cmd not understood (0), sig MTU exceeded (2:max sig MTU), invalid CID (4:req CID)}, data len, data
    "22",   // 0x02 connection request: PSM, Source CID
    "2222", // 0x03 connection response: Dest CID, Source CID, Result, Status
    "22D",  // 0x04 config request: Dest CID, Flags, Configuration options
    "222D", // 0x05 config response: Source CID, Flags, Result, Configuration options
    "22",   // 0x06 disconection request: Dest CID, Source CID
    "22",   // 0x07 disconection response: Dest CID, Source CID
    "D",    // 0x08 echo request: Data
    "D",    // 0x09 echo response: Data
    "2",    // 0x0a information request: InfoType {1=Connectionless MTU, 2=Extended features supported}
    "22D",  // 0x0b information response: InfoType, Result, Data
};

static uint8_t * sig_buffer;

uint8_t   sig_seq_nr;
uint16_t  local_cid;

int l2cap_send_signaling_packet(hci_con_handle_t handle, L2CAP_SIGNALING_COMMANDS cmd, uint8_t identifier, ...){

    // 0 - Connection handle : PB=10 : BC=00 
     bt_store_16(sig_buffer, 0, handle | (2 << 12) | (0 << 14));
    // 6 - L2CAP channel = 1
    bt_store_16(sig_buffer, 6, 1);
    // 8 - Code
    sig_buffer[8] = cmd;
    // 9 - id (!= 0 sequentially)
    sig_buffer[9] = identifier;

    // 12 - L2CAP signaling parameters
    uint16_t pos = 12;
    va_list argptr;
    va_start(argptr, identifier);
    const char *format = l2cap_signaling_commands_format[cmd-1];
    uint16_t word;
    uint8_t * ptr;
    while (*format) {
        switch(*format) {
            case '1': //  8 bit value
            case '2': // 16 bit value
                word = va_arg(argptr, int);
                // minimal va_arg is int: 2 bytes on 8+16 bit CPUs
                sig_buffer[pos++] = word & 0xff;
                if (*format == '2') {
                    sig_buffer[pos++] = word >> 8;
                }
                break;
            case 'D': // variable data. passed: len, ptr
                word = va_arg(argptr, int);
                ptr  = va_arg(argptr, uint8_t *);
                memcpy(&sig_buffer[pos], ptr, word);
                pos += word;
                break;
            default:
                break;
        }
        format++;
    };
    va_end(argptr);
    
    // Fill in various length fields: it's the number of bytes following for ACL lenght and l2cap parameter length
    // - the l2cap payload length is counted after the following channel id (only payload) 
    
    // 2 - ACL length
    bt_store_16(sig_buffer, 2,  pos - 4);
    // 4 - L2CAP packet length
    bt_store_16(sig_buffer, 4,  pos - 6 - 2);
    // 10 - L2CAP signaling parameter length
    bt_store_16(sig_buffer, 10, pos - 12);
    
    return hci_send_acl_packet(sig_buffer, pos);
}

void l2cap_init(){
    sig_buffer = malloc( 48 );
    sig_seq_nr = 1;
    local_cid = 0x40;
}