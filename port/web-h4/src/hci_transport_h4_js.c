/*
 * Copyright (C) 2025 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "hci_transport_h4_js.c"

#include "hci_transport_h4_js.h"

#include "bluetooth.h"
#include "btstack_bool.h"
#include "btstack_config.h"
#include "btstack_debug.h"
#include "btstack_uart.h"

#include <stdint.h>
#include <string.h>

#include <emscripten/emscripten.h>

// globals
static uint8_t hci_transport_h4_js_send_buffer[1024];
static uint8_t hci_transport_h4_js_receive_buffer[1024];
static void (*hci_transport_h4_js_packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size);
static bool hci_transport_h4_js_tx_state_busy = false;
static btstack_uart_config_t hci_transport_h4_uart_config;

// callbacks from JavaScript for HCI Transport
EMSCRIPTEN_KEEPALIVE
uint8_t * hci_transport_h4_get_receive_buffer(void){
    return hci_transport_h4_js_receive_buffer;
}

EMSCRIPTEN_KEEPALIVE
uint16_t hci_transport_h4_get_receive_buffer_size(void){
    return sizeof(hci_transport_h4_js_receive_buffer);
}

EMSCRIPTEN_KEEPALIVE
void hci_transport_h4_process_packet(uint16_t len){
    uint8_t packet_type = hci_transport_h4_js_receive_buffer[0];
    hci_transport_h4_js_packet_handler(packet_type, &hci_transport_h4_js_receive_buffer[1], len - 1);
}

static int hci_transport_h4_js_can_send_now(uint8_t packet_type) {
    return hci_transport_h4_js_tx_state_busy == false;
}

EMSCRIPTEN_KEEPALIVE
void hci_transport_h4_packet_sent(void) {
    hci_transport_h4_js_tx_state_busy = false;
    // notify upper stack that it can send again
    static const uint8_t packet_sent_event[] = { HCI_EVENT_TRANSPORT_PACKET_SENT, 0};
    hci_transport_h4_js_packet_handler(HCI_EVENT_PACKET, (uint8_t *) &packet_sent_event[0], sizeof(packet_sent_event));
}

static void hci_send_packet(uint8_t packet_type, const uint8_t * buffer, uint16_t length){
    // construct h4 packet
    hci_transport_h4_js_send_buffer[0] = packet_type;
    memcpy(&hci_transport_h4_js_send_buffer[1], buffer, length);
    hci_transport_h4_js_tx_state_busy = true;
    // create UInt8Array and call sendPacket
    EM_ASM({
      data = new Uint8Array(HEAPU8.buffer, $0, $1);
      hci_transport_h4_js_send_data(data);
    }, hci_transport_h4_js_send_buffer, length + 1);
}

static void hci_transport_h4_js_init(const void *transport_config){
    log_info("hci_transport_h4_js_init");

    // check for hci_transport_config_uart_t
    btstack_assert (transport_config != NULL);
    btstack_assert (((hci_transport_config_t*)transport_config)->type == HCI_TRANSPORT_CONFIG_UART);

    // extract UART config from transport config
    hci_transport_config_uart_t * hci_transport_config_uart = (hci_transport_config_uart_t*) transport_config;
    hci_transport_h4_uart_config.baudrate    = hci_transport_config_uart->baudrate_init;
    hci_transport_h4_uart_config.flowcontrol = hci_transport_config_uart->flowcontrol;
    hci_transport_h4_uart_config.parity      = hci_transport_config_uart->parity;
    hci_transport_h4_uart_config.device_name = hci_transport_config_uart->device_name;

    hci_transport_h4_js_tx_state_busy = false;
}

static int hci_transport_h4_js_open(void) {
    log_info("hci_transport_h4_js_open");
    // start async receiver
    EM_ASM({
        hci_transport_h4_js_receiver();
    });
    return 0;
}

EM_ASYNC_JS(int, hci_transport_h4_js_set_baudrate, (uint32_t baudrate), {
    await hci_transport_h4_js_set_baudrate_js(baudrate);
});

static int hci_transport_h4_js_close(void) {
    log_info("hci_transport_h4_js_close");
    return 0;
}

static void hci_transport_h4_js_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    hci_transport_h4_js_packet_handler = handler;
};

static int hci_transport_h4_js_send_packet(uint8_t packet_type, uint8_t *packet, int size){
    hci_send_packet(packet_type, packet, size);
    return 0;
}

// configure and return h4 singleton
static const hci_transport_t hci_transport_h4_js = {
    .name = "H4",
    .init = &hci_transport_h4_js_init,
    .open = &hci_transport_h4_js_open,
    .close = &hci_transport_h4_js_close,
    .register_packet_handler = &hci_transport_h4_js_register_packet_handler,
    .can_send_packet_now = &hci_transport_h4_js_can_send_now,
    .send_packet = &hci_transport_h4_js_send_packet,
    .set_baudrate = &hci_transport_h4_js_set_baudrate,
};

const hci_transport_t * hci_transport_h4_js_instance(void) {
    return &hci_transport_h4_js;
}
