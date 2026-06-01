/*
 * Copyright (C) 2026 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "hci_dump_buffered.c"

#include "btstack_config.h"

#include "hci_dump_buffered.h"

#include "btstack_run_loop.h"
#include "btstack_util.h"
#include "btstack_debug.h"

#include <stdio.h>
#include <string.h>

#define HCI_DUMP_BUFFERED_ENTRY_HEADER_SIZE 4u

typedef struct {
    uint8_t * buffer;
    uint32_t buffer_size;
    uint32_t bytes_stored;
    uint32_t flush_timeout_ms;
    const hci_dump_t * output;
    btstack_timer_source_t flush_timer;
} hci_dump_buffered_state_t;

static hci_dump_buffered_state_t hci_dump_buffered_state;

static const hci_dump_t hci_dump_buffered_instance;

// flush timer
static void hci_dump_buffered_flush_timeout_handler(btstack_timer_source_t * ts){
    UNUSED(ts);
    hci_dump_buffered_flush();
}

static void hci_dump_buffered_stop_timer(void){
    btstack_run_loop_remove_timer(&hci_dump_buffered_state.flush_timer);
}

static void hci_dump_buffered_restart_timer(void){
    if (hci_dump_buffered_state.flush_timeout_ms == 0u) {
        return;
    }
    if (hci_dump_buffered_state.bytes_stored == 0u) {
        return;
    }
    btstack_run_loop_remove_timer(&hci_dump_buffered_state.flush_timer);
    btstack_run_loop_set_timer(&hci_dump_buffered_state.flush_timer, hci_dump_buffered_state.flush_timeout_ms);
    btstack_run_loop_add_timer(&hci_dump_buffered_state.flush_timer);
}

// helper
static void hci_dump_buffered_forward_packet(uint8_t packet_type, uint8_t in, uint8_t * packet, uint16_t len){
    if (hci_dump_buffered_state.output == NULL) {
        return;
    }
    hci_dump_buffered_state.output->log_packet(packet_type, in, packet, len);
}

static bool hci_dump_buffered_can_store(uint32_t entry_size){
    return (hci_dump_buffered_state.buffer != NULL) && hci_dump_buffered_state.buffer_size >= entry_size;
}

static void hci_dump_buffered_store_packet(uint8_t packet_type, uint8_t in, uint8_t * packet, uint16_t len){
    uint32_t entry_size = HCI_DUMP_BUFFERED_ENTRY_HEADER_SIZE + len;

    // if packet is bigger than our buffer, flush buffer and forward current packet
    if (!hci_dump_buffered_can_store(entry_size)) {
        hci_dump_buffered_flush();
        hci_dump_buffered_forward_packet(packet_type, in, packet, len);
        return;
    }

    // if we cannot store new packet, flush buffer first
    if ((hci_dump_buffered_state.bytes_stored + entry_size) > hci_dump_buffered_state.buffer_size) {
        hci_dump_buffered_flush();
    }

    // store packet
    uint32_t offset = hci_dump_buffered_state.bytes_stored;
    hci_dump_buffered_state.buffer[offset] = packet_type;
    hci_dump_buffered_state.buffer[offset + 1u] = in;
    little_endian_store_16(hci_dump_buffered_state.buffer, (uint16_t)(offset + 2u), len);
    if ((len > 0u) && (packet != NULL)) {
        memcpy(&hci_dump_buffered_state.buffer[offset + HCI_DUMP_BUFFERED_ENTRY_HEADER_SIZE], packet, len);
    }
    hci_dump_buffered_state.bytes_stored += entry_size;

    // restart timer
    hci_dump_buffered_restart_timer();
}

void hci_dump_buffered_init(const hci_dump_t * hci_dump_impl, uint8_t * buffer, uint32_t buffer_size, uint32_t flush_timeout_ms){
    btstack_assert(hci_dump_impl != &hci_dump_buffered_instance);
    hci_dump_buffered_stop_timer();
    hci_dump_buffered_state.buffer = buffer;
    hci_dump_buffered_state.buffer_size = buffer_size;
    hci_dump_buffered_state.bytes_stored = 0;
    hci_dump_buffered_state.flush_timeout_ms = flush_timeout_ms;
    hci_dump_buffered_state.output = hci_dump_impl;
    btstack_run_loop_set_timer_handler(&hci_dump_buffered_state.flush_timer, &hci_dump_buffered_flush_timeout_handler);
}

void hci_dump_buffered_flush(void){
    uint32_t offset = 0;

    hci_dump_buffered_stop_timer();

    while (offset < hci_dump_buffered_state.bytes_stored){
        uint8_t packet_type = hci_dump_buffered_state.buffer[offset];
        uint8_t in          = hci_dump_buffered_state.buffer[offset + 1u];
        uint16_t len        = little_endian_read_16(hci_dump_buffered_state.buffer, (int)(offset + 2u));
        uint8_t * packet    = &hci_dump_buffered_state.buffer[offset + HCI_DUMP_BUFFERED_ENTRY_HEADER_SIZE];
        hci_dump_buffered_forward_packet(packet_type, in, packet, len);
        offset += HCI_DUMP_BUFFERED_ENTRY_HEADER_SIZE + len;
    }

    hci_dump_buffered_state.bytes_stored = 0;
}

// handle reset and forward call
static void hci_dump_buffered_reset(void){
    hci_dump_buffered_stop_timer();
    hci_dump_buffered_state.bytes_stored = 0;
    if ((hci_dump_buffered_state.output != NULL) && (hci_dump_buffered_state.output->reset != NULL)) {
        hci_dump_buffered_state.output->reset();
    }
}

static void hci_dump_buffered_log_packet(uint8_t packet_type, uint8_t in, uint8_t * packet, uint16_t len){
    hci_dump_buffered_store_packet(packet_type, in, packet, len);
}

static void hci_dump_buffered_log_message(int log_level, const char * format, va_list argptr){
    char log_message_buffer[HCI_DUMP_MAX_MESSAGE_LEN];
    int len;

    UNUSED(log_level);

    len = vsnprintf(log_message_buffer, sizeof(log_message_buffer), format, argptr);
    if (len < 0) {
        return;
    }
    uint16_t used_len = (uint16_t) btstack_min(sizeof(log_message_buffer) - 1,len);
    // strip trailing \n for messages caused by printf via ENABLE_PRINTF_TO_LOG
    if ((log_level == HCI_DUMP_LOG_LEVEL_PRINT) && (log_message_buffer[len - 1] == '\n')) {
        log_message_buffer[len - 1] = '\0';
        len--;
    }
    hci_dump_buffered_store_packet(LOG_MESSAGE_PACKET, 0, (uint8_t *) log_message_buffer, used_len);
}
static const hci_dump_t hci_dump_buffered_instance = {
    .reset = hci_dump_buffered_reset,
    .log_packet = hci_dump_buffered_log_packet,
    .log_message = hci_dump_buffered_log_message
};

const hci_dump_t * hci_dump_buffered_get_instance(void){
    return &hci_dump_buffered_instance;
}
