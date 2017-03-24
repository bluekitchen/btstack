/* Copyright (c) 2014 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

#define __BTSTACK_FILE__ "main.c"

/** 
 * BTstack Link Layer implementation
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>

#include "app_uart.h"
#include "app_error.h"
#include "nrf_delay.h"
#include "nrf.h"
#include "bsp.h"

#include "btstack_config.h"
#include "btstack_debug.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_embedded.h"
#include "hci_transport.h"
#include "hci_dump.h"
#include "hal_cpu.h"
#include "hal_time_ms.h"

// bluetooth.h
#define ADVERTISING_RADIO_ACCESS_ADDRESS 0x8E89BED6
#define ADVERTISING_CRC_INIT             0x555555

// HCI CMD OGF/OCF
#define READ_CMD_OGF(buffer) (buffer[1] >> 2)
#define READ_CMD_OCF(buffer) ((buffer[1] & 0x03) << 8 | buffer[0])

typedef enum {
    LL_STATE_STANDBY,
    LL_STATE_SCANNING,
    LL_STATE_ADVERTISING,
    LL_STATE_INITIATING,
    LL_STATE_CONNECTED
} ll_state_t;

// from SDK UART exzmple
#define UART_TX_BUF_SIZE 128                         /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE 1                           /**< UART RX buffer size. */

// packet receive buffer
#define MAXLEN 255
static uint8_t rx_adv_buffer[2 + MAXLEN];

// hci transport
static void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size);
static hci_transport_t hci_transport;
static uint8_t hci_outgoing_event[258];
static volatile uint8_t hci_outgoing_event_ready;
static volatile uint8_t hci_outgoing_event_free;
static btstack_data_source_t hci_transport_data_source;

// Link Layer State
static ll_state_t ll_state;
static uint32_t ll_scan_interval_us;
static uint32_t ll_scan_window_us;


void uart_error_handle(app_uart_evt_t * p_event)
{
    if (p_event->evt_type == APP_UART_COMMUNICATION_ERROR)
    {
        APP_ERROR_HANDLER(p_event->data.error_communication);
    }
    else if (p_event->evt_type == APP_UART_FIFO_ERROR)
    {
        APP_ERROR_HANDLER(p_event->data.error_code);
    }
}

static void init_timer(void) {

#if 1
    // start high frequency clock source if not done yet
    if ( !NRF_CLOCK->EVENTS_HFCLKSTARTED ) {
        NRF_CLOCK->TASKS_HFCLKSTART = 1;
        while ( !NRF_CLOCK->EVENTS_HFCLKSTARTED ){
            // just wait
        }
    }
#endif

    NRF_TIMER0->MODE        = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;
    NRF_TIMER0->BITMODE     = TIMER_BITMODE_BITMODE_32Bit;
    NRF_TIMER0->PRESCALER   = 4; // 16 Mhz / (2 ^ 4) = 1 Mhz == 1 us

    NRF_TIMER0->TASKS_STOP  = 1;
    NRF_TIMER0->TASKS_CLEAR = 1;
    NRF_TIMER0->EVENTS_COMPARE[0] = 0;
    NRF_TIMER0->EVENTS_COMPARE[1] = 0;
    NRF_TIMER0->EVENTS_COMPARE[2] = 0;
    NRF_TIMER0->EVENTS_COMPARE[3] = 0;
    NRF_TIMER0->INTENCLR    = 0xffffffff;
    NRF_TIMER0->TASKS_START = 1;
}

static void radio_set_access_address(uint32_t access_address) {
    NRF_RADIO->BASE0     = ( access_address << 8 ) & 0xFFFFFF00;
    NRF_RADIO->PREFIX0   = ( access_address >> 24 ) & RADIO_PREFIX0_AP0_Msk;
}

static void radio_set_crc_init(uint32_t crc_init){
    NRF_RADIO->CRCINIT   = crc_init;
}

// look up RF Center Frequency for data channels 0..36 and advertising channels 37..39 
static uint8_t radio_frequency_for_channel(uint8_t channel){
    if (channel <= 10){
        return 4 + 2 * channel;
    }
    if (channel <= 36){
        return 6 + 2 * channel;
    }
    if (channel == 37){
        return 2;
    }
    if (channel == 38){
        return 26;
    }
    return 80;
}

static void radio_init(void){

#ifdef NRF51
    // Handle BLE Radio tuning parameters from production if required.
    // Does not exist on NRF52
    // See PCN-083.
    if (NRF_FICR->OVERRIDEEN & FICR_OVERRIDEEN_BLE_1MBIT_Msk){
        NRF_RADIO->OVERRIDE0 = NRF_FICR->BLE_1MBIT[0];
        NRF_RADIO->OVERRIDE1 = NRF_FICR->BLE_1MBIT[1];
        NRF_RADIO->OVERRIDE2 = NRF_FICR->BLE_1MBIT[2];
        NRF_RADIO->OVERRIDE3 = NRF_FICR->BLE_1MBIT[3];
        NRF_RADIO->OVERRIDE4 = NRF_FICR->BLE_1MBIT[4] | 0x80000000;
    }
#endif // NRF51

    // Mode: BLE 1 Mbps
    NRF_RADIO->MODE  = RADIO_MODE_MODE_Ble_1Mbit << RADIO_MODE_MODE_Pos;

    // PacketConfig 0: 
    // ---
    // LENGTH field in bits = 8
    // S0 field in bytes = 1
    // S1 field not used
    // 8 bit preamble
    NRF_RADIO->PCNF0 =
        ( 8 << RADIO_PCNF0_LFLEN_Pos ) |
        ( 1 << RADIO_PCNF0_S0LEN_Pos ) |
        ( 0 << RADIO_PCNF0_S1LEN_Pos );

    // PacketConfig 1:
    // --- 
    // Payload MAXLEN = MAXLEN
    // No additional bytes
    // 4 address bytes (1 + 3)
    // S0, LENGTH, S1, PAYLOAD in little endian
    // Packet whitening enabled
    NRF_RADIO->PCNF1 =
        ( MAXLEN << RADIO_PCNF1_MAXLEN_Pos) |
        ( 0 << RADIO_PCNF1_STATLEN_Pos ) |
        ( 3 << RADIO_PCNF1_BALEN_Pos ) |
        ( RADIO_PCNF1_ENDIAN_Little << RADIO_PCNF1_ENDIAN_Pos ) |
        ( RADIO_PCNF1_WHITEEN_Enabled << RADIO_PCNF1_WHITEEN_Pos );

    // Use logical address 0 for sending and receiving
    NRF_RADIO->TXADDRESS   = 0;
    NRF_RADIO->RXADDRESSES = 1 << 0;

    // 24 bit CRC, skip address field
    NRF_RADIO->CRCCNF    =
    ( RADIO_CRCCNF_SKIPADDR_Skip << RADIO_CRCCNF_SKIPADDR_Pos ) |
    ( RADIO_CRCCNF_LEN_Three << RADIO_CRCCNF_LEN_Pos );

    // The polynomial has the form of x^24 +x^10 +x^9 +x^6 +x^4 +x^3 +x+1
    NRF_RADIO->CRCPOLY   = 0x100065B;

    // Inter frame spacing 150 us
    NRF_RADIO->TIFS      = 150;

    // Shorts:
    // - READY->START
    // - ADDRESS0>RSSISTART
    NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Enabled << RADIO_SHORTS_READY_START_Pos
                      | RADIO_SHORTS_ADDRESS_RSSISTART_Enabled << RADIO_SHORTS_ADDRESS_RSSISTART_Pos;

    // Disable all interrrupts
    NRF_RADIO->INTENCLR    = 0xffffffff;
}


void radio_dump_state(void){
    printf("Radio state: %lx\n", NRF_RADIO->STATE);
}

// static 
void radio_receive_on_channel(int channel){
    // set frequency based on channel
    NRF_RADIO->FREQUENCY   = radio_frequency_for_channel( channel );

    // initializes data whitening with channel index
    NRF_RADIO->DATAWHITEIV = channel & 0x3F;

    // set receive buffer
    NRF_RADIO->PACKETPTR   = (uintptr_t) rx_adv_buffer;

    // set MAXLEN based on receive buffer size
    NRF_RADIO->PCNF1       = ( NRF_RADIO->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( MAXLEN << RADIO_PCNF1_MAXLEN_Pos );

    // clear events
    NRF_RADIO->EVENTS_END       = 0;
    NRF_RADIO->EVENTS_DISABLED  = 0;
    NRF_RADIO->EVENTS_READY     = 0;
    NRF_RADIO->EVENTS_ADDRESS   = 0;

    radio_set_access_address(ADVERTISING_RADIO_ACCESS_ADDRESS);
    radio_set_crc_init(ADVERTISING_CRC_INIT);

    // ramp up receiver
    NRF_RADIO->TASKS_RXEN = 1;    

    // enable IRQ for END event
    NRF_RADIO->INTENSET = RADIO_INTENSET_END_Enabled << RADIO_INTENSET_END_Pos;
}

// static 
void radio_disable(void){
    // testing
    NRF_RADIO->TASKS_DISABLE = 1;

    // wait for ready
    while (!NRF_RADIO->EVENTS_DISABLED) {
        // just wait
    }
}

// static
void radio_dump_packet(void){
    // print data
    int len = rx_adv_buffer[1] & 0x3f;
    int i;
    for (i=0;i<len;i++){
        printf("%02x ", rx_adv_buffer[i]);
    }
    printf("\n");
}

void RADIO_IRQHandler(void){

    // IRQ only triggered on EVENTS_END so far
    NRF_RADIO->EVENTS_END = 0;

    if (ll_state == LL_STATE_SCANNING){

        // check if outgoing buffer available and if CRC was ok
        if (hci_outgoing_event_free && 
            ((NRF_RADIO->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk) == (RADIO_CRCSTATUS_CRCSTATUS_CRCOk << RADIO_CRCSTATUS_CRCSTATUS_Pos))){
            hci_outgoing_event_free = 0;
            int len = rx_adv_buffer[1] & 0x3f;
            hci_outgoing_event[0] = HCI_EVENT_LE_META;
            hci_outgoing_event[1] = 11 + len - 6;
            hci_outgoing_event[2] = HCI_SUBEVENT_LE_ADVERTISING_REPORT;
            hci_outgoing_event[3] = 1;
            hci_outgoing_event[4] = rx_adv_buffer[0] & 0x0f;
            hci_outgoing_event[5] = (rx_adv_buffer[0] & 0x40) ? 1 : 0;
            memcpy(&hci_outgoing_event[6], &rx_adv_buffer[2], 6);
            hci_outgoing_event[12] = len - 6;   // rest after bd addr
            memcpy(&hci_outgoing_event[13], &rx_adv_buffer[8], len - 6);
            hci_outgoing_event[13 + len - 6] = -NRF_RADIO->RSSISAMPLE; // RSSI is stored without sign but is negative
            hci_outgoing_event_ready = 1;
        } else {
            // ... for now, we just throw the adv away and try to receive the next one
        }

        // TODO: check if there's enough time left before the end of the scan interval

        // restart receiving
        NRF_RADIO->TASKS_START = 1;
    }
}

uint8_t random_generator_next(void){
    NRF_RNG->SHORTS = RNG_SHORTS_VALRDY_STOP_Enabled << RNG_SHORTS_VALRDY_STOP_Pos;
    NRF_RNG->TASKS_START = 1;
    while (!NRF_RNG->EVENTS_VALRDY){
    }
    return NRF_RNG->VALUE;
}

// uses TIMER0-CC2 for reads
uint32_t get_time_us(void){
    NRF_TIMER0->TASKS_CAPTURE[2] = 1;
    return NRF_TIMER0->CC[2]; 
}

volatile int timer_irq_happened;
void TIMER0_IRQHandler(void){
    // Reset IRQ flag
    NRF_TIMER0->EVENTS_COMPARE[0] = 0;
    NRF_TIMER0->TASKS_CLEAR = 1;

    timer_irq_happened = 1;

    // if (ll_state == LL_STATE_SCANNING){
    //     // Restart scanning
    //     // TODO: use all channels
    //     radio_receive_on_channel(37);
    // }
}

// TODO: implement
void hal_cpu_disable_irqs(void){}
void hal_cpu_enable_irqs(void){}
void hal_cpu_enable_irqs_and_sleep(void){}

// TODO: get time from RTC
uint32_t hal_time_ms(void){
    return 999;
}

static void ll_set_scan_parameters(uint8_t le_scan_type, uint16_t le_scan_interval, uint16_t le_scan_window, uint8_t own_address_type, uint8_t scanning_filter_policy){
    // TODO .. store other params
    ll_scan_interval_us = ((uint32_t) le_scan_interval) * 625;
    ll_scan_window_us   = ((uint32_t) le_scan_window)   * 625;
    log_info("LE Scan Params: window %lu, interval %lu ms", ll_scan_interval_us, ll_scan_window_us);
}

static uint8_t ll_start_scanning(uint8_t filter_duplicates){
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

    return 0;
}

static uint8_t ll_stop_scanning(void){
    // COMMAND DISALLOWED if wrong state.
    if (ll_state != LL_STATE_SCANNING)  return 0x0c;

    log_info("LE Scan Stop");

    ll_state = LL_STATE_STANDBY;

    // stop radio
    radio_disable();

    return 0;
}

static uint8_t ll_set_scan_enable(uint8_t le_scan_enable, uint8_t filter_duplicates){
    if (le_scan_enable){
        return ll_start_scanning(filter_duplicates);
    } else {
        return ll_stop_scanning();
    }
}

static void fake_command_complete(uint16_t opcode){
    hci_outgoing_event[0] = HCI_EVENT_COMMAND_COMPLETE;
    hci_outgoing_event[1] = 4;
    hci_outgoing_event[2] = 1;
    little_endian_store_16(hci_outgoing_event, 3, opcode);
    hci_outgoing_event[5] = 0;
    hci_outgoing_event_ready = 1;
}

static void send_hardware_error(uint8_t error_code){
    hci_outgoing_event[0] = HCI_EVENT_HARDWARE_ERROR;
    hci_outgoing_event[1] = 1;
    hci_outgoing_event[2] = error_code;
    hci_outgoing_event_ready = 1;
}


// command handler
static void controller_handle_hci_command(uint8_t * packet, uint16_t size){
    uint16_t opcode = little_endian_read_16(packet, 0);
#if 0
    uint16_t ocf = READ_CMD_OCF(packet);
    switch (READ_CMD_OGF(packet)){
        case OGF_CONTROLLER_BASEBAND:
            switch (ocf):
                break;
        default:
            break;
    }
#endif
    if (opcode == hci_reset.opcode) {
        fake_command_complete(opcode);
        return;
    }
    if (opcode == hci_le_set_scan_enable.opcode){
        ll_set_scan_enable(packet[3], packet[4]);
        fake_command_complete(opcode);
        return;
    }
    if (opcode == hci_le_set_scan_parameters.opcode){
        ll_set_scan_parameters(packet[3], little_endian_read_16(packet, 4), little_endian_read_16(packet, 6), packet[8], packet[9]);
        fake_command_complete(opcode);
        return;
    }
    // try with "OK" 
    printf("CMD opcode %02x not handled yet\n", opcode);
    fake_command_complete(opcode);
}

// ACL handler
static void controller_handle_acl_data(uint8_t * packet, uint16_t size){
    // TODO
}

static void transport_run(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type){
    // deliver hci packet on main thread
    if (hci_outgoing_event_ready){
        hci_outgoing_event_ready = 0;
        packet_handler(HCI_EVENT_PACKET, hci_outgoing_event, hci_outgoing_event[1]+2);
        hci_outgoing_event_free = 1;
    }
    if (timer_irq_happened){
        // printf("Timer irq occured\n");
        // radio_dump_state();
        timer_irq_happened = 0;

        if (ll_state == LL_STATE_SCANNING){
            // next channel to scan
            int adv_channel = (random_generator_next() % 3) + 37;
            log_debug("Restart scan on channel %u", adv_channel);
            radio_receive_on_channel(adv_channel);
        }
    }
}

/**
 * init transport
 * @param transport_config
 */
void transport_init(const void *transport_config){

}

/**
 * open transport connection
 */
static int transport_open(void){
    btstack_run_loop_set_data_source_handler(&hci_transport_data_source, &transport_run);
    btstack_run_loop_enable_data_source_callbacks(&hci_transport_data_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_add_data_source(&hci_transport_data_source);
    return 0;
}

/**
 * close transport connection
 */
static int transport_close(void){
    btstack_run_loop_remove_data_source(&hci_transport_data_source);
    return 0;
}

/**
 * register packet handler for HCI packets: ACL, SCO, and Events
 */
static void transport_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    packet_handler = handler;
}

int transport_send_packet(uint8_t packet_type, uint8_t *packet, int size){

    switch (packet_type){
        case HCI_COMMAND_DATA_PACKET:
            controller_handle_hci_command(packet, size);
            break;
        case HCI_ACL_DATA_PACKET:
            controller_handle_acl_data(packet, size);
            break;
        default:
            send_hardware_error(0x01);  // invalid HCI packet
            break;
    }
    return 0;    
}

int btstack_main(void);

/**
 * @brief Function for main application entry.
 */
int main(void)
{

    LEDS_CONFIGURE(LEDS_MASK);
    LEDS_OFF(LEDS_MASK);
    uint32_t err_code;
    const app_uart_comm_params_t comm_params = {
          RX_PIN_NUMBER,
          TX_PIN_NUMBER,
          RTS_PIN_NUMBER,
          CTS_PIN_NUMBER,
          APP_UART_FLOW_CONTROL_ENABLED,
          false,
          UART_BAUDRATE_BAUDRATE_Baud115200
    };

    APP_UART_FIFO_INIT(&comm_params,
                         UART_RX_BUF_SIZE,
                         UART_TX_BUF_SIZE,
                         uart_error_handle,
                         APP_IRQ_PRIORITY_LOW,
                         err_code);

    APP_ERROR_CHECK(err_code);

    init_timer();
    radio_init();
    hci_outgoing_event_free = 1;

    // enable Radio IRQs
    NVIC_SetPriority( RADIO_IRQn, 0 );
    NVIC_ClearPendingIRQ( RADIO_IRQn );
    NVIC_EnableIRQ( RADIO_IRQn );

    // enable Timer IRQs
    NVIC_SetPriority( TIMER0_IRQn, 0 );
    NVIC_ClearPendingIRQ( TIMER0_IRQn );
    NVIC_EnableIRQ( TIMER0_IRQn );

    // HCI Controller Defaults
    ll_scan_window_us   = 10000;
    ll_scan_interval_us = 10000;

    // Bring up BTstack
    printf("BTstack on Nordic nRF5 SDK\n");

    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_embedded_get_instance());

    // setup hci transport wrapper
    hci_transport.name                          = "nRF5";
    hci_transport.init                          = transport_init;
    hci_transport.open                          = transport_open;
    hci_transport.close                         = transport_close;
    hci_transport.register_packet_handler       = transport_register_packet_handler;
    hci_transport.can_send_packet_now           = NULL;
    hci_transport.send_packet                   = transport_send_packet;
    hci_transport.set_baudrate                  = NULL;

    // init HCI
    hci_init(&hci_transport, NULL);
    
    // enable full log output while porting
    hci_dump_open(NULL, HCI_DUMP_STDOUT);

    // hand over to btstack embedded code 
    btstack_main();

    // go
    btstack_run_loop_execute();

    while (1){};

#if 0

    // start listening
    radio_receive_on_channel(37);

    while (1){
        if (NRF_RADIO->EVENTS_END){
            NRF_RADIO->EVENTS_END = 0;
            // process packet
            radio_dump_packet();
            // receive next packet
            NRF_RADIO->TASKS_START = 1;
        }
    }

    radio_disable();
#endif
}


/** @} */
