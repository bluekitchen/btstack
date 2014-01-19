/*
 * Copyright (C) 2009-2012 by Matthias Ringwald
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
 *  ant_cmds.c
 * 
 *  See http://www.thisisant.com/ for more info on ANT(tm) 
 *
 *  Created by Matthias Ringwald on 2012-11-06.
 */

#include <btstack/ant_cmds.h>

#include <string.h>

#include <btstack/sdp_util.h>
#include "btstack-config.h"
#include "hci.h"

/**
 * construct ANT HCI Command based on template
 *
 * Format:
 *   0: adds a zero byte
 *   1,2,3,4: one to four byte value
 *   D: pointer to 8 bytes of ANT data
 */
uint16_t ant_create_cmd_internal(uint8_t *hci_cmd_buffer, const ant_cmd_t *cmd, va_list argptr){
    
    hci_cmd_buffer[0] = 0xd1;
    hci_cmd_buffer[1] = 0xfd;
    // hci packet lengh 2
    // ant packet length 3
    hci_cmd_buffer[4] = 0x00;
    hci_cmd_buffer[6] = cmd->message_id;
    int pos = 7;
    
    const char *format = cmd->format;
    uint16_t word;
    uint32_t longword;
    uint8_t * ptr;
    while (*format) {
        switch(*format) {
            case '0': // dummy
                hci_cmd_buffer[pos++] = 0;
                break;
            case '1': //  8 bit value
            case '2': // 16 bit value
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
            case 'D': // 8 byte data block
                ptr = va_arg(argptr, uint8_t *);
                memcpy(&hci_cmd_buffer[pos], ptr, 8);
                pos += 8;
                break;
 
            default:
                break;
        }
        format++;
    };

    hci_cmd_buffer[2] = pos - 3;
    hci_cmd_buffer[3] = pos - 5;
    hci_cmd_buffer[5] = pos - 7;
    return pos;
}

/**
 * construct ANT HCI Command based on template
 *
 * mainly calls ant_create_cmd_internal
 */
uint16_t ant_create_cmd(uint8_t *hci_cmd_buffer, const ant_cmd_t *cmd, ...){
    va_list argptr;
    va_start(argptr, cmd);
    uint16_t len = ant_create_cmd_internal(hci_cmd_buffer, cmd, argptr);
    va_end(argptr);
    return len;
}

/**
 * pre: numcmds >= 0 - it's allowed to send a command to the controller
 */
uint8_t ant_packet_buffer[30];
int ant_send_cmd(const ant_cmd_t *cmd, ...){
    va_list argptr;
    va_start(argptr, cmd);
    uint16_t size = ant_create_cmd_internal(ant_packet_buffer, cmd, argptr);
    va_end(argptr);
    return hci_send_cmd_packet(ant_packet_buffer, size);
}

/**
 *  ANT commands as found in http://www.thisisant.com/resources/ant-message-protocol-and-usage/
 */

const ant_cmd_t ant_reset = {
MESG_SYSTEM_RESET_ID, "0"
};

const ant_cmd_t ant_assign_channel = {
MESG_ASSIGN_CHANNEL_ID, "111"
// channel number, channel type, network number
};

const ant_cmd_t ant_un_assign_channel = {
MESG_UNASSIGN_CHANNEL_ID, "1"
// channel number
};

const ant_cmd_t ant_search_timeout = {
MESG_CHANNEL_SEARCH_TIMEOUT_ID, "11"
// channel number, timeout
};

const ant_cmd_t ant_lp_search_timeout = {
MESG_SET_LP_SEARCH_TIMEOUT_ID, "11"
// channel number, timeout
};

const ant_cmd_t ant_network_key = {
MESG_NETWORK_KEY_ID, "1D"
// network number, pointer to 8 byte network key
};

const ant_cmd_t ant_channel_id = {
MESG_CHANNEL_ID_ID, "1211"
// channel number, device number, device type, transmit type
};

const ant_cmd_t ant_channel_power = {
MESG_RADIO_TX_POWER_ID, "11"
// channel number, power
};

const ant_cmd_t ant_channel_period = {
MESG_CHANNEL_MESG_PERIOD_ID, "12"
// channel number, period 
};

const ant_cmd_t ant_prox_search_config = {
MESG_PROX_SEARCH_CONFIG_ID, "11"
// channel number, prox level 
};

const ant_cmd_t ant_broadcast = {
MESG_BROADCAST_DATA_ID, "1D"
// channel number, pointer to 8 byte data 
};

const ant_cmd_t ant_acknowledged = {
MESG_ACKNOWLEDGED_DATA_ID, "1D"
// channel number, pointer to 8 byte data 
};

const ant_cmd_t ant_burst_packet = {
MESG_BURST_DATA_ID, "1D"
// channel number, pointer to 8 byte data 
};

const ant_cmd_t ant_open_channel = {
MESG_OPEN_CHANNEL_ID, "1"
// channel number
};

const ant_cmd_t ant_close_channel = {
MESG_CLOSE_CHANNEL_ID, "1"
// channel number
};

const ant_cmd_t ant_request_message = {
MESG_REQUEST_ID, "11"
// channel number, requested message
};
