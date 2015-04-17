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
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */


// *****************************************************************************
//
// att device demo
//
// *****************************************************************************

// TODO: seperate BR/EDR from LE ACL buffers
// TODO: move LE init into HCI
// ..

// NOTE: Supports only a single connection

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack-config.h"

#include <msp430x54x.h>

#include "bt_control_cc256x.h"
#include "hal_board.h"
#include "hal_compat.h"
#include "hal_usb.h"
#include "hal_lcd.h"
#include "hal_usb.h"
#include "UserExperienceGraphics.h"

#include <btstack/run_loop.h>

#include "btstack_memory.h"
#include "bt_control_cc256x.h"
#include "hci.h"
#include "hci_dump.h"
#include "l2cap.h"

#include "sm.h"
#include "att.h"
#include "att_server.h"
#include "gap_le.h"
#include "le_device_db.h"

#define FONT_HEIGHT		12                    // Each character has 13 lines 
#define FONT_WIDTH       8
#define MAX_CHR01_VALUE_LENGTH 40

static uint16_t chr01_value_length = 0;
static char chr01_value[MAX_CHR01_VALUE_LENGTH];
static char chr02_value = 0;

void doLCD(void){
    //Initialize LCD and backlight    
    // 138 x 110, 4-level grayscale pixels.
    halLcdInit();       
    // halLcdBackLightInit();  
    // halLcdSetBackLight(0);  // 8 for normal
    halLcdSetContrast(100);
    halLcdClearScreen(); 
    halLcdImage(TI_TINY_BUG, 4, 32, 104, 12 );
    
    halLcdPrintLine("BTstack on ", 0, 0);
    halLcdPrintLine("TI MSP430", 1, 0);
    halLcdPrintLine("LE Write Test", 2, 0);
    halLcdPrintLine("NOT CONNECTED", 4, 0);
    halLcdPrintLine("Attribute 0x0022:", 6, 0);
    halLcdPrintLine("- NO VALUE -", 7, 0);
}

void overwriteLine(int line, char *text){
    halLcdClearImage(130, FONT_HEIGHT, 0, line*FONT_HEIGHT);
    halLcdPrintLine(text, line, 0);
}

// enable LE, setup ADV data
static void app_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
    bd_addr_t addr;
    uint8_t adv_data[] = { 02, 01, 05,   03, 02, 0xf0, 0xff }; 
    
    switch (packet[0]) {
        case BTSTACK_EVENT_STATE:
            // bt stack activated, get started - set local name
            if (packet[2] == HCI_STATE_WORKING) {
                printf("Working!\n");
                hci_send_cmd(&hci_le_set_advertising_data, sizeof(adv_data), adv_data);
            }
            break;
            
        case BTSTACK_EVENT_NR_CONNECTIONS_CHANGED:
            if (packet[2]) {
                overwriteLine(4, "CONNECTED");
            } else {
                overwriteLine(4, "NOT CONNECTED");
            }
            break;
            
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            // restart advertising
            hci_send_cmd(&hci_le_set_advertise_enable, 1);
            break;
            
        case HCI_EVENT_COMMAND_COMPLETE:
            if (COMMAND_COMPLETE_EVENT(packet, hci_read_bd_addr)){
                bt_flip_addr(addr, &packet[6]);
                printf("BD ADDR: %s\n", bd_addr_to_str(addr));
                break;
            }
            if (COMMAND_COMPLETE_EVENT(packet, hci_le_set_advertising_data)){
               hci_send_cmd(&hci_le_set_scan_response_data, 10, adv_data);
               break;
            }
            if (COMMAND_COMPLETE_EVENT(packet, hci_le_set_scan_response_data)){
               hci_send_cmd(&hci_le_set_advertise_enable, 1);
               break;
            }
        default:
            break;
    }
	
}

// test profile
#include "profile.h"

static uint16_t get_read_att_value_len(uint16_t att_handle){
    uint16_t value_len;
    switch(att_handle){
        case ATT_CHARACTERISTIC_FFF1_01_VALUE_HANDLE:
            value_len = chr01_value_length;
            break;
        case ATT_CHARACTERISTIC_FFF2_01_VALUE_HANDLE:
            value_len = 1;
            break;
        default:
            value_len = 0;
            break;
    }
    return value_len;
}

static uint16_t get_write_att_value_len(uint16_t att_handle){
    uint16_t value_len;
    switch(att_handle){
        case ATT_CHARACTERISTIC_FFF1_01_VALUE_HANDLE:
            value_len = MAX_CHR01_VALUE_LENGTH;
            break;
        case ATT_CHARACTERISTIC_FFF2_01_VALUE_HANDLE:
            value_len = 1;
            break;
        default:
            value_len = 0;
            break;
    }
    return value_len;
}

static uint16_t get_bytes_to_copy(uint16_t value_len, uint16_t offset, uint16_t buffer_size){
    if (value_len <= offset ) return 0;
    
    uint16_t bytes_to_copy = value_len - offset;
    if (bytes_to_copy > buffer_size) {
        bytes_to_copy = buffer_size;
    }
    return bytes_to_copy;
}

uint16_t att_read_callback(uint16_t con_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    printf("READ Callback, handle %04x\n", att_handle);
    uint16_t value_len = get_read_att_value_len(att_handle);
    if (!buffer) return value_len;
    
    uint16_t bytes_to_copy = get_bytes_to_copy(value_len, offset, buffer_size);
    if (!bytes_to_copy) return 0;
    
    switch(att_handle){
        case ATT_CHARACTERISTIC_FFF1_01_VALUE_HANDLE:
            memcpy(buffer, &chr01_value[offset], bytes_to_copy);
            break;
        case ATT_CHARACTERISTIC_FFF2_01_VALUE_HANDLE:
            buffer[offset] = chr02_value;
            break;
    }
    return bytes_to_copy;
}

// write requests
static int att_write_callback(uint16_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    printf("WRITE Callback, handle %04x\n", att_handle);
    
    uint16_t value_len = get_write_att_value_len(att_handle);
    uint16_t bytes_to_copy = get_bytes_to_copy(value_len, offset,buffer_size);
    if (!bytes_to_copy) return ATT_ERROR_INVALID_OFFSET;
    
    switch(att_handle){
        case ATT_CHARACTERISTIC_FFF1_01_VALUE_HANDLE:
            buffer[buffer_size] = 0;
            memcpy(&chr01_value[offset], buffer, bytes_to_copy);
            chr01_value_length = bytes_to_copy + offset;
            
            printf("New text: %s\n", buffer);
            overwriteLine(7, (char*)buffer);
            break;
        case ATT_CHARACTERISTIC_FFF2_01_VALUE_HANDLE:
            printf("New value: %u\n", buffer[offset]);
            if (buffer[offset]) {
                LED_PORT_OUT |= LED_2;
            } else {
                LED_PORT_OUT &= ~LED_2;
            }
            chr02_value = buffer[offset];
            break;
    }
    return 0;
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

    // show off
    doLCD();

    // set up l2cap_le
    l2cap_init();
    
    // setup le device db
    le_device_db_init();

    // setup SM: Display only
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_ONLY);
    sm_set_authentication_requirements( SM_AUTHREQ_BONDING | SM_AUTHREQ_MITM_PROTECTION); 

    // setup ATT server
    att_server_init(profile_data, NULL, att_write_callback);    
    att_server_register_packet_handler(app_packet_handler);
    
	printf("Run...\n\r");

    // turn on!
	hci_power_control(HCI_POWER_ON);

    LED_PORT_OUT &= ~LED_2;

    return 0;
}

