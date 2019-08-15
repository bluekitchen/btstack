/*
 * Copyright (C) 2016 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "hci_transport_h5.c"

/*
 *  hci_transport_h5.c
 *
 *  HCI Transport API implementation for basic H5 protocol
 *
 *  Created by Matthias Ringw ald on 4/29/09.
 */

#include <inttypes.h>

#include "hci.h"
#include "btstack_slip.h"
#include "btstack_debug.h"
#include "hci_transport.h"
#include "btstack_uart_block.h"

typedef enum {
    LINK_UNINITIALIZED,
    LINK_INITIALIZED,
    LINK_ACTIVE
} hci_transport_link_state_t;

typedef enum {
    HCI_TRANSPORT_LINK_SEND_SYNC                  = 1 <<  0,
    HCI_TRANSPORT_LINK_SEND_SYNC_RESPONSE         = 1 <<  1,
    HCI_TRANSPORT_LINK_SEND_CONFIG                = 1 <<  2,
    HCI_TRANSPORT_LINK_SEND_CONFIG_RESPONSE_EMPTY = 1 <<  3,
    HCI_TRANSPORT_LINK_SEND_CONFIG_RESPONSE       = 1 <<  4,
    HCI_TRANSPORT_LINK_SEND_SLEEP                 = 1 <<  5,
    HCI_TRANSPORT_LINK_SEND_WOKEN                 = 1 <<  6,
    HCI_TRANSPORT_LINK_SEND_WAKEUP                = 1 <<  7,
    HCI_TRANSPORT_LINK_SEND_QUEUED_PACKET         = 1 <<  8,
    HCI_TRANSPORT_LINK_SEND_ACK_PACKET            = 1 <<  9,
    HCI_TRANSPORT_LINK_ENTER_SLEEP                = 1 << 10,

} hci_transport_link_actions_t;

// Configuration Field. No packet buffers -> sliding window = 1, no OOF flow control, support data integrity check
#define LINK_CONFIG_SLIDING_WINDOW_SIZE 1
#define LINK_CONFIG_OOF_FLOW_CONTROL 0
#define LINK_CONFIG_DATA_INTEGRITY_CHECK 1
#define LINK_CONFIG_VERSION_NR 0
#define LINK_CONFIG_FIELD (LINK_CONFIG_SLIDING_WINDOW_SIZE | (LINK_CONFIG_OOF_FLOW_CONTROL << 3) | (LINK_CONFIG_DATA_INTEGRITY_CHECK << 4) | (LINK_CONFIG_VERSION_NR << 5))

// periodic sending during link establishment
#define LINK_PERIOD_MS 250

// resend wakeup
#define LINK_WAKEUP_MS 50

// additional packet types
#define LINK_ACKNOWLEDGEMENT_TYPE 0x00
#define LINK_CONTROL_PACKET_TYPE 0x0f

// max size of write requests
#define LINK_SLIP_TX_CHUNK_LEN 64

// ---
static const uint8_t link_control_sync[] =   { 0x01, 0x7e};
static const uint8_t link_control_sync_response[] = { 0x02, 0x7d};
static const uint8_t link_control_config[] = { 0x03, 0xfc, LINK_CONFIG_FIELD};
static const uint8_t link_control_config_prefix_len  = 2;
static const uint8_t link_control_config_response_empty[] = { 0x04, 0x7b};
static const uint8_t link_control_config_response[] = { 0x04, 0x7b, LINK_CONFIG_FIELD};
static const uint8_t link_control_config_response_prefix_len  = 2;
static const uint8_t link_control_wakeup[] = { 0x05, 0xfa};
static const uint8_t link_control_woken[] =  { 0x06, 0xf9};
static const uint8_t link_control_sleep[] =  { 0x07, 0x78};

// max size of link control messages
#define LINK_CONTROL_MAX_LEN 3

// incoming pre-bufffer + 4 bytes H5 header + max(acl header + acl payload, event header + event data) + 2 bytes opt CRC
static uint8_t   hci_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE + 6 + HCI_INCOMING_PACKET_BUFFER_SIZE];

// outgoing slip encoded buffer. +4 to assert that DIC fits in buffer. +1 to assert that last SOF fits in buffer.
static uint8_t   slip_outgoing_buffer[LINK_SLIP_TX_CHUNK_LEN+4+1];
static uint16_t  slip_outgoing_dic;
static uint16_t  slip_outgoing_dic_present;
static int       slip_write_active;

// H5 Link State
static hci_transport_link_state_t link_state;
static btstack_timer_source_t link_timer;
static uint8_t  link_seq_nr;
static uint8_t  link_ack_nr;
static uint16_t link_resend_timeout_ms;
static uint8_t  link_peer_asleep;
static uint8_t  link_peer_supports_data_integrity_check;

// auto sleep-mode
static btstack_timer_source_t inactivity_timer;
static uint16_t link_inactivity_timeout_ms; // auto-sleep if set

// Outgoing packet
static uint8_t   hci_packet_type;
static uint16_t  hci_packet_size;
static uint8_t * hci_packet;

// hci packet handler
static  void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size);

static int hci_transport_link_actions;

// UART Driver + Config
static const btstack_uart_block_t * btstack_uart;
static btstack_uart_config_t uart_config;
static btstack_uart_sleep_mode_t btstack_uart_sleep_mode;
static int hci_transport_bcsp_mode;

// Prototypes
static void hci_transport_h5_process_frame(uint16_t frame_size);
static int  hci_transport_link_have_outgoing_packet(void);
static void hci_transport_link_send_queued_packet(void);
static void hci_transport_link_set_timer(uint16_t timeout_ms);
static void hci_transport_link_timeout_handler(btstack_timer_source_t * timer);
static void hci_transport_link_run(void);
static void hci_transport_slip_init(void);

// -----------------------------
// CRC16-CCITT Calculation - compromise: use 32 byte table - 512 byte table would be faster, but that's too large

static const uint16_t crc16_ccitt_table[] ={
    0x0000, 0x1081, 0x2102, 0x3183,
    0x4204, 0x5285, 0x6306, 0x7387,
    0x8408, 0x9489, 0xa50a, 0xb58b,
    0xc60c, 0xd68d, 0xe70e, 0xf78f
};

static uint16_t crc16_ccitt_update (uint16_t crc, uint8_t ch){
    crc = (crc >> 4) ^ crc16_ccitt_table[(crc ^ ch) & 0x000f];
    crc = (crc >> 4) ^ crc16_ccitt_table[(crc ^ (ch >> 4)) & 0x000f];
    return crc;
}

static uint16_t btstack_reverse_bits_16(uint16_t value){
    int reverse = 0;
    int i;
    for (i = 0; i < 16; i++) { 
        reverse = reverse << 1;
        reverse |= value & 1;
        value = value >> 1;
    }
    return reverse;
}

static uint16_t crc16_calc_for_slip_frame(const uint8_t * header, const uint8_t * payload, uint16_t len){
    int i;
    uint16_t crc = 0xffff;
    for (i=0 ; i < 4 ; i++){
        crc = crc16_ccitt_update(crc, header[i]);
    }
    for (i=0 ; i < len ; i++){
        crc = crc16_ccitt_update(crc, payload[i]);
    }
    return btstack_reverse_bits_16(crc);
}

// -----------------------------
static void hci_transport_inactivity_timeout_handler(btstack_timer_source_t * ts){
    log_info("inactivity timeout. link state %d, peer asleep %u, actions 0x%02x, outgoing packet %u",
        link_state, link_peer_asleep, hci_transport_link_actions, hci_transport_link_have_outgoing_packet());
    if (hci_transport_link_have_outgoing_packet()) return;
    if (link_state != LINK_ACTIVE) return;
    if (hci_transport_link_actions) return;
    if (link_peer_asleep) return;
    hci_transport_link_actions |= HCI_TRANSPORT_LINK_SEND_SLEEP;
    hci_transport_link_run();
}

static void hci_transport_inactivity_timer_set(void){
    if (!link_inactivity_timeout_ms) return;
    btstack_run_loop_set_timer_handler(&inactivity_timer, &hci_transport_inactivity_timeout_handler);
    btstack_run_loop_set_timer(&inactivity_timer, link_inactivity_timeout_ms);
    btstack_run_loop_remove_timer(&inactivity_timer);
    btstack_run_loop_add_timer(&inactivity_timer);
}

// -----------------------------
// SLIP Outgoing

// Fill chunk and write
static void hci_transport_slip_encode_chunk_and_send(int pos){
    while (btstack_slip_encoder_has_data() & (pos < LINK_SLIP_TX_CHUNK_LEN)) {
        slip_outgoing_buffer[pos++] = btstack_slip_encoder_get_byte();
    }

    if (!btstack_slip_encoder_has_data()){
        // Payload encoded, append DIC if present.
        // note: slip_outgoing_buffer is guaranteed to be big enough to add DIC + SOF after LINK_SLIP_TX_CHUNK_LEN
        if (slip_outgoing_dic_present){
            uint8_t dic_buffer[2];
            big_endian_store_16(dic_buffer, 0, slip_outgoing_dic);
            btstack_slip_encoder_start(dic_buffer, 2);
            while (btstack_slip_encoder_has_data()){
                slip_outgoing_buffer[pos++] = btstack_slip_encoder_get_byte();
            } 
        }
        // Start of Frame
        slip_outgoing_buffer[pos++] = BTSTACK_SLIP_SOF;
    }
    slip_write_active = 1;
    log_debug("slip: send %d bytes", pos);
    btstack_uart->send_block(slip_outgoing_buffer, pos);
}

static inline void hci_transport_slip_send_next_chunk(void){
    hci_transport_slip_encode_chunk_and_send(0);
}

// format: 0xc0 HEADER PACKET [DIC] 0xc0
// @param uint8_t header[4]
static void hci_transport_slip_send_frame(const uint8_t * header, const uint8_t * packet, uint16_t packet_size, uint16_t data_integrity_check){
    
    int pos = 0;
    
    // store data integrity check info
    slip_outgoing_dic         = data_integrity_check;
    slip_outgoing_dic_present = header[0] & 0x40;

    // Start of Frame
    slip_outgoing_buffer[pos++] = BTSTACK_SLIP_SOF;

    // Header
    btstack_slip_encoder_start(header, 4);
    while (btstack_slip_encoder_has_data()){
        slip_outgoing_buffer[pos++] = btstack_slip_encoder_get_byte();
    }

    // Packet
    btstack_slip_encoder_start(packet, packet_size);

    // Fill rest of chunk from packet and send
    hci_transport_slip_encode_chunk_and_send(pos);
}

// SLIP Incoming

static void hci_transport_slip_init(void){
    btstack_slip_decoder_init(&hci_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE], 6 + HCI_INCOMING_PACKET_BUFFER_SIZE);
}

// H5 Three-Wire Implementation

static void hci_transport_link_calc_header(uint8_t * header,
    uint8_t  sequence_nr,
    uint8_t  acknowledgement_nr,
    uint8_t  data_integrity_check_present,
    uint8_t  reliable_packet,
    uint8_t  packet_type,
    uint16_t payload_length){

    header[0] = sequence_nr | (acknowledgement_nr << 3) | (data_integrity_check_present << 6) | (reliable_packet << 7);
    header[1] = packet_type | ((payload_length & 0x0f) << 4);
    header[2] = payload_length >> 4;
    header[3] = 0xff - (header[0] + header[1] + header[2]);
}

static void hci_transport_link_send_control(const uint8_t * message, int message_len){
    uint8_t header[4];
    hci_transport_link_calc_header(header, 0, 0, link_peer_supports_data_integrity_check, 0, LINK_CONTROL_PACKET_TYPE, message_len);
    uint16_t data_integrity_check = 0;    
    if (link_peer_supports_data_integrity_check){
        data_integrity_check = crc16_calc_for_slip_frame(header, message, message_len);
    }
    log_debug("hci_transport_link_send_control: size %u, append dic %u", message_len, link_peer_supports_data_integrity_check);
    log_debug_hexdump(message, message_len);
    hci_transport_slip_send_frame(header, message, message_len, data_integrity_check);
}

static void hci_transport_link_send_sync(void){
    log_debug("link send sync");
    hci_transport_link_send_control(link_control_sync, sizeof(link_control_sync));
}

static void hci_transport_link_send_sync_response(void){
    log_debug("link send sync response");
    hci_transport_link_send_control(link_control_sync_response, sizeof(link_control_sync_response));
}

static void hci_transport_link_send_config(void){
    log_debug("link send config");
    hci_transport_link_send_control(link_control_config, sizeof(link_control_config));
}

static void hci_transport_link_send_config_response(void){
    log_debug("link send config response");
    hci_transport_link_send_control(link_control_config_response, sizeof(link_control_config_response));
}

static void hci_transport_link_send_config_response_empty(void){
    log_debug("link send config response empty");
    hci_transport_link_send_control(link_control_config_response_empty, sizeof(link_control_config_response_empty));
}

static void hci_transport_link_send_woken(void){
    log_debug("link send woken");
    hci_transport_link_send_control(link_control_woken, sizeof(link_control_woken));
}

static void hci_transport_link_send_wakeup(void){
    log_debug("link send wakeup");
    hci_transport_link_send_control(link_control_wakeup, sizeof(link_control_wakeup));
}

static void hci_transport_link_send_sleep(void){
    log_debug("link send sleep");
    hci_transport_link_send_control(link_control_sleep, sizeof(link_control_sleep));
}

static void hci_transport_link_send_queued_packet(void){

    uint8_t header[4];
    hci_transport_link_calc_header(header, link_seq_nr, link_ack_nr, link_peer_supports_data_integrity_check, 1, hci_packet_type, hci_packet_size);

    uint16_t data_integrity_check = 0;
    if (link_peer_supports_data_integrity_check){
        data_integrity_check = crc16_calc_for_slip_frame(header, hci_packet, hci_packet_size);
    }
    log_debug("hci_transport_link_send_queued_packet: seq %u, ack %u, size %u. Append dic %u, dic = 0x%04x", link_seq_nr, link_ack_nr, hci_packet_size, link_peer_supports_data_integrity_check, data_integrity_check);
    log_debug_hexdump(hci_packet, hci_packet_size);

    hci_transport_slip_send_frame(header, hci_packet, hci_packet_size, data_integrity_check);

    // reset inactvitiy timer
    hci_transport_inactivity_timer_set();
}

static void hci_transport_link_send_ack_packet(void){
    // Pure ACK package is without DIC as there is no payload either
    log_debug("send ack %u", link_ack_nr);
    uint8_t header[4];
    hci_transport_link_calc_header(header, 0, link_ack_nr, 0, 0, LINK_ACKNOWLEDGEMENT_TYPE, 0);
    hci_transport_slip_send_frame(header, NULL, 0, 0);
}

static void hci_transport_link_run(void){
    // exit if outgoing active
    if (slip_write_active) return;

    // process queued requests
    if (hci_transport_link_actions & HCI_TRANSPORT_LINK_SEND_SYNC){
        hci_transport_link_actions &= ~HCI_TRANSPORT_LINK_SEND_SYNC;
        hci_transport_link_send_sync();
        return;
    }
    if (hci_transport_link_actions & HCI_TRANSPORT_LINK_SEND_SYNC_RESPONSE){
        hci_transport_link_actions &= ~HCI_TRANSPORT_LINK_SEND_SYNC_RESPONSE;
        hci_transport_link_send_sync_response();
        return;
    }
    if (hci_transport_link_actions & HCI_TRANSPORT_LINK_SEND_CONFIG){
        hci_transport_link_actions &= ~HCI_TRANSPORT_LINK_SEND_CONFIG;
        hci_transport_link_send_config();
        return;
    }
    if (hci_transport_link_actions & HCI_TRANSPORT_LINK_SEND_CONFIG_RESPONSE){
        hci_transport_link_actions &= ~HCI_TRANSPORT_LINK_SEND_CONFIG_RESPONSE;
        hci_transport_link_send_config_response();
        return;
    }
    if (hci_transport_link_actions & HCI_TRANSPORT_LINK_SEND_CONFIG_RESPONSE_EMPTY){
        hci_transport_link_actions &= ~HCI_TRANSPORT_LINK_SEND_CONFIG_RESPONSE_EMPTY;
        hci_transport_link_send_config_response_empty();
        return;
    }
    if (hci_transport_link_actions & HCI_TRANSPORT_LINK_SEND_WOKEN){
        hci_transport_link_actions &= ~HCI_TRANSPORT_LINK_SEND_WOKEN;
        hci_transport_link_send_woken();
        return;
    }
    if (hci_transport_link_actions & HCI_TRANSPORT_LINK_SEND_WAKEUP){
        hci_transport_link_actions &= ~HCI_TRANSPORT_LINK_SEND_WAKEUP;
        hci_transport_link_send_wakeup();
        return;
    }
    if (hci_transport_link_actions & HCI_TRANSPORT_LINK_SEND_QUEUED_PACKET){
        hci_transport_link_actions &= ~HCI_TRANSPORT_LINK_SEND_QUEUED_PACKET;
        // packet already contains ack, no need to send addtitional one
        hci_transport_link_actions &= ~HCI_TRANSPORT_LINK_SEND_ACK_PACKET;
        hci_transport_link_send_queued_packet();
        return;
    }
    if (hci_transport_link_actions & HCI_TRANSPORT_LINK_SEND_ACK_PACKET){
        hci_transport_link_actions &= ~HCI_TRANSPORT_LINK_SEND_ACK_PACKET;
        hci_transport_link_send_ack_packet();
        return;
    }
    if (hci_transport_link_actions & HCI_TRANSPORT_LINK_SEND_SLEEP){
        hci_transport_link_actions &= ~HCI_TRANSPORT_LINK_SEND_SLEEP;
        hci_transport_link_actions |=  HCI_TRANSPORT_LINK_ENTER_SLEEP;
        link_peer_asleep = 1;
        hci_transport_link_send_sleep();
        return;
    }
}

static void hci_transport_link_set_timer(uint16_t timeout_ms){
    btstack_run_loop_set_timer_handler(&link_timer, &hci_transport_link_timeout_handler);
    btstack_run_loop_set_timer(&link_timer, timeout_ms);
    btstack_run_loop_add_timer(&link_timer);
}

static void hci_transport_link_timeout_handler(btstack_timer_source_t * timer){
    switch (link_state){
        case LINK_UNINITIALIZED:
            hci_transport_link_actions |= HCI_TRANSPORT_LINK_SEND_SYNC;
            hci_transport_link_set_timer(LINK_PERIOD_MS);
            break;            
        case LINK_INITIALIZED:
            hci_transport_link_actions |= HCI_TRANSPORT_LINK_SEND_CONFIG;
            hci_transport_link_set_timer(LINK_PERIOD_MS);
            break;
        case LINK_ACTIVE:
            if (!hci_transport_link_have_outgoing_packet()){
                log_info("h5 timeout while active, but no outgoing packet");
                return;
            }
            if (link_peer_asleep){
                hci_transport_link_actions |= HCI_TRANSPORT_LINK_SEND_WAKEUP;
                hci_transport_link_set_timer(LINK_WAKEUP_MS);
                return;
            }
            // resend packet
            hci_transport_link_actions |= HCI_TRANSPORT_LINK_SEND_QUEUED_PACKET;
            hci_transport_link_set_timer(link_resend_timeout_ms);
            break;
        default:
            break;
    }

    hci_transport_link_run();
}

static void hci_transport_link_init(void){
    link_state = LINK_UNINITIALIZED;
    link_peer_asleep = 0;
    link_peer_supports_data_integrity_check = 0;
 
    // get started
    hci_transport_link_actions |= HCI_TRANSPORT_LINK_SEND_SYNC;
    hci_transport_link_set_timer(LINK_PERIOD_MS);
    hci_transport_link_run();
}

static int hci_transport_link_inc_seq_nr(int seq_nr){
    return (seq_nr + 1) & 0x07;    
}

static int hci_transport_link_have_outgoing_packet(void){
    return hci_packet != 0;
}

static void hci_transport_link_clear_queue(void){
    btstack_run_loop_remove_timer(&link_timer);
    hci_packet = NULL;
}

static void hci_transport_h5_queue_packet(uint8_t packet_type, uint8_t *packet, int size){
    hci_packet = packet;
    hci_packet_type = packet_type;
    hci_packet_size = size;
}

static void hci_transport_h5_emit_sleep_state(int sleep_active){
    static int last_state = 0;
    if (sleep_active == last_state) return;
    last_state = sleep_active;
    
    log_info("emit_sleep_state: %u", sleep_active);
    uint8_t event[3];
    event[0] = HCI_EVENT_TRANSPORT_SLEEP_MODE;
    event[1] = sizeof(event) - 2;
    event[2] = sleep_active;
    packet_handler(HCI_EVENT_PACKET, &event[0], sizeof(event));        
}

static void hci_transport_h5_process_frame(uint16_t frame_size){

    if (frame_size < 4) return;

    uint8_t * slip_header  = &hci_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE];
    uint8_t * slip_payload = &hci_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE + 4];
    int       frame_size_without_header = frame_size - 4;

    uint8_t  seq_nr =  slip_header[0] & 0x07;
    uint8_t  ack_nr = (slip_header[0] >> 3)    & 0x07;
    uint8_t  data_integrity_check_present = (slip_header[0] & 0x40) != 0;
    uint8_t  reliable_packet  = (slip_header[0] & 0x80) != 0;
    uint8_t  link_packet_type = slip_header[1] & 0x0f;
    uint16_t link_payload_len = (slip_header[1] >> 4) | (slip_header[2] << 4);

    log_debug("process_frame, reliable %u, packet type %u, seq_nr %u, ack_nr %u , dic %u, payload 0x%04x bytes", reliable_packet, link_packet_type, seq_nr, ack_nr, data_integrity_check_present, frame_size_without_header);
    log_debug_hexdump(slip_header, 4);
    log_debug_hexdump(slip_payload, frame_size_without_header);

    // CSR 8811 does not seem to auto-detect H5 mode and sends data with even parity.
    // if this byte sequence is detected, just enable even parity
    const uint8_t sync_response_bcsp[] = {0x01, 0x7a, 0x06, 0x10};
    if (memcmp(sync_response_bcsp, slip_header, 4) == 0){
        log_info("detected BSCP SYNC sent with Even Parity -> discard frame and enable Even Parity");
        btstack_uart->set_parity(1);
        return;
    }

    // validate header checksum
    uint8_t header_checksum = slip_header[0] + slip_header[1] + slip_header[2] + slip_header[3];
    if (header_checksum != 0xff){
        log_info("header checksum 0x%02x (instead of 0xff)", header_checksum);
        return;
    }

    // validate payload length
    int data_integrity_len = data_integrity_check_present ? 2 : 0;
    uint16_t received_payload_len = frame_size_without_header - data_integrity_len;
    if (link_payload_len != received_payload_len){
        log_info("expected payload len %u but got %u", link_payload_len, received_payload_len);
        return;
    }

    // validate data integrity check
    if (data_integrity_check_present){
        uint16_t dic_packet = big_endian_read_16(slip_payload, received_payload_len);
        uint16_t dic_calculate = crc16_calc_for_slip_frame(slip_header, slip_payload, received_payload_len);
        if (dic_packet != dic_calculate){
            log_info("expected dic value 0x%04x but got 0x%04x", dic_calculate, dic_packet);
            return;
        }
    }

    switch (link_state){
        case LINK_UNINITIALIZED:
            if (link_packet_type != LINK_CONTROL_PACKET_TYPE) break;
            if (memcmp(slip_payload, link_control_sync, sizeof(link_control_sync)) == 0){
                log_debug("link received sync");
                hci_transport_link_actions |= HCI_TRANSPORT_LINK_SEND_SYNC_RESPONSE;
                break;
            }
            if (memcmp(slip_payload, link_control_sync_response, sizeof(link_control_sync_response)) == 0){
                log_debug("link received sync response");
                link_state = LINK_INITIALIZED;
                btstack_run_loop_remove_timer(&link_timer);
                log_info("link initialized");
                //
                hci_transport_link_actions |= HCI_TRANSPORT_LINK_SEND_CONFIG;
                hci_transport_link_set_timer(LINK_PERIOD_MS);
                break;
            }
            break;
        case LINK_INITIALIZED:
            if (link_packet_type != LINK_CONTROL_PACKET_TYPE) break;
            if (memcmp(slip_payload, link_control_sync, sizeof(link_control_sync)) == 0){
                log_debug("link received sync");
                hci_transport_link_actions |= HCI_TRANSPORT_LINK_SEND_SYNC_RESPONSE;
                break;
            }
            if (memcmp(slip_payload, link_control_config, link_control_config_prefix_len) == 0){
                if (link_payload_len == link_control_config_prefix_len){
                    log_debug("link received config, no config field");
                    hci_transport_link_actions |= HCI_TRANSPORT_LINK_SEND_CONFIG_RESPONSE_EMPTY;
                } else {
                    log_debug("link received config, 0x%02x", slip_payload[2]);
                    hci_transport_link_actions |= HCI_TRANSPORT_LINK_SEND_CONFIG_RESPONSE;
                }
                break;
            }
            if (memcmp(slip_payload, link_control_config_response, link_control_config_response_prefix_len) == 0){
                uint8_t config = slip_payload[2];
                link_peer_supports_data_integrity_check = (config & 0x10) != 0;
                log_info("link received config response 0x%02x, data integrity check supported %u", config, link_peer_supports_data_integrity_check);
                link_state = LINK_ACTIVE;
                btstack_run_loop_remove_timer(&link_timer);
                log_info("link activated");
                // 
                link_seq_nr = 0;
                link_ack_nr = 0;
                // notify upper stack that it can start
                uint8_t event[] = { HCI_EVENT_TRANSPORT_PACKET_SENT, 0};
                packet_handler(HCI_EVENT_PACKET, &event[0], sizeof(event));
                break;
            }
            break;
        case LINK_ACTIVE:

            // validate packet sequence nr in reliable packets (check for out of sequence error)
            if (reliable_packet){
                if (seq_nr != link_ack_nr){
                    log_info("expected seq nr %u, but received %u", link_ack_nr, seq_nr);
                    hci_transport_link_actions |= HCI_TRANSPORT_LINK_SEND_ACK_PACKET;
                    break;
                }
                // ack packet right away
                link_ack_nr = hci_transport_link_inc_seq_nr(link_ack_nr);
                hci_transport_link_actions |= HCI_TRANSPORT_LINK_SEND_ACK_PACKET;
            }

            // Process ACKs in reliable packet and explicit ack packets
            if (reliable_packet || link_packet_type == LINK_ACKNOWLEDGEMENT_TYPE){
                // our packet is good if the remote expects our seq nr + 1
                int next_seq_nr = hci_transport_link_inc_seq_nr(link_seq_nr);
                if (hci_transport_link_have_outgoing_packet() && next_seq_nr == ack_nr){
                    log_debug("outoing packet with seq %u ack'ed", link_seq_nr);
                    link_seq_nr = next_seq_nr;
                    hci_transport_link_clear_queue();

                    // notify upper stack that it can send again
                    uint8_t event[] = { HCI_EVENT_TRANSPORT_PACKET_SENT, 0};
                    packet_handler(HCI_EVENT_PACKET, &event[0], sizeof(event));
                }
            } 

            switch (link_packet_type){
                case LINK_CONTROL_PACKET_TYPE:
                    if (memcmp(slip_payload, link_control_config, sizeof(link_control_config)) == 0){
                        if (link_payload_len == link_control_config_prefix_len){
                            log_debug("link received config, no config field");
                            hci_transport_link_actions |= HCI_TRANSPORT_LINK_SEND_CONFIG_RESPONSE_EMPTY;
                        } else {
                            log_debug("link received config, 0x%02x", slip_payload[2]);
                            hci_transport_link_actions |= HCI_TRANSPORT_LINK_SEND_CONFIG_RESPONSE;
                        }
                        break;
                    }
                    if (memcmp(slip_payload, link_control_sync, sizeof(link_control_sync)) == 0){
                        log_debug("link received sync in ACTIVE STATE!");
                        // TODO sync during active indicates peer reset -> full upper layer reset necessary
                        break;
                    }
                    if (memcmp(slip_payload, link_control_sleep, sizeof(link_control_sleep)) == 0){
                        if (btstack_uart_sleep_mode){
                            log_info("link: received sleep message. Enabling UART Sleep.");
                            btstack_uart->set_sleep(btstack_uart_sleep_mode);
                            hci_transport_h5_emit_sleep_state(1);
                        } else {
                            log_info("link: received sleep message. UART Sleep not supported");
                        }
                        link_peer_asleep = 1;
                        break;
                    }
                    if (memcmp(slip_payload, link_control_wakeup, sizeof(link_control_wakeup)) == 0){
                        log_info("link: received wakupe message -> send woken");
                        link_peer_asleep = 0;
                        hci_transport_link_actions |= HCI_TRANSPORT_LINK_SEND_WOKEN;
                        break;
                    }
                    if (memcmp(slip_payload, link_control_woken, sizeof(link_control_woken)) == 0){
                        log_info("link: received woken message");
                        link_peer_asleep = 0;
                        // queued packet will be sent in hci_transport_link_run if needed
                        break;
                    }
                    break;
                case HCI_EVENT_PACKET:
                case HCI_ACL_DATA_PACKET:
                case HCI_SCO_DATA_PACKET:
                    // seems like peer is awake
                    link_peer_asleep = 0;
                    // forward packet to stack
                    packet_handler(link_packet_type, slip_payload, link_payload_len);
                    // reset inactvitiy timer
                    hci_transport_inactivity_timer_set();
                    break;
            }

            break;
        default:
            break;
    }

    hci_transport_link_run();
}

// recommendet time until resend: 3 * time of largest packet
static uint16_t hci_transport_link_calc_resend_timeout(uint32_t baudrate){
    uint32_t max_packet_size_in_bit = (HCI_INCOMING_PACKET_BUFFER_SIZE + 6) << 3;
    uint32_t t_max_x3_ms = max_packet_size_in_bit * 3000 / baudrate;

    // allow for BTstack logging and other delays
    t_max_x3_ms += 50;

    log_info("resend timeout for %"PRIu32" baud: %u ms", baudrate, (int) t_max_x3_ms);
    return t_max_x3_ms;
}

static void hci_transport_link_update_resend_timeout(uint32_t baudrate){
    link_resend_timeout_ms = hci_transport_link_calc_resend_timeout(baudrate);
}

/// H5 Interface

static uint8_t hci_transport_link_read_byte;
static int hci_transport_h5_active;

static void hci_transport_h5_read_next_byte(void){
    btstack_uart->receive_block(&hci_transport_link_read_byte, 1);    
}

// track time receiving SLIP frame
static uint32_t hci_transport_h5_receive_start;
static void hci_transport_h5_block_received(){
    if (hci_transport_h5_active == 0) return;

    // track start time when receiving first byte // a bit hackish
    if (hci_transport_h5_receive_start == 0 && hci_transport_link_read_byte != BTSTACK_SLIP_SOF){
        hci_transport_h5_receive_start = btstack_run_loop_get_time_ms();
    }
    btstack_slip_decoder_process(hci_transport_link_read_byte);
    uint16_t frame_size = btstack_slip_decoder_frame_size();
    if (frame_size) {
        // track time
        uint32_t packet_receive_time = btstack_run_loop_get_time_ms() - hci_transport_h5_receive_start;
        uint32_t nominmal_time = (frame_size + 6) * 10 * 1000 / uart_config.baudrate;
        log_info("slip frame time %u ms for %u decoded bytes. nomimal time %u ms", (int) packet_receive_time, frame_size, (int) nominmal_time);
        // reset state
        hci_transport_h5_receive_start = 0;
        // 
        hci_transport_h5_process_frame(frame_size);
        hci_transport_slip_init();
    }
    hci_transport_h5_read_next_byte();
}

static void hci_transport_h5_block_sent(void){
    if (hci_transport_h5_active == 0) return;

    // check if more data to send
    if (btstack_slip_encoder_has_data()){
        hci_transport_slip_send_next_chunk();
        return;
    }

    // done
    slip_write_active = 0;

    // enter sleep mode after sending sleep message
    if (hci_transport_link_actions & HCI_TRANSPORT_LINK_ENTER_SLEEP){
        hci_transport_link_actions &= ~HCI_TRANSPORT_LINK_ENTER_SLEEP;
        if (btstack_uart_sleep_mode){
            log_info("link: sent sleep message. Enabling UART Sleep.");
            btstack_uart->set_sleep(btstack_uart_sleep_mode);
        } else {
            log_info("link: sent sleep message. UART Sleep not supported");
        }
        hci_transport_h5_emit_sleep_state(1);
    }

    hci_transport_link_run();
}

static void hci_transport_h5_init(const void * transport_config){
    // check for hci_transport_config_uart_t
    if (!transport_config) {
        log_error("hci_transport_h5: no config!");
        return;
    }
    if (((hci_transport_config_t*)transport_config)->type != HCI_TRANSPORT_CONFIG_UART) {
        log_error("hci_transport_h5: config not of type != HCI_TRANSPORT_CONFIG_UART!");
        return;
    }

    hci_transport_h5_active = 0;

    // extract UART config from transport config
    hci_transport_config_uart_t * hci_transport_config_uart = (hci_transport_config_uart_t*) transport_config;
    uart_config.baudrate    = hci_transport_config_uart->baudrate_init;
    uart_config.flowcontrol = hci_transport_config_uart->flowcontrol;
    uart_config.device_name = hci_transport_config_uart->device_name;

    // setup UART driver
    btstack_uart->init(&uart_config);
    btstack_uart->set_block_received(&hci_transport_h5_block_received);
    btstack_uart->set_block_sent(&hci_transport_h5_block_sent);
}

static int hci_transport_h5_open(void){
    int res = btstack_uart->open();
    if (res){
        return res;
    }        
    
    // 
    if (hci_transport_bcsp_mode){
        log_info("enable even parity for BCSP mode");
        btstack_uart->set_parity(1);
    }

    // check if wake on RX can be used
    btstack_uart_sleep_mode = BTSTACK_UART_SLEEP_OFF;
    int supported_sleep_modes = 0;
    if (btstack_uart->get_supported_sleep_modes){
        supported_sleep_modes = btstack_uart->get_supported_sleep_modes();
    }
    if (supported_sleep_modes & BTSTACK_UART_SLEEP_MASK_RTS_LOW_WAKE_ON_RX_EDGE){
        log_info("using wake on RX");
        btstack_uart_sleep_mode = BTSTACK_UART_SLEEP_RTS_LOW_WAKE_ON_RX_EDGE;
    } else {
        log_info("UART driver does not provide compatible sleep mode");
    }

    // setup resend timeout
    hci_transport_link_update_resend_timeout(uart_config.baudrate);

    // init slip parser state machine
    hci_transport_slip_init();

    // init link management - already starts syncing
    hci_transport_link_init();

    // start receiving
    hci_transport_h5_active = 1;
    hci_transport_h5_read_next_byte();

    return 0;
}

static int hci_transport_h5_close(void){
    hci_transport_h5_active = 0;
    return btstack_uart->close();
}

static void hci_transport_h5_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    packet_handler = handler;
}

static int hci_transport_h5_can_send_packet_now(uint8_t packet_type){
    int res = !hci_transport_link_have_outgoing_packet() && link_state == LINK_ACTIVE;
    // log_info("can_send_packet_now: %u", res);
    return res;
}

static int hci_transport_h5_send_packet(uint8_t packet_type, uint8_t *packet, int size){
    if (!hci_transport_h5_can_send_packet_now(packet_type)){
        log_error("hci_transport_h5_send_packet called but in state %d", link_state);
        return -1;
    }

    // store request
    hci_transport_h5_queue_packet(packet_type, packet, size);

    // send wakeup first
    if (link_peer_asleep){
        hci_transport_h5_emit_sleep_state(0);
        if (btstack_uart_sleep_mode){
            log_info("disable UART sleep");
            btstack_uart->set_sleep(BTSTACK_UART_SLEEP_OFF);
        }
        hci_transport_link_actions |= HCI_TRANSPORT_LINK_SEND_WAKEUP;
        hci_transport_link_set_timer(LINK_WAKEUP_MS);
    } else {
        hci_transport_link_actions |= HCI_TRANSPORT_LINK_SEND_QUEUED_PACKET;
        hci_transport_link_set_timer(link_resend_timeout_ms);
    }
    hci_transport_link_run();
    return 0;
}

static int hci_transport_h5_set_baudrate(uint32_t baudrate){

    log_info("set_baudrate %"PRIu32, baudrate);
    int res = btstack_uart->set_baudrate(baudrate);

    if (res) return res;
    uart_config.baudrate = baudrate;
    hci_transport_link_update_resend_timeout(baudrate);
    return 0;
}

static void hci_transport_h5_reset_link(void){

    log_info("reset_link");

    // clear outgoing queue
    hci_transport_link_clear_queue();

    // init slip parser state machine
    hci_transport_slip_init();
    
    // init link management - already starts syncing
    hci_transport_link_init();
}

static const hci_transport_t hci_transport_h5 = {
    /* const char * name; */                                        "H5",
    /* void   (*init) (const void *transport_config); */            &hci_transport_h5_init,
    /* int    (*open)(void); */                                     &hci_transport_h5_open,
    /* int    (*close)(void); */                                    &hci_transport_h5_close,
    /* void   (*register_packet_handler)(void (*handler)(...); */   &hci_transport_h5_register_packet_handler,
    /* int    (*can_send_packet_now)(uint8_t packet_type); */       &hci_transport_h5_can_send_packet_now,
    /* int    (*send_packet)(...); */                               &hci_transport_h5_send_packet,
    /* int    (*set_baudrate)(uint32_t baudrate); */                &hci_transport_h5_set_baudrate,
    /* void   (*reset_link)(void); */                               &hci_transport_h5_reset_link,
    /* void   (*set_sco_config)(uint16_t voice_setting, int num_connections); */ NULL, 
};

// configure and return h5 singleton
const hci_transport_t * hci_transport_h5_instance(const btstack_uart_block_t * uart_driver) {
    btstack_uart = uart_driver;
    return &hci_transport_h5;
}

void hci_transport_h5_set_auto_sleep(uint16_t inactivity_timeout_ms){
    link_inactivity_timeout_ms = inactivity_timeout_ms;
}

void hci_transport_h5_enable_bcsp_mode(void){
    hci_transport_bcsp_mode = 1;
}
