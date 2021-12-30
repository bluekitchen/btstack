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

#define BTSTACK_FILE__ "hci_transport_em9304_spi.c"

#include "btstack_config.h"
#include "hci_transport_em9304_spi.h"

#include "hci_transport.h"

// EM9304 SPI Driver
static const btstack_em9304_spi_t * btstack_em9304_spi;

/////////////////////////
// em9304 engine
#include "btstack_ring_buffer.h"
#include "btstack_debug.h"
#include "btstack_util.h"
#include "hci.h"
#include "hci_transport.h"

static void em9304_spi_engine_run(void);

#define STS_SLAVE_READY 0xc0

#define EM9304_SPI_HEADER_TX        0x42
#define EM9304_SPI_HEADER_RX        0x81

#define SPI_EM9304_BUFFER_SIZE        64
#define SPI_EM9304_RING_BUFFER_SIZE  128

// state
static volatile enum {
    SPI_EM9304_OFF,
    SPI_EM9304_READY_FOR_TX,
    SPI_EM9304_READY_FOR_TX_AND_RX,
    SPI_EM9304_RX_W4_READ_COMMAND_SENT,
    SPI_EM9304_RX_READ_COMMAND_SENT,
    SPI_EM9304_RX_W4_STS2_RECEIVED,
    SPI_EM9304_RX_STS2_RECEIVED,
    SPI_EM9304_RX_W4_DATA_RECEIVED,
    SPI_EM9304_RX_DATA_RECEIVED,
    SPI_EM9304_TX_W4_RDY,
    SPI_EM9304_TX_W4_WRITE_COMMAND_SENT,
    SPI_EM9304_TX_WRITE_COMMAND_SENT,
    SPI_EM9304_TX_W4_STS2_RECEIVED,
    SPI_EM9304_TX_STS2_RECEIVED,
    SPI_EM9304_TX_W4_DATA_SENT,
    SPI_EM9304_TX_DATA_SENT,
    SPI_EM9304_DONE,
} em9304_spi_engine_state;

static uint16_t em9304_spi_engine_rx_request_len;
static uint16_t em9304_spi_engine_tx_request_len;

static btstack_ring_buffer_t em9304_spi_engine_rx_ring_buffer;

static uint8_t em9304_spi_engine_rx_ring_buffer_storage[SPI_EM9304_RING_BUFFER_SIZE];

static const uint8_t  * em9304_spi_engine_tx_data;
static uint16_t         em9304_spi_engine_tx_size;

// handlers
static void (*em9304_spi_engine_rx_available_handler)(void);
static void (*em9304_spi_engine_tx_done_handler)(void);

// TODO: get rid of alignment requirement
union {
    uint32_t words[1];
    uint8_t  bytes[1];
} sCommand;

union {
    uint32_t words[1];
    uint8_t  bytes[1];
} sStas;

union {
    uint32_t words[SPI_EM9304_BUFFER_SIZE/4];
    uint8_t  bytes[SPI_EM9304_BUFFER_SIZE];
} em9304_spi_engine_spi_buffer;

static void em9304_spi_engine_ready_callback(void){
    // TODO: collect states
    em9304_spi_engine_run();
}

static void em9304_spi_engine_transfer_done(void){
    switch (em9304_spi_engine_state){
        case SPI_EM9304_RX_W4_READ_COMMAND_SENT:
            em9304_spi_engine_state = SPI_EM9304_RX_READ_COMMAND_SENT;
            break;
        case SPI_EM9304_RX_W4_STS2_RECEIVED:
            em9304_spi_engine_state = SPI_EM9304_RX_STS2_RECEIVED;
            break;
        case SPI_EM9304_RX_W4_DATA_RECEIVED:
            em9304_spi_engine_state = SPI_EM9304_RX_DATA_RECEIVED;
            break;
        case SPI_EM9304_TX_W4_WRITE_COMMAND_SENT:
            em9304_spi_engine_state = SPI_EM9304_TX_WRITE_COMMAND_SENT;
            break;
        case SPI_EM9304_TX_W4_STS2_RECEIVED:
            em9304_spi_engine_state = SPI_EM9304_TX_STS2_RECEIVED;
            break;
        case SPI_EM9304_TX_W4_DATA_SENT:
            em9304_spi_engine_state = SPI_EM9304_TX_DATA_SENT;
            break;
        default:
            return;
    }
    em9304_spi_engine_run();
}

static void em9304_spi_engine_start_tx_transaction(void){
    // state = wait for RDY
    em9304_spi_engine_state = SPI_EM9304_TX_W4_RDY;

    // chip select
    btstack_em9304_spi->set_chip_select(1);

    // enable IRQ
    btstack_em9304_spi->set_ready_callback(&em9304_spi_engine_ready_callback);
}

static void em9304_spi_engine_start_rx_transaction(void){
    // disable interrupt again
    btstack_em9304_spi->set_ready_callback(NULL);

    // enable chip select
    btstack_em9304_spi->set_chip_select(1);

    // send read command
    em9304_spi_engine_state = SPI_EM9304_RX_W4_READ_COMMAND_SENT;
    sCommand.bytes[0] = EM9304_SPI_HEADER_RX;
    btstack_em9304_spi->transmit(sCommand.bytes, 1);
}

static inline int em9304_engine_space_in_rx_buffer(void){
    return btstack_ring_buffer_bytes_free(&em9304_spi_engine_rx_ring_buffer) >= SPI_EM9304_BUFFER_SIZE;
}

static void em9304_engine_receive_buffer_ready(void){
    // no data ready for receive or transmit, but space in rx ringbuffer  -> enable READY IRQ
    em9304_spi_engine_state = SPI_EM9304_READY_FOR_TX_AND_RX;
    btstack_em9304_spi->set_ready_callback(&em9304_spi_engine_ready_callback);
    // avoid dead lock, check READY again
    if (btstack_em9304_spi->get_ready()){
        em9304_spi_engine_start_rx_transaction();
    }
}

static void em9304_engine_start_next_transaction(void){
    
    switch (em9304_spi_engine_state){
        case SPI_EM9304_READY_FOR_TX:
        case SPI_EM9304_READY_FOR_TX_AND_RX:
        case SPI_EM9304_DONE:
            break;
        default:
            return;
    }   

    if (btstack_em9304_spi->get_ready() && em9304_engine_space_in_rx_buffer()) {
        em9304_spi_engine_start_rx_transaction();
    } else if (em9304_spi_engine_tx_size){
        em9304_spi_engine_start_tx_transaction();
    } else if (em9304_engine_space_in_rx_buffer()){
        em9304_engine_receive_buffer_ready();
    }
}

static void em9304_engine_action_done(void){
    // chip deselect & done
    btstack_em9304_spi->set_chip_select(0);
    em9304_spi_engine_state = SPI_EM9304_DONE;
}

static void em9304_spi_engine_run(void){
    uint16_t max_bytes_to_send;
    switch (em9304_spi_engine_state){

        case SPI_EM9304_READY_FOR_TX_AND_RX:
            // check if ready
            if (!btstack_em9304_spi->get_ready()) break;
            em9304_spi_engine_start_rx_transaction();
            break;

        case SPI_EM9304_RX_READ_COMMAND_SENT:
            em9304_spi_engine_state = SPI_EM9304_RX_W4_STS2_RECEIVED;
            btstack_em9304_spi->receive(sStas.bytes, 1);
            break;

        case SPI_EM9304_RX_STS2_RECEIVED:
            // check slave status
            log_debug("RX: STS2 0x%02X", sStas.bytes[0]);

            // read data
            em9304_spi_engine_state = SPI_EM9304_RX_W4_DATA_RECEIVED;
            em9304_spi_engine_rx_request_len = sStas.bytes[0];
            btstack_em9304_spi->receive(em9304_spi_engine_spi_buffer.bytes, em9304_spi_engine_rx_request_len);
            break;

        case SPI_EM9304_RX_DATA_RECEIVED:
            // done
            em9304_engine_action_done();

            // move data into ring buffer
            btstack_ring_buffer_write(&em9304_spi_engine_rx_ring_buffer, em9304_spi_engine_spi_buffer.bytes, em9304_spi_engine_rx_request_len);
            em9304_spi_engine_rx_request_len = 0;

            // notify about new data available -- assume empty
            (*em9304_spi_engine_rx_available_handler)();

            // next
            em9304_engine_start_next_transaction();
            break;

        case SPI_EM9304_TX_W4_RDY:
            // check if ready
            if (!btstack_em9304_spi->get_ready()) break;

            // disable interrupt again
            btstack_em9304_spi->set_ready_callback(NULL);

            // send write command
            em9304_spi_engine_state = SPI_EM9304_TX_W4_WRITE_COMMAND_SENT;
            sCommand.bytes[0] = EM9304_SPI_HEADER_TX;
            btstack_em9304_spi->transmit(sCommand.bytes, 1);
            break;

        case SPI_EM9304_TX_WRITE_COMMAND_SENT:
            em9304_spi_engine_state = SPI_EM9304_TX_W4_STS2_RECEIVED;
            btstack_em9304_spi->receive(sStas.bytes, 1);
            break;

        case SPI_EM9304_TX_STS2_RECEIVED:
            // check slave status and em9304 rx buffer space
            log_debug("TX: STS2 0x%02X", sStas.bytes[0]);
            max_bytes_to_send = sStas.bytes[0];
            if (max_bytes_to_send == 0u){
                // done
                em9304_engine_action_done();
                // next
                em9304_engine_start_next_transaction();
                break;
            }

            // number bytes to send
            em9304_spi_engine_tx_request_len = btstack_min(em9304_spi_engine_tx_size, max_bytes_to_send);

            // send command
            em9304_spi_engine_state = SPI_EM9304_TX_W4_DATA_SENT;
            if ( (((uintptr_t) em9304_spi_engine_tx_data) & 0x03u) == 0u){
                // 4-byte aligned
                btstack_em9304_spi->transmit( (uint8_t*) em9304_spi_engine_tx_data, em9304_spi_engine_tx_request_len);
            } else {
                // TODO: get rid of alignment requirement
                // enforce alignment by copying to spi buffer first
                (void)memcpy(em9304_spi_engine_spi_buffer.bytes,
                             em9304_spi_engine_tx_data,
                             em9304_spi_engine_tx_request_len);
                btstack_em9304_spi->transmit( (uint8_t*) em9304_spi_engine_spi_buffer.bytes, em9304_spi_engine_tx_request_len);
            }
            break;

        case SPI_EM9304_TX_DATA_SENT:
            // done
            em9304_engine_action_done();

            // chunk sent
            em9304_spi_engine_tx_size -= em9304_spi_engine_tx_request_len;
            em9304_spi_engine_tx_data += em9304_spi_engine_tx_request_len;
            em9304_spi_engine_tx_request_len = 0;

            // notify higher layer when complete
            if (em9304_spi_engine_tx_size == 0u){
                (*em9304_spi_engine_tx_done_handler)();
            }

            // next
            em9304_engine_start_next_transaction();
            break;

        default:
            break;
    }
}

static void em9304_spi_engine_init(void){
    btstack_em9304_spi->open();
    btstack_em9304_spi->set_transfer_done_callback(&em9304_spi_engine_transfer_done);
    btstack_ring_buffer_init(&em9304_spi_engine_rx_ring_buffer, &em9304_spi_engine_rx_ring_buffer_storage[0], SPI_EM9304_RING_BUFFER_SIZE);
    em9304_spi_engine_state = SPI_EM9304_DONE;
    em9304_engine_start_next_transaction();
}

static void em9304_spi_engine_close(void){
    em9304_spi_engine_state = SPI_EM9304_OFF;
    btstack_em9304_spi->close();
}

static void em9304_spi_engine_set_data_available( void (*the_block_handler)(void)){
    em9304_spi_engine_rx_available_handler = the_block_handler;
}

static void em9304_spi_engine_set_block_sent( void (*the_block_handler)(void)){
    em9304_spi_engine_tx_done_handler = the_block_handler;
}

static void em9304_spi_engine_send_block(const uint8_t *buffer, uint16_t length){
    em9304_spi_engine_tx_data = buffer;
    em9304_spi_engine_tx_size = length;
    em9304_engine_start_next_transaction();
}

static uint16_t em9304_engine_num_bytes_available(void){
    return btstack_ring_buffer_bytes_available(&em9304_spi_engine_rx_ring_buffer);
}

static void em9304_engine_get_bytes(uint8_t * buffer, uint16_t num_bytes){
    uint32_t bytes_read;
    btstack_ring_buffer_read(&em9304_spi_engine_rx_ring_buffer, buffer, num_bytes, &bytes_read);
}

//////////////////////////////////////////////////////////////////////////////

// assert pre-buffer for packet type is available
#if !defined(HCI_OUTGOING_PRE_BUFFER_SIZE) || (HCI_OUTGOING_PRE_BUFFER_SIZE == 0)
#error HCI_OUTGOING_PRE_BUFFER_SIZE not defined. Please update hci.h
#endif

static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size); 

typedef enum {
    H4_W4_PACKET_TYPE,
    H4_W4_EVENT_HEADER,
    H4_W4_ACL_HEADER,
    H4_W4_PAYLOAD,
} H4_STATE;

typedef enum {
    TX_IDLE = 1,
    TX_W4_PACKET_SENT,
} TX_STATE;

// write state
static TX_STATE tx_state;         

static  void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size) = dummy_handler;

// packet reader state machine
static H4_STATE hci_transport_em9304_h4_state;
static uint16_t hci_transport_em9304_spi_bytes_to_read;
static uint16_t hci_transport_em9304_spi_read_pos;

// incoming packet buffer
static uint8_t hci_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE + HCI_INCOMING_PACKET_BUFFER_SIZE + 1]; // packet type + max(acl header + acl payload, event header + event data)
static uint8_t * hci_packet = &hci_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE];

static void hci_transport_em9304_spi_block_read(void);

static void hci_transport_em9304_spi_reset_statemachine(void){
    hci_transport_em9304_h4_state = H4_W4_PACKET_TYPE;
    hci_transport_em9304_spi_read_pos = 0;
    hci_transport_em9304_spi_bytes_to_read = 1;
}

static void hci_transport_em9304_spi_process_data(void){
    while (true){

        uint16_t bytes_available = em9304_engine_num_bytes_available();
        log_debug("transfer_rx_data: ring buffer has %u -> hci wants %u", bytes_available, hci_transport_em9304_spi_bytes_to_read);

        if (!bytes_available) break;
        if (!hci_transport_em9304_spi_bytes_to_read) break;

        uint16_t bytes_to_copy = btstack_min(bytes_available, hci_transport_em9304_spi_bytes_to_read);
        em9304_engine_get_bytes(&hci_packet[hci_transport_em9304_spi_read_pos], bytes_to_copy);

        hci_transport_em9304_spi_read_pos      += bytes_to_copy;
        hci_transport_em9304_spi_bytes_to_read -= bytes_to_copy;

        if (hci_transport_em9304_spi_bytes_to_read == 0u){
            hci_transport_em9304_spi_block_read();
        }
    }
}

static void hci_transport_em9304_spi_packet_complete(void){
    packet_handler(hci_packet[0u], &hci_packet[1u], hci_transport_em9304_spi_read_pos-1u);
    hci_transport_em9304_spi_reset_statemachine();
}

static void hci_transport_em9304_spi_block_read(void){
    switch (hci_transport_em9304_h4_state) {
        case H4_W4_PACKET_TYPE:
            switch (hci_packet[0]){
                case HCI_EVENT_PACKET:
                    hci_transport_em9304_spi_bytes_to_read = HCI_EVENT_HEADER_SIZE;
                    hci_transport_em9304_h4_state = H4_W4_EVENT_HEADER;
                    break;
                case HCI_ACL_DATA_PACKET:
                    hci_transport_em9304_spi_bytes_to_read = HCI_ACL_HEADER_SIZE;
                    hci_transport_em9304_h4_state = H4_W4_ACL_HEADER;
                    break;
                default:
                    log_error("invalid packet type 0x%02x", hci_packet[0]);
                    hci_transport_em9304_spi_reset_statemachine();
                    break;
            }
            break;
            
        case H4_W4_EVENT_HEADER:
            hci_transport_em9304_spi_bytes_to_read = hci_packet[2];
            // check Event length
            if (hci_transport_em9304_spi_bytes_to_read > (HCI_INCOMING_PACKET_BUFFER_SIZE - HCI_EVENT_HEADER_SIZE)){
                log_error("invalid Event len %d - only space for %u", hci_transport_em9304_spi_bytes_to_read, HCI_INCOMING_PACKET_BUFFER_SIZE - HCI_EVENT_HEADER_SIZE);
                hci_transport_em9304_spi_reset_statemachine();
                break;
            }
            if (hci_transport_em9304_spi_bytes_to_read == 0u){
                hci_transport_em9304_spi_packet_complete();
                break;
            }
            hci_transport_em9304_h4_state = H4_W4_PAYLOAD;
            break;
            
        case H4_W4_ACL_HEADER:
            hci_transport_em9304_spi_bytes_to_read = little_endian_read_16( hci_packet, 3);
            // check ACL length
            if (hci_transport_em9304_spi_bytes_to_read > (HCI_INCOMING_PACKET_BUFFER_SIZE - HCI_ACL_HEADER_SIZE)){
                log_error("invalid ACL payload len %d - only space for %u", hci_transport_em9304_spi_bytes_to_read, HCI_INCOMING_PACKET_BUFFER_SIZE - HCI_ACL_HEADER_SIZE);
                hci_transport_em9304_spi_reset_statemachine();
                break;
            }
            if (hci_transport_em9304_spi_bytes_to_read == 0u){
                hci_transport_em9304_spi_packet_complete();
                break;
            }
            hci_transport_em9304_h4_state = H4_W4_PAYLOAD;
            break;
            
        case H4_W4_PAYLOAD:
            hci_transport_em9304_spi_packet_complete();
            break;
        default:
            break;
    }
}

static void hci_transport_em9304_spi_block_sent(void){

    static const uint8_t packet_sent_event[] = { HCI_EVENT_TRANSPORT_PACKET_SENT, 0};

    switch (tx_state){
        case TX_W4_PACKET_SENT:
            // packet fully sent, reset state
            tx_state = TX_IDLE;
            // notify upper stack that it can send again
            packet_handler(HCI_EVENT_PACKET, (uint8_t *) &packet_sent_event[0], sizeof(packet_sent_event));
            break;
        default:
            break;
    }
}

static int hci_transport_em9304_spi_can_send_now(uint8_t packet_type){
    UNUSED(packet_type);
    return tx_state == TX_IDLE;
}

static int hci_transport_em9304_spi_send_packet(uint8_t packet_type, uint8_t * packet, int size){

    // store packet type before actual data and increase size
    size++;
    packet--;
    *packet = packet_type;

    // start sending
    tx_state = TX_W4_PACKET_SENT;
    em9304_spi_engine_send_block(packet, size);
    return 0;
}

static void hci_transport_em9304_spi_init(const void * transport_config){
    UNUSED(transport_config);
}

static int hci_transport_em9304_spi_open(void){

    // setup UART driver
    em9304_spi_engine_init();
    em9304_spi_engine_set_data_available(&hci_transport_em9304_spi_process_data);
    em9304_spi_engine_set_block_sent(&hci_transport_em9304_spi_block_sent);
    // setup H4 RX
    hci_transport_em9304_spi_reset_statemachine();
    // setup H4 TX
    tx_state = TX_IDLE;
    return 0;
}

static int hci_transport_em9304_spi_close(void){
    em9304_spi_engine_close();
    return 0;
}

static void hci_transport_em9304_spi_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    packet_handler = handler;
}

static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(packet);
    UNUSED(size);
}

// --- end of eHCILL implementation ---------

// configure and return h4 singleton
const hci_transport_t * hci_transport_em9304_spi_instance(const btstack_em9304_spi_t * em9304_spi_driver) {

    static const hci_transport_t hci_transport_em9304_spi = {
            /* const char * name; */                                        "H4",
            /* void   (*init) (const void *transport_config); */            &hci_transport_em9304_spi_init,
            /* int    (*open)(void); */                                     &hci_transport_em9304_spi_open,
            /* int    (*close)(void); */                                    &hci_transport_em9304_spi_close,
            /* void   (*register_packet_handler)(void (*handler)(...); */   &hci_transport_em9304_spi_register_packet_handler,
            /* int    (*can_send_packet_now)(uint8_t packet_type); */       &hci_transport_em9304_spi_can_send_now,
            /* int    (*send_packet)(...); */                               &hci_transport_em9304_spi_send_packet,
            /* int    (*set_baudrate)(uint32_t baudrate); */                NULL,
            /* void   (*reset_link)(void); */                               NULL,
            /* void   (*set_sco_config)(uint16_t voice_setting, int num_connections); */ NULL,
    };

    btstack_em9304_spi = em9304_spi_driver;
    return &hci_transport_em9304_spi;
}
