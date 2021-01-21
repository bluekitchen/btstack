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

#define BTSTACK_FILE__ "hci_transport_h2_stm32.c"

/*
 *  hci_transport_h2_stm32.c
 *
 *  HCI Transport API implementation for STM32Cube USB Host Stack
 */

#include "hci_transport_h2_stm32.h"
#include <stddef.h>
#include "btstack_debug.h"

static  void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size) = NULL;


static void hci_transport_h2_stm32_init(const void * transport_config){
    UNUSED(transport_config);
    log_info("hci_transport_h2_stm32_init");
}

static int hci_transport_h2_stm32_open(void){
    log_info("hci_transport_h2_stm32_open");
    return 0;
}

static int hci_transport_h2_stm32_close(void){
    log_info("hci_transport_h2_stm32_close");
    return -1;
}

static void hci_transport_h2_stm32_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    packet_handler = handler;
}

static int hci_transport_h2_stm32_can_send_now(uint8_t packet_type){
    log_info("hci_transport_h2_stm32_can_send_now");
    return 0;
}

static int hci_transport_h2_stm32_send_packet(uint8_t packet_type, uint8_t * packet, int size){
    log_info("hci_transport_h2_stm32_send_packet");
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
