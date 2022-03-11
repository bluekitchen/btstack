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
// test rfcomm query tests
//
// *****************************************************************************


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "hci_cmd.h"

#include "btstack_memory.h"
#include "bluetooth_data_types.h"
#include "hci.h"
#include "ad_parser.h"
#include "l2cap.h"

extern "C" void le_handle_advertisement_report(uint8_t *packet, uint16_t size);

static uint8_t expected_bt_addr[] = {0x34, 0xB1, 0xF7, 0xD1, 0x77, 0x9B};
static uint8_t adv_multi_packet[] = {
    0x3E, 0x3B, 0x02, 0x03, // num_reports = 1
    // data_length = 9; event_size = 10 + data_length = 19 = 0x13
    // ( ev_type, ev_size, address type, address)
    0xE2, 0x01, 0x34, 0xB1, 0xF7, 0xD1, 0x77, 0x9B,           
    0x09, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0xCC, // data len, data, rssi
    
    0xE2, 0x01, 0x34, 0xB1, 0xF7, 0xD1, 0x77, 0x9B,            
    0x08, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0xCB,       // data len, data, rssi
    
    0xE2, 0x01, 0x34, 0xB1, 0xF7, 0xD1, 0x77, 0x9B,            
    0x07, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0xCA              // data len, data, rssi
};

static int adv_index = 0;
static btstack_packet_callback_registration_t hci_event_callback_registration;

void CHECK_EQUAL_ARRAY(const uint8_t * expected, uint8_t * actual, int size){
    for (int i=0; i<size; i++){
        BYTES_EQUAL(expected[i], actual[i]);
    }
}

static int dummy_callback(void){
    return 0;
}

static hci_transport_t dummy_transport = {
  /*  .transport.name                          = */  "DUMMY",
  /*  .transport.init                          = */  NULL,
  /*  .transport.open                          = */  NULL,
  /*  .transport.close                         = */  NULL,
  /*  .transport.register_packet_handler       = */  (void (*)(void (*)(uint8_t, uint8_t *, uint16_t))) dummy_callback,
  /*  .transport.can_send_packet_now           = */  NULL,
  /*  .transport.send_packet                   = */  NULL,
  /*  .transport.set_baudrate                  = */  NULL,
};


void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    CHECK_EQUAL(0xE2, packet[2]);                   // event type
    CHECK_EQUAL(0x01, packet[3]);                   // address type
    CHECK_EQUAL_ARRAY(expected_bt_addr, &packet[4], 6);
    CHECK_EQUAL(0xCC - adv_index, packet[10]);      // rssi
    CHECK_EQUAL(0x09 - adv_index, packet[11]);      // data size
    
    for (int i=0; i<0x09 - adv_index; i++){         // data
        CHECK_EQUAL(i, packet[12+i]);
    }
    adv_index++;
}

TEST_GROUP(ADParser){
    void setup(void){
        hci_init(&dummy_transport, NULL);
        hci_event_callback_registration.callback = &packet_handler;
        hci_add_event_handler(&hci_event_callback_registration);
    }
};

TEST(ADParser, TestAdvertisementEventMultipleReports){
    le_handle_advertisement_report(adv_multi_packet, sizeof(adv_multi_packet));
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
