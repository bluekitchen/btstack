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
/* EXAMPLE_START(gap_le_advertisements): GAP LE Advertisements Dumper
 *
 * @text This example shows how to scan and parse advertisements.
 * 
 */
 // *****************************************************************************


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <btstack/hci_cmds.h>

#include "btstack_memory.h"
#include "hci.h"
#include "ad_parser.h"

/* @section GAP LE setup for receiving advertisements
 *
 * @text GAP LE advertisements are received as custom HCI events of the 
 * GAP_LE_ADVERTISING_REPORT type. To receive them, you'll need to register
 * the HCI packet handler, as shown in Listing GAPLEAdvSetup.
 */

/* LISTING_START(GAPLEAdvSetup): Setting up GAP LE client for receiving advertisements */
static void handle_hci_event(uint8_t packet_type, uint8_t *packet, uint16_t size);

static void gap_le_advertisements_setup(void){
    hci_register_packet_handler(handle_hci_event);
}
/* LISTING_END */

/* @section GAP LE Advertising Data Dumper
 *
 * @text Here, we use the definition of advertising data types and flags as specified in 
 * [Assigned Numbers GAP](https://www.bluetooth.org/en-us/specification/assigned-numbers/generic-access-profile)
 * and [Supplement to the Bluetooth Core Specification, v4](https://www.bluetooth.org/DocMan/handlers/DownloadDoc.ashx?doc_id=282152).
 */

/* LISTING_START(GAPLEAdvDataTypesAndFlags): Advertising data types and flags */
static char * ad_types[] = {
    "", 
    "Flags",
    "Incomplete List of 16-bit Service Class UUIDs",
    "Complete List of 16-bit Service Class UUIDs",
    "Incomplete List of 32-bit Service Class UUIDs",
    "Complete List of 32-bit Service Class UUIDs",
    "Incomplete List of 128-bit Service Class UUIDs",
    "Complete List of 128-bit Service Class UUIDs",
    "Shortened Local Name",
    "Complete Local Name",
    "Tx Power Level",
    "", 
    "", 
    "Class of Device",
    "Simple Pairing Hash C",
    "Simple Pairing Randomizer R",
    "Device ID",
    "Security Manager TK Value",
    "Slave Connection Interval Range",
    "",
    "List of 16-bit Service Solicitation UUIDs",
    "List of 128-bit Service Solicitation UUIDs",
    "Service Data",
    "Public Target Address",
    "Random Target Address",
    "Appearance",
    "Advertising Interval"
};

static char * flags[] = {
    "LE Limited Discoverable Mode",
    "LE General Discoverable Mode",
    "BR/EDR Not Supported",
    "Simultaneous LE and BR/EDR to Same Device Capable (Controller)",
    "Simultaneous LE and BR/EDR to Same Device Capable (Host)",
    "Reserved",
    "Reserved",
    "Reserved"
};
/* LISTING_END */

/* @text BTstack offers an iterator for parsing sequence of advertising data (AD) structures, 
 * see [BLE advertisements parser API](../appendix/apis/#ble-advertisements-parser-api).
 * After initializing the iterator, each AD structure is dumped according to its type.
 */

/* LISTING_START(GAPLEAdvDataParsing): Parsing advertising data */
static void dump_advertisement_data(uint8_t * adv_data, uint8_t adv_size){
    ad_context_t context;
    for (ad_iterator_init(&context, adv_size, adv_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)){
        uint8_t data_type = ad_iterator_get_data_type(&context);
        uint8_t size      = ad_iterator_get_data_len(&context);
        uint8_t * data    = ad_iterator_get_data(&context);
        
        if (data_type > 0 && data_type < 0x1B){
            printf("    %s: ", ad_types[data_type]);
        } 
        int i;
        // Assigned Numbers GAP
    
        switch (data_type){
            case 0x01: // Flags
                // show only first octet, ignore rest
                for (i=0; i<8;i++){
                    if (data[0] & (1<<i)){
                        printf("%s; ", flags[i]);
                    }

                }
                break;
            case 0x02: // Incomplete List of 16-bit Service Class UUIDs
            case 0x03: // Complete List of 16-bit Service Class UUIDs
            case 0x14: // List of 16-bit Service Solicitation UUIDs
                for (i=0; i<size;i+=2){
                    printf("%02X ", READ_BT_16(data, i));
                }
                break;
            case 0x04: // Incomplete List of 32-bit Service Class UUIDs
            case 0x05: // Complete List of 32-bit Service Class UUIDs
                for (i=0; i<size;i+=4){
                    printf("%04X ", READ_BT_32(data, i));
                }
                break;
            case 0x06: // Incomplete List of 128-bit Service Class UUIDs
            case 0x07: // Complete List of 128-bit Service Class UUIDs
            case 0x15: // List of 128-bit Service Solicitation UUIDs
                printUUID128(data);
                break;
            case 0x08: // Shortened Local Name
            case 0x09: // Complete Local Name
                for (i=0; i<size;i++){
                    printf("%c", (char)(data[i]));
                }
                break;
            case 0x0A: // Tx Power Level 
                printf("%d dBm", *(int8_t*)data);
                break;
            case 0x12: // Slave Connection Interval Range 
                printf("Connection Interval Min = %u ms, Max = %u ms", READ_BT_16(data, 0) * 5/4, READ_BT_16(data, 2) * 5/4);
                break;
            case 0x16: // Service Data 
                printf_hexdump(data, size);
                break;
            case 0x17: // Public Target Address
            case 0x18: // Random Target Address
                print_bd_addr(data);
                break;
            case 0x19: // Appearance 
                // https://developer.bluetooth.org/gatt/characteristics/Pages/CharacteristicViewer.aspx?u=org.bluetooth.characteristic.gap.appearance.xml
                printf("%02X", READ_BT_16(data, 0) );
                break;
            case 0x1A: // Advertising Interval 
                printf("%u ms", READ_BT_16(data, 0) * 5/8 );
                break;
            case 0x3D: // 3D Information Data 
                printf_hexdump(data, size);
                break;
            case 0xFF: // Manufacturer Specific Data 
                break;
            case 0x0D: // Class of Device (3B)
            case 0x0E: // Simple Pairing Hash C (16B)
            case 0x0F: // Simple Pairing Randomizer R (16B) 
            case 0x10: // Device ID 
            case 0x11: // Security Manager TK Value (16B)
            default:
                printf("Unknown Advertising Data Type"); 
                break;
        }        
        printf("\n");
    }
    printf("\n");
}
/* LISTING_END */

/* @section HCI packet handler
 * 
 * @text The HCI packet handler has to start the scanning, 
 * and to handle received advertisements. Advertisements are received 
 * as HCI event packets of the GAP_LE_ADVERTISING_REPORT type,
 * see Listing GAPLEAdvPacketHandler.  
 */

/* LISTING_START(GAPLEAdvPacketHandler): Scanning and receiving advertisements */

static void handle_hci_event(uint8_t packet_type, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
    
    switch (packet[0]) {
        case BTSTACK_EVENT_STATE:
            // BTstack activated, get started
            if (packet[2] == HCI_STATE_WORKING) {
                printf("BTstack activated, start scaning!\n");
                le_central_set_scan_parameters(0,0x0030, 0x0030);
                le_central_start_scan(); 
            }
            break;
        case GAP_LE_ADVERTISING_REPORT:{
            int pos = 2;
            uint8_t event_type = packet[pos++];
            uint8_t address_type = packet[pos++];
            bd_addr_t address;
            bt_flip_addr(address, &packet[pos]);
            pos += 6;
            uint8_t rssi = packet[pos++];
            uint8_t length = packet[pos++];
            uint8_t * data = &packet[pos];
            
            printf("Advertisement event: evt-type %u, addr-type %u, addr %s, rssi %u, data[%u] ", event_type,
               address_type, bd_addr_to_str(address), rssi, length);
            printf_hexdump(data, length);
            dump_advertisement_data(data, length);
            break;
        }
        default:
            break;
    }
}
/* LISTING_END */

int btstack_main(void);
int btstack_main(void)
{
    gap_le_advertisements_setup();

    // turn on!
    hci_power_control(HCI_POWER_ON);
        
    return 0;
}

/* EXAMPLE_END */