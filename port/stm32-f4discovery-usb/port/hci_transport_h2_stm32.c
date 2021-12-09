/*
 * Copyright (C) 2020 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "hci_transport_h2_stm32.c"

/*
 *  hci_transport_h2_stm32.c
 *
 *  HCI Transport API implementation for STM32Cube USB Host Stack
 */

// include STM32 first to avoid warning about redefinition of UNUSED
#include "usb_host.h"
#include "usbh_bluetooth.h"

#include <stddef.h>

#include "hci_transport_h2_stm32.h"

#include "bluetooth.h"

#include "btstack_debug.h"
#include "btstack_run_loop.h"

static  void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size) = NULL;

// data source for integration with BTstack Runloop
static btstack_data_source_t transport_data_source;

static void hci_transport_h2_stm32_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type) {
    switch (callback_type){
        case DATA_SOURCE_CALLBACK_POLL:
            MX_USB_HOST_Process();
            break;
        default:
            break;
    }
}

static void hci_transport_h2_stm32_block_sent(void) {
    static const uint8_t packet_sent_event[] = {HCI_EVENT_TRANSPORT_PACKET_SENT, 0};
    // notify upper stack that it can send again
    packet_handler(HCI_EVENT_PACKET, (uint8_t *) &packet_sent_event[0], sizeof(packet_sent_event));
}

static void hci_transport_h2_stm32_packet_received(uint8_t packet_type, uint8_t * packet, uint16_t size){
    packet_handler(packet_type, packet, size);
}

static void hci_transport_h2_stm32_init(const void * transport_config){
    UNUSED(transport_config);
    usbh_bluetooth_set_packet_sent(&hci_transport_h2_stm32_block_sent);
    usbh_bluetooth_set_packet_received(&hci_transport_h2_stm32_packet_received);
    log_info("hci_transport_h2_stm32_init");
}

static int hci_transport_h2_stm32_open(void){
    // set up polling data_source
    btstack_run_loop_set_data_source_handler(&transport_data_source, &hci_transport_h2_stm32_process);
    btstack_run_loop_enable_data_source_callbacks(&transport_data_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_add_data_source(&transport_data_source);
    return 0;
}

static int hci_transport_h2_stm32_close(void){
    // remove data source
    btstack_run_loop_disable_data_source_callbacks(&transport_data_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_remove_data_source(&transport_data_source);
    return -1;
}

static void hci_transport_h2_stm32_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    packet_handler = handler;
}

static int hci_transport_h2_stm32_can_send_now(uint8_t packet_type){
    return usbh_bluetooth_can_send_now();
}

static int hci_transport_h2_stm32_send_packet(uint8_t packet_type, uint8_t * packet, int size){
    switch (packet_type){
        case HCI_COMMAND_DATA_PACKET:
            usbh_bluetooth_send_cmd(packet, size);
            return 0;
        case HCI_ACL_DATA_PACKET:
            usbh_bluetooth_send_acl(packet, size);
            return 0;
        default:
            break;
    }
    return -1;
}

static void hci_transport_h2_stm32_set_sco_config(uint16_t voice_setting, int num_connections){
    log_info("hci_transport_h2_stm32_send_packet, voice 0x%02x, num connections %u", voice_setting, num_connections);
}

const hci_transport_t * hci_transport_h2_stm32_instance(void) {

    static const hci_transport_t instance = {
            /* const char * name; */                                        "H4",
            /* void   (*init) (const void *transport_config); */            &hci_transport_h2_stm32_init,
            /* int    (*open)(void); */                                     &hci_transport_h2_stm32_open,
            /* int    (*close)(void); */                                    &hci_transport_h2_stm32_close,
            /* void   (*register_packet_handler)(void (*handler)(...); */   &hci_transport_h2_stm32_register_packet_handler,
            /* int    (*can_send_packet_now)(uint8_t packet_type); */       &hci_transport_h2_stm32_can_send_now,
            /* int    (*send_packet)(...); */                               &hci_transport_h2_stm32_send_packet,
            /* int    (*set_baudrate)(uint32_t baudrate); */                NULL,
            /* void   (*reset_link)(void); */                               NULL,
            /* void   (*set_sco_config)(uint16_t voice_setting, int num_connections); */ &hci_transport_h2_stm32_set_sco_config,
    };
    return &instance;
}
