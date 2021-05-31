/*
 * Copyright (C) 2021 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "btstack_uart_slip_wrapper.c"

/*
 *  btstack_uart_slip_wrapper.c
 *
 *
 */

#include "btstack_uart_slip_wrapper.h"

#include "btstack_run_loop.h"
#include "btstack_debug.h"
#include "btstack_defines.h"
#include "btstack_slip.h"
#include "btstack_util.h"

// max size of outgoing SLIP chunks
#define SLIP_TX_CHUNK_LEN   128

#define SLIP_RECEIVE_BUFFER_SIZE 128

// wrapped driver
static const btstack_uart_t * original_uart;

// callbacks
static void (*frame_sent)(void);
static void (*frame_received)(uint16_t frame_size);

// SLIP encoding
static uint8_t  btstack_uart_slip_outgoing_buffer[SLIP_TX_CHUNK_LEN+1];

// SLIP decoding
static uint8_t  btstack_uart_slip_wrapper_read_byte;
static uint32_t btstack_uart_slip_wrapper_receive_start;
static uint32_t btstack_uart_slip_wrapper_baudrate;


// SLIP ENCODING

static void btstack_uart_slip_posix_encode_chunk_and_send(void){
    uint16_t pos = 0;
    while (btstack_slip_encoder_has_data() & (pos < SLIP_TX_CHUNK_LEN)) {
        btstack_uart_slip_outgoing_buffer[pos++] = btstack_slip_encoder_get_byte();
    }

    // setup async write and start sending
    original_uart->send_block(btstack_uart_slip_outgoing_buffer, pos);
}

static void btstack_uart_slip_wrapper_block_sent(void){
    // check if more data to send
    if (btstack_slip_encoder_has_data()){
        btstack_uart_slip_posix_encode_chunk_and_send();
        return;
    }

    // notify done
    if (frame_sent){
        frame_sent();
    }
}

static void btstack_uart_slip_wrapper_send_frame(const uint8_t * frame, uint16_t frame_size){

    // Prepare encoding of Header + Packet (+ DIC)
    btstack_slip_encoder_start(frame, frame_size);

    // Fill rest of chunk from packet and send
    btstack_uart_slip_posix_encode_chunk_and_send();
}

static void btstack_uart_slip_wrapper_set_frame_sent( void (*frame_handler)(void)){
    frame_sent = frame_handler;
    original_uart->set_block_sent(&btstack_uart_slip_wrapper_block_sent);
}

// SLIP DECODING

static void btstack_uart_slip_wrapper_read_next_byte(void){
    original_uart->receive_block(&btstack_uart_slip_wrapper_read_byte, 1);
}

// track time receiving SLIP frame
static void btstack_uart_slip_wrapper_block_received(void){

    // track start time when receiving first byte // a bit hackish
    if ((btstack_uart_slip_wrapper_receive_start == 0u) && (btstack_uart_slip_wrapper_read_byte != BTSTACK_SLIP_SOF)){
        btstack_uart_slip_wrapper_receive_start = btstack_run_loop_get_time_ms();
    }
    btstack_slip_decoder_process(btstack_uart_slip_wrapper_read_byte);
    uint16_t frame_size = btstack_slip_decoder_frame_size();
    if (frame_size) {
        // track time
        uint32_t packet_receive_time = btstack_run_loop_get_time_ms() - btstack_uart_slip_wrapper_receive_start;
        uint32_t nominal_time = (frame_size + 6u) * 10u * 1000u / btstack_uart_slip_wrapper_baudrate;
        UNUSED(nominal_time);
        UNUSED(packet_receive_time);
        if (packet_receive_time > btstack_max(2 * nominal_time, 5)){
            log_info("slip frame time %u ms for %u decoded bytes. nominal time %u ms", (int) packet_receive_time, frame_size, (int) nominal_time);
        }
        // reset state
        btstack_uart_slip_wrapper_receive_start = 0;
        // deliver frame
        frame_received(frame_size);
    } else {
        btstack_uart_slip_wrapper_read_next_byte();
    }
}

static void btstack_uart_slip_wrapper_set_frame_received( void (*frame_handler)(uint16_t frame_size)){
    frame_received = frame_handler;
    original_uart->set_block_received(&btstack_uart_slip_wrapper_block_received);
}

static void btstack_uart_slip_wrapper_receive_frame(uint8_t *buffer, uint16_t len){
    btstack_slip_decoder_init(buffer, len);
    btstack_uart_slip_wrapper_read_next_byte();
}

// SLIP End

static int btstack_uart_slip_wrapper_init(const btstack_uart_config_t * config){
    btstack_uart_slip_wrapper_baudrate = config->baudrate;
    return original_uart->init(config);
}

static int btstack_uart_slip_wrapper_set_baudrate(uint32_t baudrate){
    if (original_uart->set_baudrate != NULL){
        btstack_uart_slip_wrapper_baudrate = baudrate;
        return original_uart->set_baudrate(baudrate);
    }
    return 0;
}

static int btstack_uart_slip_wrapper_set_parity(int parity){
    if (original_uart->set_baudrate != NULL) {
        return original_uart->set_parity(parity);
    }
    return 0;
}

static int btstack_uart_slip_wrapper_set_flowcontrol(int flowcontrol){
    if (original_uart->set_baudrate != NULL) {
        return original_uart->set_flowcontrol(flowcontrol);
    }
    return 0;
}

static int btstack_uart_slip_wrapper_open(void){
    return original_uart->open();
}

static int btstack_uart_slip_wrapper_close(void){
    return original_uart->close();
}

static void btstack_uart_slip_wrapper_set_block_received( void (*block_handler)(void)){
    UNUSED(block_handler);
    btstack_assert(false);
}

static void btstack_uart_slip_wrapper_set_block_sent( void (*block_handler)(void)){
    UNUSED(block_handler);
    btstack_assert(false);
}

// SLEEP support

static int btstack_uart_slip_wrapper_get_supported_sleep_modes(void){
    if (original_uart->get_supported_sleep_modes != NULL){
        return original_uart->get_supported_sleep_modes();
    }
    return 0;
}

static void btstack_uart_slip_wrapper_set_sleep(btstack_uart_sleep_mode_t sleep_mode){
    if (original_uart->set_sleep != NULL){
        original_uart->set_sleep(sleep_mode);
    }
}

static void btstack_uart_slip_wrapper_set_wakeup_handler(void (*handler)(void)){
    if (original_uart->set_wakeup_handler != NULL){
        original_uart->set_wakeup_handler(handler);
    }
}

const btstack_uart_t * btstack_uart_slip_wrapper_instance(const btstack_uart_t * uart_without_slip){
    static const btstack_uart_t btstack_uart_slip_wrapper = {
            /* int  (*init)(hci_transport_config_uart_t * config); */         &btstack_uart_slip_wrapper_init,
            /* int  (*open)(void); */                                         &btstack_uart_slip_wrapper_open,
            /* int  (*close)(void); */                                        &btstack_uart_slip_wrapper_close,
            /* void (*set_block_received)(void (*handler)(void)); */          &btstack_uart_slip_wrapper_set_block_received,
            /* void (*set_block_sent)(void (*handler)(void)); */              &btstack_uart_slip_wrapper_set_block_sent,
            /* int  (*set_baudrate)(uint32_t baudrate); */                    &btstack_uart_slip_wrapper_set_baudrate,
            /* int  (*set_parity)(int parity); */                             &btstack_uart_slip_wrapper_set_parity,
            /* int  (*set_flowcontrol)(int flowcontrol); */                   &btstack_uart_slip_wrapper_set_flowcontrol,

            /* void (*receive_block)(uint8_t *buffer, uint16_t len); */       NULL,
            /* void (*send_block)(const uint8_t *buffer, uint16_t length); */ NULL,

            /* int (*get_supported_sleep_modes); */                           &btstack_uart_slip_wrapper_get_supported_sleep_modes,
            /* void (*set_sleep)(btstack_uart_sleep_mode_t sleep_mode); */    &btstack_uart_slip_wrapper_set_sleep,
            /* void (*set_wakeup_handler)(void (*handler)(void)); */          &btstack_uart_slip_wrapper_set_wakeup_handler,

            /* void (*set_frame_received)(void (*cb)(uint16_t frame_size) */  &btstack_uart_slip_wrapper_set_frame_received,
            /* void (*set_frame_sent)(void (*block_handler)(void)); */        &btstack_uart_slip_wrapper_set_frame_sent,
            /* void (*receive_frame)(uint8_t *buffer, uint16_t len); */       &btstack_uart_slip_wrapper_receive_frame,
            /* void (*send_frame)(const uint8_t *buffer, uint16_t length); */ &btstack_uart_slip_wrapper_send_frame
    };
    original_uart = uart_without_slip;
    return &btstack_uart_slip_wrapper;
}
