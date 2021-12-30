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

#define BTSTACK_FILE__ "ll_nrf5.c"

#define DEBUG

#include <string.h>
#include <inttypes.h>
#include <stdio.h>

#include "ll.h"

#include "btstack_config.h"
#include "btstack_debug.h"
#include "btstack_memory.h"
#include "btstack_memory_pool.h"
#include "btstack_linked_queue.h"
#include "bluetooth_company_id.h"
#include "hal_cpu.h"
#include "hci_event.h"
#include "hopping.h"
#include "hal_timer.h"
#include "radio.h"
#include "nrf.h"

//
// configuration
//

// bluetooth
// bluetooth.h
#define ADVERTISING_RADIO_ACCESS_ADDRESS 0x8E89BED6
#define ADVERTISING_CRC_INIT             0x555555

#define ACL_LE_MAX_PAYLOAD 31
#define ADV_MAX_PAYLOAD    (6+6+22)
#define LL_MAX_PAYLOAD      37

// sync hop delay - time we prepare for next connection event
#define SYNC_HOP_DELAY_US                           700

// timeout between RX complete and next RX packet
#define TX_TO_RX_TIMEOUT_US                         250

// num tx buffers for use by link layer
#define HCI_NUM_TX_BUFFERS_LL                       4

// num rx buffers
#define HCI_NUM_RX_BUFFERS                          16

// total number PDU buffers
#define MAX_NUM_LL_PDUS (HCI_NUM_TX_BUFFERS_STACK + HCI_NUM_TX_BUFFERS_LL + HCI_NUM_RX_BUFFERS)

// HCI Connection Handle used for all HCI events/connections
#define HCI_CON_HANDLE 0x0001

// convert us to ticks, rounding to the closest tick count
// @note us must be <= 1000000 us = 1 s
#define US_TO_TICKS(US) (((((uint32_t)(US)) * 4096) + 6125) / 125000L)

// ADV PDU Types
enum pdu_adv_type {
    PDU_ADV_TYPE_ADV_IND = 0x00,
    PDU_ADV_TYPE_DIRECT_IND = 0x01,
    PDU_ADV_TYPE_NONCONN_IND = 0x02,
    PDU_ADV_TYPE_SCAN_REQ = 0x03,
    PDU_ADV_TYPE_AUX_SCAN_REQ = PDU_ADV_TYPE_SCAN_REQ,
    PDU_ADV_TYPE_SCAN_RSP = 0x04,
    PDU_ADV_TYPE_CONNECT_IND = 0x05,
    PDU_ADV_TYPE_AUX_CONNECT_REQ = PDU_ADV_TYPE_CONNECT_IND,
    PDU_ADV_TYPE_SCAN_IND = 0x06,
    PDU_ADV_TYPE_EXT_IND = 0x07,
    PDU_ADV_TYPE_AUX_ADV_IND = PDU_ADV_TYPE_EXT_IND,
    PDU_ADV_TYPE_AUX_SCAN_RSP = PDU_ADV_TYPE_EXT_IND,
    PDU_ADV_TYPE_AUX_SYNC_IND = PDU_ADV_TYPE_EXT_IND,
    PDU_ADV_TYPE_AUX_CHAIN_IND = PDU_ADV_TYPE_EXT_IND,
    PDU_ADV_TYPE_AUX_CONNECT_RSP = 0x08,
};

// DATA PDU Types
enum pdu_data_llid {
    PDU_DATA_LLID_RESV = 0x00,
    PDU_DATA_LLID_DATA_CONTINUE = 0x01,
    PDU_DATA_LLID_DATA_START = 0x02,
    PDU_DATA_LLID_CTRL = 0x03,
};

// DATA Link Layer Control Types
enum pdu_data_llctrl_type {
    PDU_DATA_LLCTRL_TYPE_CONN_UPDATE_IND = 0x00,
    PDU_DATA_LLCTRL_TYPE_CHAN_MAP_IND = 0x01,
    PDU_DATA_LLCTRL_TYPE_TERMINATE_IND = 0x02,
    PDU_DATA_LLCTRL_TYPE_ENC_REQ = 0x03,
    PDU_DATA_LLCTRL_TYPE_ENC_RSP = 0x04,
    PDU_DATA_LLCTRL_TYPE_START_ENC_REQ = 0x05,
    PDU_DATA_LLCTRL_TYPE_START_ENC_RSP = 0x06,
    PDU_DATA_LLCTRL_TYPE_UNKNOWN_RSP = 0x07,
    PDU_DATA_LLCTRL_TYPE_FEATURE_REQ = 0x08,
    PDU_DATA_LLCTRL_TYPE_FEATURE_RSP = 0x09,
    PDU_DATA_LLCTRL_TYPE_PAUSE_ENC_REQ = 0x0A,
    PDU_DATA_LLCTRL_TYPE_PAUSE_ENC_RSP = 0x0B,
    PDU_DATA_LLCTRL_TYPE_VERSION_IND = 0x0C,
    PDU_DATA_LLCTRL_TYPE_REJECT_IND = 0x0D,
    PDU_DATA_LLCTRL_TYPE_SLAVE_FEATURE_REQ = 0x0E,
    PDU_DATA_LLCTRL_TYPE_CONN_PARAM_REQ = 0x0F,
    PDU_DATA_LLCTRL_TYPE_CONN_PARAM_RSP = 0x10,
    PDU_DATA_LLCTRL_TYPE_REJECT_EXT_IND = 0x11,
    PDU_DATA_LLCTRL_TYPE_PING_REQ = 0x12,
    PDU_DATA_LLCTRL_TYPE_PING_RSP = 0x13,
    PDU_DATA_LLCTRL_TYPE_LENGTH_REQ = 0x14,
    PDU_DATA_LLCTRL_TYPE_LENGTH_RSP = 0x15,
    PDU_DATA_LLCTRL_TYPE_PHY_REQ = 0x16,
    PDU_DATA_LLCTRL_TYPE_PHY_RSP = 0x17,
    PDU_DATA_LLCTRL_TYPE_PHY_UPD_IND = 0x18,
    PDU_DATA_LLCTRL_TYPE_MIN_USED_CHAN_IND = 0x19,
};

// Link Layer State
typedef enum {
    LL_STATE_STANDBY,
    LL_STATE_SCANNING,
    LL_STATE_ADVERTISING,
    LL_STATE_INITIATING,
    LL_STATE_CONNECTED
} ll_state_t;

// Link Layer PDU Flags
typedef enum {
    LL_PDU_FLAG_DATA_PDU = 1,
} ll_pdu_flags;

// Link Layer PDU, used in linked list
typedef struct {
    // header
    void *                 item;
    hci_con_handle_t       con_handle;
    uint8_t                flags;
    int8_t                 rssi;
    uint16_t               connection_event;
    uint16_t               packet_nr;
    // over the air data
    uint8_t                header;
    uint8_t                len;
    uint8_t                payload[LL_MAX_PAYLOAD];
} ll_pdu_t;


// hopping context
static hopping_t h;

static struct {

    volatile bool     synced;

    volatile uint16_t packet_nr_in_connection_event;

    volatile uint16_t conn_interval_1250us;
    volatile uint32_t conn_interval_us;
    volatile uint16_t conn_interval_ticks;

    volatile uint16_t conn_latency;

    volatile uint16_t supervision_timeout_10ms;
    volatile uint32_t supervision_timeout_us;

    //
    volatile uint32_t time_without_any_packets_us;

    // access address
    volatile uint32_t aa;

    // start of current connection event
    volatile uint32_t anchor_ticks;

    // custom anchor delta to apply on next sync hop (if != 0)
    volatile uint16_t anchor_delta_ticks;

    // latest time to send tx packet before sync hop
    volatile uint16_t conn_latest_tx_ticks;

    // timeout for sync relative to anchor
    volatile uint16_t conn_sync_hop_ticks;

    // current channel
    volatile uint8_t  channel;

    // CSA #2 supported
    uint8_t csa2_support;

    // channels selection algorithm index (1 for csa #2)
    volatile uint8_t channel_selection_algorithm;

    // current connection event, first one starts with 0
    // - needed for connection param and channel map updates as well as encryption
    volatile uint16_t connection_event;

    // pending channel map update
    volatile bool     channel_map_update_pending;
    volatile uint16_t channel_map_update_instant;
    volatile uint8_t  channel_map_update_map[5];

    // pending connection param update
    volatile bool     conn_param_update_pending;
    volatile uint16_t conn_param_update_instant;
    volatile uint8_t  conn_param_update_win_size;
    volatile uint16_t conn_param_update_win_offset;
    volatile uint16_t conn_param_update_interval_1250us;
    volatile uint16_t conn_param_update_latency;
    volatile uint32_t conn_param_update_timeout_us;

    // our bd_addr as little endian
    uint8_t bd_addr_le[6];

    // peer addr
    uint8_t peer_addr_type;
    uint8_t peer_addr[6];

    // adv data
    uint8_t adv_len;
    uint8_t adv_data[31];

    // adv param
    uint8_t  adv_map;
    uint32_t adv_interval_us;
    uint8_t  adv_type;

    // adv data
    uint8_t scan_resp_len;
    uint8_t scan_resp_data[31];

    // transmit window size in us
    volatile uint32_t transmit_window_size_us;

    // transmit window offset in us
    volatile uint32_t transmit_window_offset_us;

    // next expected sequence number
    volatile uint8_t next_expected_sequence_number;

    // transmit sequence number
    volatile uint8_t transmit_sequence_number;

    // remote active: more data or non-empty packet
    volatile bool remote_active;


    // rx queue
    btstack_linked_queue_t rx_queue;

    // current incoming packet
    ll_pdu_t * rx_pdu;

    // tx queue of outgoing pdus
    btstack_linked_queue_t tx_queue;

    // current outgoing packet
    ll_pdu_t * tx_pdu;

    // num completed packets
    volatile uint8_t num_completed;

    // used for controller events
    volatile uint8_t error_code;

    volatile bool ll_send_disconnected;

    volatile bool ll_send_connection_complete;

} ctx;

// Buffer pool
static ll_pdu_t ll_pdu_pool_storage[MAX_NUM_LL_PDUS];
static btstack_memory_pool_t ll_pdu_pool;

// prepared adv + scan packets
static uint8_t adv_packet_data[39];
static uint8_t adv_packet_len;
static uint8_t scan_packet_data[39];
static uint8_t scan_packet_len;

// single ll empty pdu
static uint8_t ll_empty_pdu[2];

// single ll control response
static ll_pdu_t ll_tx_packet;

// Link Layer State
static ll_state_t ll_state;
static uint32_t ll_scan_interval_us;
static uint32_t ll_scan_window_us;

static ll_pdu_t * ll_reserved_acl_buffer;

// Controller interface
static uint8_t ll_outgoing_hci_event[258];
static void (*controller_packet_handler)(uint8_t packet_type, uint8_t * packet, uint16_t size);

// Memory Pool for acl-le pdus

static ll_pdu_t * btstack_memory_ll_pdu_get(void){
    void * buffer = btstack_memory_pool_get(&ll_pdu_pool);
    if (buffer){
        memset(buffer, 0, sizeof(ll_pdu_t));
    }
    return (ll_pdu_t *) buffer;
}

static void btstack_memory_ll_pdu_free(ll_pdu_t *acl_le_pdu){
    btstack_memory_pool_free(&ll_pdu_pool, acl_le_pdu);
}

// Link Layer

// prototypes

static bool ll_prepare_rx_buffer(void){
    if (ctx.rx_pdu == NULL){
        ctx.rx_pdu = btstack_memory_ll_pdu_get();
    }
    if (ctx.rx_pdu == NULL){
        printf("No free RX buffer\n");
        return false;
    } else {
        return true;
    }
}

static void ll_stop_timer(void){
    hal_timer_stop();
}

static void ll_set_timer_ticks(uint32_t anchor_offset_ticks){
    ll_stop_timer();
    // set timer for next radio event relative to anchor
    uint32_t timeout_ticks = ctx.anchor_ticks + anchor_offset_ticks;
    hal_timer_start(timeout_ticks);
}

// preamble (1) + aa (4) + header (1) + len (1) + payload (len) + crc (3) -- ISR handler ca. 5 us (educated guess)
static uint32_t ll_start_ticks_for_end_time_and_len(uint32_t packet_end_ticks, uint16_t len){
    uint32_t timestamp_delay = (10 + len) * 8 - 5;
    uint32_t packet_start_ticks = packet_end_ticks - US_TO_TICKS(timestamp_delay);
    return packet_start_ticks;
}

static void ll_emit_hci_event(const hci_event_t * event, ...){
    va_list argptr;
    va_start(argptr, event);
    uint16_t length = hci_event_create_from_template_and_arglist(ll_outgoing_hci_event, event, argptr);
    va_end(argptr);
    controller_packet_handler(HCI_EVENT_PACKET, ll_outgoing_hci_event, length);
}

// ll adv prototypes

static void ll_advertising_timer_handler(void);
static void ll_advertising_tx_done(radio_result_t result);
static void ll_advertising_tx_to_rx(radio_result_t result);

static void ll_advertising_statemachine(void){
    // find next channel
    while (ctx.channel < 40){
        ctx.channel++;
        if ((ctx.adv_map & (1 << (ctx.channel - 37))) != 0) {
            // Set Channel
            radio_set_channel(ctx.channel);
            // Expect response?
            radio_transition_t transition;
            radio_callback_t callback;
            if (ctx.adv_type == 3){
                // Non connectable undirected advertising (ADV_NONCONN_IND)
                transition = RADIO_TRANSITION_TX_ONLY;
                callback = &ll_advertising_tx_done;
            } else {
                // All other are either connectable and/or scannable
                transition = RADIO_TRANSITION_TX_TO_RX;
                callback = &ll_advertising_tx_to_rx;
            }
            // log_info("Send adv on #%u", ctx.channel);
            radio_transmit(callback, transition, adv_packet_data, adv_packet_len);
            break;
        }
        // adv sent on all active channels
        if (ctx.channel >= 40){
            // Disable HF Clock
            radio_hf_clock_disable();

            // Set timer
            uint32_t adv_interval_ticks = US_TO_TICKS(ctx.adv_interval_us);
            hal_timer_set_callback(&ll_advertising_timer_handler);
            ll_set_timer_ticks(adv_interval_ticks);
        }
    }
}

static void ll_advertising_tx_done(radio_result_t result){
    UNUSED(result);
    ll_advertising_statemachine();
}

static void ll_advertising_disabled(radio_result_t result){
    UNUSED(result);
    ll_advertising_statemachine();
}

static void ll_advertising_conn_ind_received(radio_result_t result){
    UNUSED(result);
    ll_pdu_t * rx_packet = ctx.rx_pdu;
    // packet used
    ctx.rx_pdu = NULL;
    // mark as adv packet
    rx_packet->flags = 0;
    // queue received packet -> ll_execute_once
    btstack_linked_queue_enqueue(&ctx.rx_queue, (btstack_linked_item_t *) rx_packet);
}

static void ll_advertising_rx_done(radio_result_t result){
    uint8_t pdu_type;
    switch (result){
        case RADIO_RESULT_OK:
            // check for Scan and Connect requests
            pdu_type = ctx.rx_pdu->header & 0x0f;
            switch (pdu_type){
                case PDU_ADV_TYPE_SCAN_REQ:
                    switch (ctx.adv_type) {
                        case 3:
                            // ignore for ADV_NONCONN_IND
                            radio_stop(&ll_advertising_disabled);
                            break;
                         default:
                            radio_transmit(&ll_advertising_tx_done, RADIO_TRANSITION_TX_ONLY, scan_packet_data, scan_packet_len);
                            break;
                    }
                    break;
                case PDU_ADV_TYPE_CONNECT_IND:
                    switch (ctx.adv_type){
                        case 2: // ADV_SCAN_IND
                        case 3: // ADV_NONCONN_IND
                            radio_stop(&ll_advertising_disabled);
                            break;
                        default:
                            // store ticks as anchor
                            ctx.anchor_ticks = hal_timer_get_ticks();
                            // stop radio and
                            radio_stop(&ll_advertising_conn_ind_received);
                            break;
                    }
                    break;
                default:
                    radio_stop(&ll_advertising_disabled);
                    break;
            }
            break;
        case RADIO_RESULT_TIMEOUT:
            ll_advertising_statemachine();
            break;
        case RADIO_RESULT_CRC_ERROR:
            radio_stop(&ll_advertising_disabled);
            break;
        default:
            btstack_assert(false);
            break;
    }
}

static void ll_advertising_tx_to_rx(radio_result_t result){
    UNUSED(result);
    if (ll_prepare_rx_buffer()){
        radio_receive(&ll_advertising_rx_done, TX_TO_RX_TIMEOUT_US, &ctx.rx_pdu->header, 2 + LL_MAX_PAYLOAD, NULL);
    } else {
        // TODO: stop radio
        btstack_assert(false);
    }
}

static void ll_advertising_timer_handler(void){

    uint32_t t0 = hal_timer_get_ticks();

    // enable HF Clock
    radio_hf_clock_enable(true);

    // send adv on all configured channels
    ctx.channel = 36;
    ctx.anchor_ticks = t0;
    ll_advertising_statemachine();
}

static uint16_t ll_advertising_setup_pdu(uint8_t * buffer, uint8_t header, uint8_t len, const uint8_t * data){
    buffer[0] = header;
    buffer[1] = 6 + len;
    memcpy(&buffer[2], ctx.bd_addr_le, 6);
    memcpy(&buffer[8], data, len);
    uint16_t packet_size = 2 + buffer[1];
    return packet_size;
}

static uint8_t ll_advertising_start(void){
    // COMMAND DISALLOWED if wrong state.
    if (ll_state != LL_STATE_STANDBY) return ERROR_CODE_COMMAND_DISALLOWED;
    log_info("Start Advertising on channels 0x%0x, interval %lu us", ctx.adv_map, ctx.adv_interval_us);

    radio_set_access_address(ADVERTISING_RADIO_ACCESS_ADDRESS);
    radio_set_crc_init(ADVERTISING_CRC_INIT);

    ll_state = LL_STATE_ADVERTISING;

    // prepare adv and scan data in tx0 and tx1
    enum pdu_adv_type adv_type;
    switch (ctx.adv_type){
        case 0:
            // Connectable and scannable undirected advertising
            adv_type = PDU_ADV_TYPE_ADV_IND;
            break;
        case 1:
            // Connectable high duty cycle directed advertising
            adv_type = PDU_ADV_TYPE_DIRECT_IND;
            break;
        case 2:
            // Scannable undirected advertising (ADV_SCAN_IND);
            adv_type = PDU_ADV_TYPE_SCAN_IND;
            break;
        case 3:
            // Non connectable undirected advertising (ADV_NONCONN_IND)
            adv_type = PDU_ADV_TYPE_NONCONN_IND;
            break;
        case 4:
            // Connectable low duty cycle directed advertising
            adv_type = PDU_ADV_TYPE_DIRECT_IND;
            break;
        default:
            adv_type = PDU_ADV_TYPE_ADV_IND;
            break;
    }
    adv_packet_len  = ll_advertising_setup_pdu(adv_packet_data, adv_type,  ctx.adv_len,       ctx.adv_data);
    scan_packet_len = ll_advertising_setup_pdu(scan_packet_data, PDU_ADV_TYPE_SCAN_RSP, ctx.scan_resp_len, ctx.scan_resp_data);

    ctx.channel = 36;
    ctx.anchor_ticks = hal_timer_get_ticks();

    // and get started
    radio_hf_clock_enable(true);
    ll_advertising_statemachine();

    return ERROR_CODE_SUCCESS;
}

static uint8_t ll_advertising_stop(void){
    // COMMAND DISALLOWED if wrong state.
    if (ll_state != LL_STATE_ADVERTISING) return ERROR_CODE_COMMAND_DISALLOWED;

    // TODO:
    return ERROR_CODE_SUCCESS;
}

// ll scanning

static void ll_scanning_statemachine(void);

static void ll_scanning_for_window(void){
    radio_hf_clock_enable(true);
    // next channel
    ctx.channel++;
    if (ctx.channel >= 40){
        ctx.channel = 37;
    }
    radio_set_channel(ctx.channel);
    ctx.anchor_ticks = hal_timer_get_ticks();
    log_info("Scan channel %u", ctx.channel);
    ll_scanning_statemachine();
}

static void ll_scanning_tx_interrupted(radio_result_t result){
    UNUSED(result);
    ll_scanning_statemachine();
}

static void ll_scanning_rx_done(radio_result_t result){
    ll_pdu_t * rx_packet;
    switch (result){
        case RADIO_RESULT_OK:
            rx_packet = ctx.rx_pdu;
            btstack_assert(rx_packet != NULL);
            // packet used
            ctx.rx_pdu = NULL;
            // mark as adv packet
            rx_packet->flags = 0;
            // queue received packet
            btstack_linked_queue_enqueue(&ctx.rx_queue, (btstack_linked_item_t *) rx_packet);
            // stop rx->tx transition
            radio_stop(&ll_scanning_tx_interrupted);
            break;
        case RADIO_RESULT_CRC_ERROR:
            // stop rx->tx transition
            radio_stop(&ll_scanning_tx_interrupted);
            break;
        case RADIO_RESULT_TIMEOUT:
            ll_scanning_statemachine();
            break;
        default:
            break;
    }
}

static void ll_scanning_statemachine(void){
    uint32_t now =  hal_timer_get_ticks();
    uint32_t scanning_active_ticks = now - ctx.anchor_ticks;
    uint32_t scanning_active_us    = (scanning_active_ticks * 70) / 2;

    ll_prepare_rx_buffer();

    if ((scanning_active_us < ll_scan_window_us) && (ctx.rx_pdu != NULL)){
        uint32_t scan_interval_remaining_us = ll_scan_window_us - scanning_active_us;
        // start receiving
        radio_receive(&ll_scanning_rx_done, scan_interval_remaining_us, &ctx.rx_pdu->header, 2 + LL_MAX_PAYLOAD, &ctx.rx_pdu->rssi);
    } else {
        // scan window over or no buffer

        // disable radio if there is enough time
        uint32_t pause_us = ll_scan_interval_us - scanning_active_us;
        if (pause_us < 500){
            // almost 100% scanning, keep hf clock on
            ll_scanning_for_window();
        } else {
            radio_hf_clock_disable();
            hal_timer_set_callback(&ll_scanning_for_window);
            hal_timer_start(now + US_TO_TICKS(pause_us));
        }
    }
}

static uint8_t ll_scanning_start(uint8_t filter_duplicates){

    // COMMAND DISALLOWED if wrong state.
    if (ll_state != LL_STATE_STANDBY)  return ERROR_CODE_COMMAND_DISALLOWED;

    ll_state = LL_STATE_SCANNING;
    ctx.channel = 36;

    radio_set_access_address(ADVERTISING_RADIO_ACCESS_ADDRESS);
    radio_set_crc_init(ADVERTISING_CRC_INIT);

    log_info("LE Scan Start: window %lu, interval %lu ms", ll_scan_interval_us, ll_scan_window_us);

    ll_scanning_for_window();

    return ERROR_CODE_SUCCESS;
}

static void ll_scanning_stop_done(radio_result_t result){
    UNUSED(result);
    ll_state = LL_STATE_STANDBY;
    radio_hf_clock_disable();
}

static uint8_t ll_scanning_stop(void){
    // COMMAND DISALLOWED if wrong state.
    if (ll_state != LL_STATE_SCANNING)  return 0x0c;

    // TODO: post-pone result until scanning actually stopped
    log_info("LE Scan Stop");

    // stop radio
    radio_stop(&ll_scanning_stop_done);
    return ERROR_CODE_SUCCESS;
}

// ll connected

static bool ll_connected_one_more_packet(void){
    uint32_t now = hal_timer_get_ticks();
    int32_t connection_ticks = now - ctx.anchor_ticks;
    return connection_ticks < ctx.conn_latest_tx_ticks;
}

static void ll_connected_next_channel(void){
    switch (ctx.channel_selection_algorithm){
        case 0:
            ctx.channel = hopping_csa1_get_next_channel( &h );
            break;
        case 1:
            ctx.channel = hopping_csa2_get_channel_for_counter( &h,  ctx.connection_event);
            break;
        default:
            break;
    }
    radio_set_channel(ctx.channel);
}

static void ll_connected_ctx_set_conn_interval(uint16_t conn_interval_1250us){
    ctx.conn_interval_1250us = conn_interval_1250us;
    ctx.conn_interval_us     = ctx.conn_interval_1250us * 1250;
    ctx.conn_interval_ticks  = US_TO_TICKS(ctx.conn_interval_us);
    ctx.conn_sync_hop_ticks  = US_TO_TICKS(ctx.conn_interval_us - SYNC_HOP_DELAY_US);

    // latest time to send a packet before getting ready for next cnonection event
    uint16_t max_packet_time_incl_ifs_us = 500;
    ctx.conn_latest_tx_ticks = US_TO_TICKS(ctx.conn_interval_us - SYNC_HOP_DELAY_US - max_packet_time_incl_ifs_us);
}

static void ll_connected_terminate(uint8_t error_code){
    ll_state = LL_STATE_STANDBY;
    ctx.conn_param_update_pending = false;
    ctx.channel_map_update_pending = false;
    // turn off clock
    radio_hf_clock_disable();
    // stop sync hop timer
    ll_stop_timer();
    // free outgoing tx packets
    if ((ctx.tx_pdu != NULL) && (ctx.tx_pdu != &ll_tx_packet)){
        btstack_memory_ll_pdu_free(ctx.tx_pdu);
        ctx.tx_pdu = NULL;
    }
    // free queued tx packets
    while (true){
        ll_pdu_t * tx_packet = (ll_pdu_t *) btstack_linked_queue_dequeue(&ctx.tx_queue);
        if (tx_packet != NULL) {
            btstack_memory_ll_pdu_free(tx_packet);
        } else {
            break;
        }
    }
    // notify host stack
    ctx.error_code = error_code;
    ctx.ll_send_disconnected = true;
}

static void ll_connected_timer_handler(void);
static void lL_connected_rx_done(radio_result_t result);

static void ll_connected_connection_event_complete(void){
    radio_hf_clock_disable();
}

static void ll_connected_radio_stopped(radio_result_t result){
    UNUSED(result);
    ll_connected_connection_event_complete();
}

static void ll_connected_tx_only_done(radio_result_t result){
    UNUSED(result);
    ll_connected_connection_event_complete();
}

static void ll_connected_tx_to_rx_done(radio_result_t result) {
    UNUSED(result);
    if (!ll_connected_one_more_packet()){
        // stop tx
        radio_stop(&ll_connected_radio_stopped);
        return;
    }

    // receive next packet
    (void) ll_prepare_rx_buffer();
    if (ctx.rx_pdu == NULL){
        radio_stop(&ll_connected_radio_stopped);
        return;
    }

    // receive master packet
    radio_receive(&lL_connected_rx_done, TX_TO_RX_TIMEOUT_US, &ctx.rx_pdu->header, 2 + LL_MAX_PAYLOAD, NULL);
}

static void ll_connected_terminate_received(radio_result_t result){
    UNUSED(result);
    ll_connected_terminate(ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION);
}

static void lL_connected_rx_done(radio_result_t result){

    uint32_t packet_end_ticks = hal_timer_get_ticks();

    if (result == RADIO_RESULT_TIMEOUT){
        ll_connected_connection_event_complete();
        return;
    }

    // Handle RX

    // set anchor on first packet in connection event
    if (ctx.packet_nr_in_connection_event == 0){
        ctx.anchor_ticks = ll_start_ticks_for_end_time_and_len(packet_end_ticks, ctx.rx_pdu->len);
        ctx.synced = true;

        // set timer for sync-hop
        hal_timer_set_callback(&ll_connected_timer_handler);
        ll_set_timer_ticks( ctx.conn_sync_hop_ticks);
    }

    // packet received
    if (result == RADIO_RESULT_OK) {

        // parse rx pdu header
        uint8_t rx_header = ctx.rx_pdu->header;
        uint8_t next_expected_sequence_number = (rx_header >> 2) & 1;
        uint8_t sequence_number = (rx_header >> 3) & 1;
        uint8_t more_data = (rx_header >> 4) & 1;

        // remote active if md or len > 0
        ctx.remote_active = (more_data != 0) || ctx.rx_pdu->len > 0;

        // only accept packets with new sequence number and  len <= payload size
        if ((sequence_number == ctx.next_expected_sequence_number) && (ctx.rx_pdu->len <= LL_MAX_PAYLOAD)) {

            // update state
            ctx.next_expected_sequence_number = 1 - sequence_number;

            // queue if not empty
            ll_pdu_t *rx_packet = ctx.rx_pdu;
            if (rx_packet->len != 0) {

                // handle terminate immediately
                uint8_t ll_id = rx_packet->header & 3;
                if (ll_id == PDU_DATA_LLID_CTRL) {
                    if (rx_packet->payload[0] == PDU_DATA_LLCTRL_TYPE_TERMINATE_IND){
                        ll_stop_timer();
                        radio_stop(&ll_connected_terminate_received);
                        return;
                    }
                }

                // packet used
                ctx.rx_pdu = NULL;

                // mark as data packet and add meta data
                rx_packet->flags |= LL_PDU_FLAG_DATA_PDU;
                rx_packet->connection_event = ctx.connection_event;
                rx_packet->packet_nr = ctx.packet_nr_in_connection_event;

                // queue received packet
                btstack_linked_queue_enqueue(&ctx.rx_queue, (btstack_linked_item_t *) rx_packet);
            }
        }

        ctx.packet_nr_in_connection_event++;

        // report outgoing packet as ack'ed and free if confirmed by peer
        bool tx_acked = ctx.transmit_sequence_number != next_expected_sequence_number;
        if (tx_acked) {
            // if non link-layer packet, free buffer and report as completed
            if ((ctx.tx_pdu != NULL) && (ctx.tx_pdu != &ll_tx_packet)) {
                btstack_memory_ll_pdu_free(ctx.tx_pdu);
                ctx.num_completed++;
            }
            ctx.tx_pdu = NULL;
            ctx.transmit_sequence_number = next_expected_sequence_number;
        }
    }

    // restart supervision timeout
    ctx.time_without_any_packets_us = 0;

    // Prepare TX

    // check if we can sent a full packet before sync hop
    if (!ll_connected_one_more_packet()){
        // stop tx
        radio_stop(&ll_connected_radio_stopped);
        return;
    }

    // fetch next packet
    if (ctx.tx_pdu == NULL){
        ctx.tx_pdu = (ll_pdu_t *) btstack_linked_queue_dequeue(&ctx.tx_queue);
    }

    // setup empty packet if no tx packet ready
    uint8_t * tx_buffer;
    if (ctx.tx_pdu == NULL){
        ll_empty_pdu[0] = PDU_DATA_LLID_DATA_CONTINUE;
        ll_empty_pdu[1] = 0;
        tx_buffer = ll_empty_pdu;
    } else {
        tx_buffer = &ctx.tx_pdu->header;
    }

    // setup pdu header
    uint8_t md = btstack_linked_queue_empty(&ctx.tx_queue) ? 0 : 1;
    tx_buffer[0] |= (md << 4) | (ctx.transmit_sequence_number << 3) | (ctx.next_expected_sequence_number << 2);

    // send packet
    bool tx_to_rx = ctx.remote_active || (tx_buffer[1] > 0);
    if (tx_to_rx){
        radio_transmit(&ll_connected_tx_to_rx_done, RADIO_TRANSITION_TX_TO_RX, tx_buffer, 2 + tx_buffer[1]);
    } else {
        radio_transmit(&ll_connected_tx_only_done, RADIO_TRANSITION_TX_ONLY, tx_buffer, 2 + tx_buffer[1]);
    }
}

static void ll_connected_handle_conn_ind(ll_pdu_t * rx_packet){
    // parse packet
    uint8_t * init_addr = &rx_packet->payload[0];
    uint8_t * adv_addr =  &rx_packet->payload[6];
    uint8_t   chan_sel = (rx_packet->header >> 5) & 1;

    // verify AdvA
    if (memcmp(ctx.bd_addr_le, adv_addr, 6) != 0){
        // differs, go back to adv sending
        ll_advertising_statemachine();
        return;
    }

    // next event is > 1.25 ms away
    radio_hf_clock_disable();

    // TODO: get remote addr type
    ctx.peer_addr_type = 0;
    memcpy(ctx.peer_addr, init_addr, 6);

    // get params for HCI event
    const uint8_t * ll_data = &rx_packet->payload[12];

    ctx.aa                          = little_endian_read_32(ll_data, 0);
    uint32_t crc_init               = little_endian_read_24(ll_data, 4);
    uint8_t  transmit_window_size   = ll_data[7];
    uint16_t transmit_window_offset = little_endian_read_16(ll_data, 8);
    uint16_t conn_interval_1250us   = little_endian_read_16(ll_data, 10);
    ctx.conn_latency                = little_endian_read_16(ll_data, 12);
    ctx.supervision_timeout_10ms    = little_endian_read_16(ll_data, 14);
    const uint8_t * channel_map = &ll_data[16];
    uint8_t hop = ll_data[21] & 0x1f;
    uint8_t sca = ll_data[21] >> 5;

    // TODO: handle sleep clock accuracy of initiator
    UNUSED(sca);

    ll_connected_ctx_set_conn_interval(conn_interval_1250us);

    // convert to us
    ctx.supervision_timeout_us    = ctx.supervision_timeout_10ms  * 10000;
    ctx.transmit_window_size_us   = transmit_window_size   * 1250;
    ctx.transmit_window_offset_us = transmit_window_offset * 1250;

    // init connection state
    ctx.connection_event = 0;
    ctx.packet_nr_in_connection_event = 0;
    ctx.next_expected_sequence_number = 0;
    ctx.transmit_sequence_number = 0;

    // set AA
    radio_set_access_address(ctx.aa);

    // set CRC init value
    radio_set_crc_init(crc_init);

    printf("Transmit window offset %u us\n", (int) ctx.transmit_window_offset_us);
    printf("Transmit window size   %u us\n", (int) ctx.transmit_window_size_us);
    printf("Connection interval    %u us\n", (int) ctx.conn_interval_us);
    printf("Connection timeout     %u us\n", (int) ctx.supervision_timeout_us);
    printf("AA %08x\n", (int) ctx.aa);
    printf("CRC Init 0x%06" PRIx32 "x\n", crc_init);

    // init hopping
    hopping_init( &h );
    hopping_set_channel_map( &h, channel_map);
    ctx.channel_selection_algorithm = ctx.csa2_support & chan_sel;
    switch (ctx.channel_selection_algorithm){
        case 0:
            hopping_csa1_set_hop_increment(  &h, hop );
            break;
        case 1:
            hopping_csa2_set_access_address( &h, ctx.aa);
            break;
        default:
            break;
    }

    // connected -> notify controller
    ll_state = LL_STATE_CONNECTED;
    ctx.synced = false;
    ctx.ll_send_connection_complete = true;

    // sleep until transmit window
    hal_timer_set_callback(&ll_connected_timer_handler);
    ctx.anchor_delta_ticks = US_TO_TICKS(ctx.transmit_window_offset_us + 1250);
    ll_set_timer_ticks(US_TO_TICKS(ctx.transmit_window_offset_us + 1250 - SYNC_HOP_DELAY_US));
}

static void ll_connected_timer_handler(void){

    // Check supervision timeout
    if (ctx.synced){
        // check supervision timeout if connection was established
        ctx.time_without_any_packets_us += ctx.conn_interval_us;
        if (ctx.time_without_any_packets_us > ctx.supervision_timeout_us) {
            printf("Supervision timeout (regular)\n\n");
            ll_connected_terminate(ERROR_CODE_CONNECTION_TIMEOUT);
            return;
        }
    } else {
        // give up on receiving first packet after 6 tries
        if (ctx.connection_event > 6){
            printf("Supervision timeout(establishment)\n\n");
            ll_connected_terminate(ERROR_CODE_CONNECTION_FAILED_TO_BE_ESTABLISHED);
            return;
        }
    }

    // update anchor using custom value when transmit windows was used
    // connection event counter is only incremented for regular connection interval
    if (ctx.anchor_delta_ticks == 0){
        ctx.anchor_ticks += ctx.conn_interval_ticks;
        ctx.connection_event++;
    } else {
        ctx.anchor_ticks += ctx.anchor_delta_ticks;
        ctx.anchor_delta_ticks = 0;
    }

    if (ctx.channel_map_update_pending && (ctx.channel_map_update_instant == ctx.connection_event)) {
        ctx.channel_map_update_pending = false;

        log_info("Chan map update now");

        hopping_set_channel_map( &h, (const uint8_t *) &ctx.channel_map_update_map );
    }

    if (ctx.conn_param_update_pending && ((ctx.conn_param_update_instant) == ctx.connection_event) ) {
        ctx.conn_param_update_pending = false;

        log_info("Conn param update now");

        ll_connected_ctx_set_conn_interval(ctx.conn_param_update_interval_1250us);
        ctx.conn_latency              = ctx.conn_param_update_latency;
        ctx.supervision_timeout_us    = ctx.conn_param_update_timeout_us;
        ctx.transmit_window_offset_us = ctx.conn_param_update_win_offset * 1250;
        ctx.transmit_window_size_us   = ctx.conn_param_update_win_size   * 1250;
        ctx.synced = false;

        // See Core V5.2, Vol 6B, 5.1.1, Figure 5.1
        // if transmit window == 0, continue. If not sleep again
        if (ctx.conn_param_update_win_offset > 0){
            ctx.anchor_delta_ticks = US_TO_TICKS(ctx.transmit_window_offset_us);
            ll_set_timer_ticks(US_TO_TICKS(ctx.transmit_window_offset_us - SYNC_HOP_DELAY_US));
            return;
        }
    }

    // prepare connection event
    ctx.packet_nr_in_connection_event = 0;

    radio_hf_clock_enable(true);

    ll_connected_next_channel();

    // set radio timer (should get updated by first packet)
    ll_set_timer_ticks( ctx.conn_sync_hop_ticks);

    (void) ll_prepare_rx_buffer();
    if (ctx.rx_pdu == NULL) {
        log_info("No RX PDU  for first packet, skip connection event");
    } else {
        if (ctx.synced){
            radio_receive(&lL_connected_rx_done, SYNC_HOP_DELAY_US + 100, &ctx.rx_pdu->header, 2 + LL_MAX_PAYLOAD, NULL);
        } else {
            radio_receive(&lL_connected_rx_done, SYNC_HOP_DELAY_US + ctx.transmit_window_size_us, &ctx.rx_pdu->header, 2 + LL_MAX_PAYLOAD, NULL);
        }
    }

    // printf("--SYNC-Ch %02u-Event %04u - t %08" PRIu32 "--\n", ctx.channel, ctx.connection_event, t0);
}

static void ll_connected_handle_control(ll_pdu_t * rx_packet){
    ll_pdu_t * tx_packet = &ll_tx_packet;
    uint8_t opcode = rx_packet->payload[0];
    switch (opcode){
        case PDU_DATA_LLCTRL_TYPE_VERSION_IND:
            tx_packet->len = 6;
            tx_packet->header = PDU_DATA_LLID_CTRL;
            tx_packet->payload[0] = PDU_DATA_LLCTRL_TYPE_VERSION_IND;
            tx_packet->payload[1] = 0x06; // VersNr = Bluetooth Core V4.0
            little_endian_store_16(tx_packet->payload, 2, BLUETOOTH_COMPANY_ID_BLUEKITCHEN_GMBH);
            little_endian_store_16(tx_packet->payload, 4, 0);
            btstack_linked_queue_enqueue(&ctx.tx_queue, (btstack_linked_item_t *) tx_packet);
            printf("Queue Version Ind\n");
            break;
        case PDU_DATA_LLCTRL_TYPE_FEATURE_REQ:
            tx_packet->len = 9;
            tx_packet->header = PDU_DATA_LLID_CTRL;
            tx_packet->payload[0] = PDU_DATA_LLCTRL_TYPE_FEATURE_RSP;
            // TODO: set features of our controller
            memset(&tx_packet->payload[1], 0, 8);
            btstack_linked_queue_enqueue(&ctx.tx_queue, (btstack_linked_item_t *) tx_packet);
            printf("Queue Feature Rsp\n");
            break;
        case PDU_DATA_LLCTRL_TYPE_CHAN_MAP_IND:
            memcpy((uint8_t *) ctx.channel_map_update_map, &rx_packet->payload[1], 5);
            ctx.channel_map_update_instant   = little_endian_read_16(rx_packet->payload, 6);
            ctx.channel_map_update_pending   = true;
            break;
        case PDU_DATA_LLCTRL_TYPE_CONN_UPDATE_IND:
            ctx.conn_param_update_win_size        = rx_packet->payload[1];
            ctx.conn_param_update_win_offset      = little_endian_read_16(rx_packet->payload, 2);
            ctx.conn_param_update_interval_1250us = little_endian_read_16(rx_packet->payload, 4);
            ctx.conn_param_update_latency         = little_endian_read_16(rx_packet->payload, 6);
            ctx.conn_param_update_timeout_us      = little_endian_read_16(rx_packet->payload, 8) * 10000;
            ctx.conn_param_update_instant         = little_endian_read_16(rx_packet->payload, 10);
            ctx.conn_param_update_pending         = true;
            log_info("PDU_DATA_LLCTRL_TYPE_CONN_UPDATE_IND, conn interval %u 1250us at instant %u",
                     (unsigned int) ctx.conn_param_update_interval_1250us, ctx.conn_param_update_instant);
            break;
        default:
            break;
    }
}

static void ll_connected_handle_data(ll_pdu_t * rx_packet){
    btstack_assert(rx_packet->len <= LL_MAX_PAYLOAD);
    log_debug("CE: %u, nr %u, header 0x%02x, len %u", rx_packet->connection_event, rx_packet->packet_nr, rx_packet->header, rx_packet->len);
    uint8_t acl_packet[4 + LL_MAX_PAYLOAD];
    // ACL Header
    uint8_t ll_id = rx_packet->header & 3;
    acl_packet[0] = 0x01;
    acl_packet[1] = ll_id << 4;
    little_endian_store_16(acl_packet, 2, rx_packet->len);
    memcpy(&acl_packet[4], rx_packet->payload, rx_packet->len);
    (*controller_packet_handler)(HCI_ACL_DATA_PACKET, acl_packet, rx_packet->len + 4);
}

static void ll_handle_adv(ll_pdu_t * rx_packet) {
    // Map PDU_ADV_TYPE to HCI Event_Type
    uint8_t event_type = 0;
    switch (rx_packet->header & 0x0f){
        case PDU_ADV_TYPE_ADV_IND:
            event_type = 0;
            break;
        case PDU_ADV_TYPE_DIRECT_IND:
            event_type = 1;
            break;
        case PDU_ADV_TYPE_NONCONN_IND:
            event_type = 3;
            break;
        case PDU_ADV_TYPE_SCAN_RSP:
            event_type = 3;
            break;
        default:
            return;
    }
    uint8_t advertiser_addr_type = ((rx_packet->header & 0x40) != 0) ? 1 : 0;
    uint8_t adv_data_len = rx_packet->len - 6;

    uint16_t pos = 0;
    ll_outgoing_hci_event[pos++] = HCI_EVENT_LE_META;
    pos++;
    ll_outgoing_hci_event[pos++] = HCI_SUBEVENT_LE_ADVERTISING_REPORT;
    ll_outgoing_hci_event[pos++] = 1;
    ll_outgoing_hci_event[pos++] = event_type;
    ll_outgoing_hci_event[pos++] = advertiser_addr_type;
    memcpy(&ll_outgoing_hci_event[pos], &rx_packet->payload[0], 6);
    pos += 6;
    ll_outgoing_hci_event[pos++] = adv_data_len;
    memcpy(&ll_outgoing_hci_event[pos], &rx_packet->payload[6], adv_data_len);
    pos += adv_data_len;
    ll_outgoing_hci_event[pos++] = (uint8_t) rx_packet->rssi;
    ll_outgoing_hci_event[1] = pos - 2;
    (*controller_packet_handler)(HCI_EVENT_PACKET, ll_outgoing_hci_event, pos);
}

// public API

void ll_init(void){
    // setup memory pools
    btstack_memory_pool_create(&ll_pdu_pool, ll_pdu_pool_storage, MAX_NUM_LL_PDUS, sizeof(ll_pdu_t));

    // set test bd addr 33:33:33:33:33:33
    memset(ctx.bd_addr_le, 0x33, 6);

    // default channels, advertising interval
    ctx.adv_map = 0x7;
    ctx.adv_interval_us = 1280000;

    // init radio
    radio_init();
}

void ll_radio_on(void){
    ll_state = LL_STATE_STANDBY;
}

void ll_set_scan_parameters(uint8_t le_scan_type, uint16_t le_scan_interval, uint16_t le_scan_window, uint8_t own_address_type, uint8_t scanning_filter_policy){
    // TODO .. store other params
    ll_scan_interval_us = ((uint32_t) le_scan_interval) * 625;
    ll_scan_window_us   = ((uint32_t) le_scan_window)   * 625;
    log_info("LE Scan Params: window %lu, interval %lu ms", ll_scan_interval_us, ll_scan_window_us);
}

uint8_t ll_set_scan_enable(uint8_t le_scan_enable, uint8_t filter_duplicates){
    if (le_scan_enable){
        return ll_scanning_start(filter_duplicates);
    } else {
        return ll_scanning_stop();
    }
}

uint8_t ll_set_advertise_enable(uint8_t le_adv_enable){
    if (le_adv_enable){
        return ll_advertising_start();
    } else {
        return ll_advertising_stop();
    }
}

uint8_t ll_set_advertising_parameters(uint16_t advertising_interval_min, uint16_t advertising_interval_max,
                                      uint8_t advertising_type, uint8_t own_address_type, uint8_t peer_address_types, uint8_t * peer_address,
                                      uint8_t advertising_channel_map, uint8_t advertising_filter_policy){

    // validate channel map
    if (advertising_channel_map == 0) return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    if ((advertising_channel_map & 0xf8) != 0) return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;

    // validate advertising interval
    if (advertising_interval_min < 0x20)   return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    if (advertising_interval_min > 0x4000) return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    if (advertising_interval_max < 0x20)   return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    if (advertising_interval_max > 0x4000) return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    if (advertising_interval_min > advertising_interval_max) return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;

    ctx.adv_map = advertising_channel_map;
    ctx.adv_interval_us = advertising_interval_max * 625;
    ctx.adv_type= advertising_type;

    // TODO: validate other params
    // TODO: process other params

    return ERROR_CODE_SUCCESS;
}

uint8_t ll_set_advertising_data(uint8_t adv_len, const uint8_t * adv_data){
    // COMMAND DISALLOWED if wrong state.
    if (ll_state == LL_STATE_ADVERTISING) return ERROR_CODE_COMMAND_DISALLOWED;
    if (adv_len > 31) return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    ctx.adv_len = adv_len;
    memcpy(ctx.adv_data, adv_data, adv_len);

    return ERROR_CODE_SUCCESS;
}

uint8_t ll_set_scan_response_data(uint8_t adv_len, const uint8_t * adv_data){
    // COMMAND DISALLOWED if wrong state.
    if (ll_state == LL_STATE_ADVERTISING) return ERROR_CODE_COMMAND_DISALLOWED;
    if (adv_len > 31) return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    ctx.scan_resp_len = adv_len;
    memcpy(ctx.scan_resp_data, adv_data, adv_len);

    return ERROR_CODE_SUCCESS;
}

void ll_execute_once(void){
    // process received packets
    while (1){
        ll_pdu_t * rx_packet;
        /** critical section start */
        hal_cpu_disable_irqs();
        rx_packet = (ll_pdu_t *) btstack_linked_queue_dequeue(&ctx.rx_queue);
        hal_cpu_enable_irqs();
        /** critical section end */
        if (rx_packet == NULL) break;
        uint8_t ll_id;
        if (rx_packet->len > 0){
            switch (ll_state){
                case LL_STATE_ADVERTISING:
                    if ((rx_packet->flags & LL_PDU_FLAG_DATA_PDU) != 0){
                        break;
                    }
                    if ((rx_packet->header & 0x0f) != PDU_ADV_TYPE_CONNECT_IND) {
                        break;
                    }
                    ll_connected_handle_conn_ind(rx_packet);
                    break;
                case LL_STATE_SCANNING:
                    if ((rx_packet->flags & LL_PDU_FLAG_DATA_PDU) != 0){
                        break;
                    }
                    ll_handle_adv(rx_packet);
                    break;
                case LL_STATE_CONNECTED:
                    // DATA PDU
                    if ((rx_packet->flags & LL_PDU_FLAG_DATA_PDU) == 0){
                        break;
                    }
                    ll_id = rx_packet->header & 3;
                    if (ll_id == PDU_DATA_LLID_CTRL) {
                        ll_connected_handle_control(rx_packet);
                    } else {
                        ll_connected_handle_data(rx_packet);
                    }
                    break;
                default:
                    break;
            }
        }
        // free packet
        /** critical section start */
        hal_cpu_disable_irqs();
        btstack_memory_ll_pdu_free(rx_packet);
        hal_cpu_enable_irqs();
        /** critical section end */
    }

    // generate HCI events

    // report num complete packets
    /** critical section start */
    hal_cpu_disable_irqs();
    uint8_t num_completed = ctx.num_completed;
    ctx.num_completed = 0;
    hal_cpu_enable_irqs();
    /** critical section end */
    if (num_completed > 0){
        ll_emit_hci_event(&hci_event_number_of_completed_packets_1, 1, HCI_CON_HANDLE, num_completed);
    }

    // report connection event
    if (ctx.ll_send_connection_complete){
        ctx.ll_send_connection_complete = false;
        ll_emit_hci_event(&hci_subevent_le_connection_complete,
                          ERROR_CODE_SUCCESS, HCI_CON_HANDLE, 0x01 /* slave */, ctx.peer_addr_type, ctx.peer_addr,
                          ctx.conn_interval_1250us, ctx.conn_latency, ctx.supervision_timeout_10ms, 0 /* master clock accuracy */);
    }

    // report disconnection event
    if (ctx.ll_send_disconnected){
        ctx.ll_send_disconnected = false;
        uint8_t error_code = ctx.error_code;
        ctx.error_code = ERROR_CODE_SUCCESS;
        ll_emit_hci_event(&hci_event_disconnection_complete, ERROR_CODE_SUCCESS, HCI_CON_HANDLE, error_code);
    }
}

bool ll_reserve_acl_packet(void){
    if (ll_reserved_acl_buffer == NULL){

        /** critical section start */
        hal_cpu_disable_irqs();
        ll_reserved_acl_buffer = btstack_memory_ll_pdu_get();
        hal_cpu_enable_irqs();
        /** critical section end */

    }
    return ll_reserved_acl_buffer != NULL;
}

void ll_queue_acl_packet(const uint8_t * packet, uint16_t size){
    btstack_assert(ll_reserved_acl_buffer != NULL);

    ll_pdu_t * tx_packet = ll_reserved_acl_buffer;
    ll_reserved_acl_buffer = NULL;

    switch ((packet[1] >> 4) & 0x03){
        case 0:
        case 2:
            tx_packet->header = PDU_DATA_LLID_DATA_START;
            break;
        case 1:
            tx_packet->header = PDU_DATA_LLID_DATA_CONTINUE;
            break;
        case 3:
            while(1);
            break;
        default:
            break;
    }
    tx_packet->len = size - 4;
    memcpy(tx_packet->payload, &packet[4], size - 4);

    /** critical section start */
    hal_cpu_disable_irqs();
    btstack_linked_queue_enqueue(&ctx.tx_queue, (btstack_linked_item_t *) tx_packet);
    hal_cpu_enable_irqs();
    /** critical section end */
}

void ll_register_packet_handler(void (*packet_handler)(uint8_t packet_type, uint8_t * packet, uint16_t size)){
    controller_packet_handler = packet_handler;
}
