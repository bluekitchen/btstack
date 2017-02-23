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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack_config.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_embedded.h"
//#include "classic/btstack_link_key_db.h"
#include "hci.h"
#include "hci_dump.h"
#include "bt.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static uint8_t _hci_cmd_buf[1028];

static int _can_send_packet_now = 1;

static void (*transport_packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size);

static void host_send_pkt_available_cb(void)
{
    printf("host_send_pkt_available_cb\n");
    _can_send_packet_now = 1;
}

static int host_recv_pkt_cb(uint8_t *data, uint16_t len)
{
    printf("host_recv_pkt_cb: len = %u, data = [ ", len);
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("]\n");
    transport_packet_handler(HCI_EVENT_PACKET, data, len);
    return 0;
}

static const esp_vhci_host_callback_t vhci_host_cb = {
    .notify_host_send_available = host_send_pkt_available_cb,
    .notify_host_recv = host_recv_pkt_cb,
};

/**
 * init transport
 * @param transport_config
 */
static void transport_init(const void *transport_config){
    printf("transport_init\n");
}

static btstack_data_source_t hci_transport_data_source;

static void transport_run(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type){
    (void)ds;
    (void)callback_type;
}

/**
 * open transport connection
 */
static int transport_open(void){
    printf("transport_open\n");
    btstack_run_loop_set_data_source_handler(&hci_transport_data_source, &transport_run);
    btstack_run_loop_enable_data_source_callbacks(&hci_transport_data_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_add_data_source(&hci_transport_data_source);
    return 0;
}

/**
 * close transport connection
 */
static int transport_close(void){
    printf("transport_close\n");
    btstack_run_loop_remove_data_source(&hci_transport_data_source);
    return 0;
}

/**
 * register packet handler for HCI packets: ACL, SCO, and Events
 */
static void transport_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    printf("transport_register_packet_handler\n");
    transport_packet_handler = handler;
}

static int transport_can_send_packet_now(uint8_t packet_type) {
    printf("transport_can_send_packet_now\n");
    return _can_send_packet_now;
}

static int transport_send_packet(uint8_t packet_type, uint8_t *packet, int size){
    (void)packet_type;
    printf("transport_send_packet: type = %u, len = %u, data = [ ", packet_type, size);
    for (size_t i = 0; i < size; i++) {
        printf("%02X ", packet[i]);
    }
    printf("]\n");
    switch (packet_type){
        case HCI_COMMAND_DATA_PACKET:
            _hci_cmd_buf[0] = HCI_COMMAND_DATA_PACKET;
            break;
        case HCI_ACL_DATA_PACKET:
            _hci_cmd_buf[0] = HCI_ACL_DATA_PACKET;
            break;
        default:
            // @TODO
            break;
    }

    memcpy(&_hci_cmd_buf[1], packet, size);
    esp_vhci_host_send_packet(_hci_cmd_buf, size + 1);

    _can_send_packet_now = 0;

    return 0;
}

static const hci_transport_t transport = {
    "VHCI",
    &transport_init,
    &transport_open,
    &transport_close,
    &transport_register_packet_handler,
    &transport_can_send_packet_now,
    &transport_send_packet,
    NULL, // set baud rate
    NULL, // reset link
    NULL, // set SCO config
};

static const hci_transport_t * transport_get_instance(void){
    return &transport;
}

static btstack_packet_callback_registration_t hci_event_callback_registration;

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
    switch(hci_event_packet_get_type(packet)){
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
            printf("BTstack up and running.\n");
            break;
        case HCI_EVENT_COMMAND_COMPLETE:
            if (HCI_EVENT_IS_COMMAND_COMPLETE(packet, hci_read_local_version_information)){
                // @TODO
            }
            break;
        default:
            break;
    }
}

static void btstack_setup(void){

    hci_dump_open(NULL, HCI_DUMP_STDOUT);

    /// GET STARTED with BTstack ///
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_embedded_get_instance());

    // init HCI
    hci_init(transport_get_instance(), NULL);
    //hci_set_link_key_db(btstack_link_key_db_memory_instance()); // @TODO
    //hci_set_chipset(btstack_chipset_cc256x_instance()); // @TODO

    // inform about BTstack state
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
}

extern int btstack_main(int argc, const char * argv[]);

/*
 * @brief: send HCI commands to perform BLE advertising;
 */
void btstack_task(void *pvParameters)
{
    esp_vhci_host_register_callback(&vhci_host_cb);
    printf("btstack_task start\n");

    printf("-------------- btstack_setup\n");
    btstack_setup();
    printf("-------------- btstack_setup end\n");

    printf("-------------- btstack_main\n");
    btstack_main(0, NULL);
    printf("-------------- btstack_main end\n");

    printf("Entering btstack run loop!\n");
    btstack_run_loop_execute();
    printf("Run loop exited...this is unexpected\n");
}

void hal_led_toggle() {}

// main
int app_main(void){

    esp_bt_controller_init();

    xTaskCreatePinnedToCore(&btstack_task, "btstack_task", 2048, NULL, 5, NULL, 0);

    return 0;
}

