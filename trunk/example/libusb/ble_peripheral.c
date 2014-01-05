/*
 * Copyright (C) 2011-2013 by Matthias Ringwald
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
 * 4. This software may not be used in a commercial product
 *    without an explicit license granted by the copyright holder. 
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

//*****************************************************************************
//
// att device demo
//
//*****************************************************************************

// TODO: seperate BR/EDR from LE ACL buffers
// ..

// NOTE: Supports only a single connection

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#include <btstack/run_loop.h>
#include "debug.h"
#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"

#include "l2cap.h"

#include "sm.h"
#include "att.h"
#include "att_server.h"
#include "gap_le.h"
#include "central_device_db.h"

///------
static int advertisements_enabled = 0;

// test profile
#include "profile.h"

// write requests
static void att_write_callback(uint16_t handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size, signature_t * signature){
    printf("WRITE Callback, handle %04x\n", handle);
    switch(handle){
        case 0x000b:
            buffer[buffer_size]=0;
            printf("New text: %s\n", buffer);
            break;
        case 0x000d:
            printf("New value: %u\n", buffer[0]);
            break;
    }
}

static void app_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    
    uint8_t adv_data[] = { 02, 01, 05,   03, 02, 0xf0, 0xff }; 

    switch (packet_type) {
            
        case HCI_EVENT_PACKET:
            switch (packet[0]) {
                
                case BTSTACK_EVENT_STATE:
                    // bt stack activated, get started
                    if (packet[2] == HCI_STATE_WORKING) {
                        printf("SM Init completed\n");
                        hci_send_cmd(&hci_le_set_advertising_data, sizeof(adv_data), adv_data);
                    }
                    break;
                
                case HCI_EVENT_LE_META:
                    switch (packet[2]) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                            advertisements_enabled = 0;
                            break;

                        default:
                            break;
                    }
                    break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    // restart advertising if we have been connected before
                    // -> avoid sending advertise enable a second time before command complete was received 
                    if (advertisements_enabled == 0) {
                        hci_send_cmd(&hci_le_set_advertise_enable, 1);
                        advertisements_enabled = 1;
                    }
                    break;
                    
                case HCI_EVENT_COMMAND_COMPLETE:
                    if (COMMAND_COMPLETE_EVENT(packet, hci_le_set_advertising_data)){
                         // only needed for BLE Peripheral
                       hci_send_cmd(&hci_le_set_scan_response_data, 10, adv_data);
                       break;
                    }
                    if (COMMAND_COMPLETE_EVENT(packet, hci_le_set_scan_response_data)){
                         // only needed for BLE Peripheral
                       hci_send_cmd(&hci_le_set_advertise_enable, 1);
                       advertisements_enabled = 1;
                       break;
                    }
                    break;
                default:
                    break;
            }
    }
}

void setup(void){
    /// GET STARTED with BTstack ///
    btstack_memory_init();
    run_loop_init(RUN_LOOP_POSIX);
        
    // use logger: format HCI_DUMP_PACKETLOGGER, HCI_DUMP_BLUEZ or HCI_DUMP_STDOUT
    hci_dump_open("/tmp/hci_dump.pklg", HCI_DUMP_PACKETLOGGER);

    // init HCI
    hci_transport_t    * transport = hci_transport_usb_instance();
    hci_uart_config_t  * config    = NULL;
    bt_control_t       * control   = NULL;
    remote_device_db_t * remote_db = (remote_device_db_t *) &remote_device_db_memory;
    hci_init(transport, config, control, remote_db);

    // set up l2cap_le
    l2cap_init();
    
    // setup central device db
    central_device_db_init();

    // setup SM
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements( SM_AUTHREQ_BONDING );
    sm_set_request_security(1);

    // setup ATT server
    att_server_init(profile_data, NULL, att_write_callback);    
    att_server_register_packet_handler(app_packet_handler);
}

int main(void)
{
    setup();
    // gap_random_address_set_update_period(5000);
    gap_random_address_set_mode(GAP_RANDOM_ADDRESS_RESOLVABLE);

    // turn on!
    hci_power_control(HCI_POWER_ON);
    
    // go!
    run_loop_execute(); 
    
    // happy compiler!
    return 0;
}
