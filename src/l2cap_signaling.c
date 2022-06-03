/*
 * Copyright (C) 2014 BlueKitchen GmbH
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
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

#define BTSTACK_FILE__ "l2cap_signaling.c"

/*
 *  l2cap_signaling.h
 */

#include "l2cap_signaling.h"
#include "btstack_config.h"
#include "btstack_debug.h"
#include "hci.h"

#include <string.h>

uint16_t l2cap_create_signaling_packet(uint8_t * acl_buffer, hci_con_handle_t handle, uint8_t pb_flags, uint16_t cid, L2CAP_SIGNALING_COMMANDS cmd, uint8_t identifier, va_list argptr){

    static const char *l2cap_signaling_commands_format[] = {
            "2D",    // 0x01 command reject: reason {cmd not understood (0), sig MTU exceeded (2:max sig MTU), invalid CID (4:req CID)}, data len, data
            "22",    // 0x02 connection request: psm, source cid
            "2222",  // 0x03 connection response: destination cid, source cid, result, status
            "22D",   // 0x04 config request: destination cid, flags, configuration options
            "222D",  // 0x05 config response: source cid, flags, result, configuration options
            "22",    // 0x06 disconnection request: destination cid, source cid
            "22",    // 0x07 disconnection response: destination cid, source CID
            "D",     // 0x08 echo request: data
            "D",     // 0x09 echo response: data
            "2",     // 0x0a information request: info type {1=Connectionless MTU, 2=Extended features supported}
            "22D",   // 0x0b information response: info type, Result, Data
            NULL,    // 0x0c non-supported AMP command
            NULL,    // 0x0d non-supported AMP command
            NULL,    // 0x0e non-supported AMP command
            NULL,    // 0x0f non-supported AMP command
            NULL,    // 0x10 non-supported AMP command
            NULL,    // 0x11 non-supported AMP command
            "2222",  // 0x12 connection parameter update request: interval min, interval max, slave latency, timeout multiplier
            "2",     // 0x13 connection parameter update response: result
            "22222", // 0X14 le credit-based connection request: simplified psm, source cid, mtu, mps, initial credits
            "22222", // 0x15 le credit-based connection response: dest cid, mtu, mps, initial credits, result
            "22",    // 0x16 le flow control credit indication: source cid, credits
            "2222C", // 0x17 l2cap credit-based connection request: simplified psm, mtu, mps, initial credits, source cid[0]
            "2222C", // 0x18 l2cap credit-based connection request: mtu, mps, initial credits, result, destinations cid[0]
            "22C",   // 0x19 l2cap credit-based reconfigure request: mu, mps, destination cid[0]
            "2",     // 0x1a l2cap credit-based reconfigure response: result
#ifdef UNIT_TEST
            "M",     // invalid format for unit testing
#endif           
    };

    btstack_assert(0 < cmd);
    uint8_t cmd_index = (uint8_t) cmd;
    static const uint8_t num_l2cap_commands = (uint8_t) (sizeof(l2cap_signaling_commands_format) / sizeof(const char *));
    btstack_assert(cmd_index <= num_l2cap_commands);
    UNUSED(num_l2cap_commands);

    const char *format = l2cap_signaling_commands_format[cmd_index -1u];
    btstack_assert(format != NULL);

    // 0 - Connection handle : PB=pb : BC=00 
    little_endian_store_16(acl_buffer, 0u, handle | (pb_flags << 12u) | (0u << 14u));
    // 6 - L2CAP channel = 1
    little_endian_store_16(acl_buffer, 6, cid);
    // 8 - Code
    acl_buffer[8] = cmd_index;
    // 9 - id (!= 0 sequentially)
    acl_buffer[9] = identifier;
    
    // 12 - L2CAP signaling parameters
    uint16_t pos = 12;
    uint16_t word;
    uint8_t  * ptr_u8;
    uint16_t * ptr_u16;
    while (*format) {
        switch(*format) {
            case '2': // 16 bit value
                word = va_arg(argptr, int);         // LCOV_EXCL_BR_LINE
                // minimal va_arg is int: 2 bytes on 8+16 bit CPUs
                acl_buffer[pos++] = word & 0xffu;
                acl_buffer[pos++] = word >> 8;
                break;
            case 'C': // list of cids != zero, last one is 0xffff
                ptr_u16  = va_arg(argptr, uint16_t *);   // LCOV_EXCL_BR_LINE
                while (*ptr_u16 != 0xffff){
                    little_endian_store_16(acl_buffer, pos, *ptr_u16);
                    ptr_u16++;
                    pos += 2;
                }
                break;
            case 'D': // variable data. passed: len, ptr
                word = va_arg(argptr, int);         // LCOV_EXCL_BR_LINE
                ptr_u8  = va_arg(argptr, uint8_t *);   // LCOV_EXCL_BR_LINE
                (void)memcpy(&acl_buffer[pos], ptr_u8, word);
                pos += word;
                break;
            default:
                btstack_unreachable();
                break;
        }
        format++;
    };
    va_end(argptr);
    
    // Fill in various length fields: it's the number of bytes following for ACL length and l2cap parameter length
    // - the l2cap payload length is counted after the following channel id (only payload) 
    
    // 2 - ACL length
    little_endian_store_16(acl_buffer, 2u,  pos - 4u);
    // 4 - L2CAP packet length
    little_endian_store_16(acl_buffer, 4u,  pos - 6u - 2u);
    // 10 - L2CAP signaling parameter length
    little_endian_store_16(acl_buffer, 10u, pos - 12u);
    
    return pos;
}
