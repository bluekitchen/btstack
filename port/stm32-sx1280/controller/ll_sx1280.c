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

#define DEBUG

#include <string.h>

#include "ll.h"

#include "hw.h"
#include "radio.h"
#include "sx1280.h"
#include "debug.h"
#include "btstack_config.h"
#include "btstack_debug.h"
#include "btstack_memory.h"
#include "btstack_memory_pool.h"
#include "btstack_linked_queue.h"
#include "bluetooth_company_id.h"
#include "hal_cpu.h"
#include "hci_event.h"
#include "hopping.h"

// access to timers
extern TIM_HandleTypeDef   htim2;
extern LPTIM_HandleTypeDef hlptim1;

#define ACL_LE_MAX_PAYLOAD 31
#define ADV_MAX_PAYLOAD    (6+6+22)
#define LL_MAX_PAYLOAD      37

// output power in dBM, range [-18..+13] dBm
#define TX_OUTPUT_POWER                             13

// Mask of IRQs to listen in tx and rx mode
#define RX_TX_IRQ_MASK (IRQ_RX_DONE | IRQ_TX_DONE | IRQ_RX_TX_TIMEOUT | IRQ_CRC_ERROR)

// sync hop delay - time we prepare for next connection event
#define SYNC_HOP_DELAY_US                           600

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

// Radio State
typedef enum {
    RADIO_LOWPOWER,
    RADIO_RX_ERROR,
    RADIO_TX_TIMEOUT,
    RADIO_W4_TX_DONE_TO_RX,
} radio_state_t;

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
    // over the air data
    uint8_t                header;
    uint8_t                len;
    uint8_t                payload[LL_MAX_PAYLOAD];
} ll_pdu_t;

// channel table: freq in hertz and whitening seed
static const struct {
    uint32_t freq_hz;
    uint8_t  whitening;
}  channel_table[] = {
        { 2404000000, 0x01 /* 00000001 */ },
        { 2406000000, 0x41 /* 01000001 */ },
        { 2408000000, 0x21 /* 00100001 */ },
        { 2410000000, 0x61 /* 01100001 */ },
        { 2412000000, 0x11 /* 00010001 */ },
        { 2414000000, 0x51 /* 01010001 */ },
        { 2416000000, 0x31 /* 00110001 */ },
        { 2418000000, 0x71 /* 01110001 */ },
        { 2420000000, 0x09 /* 00001001 */ },
        { 2422000000, 0x49 /* 01001001 */ },
        { 2424000000, 0x29 /* 00101001 */ },
        { 2428000000, 0x69 /* 01101001 */ },
        { 2430000000, 0x19 /* 00011001 */ },
        { 2432000000, 0x59 /* 01011001 */ },
        { 2434000000, 0x39 /* 00111001 */ },
        { 2436000000, 0x79 /* 01111001 */ },
        { 2438000000, 0x05 /* 00000101 */ },
        { 2440000000, 0x45 /* 01000101 */ },
        { 2442000000, 0x25 /* 00100101 */ },
        { 2444000000, 0x65 /* 01100101 */ },
        { 2446000000, 0x15 /* 00010101 */ },
        { 2448000000, 0x55 /* 01010101 */ },
        { 2450000000, 0x35 /* 00110101 */ },
        { 2452000000, 0x75 /* 01110101 */ },
        { 2454000000, 0x0d /* 00001101 */ },
        { 2456000000, 0x4d /* 01001101 */ },
        { 2458000000, 0x2d /* 00101101 */ },
        { 2460000000, 0x6d /* 01101101 */ },
        { 2462000000, 0x1d /* 00011101 */ },
        { 2464000000, 0x5d /* 01011101 */ },
        { 2466000000, 0x3d /* 00111101 */ },
        { 2468000000, 0x7d /* 01111101 */ },
        { 2470000000, 0x03 /* 00000011 */ },
        { 2472000000, 0x43 /* 01000011 */ },
        { 2474000000, 0x23 /* 00100011 */ },
        { 2476000000, 0x63 /* 01100011 */ },
        { 2478000000, 0x13 /* 00010011 */ },
        { 2402000000, 0x53 /* 01010011 */ },
        { 2426000000, 0x33 /* 00110011 */ },
        { 2480000000, 0x73 /* 01110011 */ },
};

// hopping context
static hopping_t h;

static struct {

    volatile bool     synced;

    volatile uint16_t packet_nr_in_connection_event;
    
    volatile uint16_t conn_interval_1250us;
    volatile uint32_t conn_interval_us;

    volatile uint16_t conn_latency;

    volatile uint16_t supervision_timeout_10ms;
    volatile uint32_t supervision_timeout_us;

    //
    volatile uint32_t time_without_any_packets_us;

    // access address
    volatile uint32_t aa;

    // start of current connection event
    volatile uint16_t anchor_ticks;

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
    volatile uint32_t conn_param_update_interval_us;
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
    uint8_t adv_map;

    // next expected sequence number
    volatile uint8_t next_expected_sequence_number;

    // transmit sequence number
    volatile uint8_t transmit_sequence_number;

    // num queued tx buffers
    volatile uint8_t num_tx_queued;

    // num completed packets
    volatile uint8_t num_completed;

    // current outgoing packet
    ll_pdu_t * tx_pdu;

    // tx queue
    btstack_linked_queue_t tx_queue;

    // rx queue
    btstack_linked_queue_t rx_queue; 

} ctx;

static radio_state_t radio_state = RADIO_LOWPOWER;

// Buffer pool
static ll_pdu_t ll_pdu_pool_storage[MAX_NUM_LL_PDUS];
static btstack_memory_pool_t ll_pdu_pool;

// single ll control response
static ll_pdu_t ll_tx_packet;

// Link Layer State
static ll_state_t ll_state;
static uint32_t ll_scan_interval_us;
static uint32_t ll_scan_window_us;

static ll_pdu_t * ll_reserved_acl_buffer;
static void (*controller_packet_handler)(uint8_t packet_type, uint8_t * packet, uint16_t size);

static uint8_t empty_packet[2];
static uint8_t ll_outgoing_hci_event[258];
static bool ll_send_disconnected;
static bool ll_send_connection_complete;


// memory pool for acl-le pdus
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

//
static void receive_first_master(void){
    Radio.SetRx( ( TickTime_t ) { RADIO_TICK_SIZE_1000_US, 1000 } );
}

static void receive_master(void){
    Radio.SetRx( ( TickTime_t ) { RADIO_TICK_SIZE_1000_US, 1 } );
}

static void send_adv(void){
    // setup advertisement: header (2) + addr (6) + data (31)
    uint8_t adv_buffer[39];
    adv_buffer[0] = PDU_ADV_TYPE_ADV_IND;  // TODO: also set private address bits
    adv_buffer[1] = 6 + ctx.adv_len;
    memcpy(&adv_buffer[2], ctx.bd_addr_le, 6);
    memcpy(&adv_buffer[8], ctx.adv_data, ctx.adv_len);
    uint16_t packet_size = 2 + adv_buffer[1];
    Radio.SendPayload( adv_buffer, packet_size, ( TickTime_t ){ RADIO_TICK_SIZE_1000_US, 1 } );
}

static void receive_adv_response(void){
    Radio.SetRx( ( TickTime_t ) { RADIO_TICK_SIZE_0015_US, 10 } );  // 220 us
}

static void select_channel(uint8_t channel){
    // Set Whitening seed
    Radio.SetWhiteningSeed( channel_table[channel].whitening );

    // Sel Frequency
    Radio.SetRfFrequency( channel_table[channel].freq_hz );
}

static void next_channel(void){
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
    select_channel(ctx.channel);
}

static void start_advertising(void){

    Radio.SetAutoTx(AUTO_RX_TX_OFFSET);

    PacketParams_t packetParams;
    packetParams.PacketType = PACKET_TYPE_BLE;
    packetParams.Params.Ble.BlePacketType = BLE_EYELONG_1_0;
    packetParams.Params.Ble.ConnectionState = BLE_PAYLOAD_LENGTH_MAX_37_BYTES;
    packetParams.Params.Ble.CrcField = BLE_CRC_3B;
    packetParams.Params.Ble.Whitening = RADIO_WHITENING_ON;
    Radio.SetPacketParams( &packetParams );

    // Set CRC init value 0x555555
    Radio.WriteRegister(0x9c7, 0x55 );
    Radio.WriteRegister(0x9c8, 0x55 );
    Radio.WriteRegister(0x9c9, 0x55 );

    // Set AccessAddress for ADV packets
    Radio.SetBleAdvertizerAccessAddress( );

    ll_state = LL_STATE_ADVERTISING;

    // dummy channel
    ctx.channel = 36;
}

static void start_hopping(void){
    PacketParams_t packetParams;
    packetParams.PacketType = PACKET_TYPE_BLE;
    packetParams.Params.Ble.BlePacketType = BLE_EYELONG_1_0;
    packetParams.Params.Ble.ConnectionState = BLE_PAYLOAD_LENGTH_MAX_31_BYTES;
    packetParams.Params.Ble.CrcField = BLE_CRC_3B;
    packetParams.Params.Ble.Whitening = RADIO_WHITENING_ON;
    Radio.SetPacketParams( &packetParams );

}
static void stop_timer_for_sync_hop(void){
    __HAL_LPTIM_DISABLE_IT(&hlptim1, LPTIM_IT_CMPM);
    __HAL_LPTIM_CLEAR_FLAG(&hlptim1, LPTIM_IT_CMPM);
}

static void set_timer_for_sync_hop(void){
    // stop
    stop_timer_for_sync_hop();
    // set timer
    uint16_t timeout_ticks = ctx.anchor_ticks + US_TO_TICKS(ctx.conn_interval_us - SYNC_HOP_DELAY_US);
    __HAL_LPTIM_COMPARE_SET(&hlptim1, timeout_ticks);
    __HAL_LPTIM_ENABLE_IT(&hlptim1, LPTIM_IT_CMPM);
}

static void ll_terminate(void){
    ll_state = LL_STATE_STANDBY;
    // stop sync hop timer
    stop_timer_for_sync_hop();
    // free outgoing tx packet
    if ((ctx.tx_pdu != NULL) && (ctx.tx_pdu != &ll_tx_packet)){
        btstack_memory_ll_pdu_free(ctx.tx_pdu);
        ctx.tx_pdu = NULL;
    }
    // disable auto tx
    Radio.StopAutoTx();
    // notify host stack
    ll_send_disconnected = true;
}

static void sync_next_hop(void){

    uint16_t t0 = HAL_LPTIM_ReadCounter(&hlptim1);

    // check supervision timeout
    ctx.time_without_any_packets_us += ctx.conn_interval_us;
    if (ctx.time_without_any_packets_us > ctx.supervision_timeout_us) {
        printf("Supervision timeout\n\n");
        ll_terminate();
        return;
    }

    // prepare next connection event
    ctx.connection_event++;
    ctx.anchor_ticks += US_TO_TICKS(ctx.conn_interval_us);

    ctx.packet_nr_in_connection_event = 0;
    next_channel();

    if (ctx.channel_map_update_pending && (ctx.channel_map_update_instant == ctx.connection_event)) {
        hopping_set_channel_map( &h, (const uint8_t *) &ctx.channel_map_update_map );
        ctx.channel_map_update_pending = false;
    }

    if ( ctx.conn_param_update_pending && ((ctx.conn_param_update_instant) == ctx.connection_event) ) {
        ctx.conn_interval_us        = ctx.conn_param_update_interval_us;
        ctx.conn_latency            = ctx.conn_param_update_latency;
        ctx.supervision_timeout_us  = ctx.conn_param_update_timeout_us;
        ctx.conn_param_update_pending = false;

        stop_timer_for_sync_hop();
        ctx.synced = false;
    }

    // restart sync next hop timer (might get overwritten by first packet)
    set_timer_for_sync_hop();

    receive_master();

    printf("--SYNC-Ch %02u-Event %04u - t %08u--\n", ctx.channel, ctx.connection_event, t0);
}

void HAL_LPTIM_CompareMatchCallback(LPTIM_HandleTypeDef *hlptim){
    UNUSED(hlptim);
    sync_next_hop();
}

void HAL_LPTIM_AutoReloadMatchCallback(LPTIM_HandleTypeDef *hlptim){
    UNUSED(hlptim);
    static uint32_t time_seconds = 0;
    time_seconds += 2;
    printf("Time: %4u s\n", time_seconds);
}

/** Radio IRQ handlers */
static void radio_on_tx_done(void ){
    switch (radio_state){
        case RADIO_W4_TX_DONE_TO_RX:
            receive_adv_response();
            break;
        default:
            break;
    }
}

static void radio_on_rx_done(void ){
    uint16_t packet_end_ticks =   HAL_LPTIM_ReadCounter(&hlptim1);
    ll_pdu_t * rx_packet;
    bool tx_acked;

    uint8_t sequence_number;
    uint8_t next_expected_sequence_number;
    // uint8_t more_data;

    rx_packet = btstack_memory_ll_pdu_get();

    // if no buffer ready, just drop it
    if (rx_packet == NULL) {
        printf("No free RX buffer\n");
        return;
    }

    // Read complete buffer
    SX1280HalReadBuffer( 0, &rx_packet->header, 2 + LL_MAX_PAYLOAD );

    if (ll_state == LL_STATE_CONNECTED){
        // mark as data packet
        rx_packet->flags |= LL_PDU_FLAG_DATA_PDU;
    }

    // queue received packet
    btstack_linked_queue_enqueue(&ctx.rx_queue, (btstack_linked_item_t *) rx_packet);

    if (ll_state == LL_STATE_CONNECTED){
        // parse header
        next_expected_sequence_number = (rx_packet->header >> 2) & 1;
        sequence_number = (rx_packet->header >> 3) & 1;
        // more_data = (rx_packet->header >> 4) & 1;

        // update state
        ctx.next_expected_sequence_number = 1 - sequence_number;

        // tx packet ack'ed?
        tx_acked = ctx.transmit_sequence_number != next_expected_sequence_number;
        if (tx_acked){
            if ((ctx.tx_pdu != NULL) && (ctx.tx_pdu != &ll_tx_packet)){
                btstack_memory_ll_pdu_free(ctx.tx_pdu);
                ctx.num_completed++;
            }
            ctx.tx_pdu = (ll_pdu_t *) btstack_linked_queue_dequeue(&ctx.tx_queue);
            ctx.transmit_sequence_number = next_expected_sequence_number;
        }
        // refill
        if (ctx.tx_pdu == NULL){
            ctx.tx_pdu = (ll_pdu_t *) btstack_linked_queue_dequeue(&ctx.tx_queue);
        }

        // tx packet ready?
        if (ctx.tx_pdu == NULL){
            empty_packet[0] = (ctx.transmit_sequence_number << 3) | (ctx.next_expected_sequence_number << 2) | PDU_DATA_LLID_DATA_CONTINUE;
            empty_packet[1] = 0;
            Radio.SetPayload(empty_packet, 2);
        } else {
            uint8_t md = btstack_linked_queue_empty(&ctx.tx_queue) ? 0 : 1;
            ctx.tx_pdu->header |= (md << 4) | (ctx.transmit_sequence_number << 3) | (ctx.next_expected_sequence_number << 2);
            Radio.SetPayload((uint8_t *) &ctx.tx_pdu->header, 2 + ctx.tx_pdu->len);
        }

        // preamble (1) + aa (4) + header (1) + len (1) + payload (len) + crc (3) -- ISR handler ca. 50 us
        uint16_t timestamp_delay = (10 + rx_packet->len) * 8 - 50;
        uint16_t packet_start_ticks = packet_end_ticks - US_TO_TICKS(timestamp_delay);

        // restart supervision timeout
        ctx.time_without_any_packets_us = 0;

        // set anchor on first packet in connection event
        if (ctx.packet_nr_in_connection_event == 0){
            ctx.anchor_ticks = packet_start_ticks;
            ctx.synced = true;
            set_timer_for_sync_hop();
        }

        ctx.packet_nr_in_connection_event++;

        printf("RX %02x\n", rx_packet->header);
    }
}

static void radio_on_tx_timeout(void ){
    radio_state = RADIO_TX_TIMEOUT;
    printf( "<>>>>>>>>TXE\n\r" ); 
}

static void radio_on_rx_timeout(void ){
    switch (ll_state){
        case LL_STATE_ADVERTISING:
            radio_state = RADIO_RX_ERROR;
            break;
        default:
            break;
    }
}

static void radio_on_rx_error(IrqErrorCode_t errorCode ){
    switch (ll_state){
        case LL_STATE_ADVERTISING:
            radio_state = RADIO_RX_ERROR;
            break;
        default:
            break;
    }
}

const static RadioCallbacks_t Callbacks =
{
    &radio_on_tx_done,     // txDone
    &radio_on_rx_done,     // rxDone
    NULL,                  // syncWordDone
    NULL,                  // headerDone
    &radio_on_tx_timeout,  // txTimeout
    &radio_on_rx_timeout,  // rxTimeout
    &radio_on_rx_error,    // rxError
    NULL,                  // rangingDone
    NULL,                  // cadDone
};

// Link Layer

static void ll_emit_hci_event(const hci_event_t * event, ...){
    va_list argptr;
    va_start(argptr, event);
    uint16_t length = hci_event_create_from_template_and_arglist(ll_outgoing_hci_event, event, argptr);
    va_end(argptr);
    controller_packet_handler(HCI_EVENT_PACKET, ll_outgoing_hci_event, length);
}

void ll_init(void){

    // setup memory pools
    btstack_memory_pool_create(&ll_pdu_pool, ll_pdu_pool_storage, MAX_NUM_LL_PDUS, sizeof(ll_pdu_t));

    // set test bd addr 33:33:33:33:33:33
    memset(ctx.bd_addr_le, 0x33, 6);

    // default channels
    ctx.adv_map = 0x7;
}

void ll_radio_on(void){
    Radio.Init( (RadioCallbacks_t *) &Callbacks );
    Radio.SetRegulatorMode( USE_DCDC ); // Can also be set in LDO mode but consume more power
    Radio.SetInterruptMode( );
    Radio.SetDioIrqParams( RX_TX_IRQ_MASK, RX_TX_IRQ_MASK, IRQ_RADIO_NONE, IRQ_RADIO_NONE );

    ModulationParams_t modulationParams;
    modulationParams.PacketType = PACKET_TYPE_BLE;
    modulationParams.Params.Ble.BitrateBandwidth = GFSK_BLE_BR_1_000_BW_1_2;
    modulationParams.Params.Ble.ModulationIndex = GFSK_BLE_MOD_IND_0_50;
    modulationParams.Params.Ble.ModulationShaping = RADIO_MOD_SHAPING_BT_0_5;

    Radio.SetStandby( STDBY_RC );
    Radio.SetPacketType( modulationParams.PacketType );
    Radio.SetModulationParams( &modulationParams );
    Radio.SetBufferBaseAddresses( 0x00, 0x00 );
    Radio.SetTxParams( TX_OUTPUT_POWER, RADIO_RAMP_02_US );
    
    // Go back to Frequcency Synthesis Mode, reduces transition time between Rx<->TX
    Radio.SetAutoFS(1);

    ll_state = LL_STATE_STANDBY;
}

static void ll_handle_conn_ind(ll_pdu_t * rx_packet){
    printf("Connect Req: ");
    printf_hexdump(&rx_packet->header, rx_packet->len + 2);

    uint8_t * init_addr = &rx_packet->payload[0];
    uint8_t * adv_addr =  &rx_packet->payload[6];
    uint8_t   chan_sel = (rx_packet->header >> 5) & 1;

    // verify AdvA
    if (memcmp(ctx.bd_addr_le, adv_addr, 6) != 0){
        // differs, go back to adv sending
        radio_state = RADIO_LOWPOWER;
        return;
    }

    // TODO: get remote addr type
    ctx.peer_addr_type = 0;
    memcpy(ctx.peer_addr, init_addr, 6);

    // get params for HCI event
    const uint8_t * ll_data = &rx_packet->payload[12];

    ctx.aa                       = little_endian_read_32(ll_data, 0);
    uint8_t crc_init_0           = ll_data[4];
    uint8_t crc_init_1           = ll_data[5];
    uint8_t crc_init_2           = ll_data[6];
    uint8_t win_size             = ll_data[7];
    uint16_t win_offset          = little_endian_read_16(ll_data, 8);
    ctx.conn_interval_1250us     = little_endian_read_16(ll_data, 10);
    ctx.conn_latency             = little_endian_read_16(ll_data, 12);
    ctx.supervision_timeout_10ms = little_endian_read_16(ll_data, 14);
    const uint8_t * channel_map = &ll_data[16];
    uint8_t hop = ll_data[21] & 0x1f;
    uint8_t sca = ll_data[21] >> 5;

    UNUSED(sca);
    UNUSED(win_offset);
    UNUSED(win_size);

    // convert to us
    ctx.conn_interval_us = ctx.conn_interval_1250us * 1250;
    ctx.supervision_timeout_us  = ctx.supervision_timeout_10ms  * 10000;

    ctx.connection_event = 0;
    ctx.packet_nr_in_connection_event = 0;
    ctx.next_expected_sequence_number = 0;
    ctx.transmit_sequence_number = 0;

    // set AA
    Radio.SetBleAccessAddress(ctx.aa);

    // set CRC init value
    Radio.WriteRegister(0x9c7, crc_init_2);
    Radio.WriteRegister(0x9c8, crc_init_1);
    Radio.WriteRegister(0x9c9, crc_init_0);

    printf("Connection interval %u us\n", ctx.conn_interval_us);
    printf("Connection timeout  %u us\n", ctx.supervision_timeout_us);
    printf("AA %08x\n", ctx.aa);
    printf("CRC Init 0x%02x%02x%02x\n", crc_init_2, crc_init_1, crc_init_0);

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
    next_channel();

    start_hopping();
    
    // Enable Rx->Tx in 150 us for BLE
    // Note: Driver subtracts AUTO_RX_TX_OFFSET (33) from it and 150 should be correct, Raccoon reports 181 us then, so -31
    Radio.SetAutoTx(119);

    // get next packet
    ll_state = LL_STATE_CONNECTED;

    receive_first_master();
    ll_send_connection_complete = true;
}

static void ll_handle_control(ll_pdu_t * rx_packet){
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
            memcpy((uint8_t *) ctx.channel_map_update_map, &tx_packet->payload[1], 5);
            ctx.channel_map_update_instant   = little_endian_read_16(tx_packet->payload, 6);
            ctx.channel_map_update_pending   = true;
            break;
        case PDU_DATA_LLCTRL_TYPE_TERMINATE_IND:
            printf("Terminate!\n");
            ll_terminate();
            break;
        default:
            break;
    }
}

static void ll_handle_data(ll_pdu_t * rx_packet){
    if (ll_state != LL_STATE_CONNECTED) return;
    uint8_t acl_packet[40];
    // ACL Header
    uint8_t ll_id = rx_packet->header & 3;
    acl_packet[0] = 0x01;
    acl_packet[1] = ll_id << 4;
    little_endian_store_16(acl_packet, 2, rx_packet->len);
    memcpy(&acl_packet[4], rx_packet->payload, rx_packet->len);
    (*controller_packet_handler)(HCI_ACL_DATA_PACKET, acl_packet, rx_packet->len + 4);
}

void ll_set_scan_parameters(uint8_t le_scan_type, uint16_t le_scan_interval, uint16_t le_scan_window, uint8_t own_address_type, uint8_t scanning_filter_policy){
    // TODO .. store other params
    ll_scan_interval_us = ((uint32_t) le_scan_interval) * 625;
    ll_scan_window_us   = ((uint32_t) le_scan_window)   * 625;
    log_info("LE Scan Params: window %lu, interval %lu ms", ll_scan_interval_us, ll_scan_window_us);
}

static uint8_t ll_start_scanning(uint8_t filter_duplicates){
#if 0
    // COMMAND DISALLOWED if wrong state.
    if (ll_state != LL_STATE_STANDBY)  return 0x0c;

    ll_state = LL_STATE_SCANNING;

    log_info("LE Scan Start: window %lu, interval %lu ms", ll_scan_interval_us, ll_scan_window_us);

    // reset timer and capature events
    NRF_TIMER0->TASKS_CLEAR = 1;
    NRF_TIMER0->TASKS_STOP  = 1;
    NRF_TIMER0->EVENTS_COMPARE[0] = 0;
    NRF_TIMER0->EVENTS_COMPARE[1] = 0;
    
    // limit scanning        
    if (ll_scan_window_us < ll_scan_interval_us){
        // setup PPI to disable radio after end of scan_window
        NRF_TIMER0->CC[1]    = ll_scan_window_us;
        NRF_PPI->CHENSET     = 1 << 22; // TIMER0->EVENTS_COMPARE[1] ->  RADIO->TASKS_DISABLE
    }

    // set timer to trigger IRQ for next scan interval
    NRF_TIMER0->CC[0]    = ll_scan_interval_us;
    NRF_TIMER0->INTENSET = TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos;

    // next channel to scan
    int adv_channel = (random_generator_next() % 3) + 37;
    log_debug("LE Scan Channel: %u", adv_channel);

    // start receiving
    NRF_TIMER0->TASKS_START = 1;
    radio_receive_on_channel(adv_channel);
#endif
    return 0;
}

static uint8_t ll_stop_scanning(void){
#if 0
    // COMMAND DISALLOWED if wrong state.
    if (ll_state != LL_STATE_SCANNING)  return 0x0c;

    log_info("LE Scan Stop");

    ll_state = LL_STATE_STANDBY;

    // stop radio
    radio_disable();

#endif
    return 0;
}

uint8_t ll_set_scan_enable(uint8_t le_scan_enable, uint8_t filter_duplicates){
    if (le_scan_enable){
        return ll_start_scanning(filter_duplicates);
    } else {
        return ll_stop_scanning();
    }
}

static uint8_t ll_start_advertising(void){
    // COMMAND DISALLOWED if wrong state.
    if (ll_state != LL_STATE_STANDBY) return ERROR_CODE_COMMAND_DISALLOWED;
    printf("Start Advertising on channels 0x%0x\n", ctx.adv_map);
    start_advertising();
    return ERROR_CODE_SUCCESS;
}

static uint8_t ll_stop_advertising(void){
    // COMMAND DISALLOWED if wrong state.
    if (ll_state != LL_STATE_ADVERTISING) return ERROR_CODE_COMMAND_DISALLOWED;

    // TODO:
    return ERROR_CODE_SUCCESS;
}

uint8_t ll_set_advertise_enable(uint8_t le_adv_enable){
    if (le_adv_enable){
        return ll_start_advertising();
    } else {
        return ll_stop_advertising();
    }
}

uint8_t ll_set_advertising_parameters(uint16_t advertising_interval_min, uint16_t advertising_interval_max,
                                      uint8_t advertising_type, uint8_t own_address_type, uint8_t peer_address_types, uint8_t * peer_address,
                                      uint8_t advertising_channel_map, uint8_t advertising_filter_policy){
    if (advertising_channel_map == 0) return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    if ((advertising_channel_map & 0xf8) != 0) return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    ctx.adv_map = advertising_channel_map;

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

    // TODO:
    return ERROR_CODE_SUCCESS;
}

void ll_execute_once(void){
    // process received packets
    while (1){
        ll_pdu_t * rx_packet = (ll_pdu_t *) btstack_linked_queue_dequeue(&ctx.rx_queue);
        if (rx_packet == NULL) break;
        if (rx_packet->len > 0){
            if ((rx_packet->flags & LL_PDU_FLAG_DATA_PDU) == 0){
                // ADV PDU
                // connect ind?
                if ((rx_packet->header & 0x0f) == PDU_ADV_TYPE_CONNECT_IND){
                    ll_handle_conn_ind(rx_packet);
                }
                else {
                    radio_state = RADIO_LOWPOWER;
                }
            } else {
                // DATA PDU
                uint8_t ll_id = rx_packet->header & 3;
                if (ll_id == PDU_DATA_LLID_CTRL) {
                    ll_handle_control(rx_packet);
                } else {
                    ll_handle_data(rx_packet);
                }
            }
        }
        // free packet
        btstack_memory_ll_pdu_free(rx_packet);
    }

    switch ( ll_state ){
        case LL_STATE_ADVERTISING:
            switch ( radio_state) {
                case RADIO_RX_ERROR:
                case RADIO_LOWPOWER:
                    // find next channel
                    while (ctx.adv_map != 0){
                        ctx.channel++;
                        if ((ctx.adv_map & (1 << (ctx.channel - 37))) != 0) {
                            // Set Channel
                            select_channel(ctx.channel);
                            radio_state = RADIO_W4_TX_DONE_TO_RX;
                            send_adv();
                            break;
                        }
                        if (ctx.channel >= 40){
                            ctx.channel = 36;
                        }
                    }
                    break;
                default:
                    break;
            }
        default:
            break;
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
    if (ll_send_connection_complete){
        ll_send_connection_complete = false;
        ll_emit_hci_event(&hci_subevent_le_connection_complete,
                                 ERROR_CODE_SUCCESS, HCI_CON_HANDLE, 0x01 /* slave */, ctx.peer_addr_type, ctx.peer_addr,
                                 ctx.conn_interval_1250us, ctx.conn_latency, ctx.supervision_timeout_10ms, 0 /* master clock accuracy */);
    }

    // report disconnection event
    if (ll_send_disconnected){
        ll_send_disconnected = false;
        ll_emit_hci_event(&hci_event_disconnection_complete, ERROR_CODE_SUCCESS, HCI_CON_HANDLE, 0);
    }
}
bool ll_reserve_acl_packet(void){
    if (ll_reserved_acl_buffer == NULL){
        ll_reserved_acl_buffer = btstack_memory_ll_pdu_get();
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
    btstack_linked_queue_enqueue(&ctx.tx_queue, (btstack_linked_item_t *) tx_packet);
}

void ll_register_packet_handler(void (*packet_handler)(uint8_t packet_type, uint8_t * packet, uint16_t size)){
    controller_packet_handler = packet_handler;
}
