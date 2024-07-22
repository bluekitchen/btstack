/*
 * Copyright (C) 2024 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "hci_dump_dispatch.c"

#include "hci_dump_dispatch.h"

#include <stdint.h>
#include <string.h>

static btstack_linked_list_t hci_dump_list = NULL;

static void hci_dump_reset_all(void) {
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &hci_dump_list);
    while (btstack_linked_list_iterator_has_next(&it)) {
        hci_dump_dispatch_item_t *list_item = (hci_dump_dispatch_item_t *)btstack_linked_list_iterator_next(&it);
        if (list_item->hci_dump->reset) {
            list_item->hci_dump->reset();
        }
    }
}

static void hci_dump_log_packet_all(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len) {
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &hci_dump_list);
    while (btstack_linked_list_iterator_has_next(&it)) {
        hci_dump_dispatch_item_t *list_item = (hci_dump_dispatch_item_t *)btstack_linked_list_iterator_next(&it);
        if (list_item->hci_dump->log_packet) {
            list_item->hci_dump->log_packet(packet_type, in, packet, len);
        }
    }
}

static void hci_dump_log_message_all(int log_level, const char *format, va_list argptr) {
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &hci_dump_list);
    while (btstack_linked_list_iterator_has_next(&it)) {
        hci_dump_dispatch_item_t *list_item = (hci_dump_dispatch_item_t *)btstack_linked_list_iterator_next(&it);
        if (list_item->hci_dump->log_message) {
            list_item->hci_dump->log_message(log_level, format, argptr);
        }
    }
}

void hci_dump_dispatch_init(void){
}

void hci_dump_dispatch_register(hci_dump_dispatch_item_t *list_item, const hci_dump_t *dump) {
    list_item->hci_dump = dump;
    btstack_linked_list_add(&hci_dump_list, (btstack_linked_item_t *)list_item);
}

void hci_dump_dispatch_unregister(hci_dump_t *dump) {
    btstack_linked_list_remove(&hci_dump_list, (btstack_linked_item_t *)dump);
}

void hci_dump_dispatch_deinit(void){
    hci_dump_list = NULL;
}

static const hci_dump_t hci_dump_dispatch = {
    .reset = hci_dump_reset_all,
    .log_packet = hci_dump_log_packet_all,
    .log_message = hci_dump_log_message_all
};

const hci_dump_t * hci_dump_dispatch_instance(void){
    return &hci_dump_dispatch;
}
