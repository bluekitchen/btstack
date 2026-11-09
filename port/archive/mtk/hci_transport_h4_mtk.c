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
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
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
 * Please inquire about commercial licensing options at contact@bluekitchen-gmbh.com
 *
 */

/*
 *  hci_h4_transport.c
 *
 *  HCI Transport API implementation for basic H4 protocol over POSIX
 *
 *  Created by Matthias Ringwald on 4/29/09.
 */

#include "btstack_config.h"

#include <stdio.h>

#include "bluetoothdrv.h"

#include <string.h>

#include "btstack_debug.h"
#include "hci.h"
#include "hci_transport.h"
#include "hci_transport_h4.h"

static void h4_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type);
static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size); 

typedef struct hci_transport_h4 {
    hci_transport_t transport;
    btstack_data_source_t *ds;
    /* power management support, e.g. used by iOS */
    btstack_timer_source_t sleep_timer;
} hci_transport_h4_t;

// single instance
static hci_transport_h4_t * hci_transport_h4 = NULL;

static  void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size) = dummy_handler;

// packet bufffers
static uint8_t hci_packet_out[1+HCI_OUTGOING_PACKET_BUFFER_SIZE]; // packet type + max(acl header + acl payload, cmd header   +   cmd data)
static uint8_t hci_packet_in[ 1+HCI_INCOMING_PACKET_BUFFER_SIZE]; // packet type + max(acl header + acl payload, event header + event data)

static int h4_open(void){
    int fd = mtk_bt_enable();

    if (fd < 0) {
        log_error("mtk_bt_enable failed");
        return -1;
    }

    // set up data_source
    hci_transport_h4->ds = (btstack_data_source_t*) malloc(sizeof(btstack_data_source_t));
    if (!hci_transport_h4->ds) return -1;
    memset(hci_transport_h4->ds, 0, sizeof(btstack_data_source_t));
    btstack_run_loop_set_data_source_fd(hci_transport_h4->ds, fd);
    btstack_run_loop_set_data_source_handler(hci_transport_h4->ds, &h4_process);
    btstack_run_loop_enable_data_source_callbacks(hci_transport_h4->ds, DATA_SOURCE_CALLBACK_READ);
    btstack_run_loop_add_data_source(hci_transport_h4->ds);
    return 0;
}

static int h4_close(void){

    mtk_bt_disable(hci_transport_h4->ds->source.fd);

    // first remove run loop handler
	btstack_run_loop_remove_data_source(hci_transport_h4->ds);
    
    // free struct
    free(hci_transport_h4->ds);
    hci_transport_h4->ds = NULL;
    return 0;
}

static int h4_send_packet(uint8_t packet_type, uint8_t * packet, int size){

    if (hci_transport_h4->ds == NULL) return -1;

    // preapare packet
    hci_packet_out[0] = packet_type;
    memcpy(&hci_packet_out[1], packet, size);

    // send
    int res = mtk_bt_write(hci_transport_h4->ds->source.fd, hci_packet_out, size + 1);

    return 0;
}

static void   h4_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    packet_handler = handler;
}

static void h4_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type) {
    if (hci_transport_h4->ds->source.fd == 0) return;

    // read up to bytes_to_read data in
    ssize_t bytes_read = mtk_bt_read(hci_transport_h4->ds->source.fd, &hci_packet_in[0], sizeof(hci_packet_in));

    if (bytes_read == 0) return;

    // iterate over packets
    uint16_t pos = 0;
    while (pos < bytes_read) {
        uint16_t packet_len;
        switch(hci_packet_in[pos]){
            case HCI_EVENT_PACKET:
                packet_len = hci_packet_in[pos+2] + 3;
                break;
            case HCI_ACL_DATA_PACKET:
                 packet_len = little_endian_read_16(hci_packet_in, pos + 3) + 5;
                 break;
            default:
                log_error("h4_process: invalid packet type 0x%02x\n", hci_packet_in[pos]);
                return;
        }

        packet_handler(hci_packet_in[pos], &hci_packet_in[pos+1], packet_len-1);
        pos += packet_len;
    }
}

static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){
}

// get h4 singleton
const hci_transport_t * hci_transport_h4_instance_for_uart(const btstack_uart_t * uart_driver){
    (void) uart_driver;
    if (hci_transport_h4 == NULL) {
        hci_transport_h4 = (hci_transport_h4_t*)malloc( sizeof(hci_transport_h4_t));
        memset(hci_transport_h4, 0, sizeof(hci_transport_h4_t));
        hci_transport_h4->transport.name                          = "H4 MTK";
        hci_transport_h4->transport.open                          = h4_open;
        hci_transport_h4->transport.close                         = h4_close;
        hci_transport_h4->transport.send_packet                   = h4_send_packet;
        hci_transport_h4->transport.register_packet_handler       = h4_register_packet_handler;
    }
    return (hci_transport_t *) hci_transport_h4;
}
