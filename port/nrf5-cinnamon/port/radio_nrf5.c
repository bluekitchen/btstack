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

#define BTSTACK_FILE__ "radio_nrf5.c"

#include "radio.h"
#include "btstack_debug.h"
#include <inttypes.h>
#include "nrf.h"
#include "nrf52.h"
#include "nrf_gpio.h"
#include "nrf_gpiote.h"

#define MAXLEN 37

static enum {
    RADIO_OFF,
    RADIO_DISABLED,
    RADIO_W2_TX,
    RADIO_W4_TX_DONE,
    RADIO_W4_TX_TO_RX,
    RADIO_W2_RX,
    RADIO_W4_RX_DONE,
    RADIO_W4_RX_TIMEOUT,
    RADIO_W4_DISABLED,
} volatile radio_state;

static radio_callback_t radio_callback;
static int8_t * rssi_buffer;

// channel table: freq in hertz and whitening seed
static const struct {
    uint8_t freq_index;
    uint8_t  whitening;
}  channel_table[] = {
        {  4, 0x01 /* 00000001 */ },
        {  6, 0x41 /* 01000001 */ },
        {  8, 0x21 /* 00100001 */ },
        { 10, 0x61 /* 01100001 */ },
        { 12, 0x11 /* 00010001 */ },
        { 14, 0x51 /* 01010001 */ },
        { 16, 0x31 /* 00110001 */ },
        { 18, 0x71 /* 01110001 */ },
        { 20, 0x09 /* 00001001 */ },
        { 22, 0x49 /* 01001001 */ },
        { 24, 0x29 /* 00101001 */ },
        { 28, 0x69 /* 01101001 */ },
        { 30, 0x19 /* 00011001 */ },
        { 32, 0x59 /* 01011001 */ },
        { 34, 0x39 /* 00111001 */ },
        { 36, 0x79 /* 01111001 */ },
        { 38, 0x05 /* 00000101 */ },
        { 40, 0x45 /* 01000101 */ },
        { 42, 0x25 /* 00100101 */ },
        { 44, 0x65 /* 01100101 */ },
        { 46, 0x15 /* 00010101 */ },
        { 48, 0x55 /* 01010101 */ },
        { 50, 0x35 /* 00110101 */ },
        { 52, 0x75 /* 01110101 */ },
        { 54, 0x0d /* 00001101 */ },
        { 56, 0x4d /* 01001101 */ },
        { 58, 0x2d /* 00101101 */ },
        { 60, 0x6d /* 01101101 */ },
        { 62, 0x1d /* 00011101 */ },
        { 64, 0x5d /* 01011101 */ },
        { 66, 0x3d /* 00111101 */ },
        { 68, 0x7d /* 01111101 */ },
        { 70, 0x03 /* 00000011 */ },
        { 72, 0x43 /* 01000011 */ },
        { 74, 0x23 /* 00100011 */ },
        { 76, 0x63 /* 01100011 */ },
        { 78, 0x13 /* 00010011 */ },
        {  2, 0x53 /* 01010011 */ },
        { 26, 0x33 /* 00110011 */ },
        { 80, 0x73 /* 01110011 */ },
};

void radio_init(void){

    radio_state = RADIO_OFF;

    /* TIMER0 setup */
    NRF_TIMER0->TASKS_STOP = 1;
    NRF_TIMER0->TASKS_SHUTDOWN = 1;
    NRF_TIMER0->BITMODE = 3;    /* 32-bit timer */
    NRF_TIMER0->MODE = 0;       /* Timer mode */
    NRF_TIMER0->PRESCALER = 4;  /* gives us 1 MHz */

    // PPI setup
    // Channel 0: RADIO END -> TIMER0 Start
    NRF_PPI->CH[0].EEP = (uint32_t)&(NRF_RADIO->EVENTS_END);
    NRF_PPI->CH[0].TEP = (uint32_t)&(NRF_TIMER0->TASKS_START);
    // Channel 1: RADIO ADDRESS -> TIMER0 Stop
    NRF_PPI->CH[1].EEP = (uint32_t)&(NRF_RADIO->EVENTS_ADDRESS);
    NRF_PPI->CH[1].TEP = (uint32_t)&(NRF_TIMER0->TASKS_STOP);

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

    // Transmit with max power
    NRF_RADIO->TXPOWER = (RADIO_TXPOWER_TXPOWER_Pos4dBm << RADIO_TXPOWER_TXPOWER_Pos);

    // Disable all interrupts
    NRF_RADIO->INTENCLR = 0xffffffff;

    // enable Radio IRQs
    NVIC_SetPriority( RADIO_IRQn, 0 );
    NVIC_ClearPendingIRQ( RADIO_IRQn );
    NVIC_EnableIRQ( RADIO_IRQn );

#ifdef DEBUG_PIN_HF_CLOCK
    // debug pins
    nrf_gpio_cfg_output(DEBUG_PIN_HF_CLOCK);
    nrf_gpio_cfg_output(DEBUG_PIN_ADDRESS);
    nrf_gpio_cfg_output(DEBUG_PIN_RX);
    nrf_gpio_cfg_output(DEBUG_PIN_TX);
    nrf_gpio_cfg_output(DEBUG_PIN_RADIO_IRQ);
    // toggle DEBUG_PIN_ADDRESS on RADIO ADDRESS event. Use PPI Channel 19 and GPIOT[0]
    // NOTE: unclear how pin could be cleared after set on address.
    nrf_gpiote_task_configure(0, DEBUG_PIN_ADDRESS, GPIOTE_CONFIG_POLARITY_Toggle, NRF_GPIOTE_INITIAL_VALUE_LOW);
    nrf_gpiote_task_enable(0);
    NRF_PPI->CH[19].EEP = (uint32_t)&(NRF_RADIO->EVENTS_ADDRESS);
    NRF_PPI->CH[19].TEP = nrf_gpiote_task_addr_get(0);
    NRF_PPI->CHENSET = PPI_CHEN_CH19_Msk;
#endif
}

// Enable the High Frequency clock on the processor.
static void radio_hf_clock_enable_reset(radio_result_t result){
    UNUSED(result);
}
void radio_hf_clock_enable(bool wait_until_ready){

    // Work around for incomplete RX
    if (radio_state == RADIO_W4_RX_DONE){
#ifdef DEBUG_PIN_HF_CLOCK
        nrf_gpio_pin_clear(DEBUG_PIN_HF_CLOCK);
        nrf_gpio_pin_set(DEBUG_PIN_HF_CLOCK);
#endif
#if 0
        // state = RX, PAYLOAD = 1, END = 0, DISABLED = 0
        printf("Enable: STATE    %u\n", (int) NRF_RADIO->STATE);
        printf("Enable: PAYLOAD  %u\n", (int) NRF_RADIO->EVENTS_PAYLOAD);
        printf("Enable: END      %u\n", (int) NRF_RADIO->EVENTS_END);
        printf("Enable: DISABLED %u\n", (int) NRF_RADIO->EVENTS_DISABLED);
        btstack_assert(false);
#else
        printf("\n\nRADIO_W4_RX_DONE hang\n\n\n");
        radio_stop(&radio_hf_clock_enable_reset);
        radio_state = RADIO_DISABLED;
        return;
#endif
    }

#ifdef DEBUG_PIN_HF_CLOCK
    nrf_gpio_pin_set(DEBUG_PIN_HF_CLOCK);
#endif


    // the RADIO module. Without this clock, no communication is possible.
    NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_HFCLKSTART = 1;
    if (wait_until_ready){
        while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0);
    }

    radio_state = RADIO_DISABLED;
}

void radio_hf_clock_disable(void) {
#ifdef DEBUG_PIN_HF_CLOCK
    nrf_gpio_pin_clear(DEBUG_PIN_HF_CLOCK);
#endif

    NRF_CLOCK->TASKS_HFCLKSTOP = 1;
    radio_state = RADIO_OFF;
}

void radio_set_access_address(uint32_t access_address) {
    NRF_RADIO->BASE0     = ( access_address << 8 ) & 0xFFFFFF00;
    NRF_RADIO->PREFIX0   = ( access_address >> 24 ) & RADIO_PREFIX0_AP0_Msk;
}

void radio_set_crc_init(uint32_t crc_init){
    NRF_RADIO->CRCINIT   = crc_init;
}

void radio_set_channel(uint8_t channel){
    // set frequency based on channel
    NRF_RADIO->FREQUENCY   = channel_table[channel].freq_index;

    // initializes data whitening with channel index
    NRF_RADIO->DATAWHITEIV = channel & 0x3F;
}

void radio_transmit(radio_callback_t callback, radio_transition_t transition, const uint8_t * packet, uint16_t len){

#ifdef DEBUG_PIN_TX
    nrf_gpio_pin_set(DEBUG_PIN_TX);
#endif

    uint16_t state = (uint16_t) NRF_RADIO->STATE;

    switch (radio_state){
        case RADIO_W2_TX:
            // already in transition to tx
            if (state != RADIO_STATE_STATE_TxRu){
                log_info("TX Start after RX, transition %u, state 0x%04x", (int) transition, state);
                btstack_assert(false);
            }
            break;
        case RADIO_DISABLED:
            if (state != RADIO_STATE_STATE_Disabled){
                log_info("TX Start after Disabled, transition %u, state 0x%04x", (int) transition, state);
                btstack_assert(false);
            }
            // start tx
            NRF_RADIO->TASKS_TXEN = 1;
            break;
        default:
            log_info("TX Start unexpected state: our state %u, transition %u, state 0x%04x", radio_state, (int) transition, state);
            btstack_assert(false);
            break;
    }

    radio_callback = callback;

    // set data to send (assume it's valid until tx done)
    NRF_RADIO->PACKETPTR = (uint32_t) packet;

    switch (transition){
        case RADIO_TRANSITION_TX_ONLY:
            radio_state = RADIO_W4_TX_DONE;
            NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk;
            break;
        case RADIO_TRANSITION_TX_TO_RX:
            radio_state = RADIO_W4_TX_TO_RX;
            NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk | RADIO_SHORTS_DISABLED_RXEN_Msk;
            // - Clear Timer0
            NRF_TIMER0->TASKS_CLEAR = 1;
            // - Set CC for receive (ca. 300 us)
            NRF_TIMER0->CC[1] = 300;    // 300 us
            NRF_TIMER0->EVENTS_COMPARE[1] = 0;
            // - END -> Start Timer0
            NRF_PPI->CHENSET = PPI_CHEN_CH0_Msk;
            // - Timer0 CC[1] -> Radio END
            NRF_PPI->CHENSET = PPI_CHEN_CH22_Msk;
            // - Disable address->stop
            NRF_PPI->CHENCLR = PPI_CHEN_CH1_Msk;
            break;
        default:
            btstack_assert(false);
            break;
    }

    NRF_RADIO->INTENCLR = 0xffffffff;
    NRF_RADIO->EVENTS_END = 0;
    NRF_RADIO->EVENTS_DISABLED = 0;

    NVIC_ClearPendingIRQ(RADIO_IRQn);
    NVIC_EnableIRQ(RADIO_IRQn);

    // Interrupt on DISABLED
    NRF_RADIO->INTENSET = 0x00000010;
}

static void radio_setup_rx(void){
    NRF_RADIO->EVENTS_ADDRESS = 0;
    NRF_RADIO->EVENTS_END = 0;
    NRF_RADIO->EVENTS_DISABLED = 0;
    // PPI0: END -> Start Timer0
    NRF_PPI->CHENCLR = PPI_CHEN_CH0_Msk;
    // PPI1: Radio Address -> Stop Timer
    NRF_PPI->CHENSET = PPI_CHEN_CH1_Msk;
    // Update Shortcuts
    NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_ADDRESS_RSSISTART_Msk | RADIO_SHORTS_END_DISABLE_Msk | RADIO_SHORTS_DISABLED_TXEN_Msk;
}

void radio_receive(radio_callback_t callback, uint32_t timeout_us, uint8_t * buffer, uint16_t len, int8_t * rssi){

#ifdef DEBUG_PIN_RX
    nrf_gpio_pin_set(DEBUG_PIN_RX);
#endif

    uint16_t state = (uint16_t) NRF_RADIO->STATE;

    // log_info("RX Start: our state = 0x%0x, radio_state 0x%04x", radio_state, state);

    radio_callback = callback;
    rssi_buffer = rssi;

    NRF_RADIO->PACKETPTR = (uint32_t) buffer;
    buffer[0] = 0;
    buffer[1] = 0;

    switch (radio_state){
        case RADIO_W2_RX:
            // radio setup as part of TX->RX transition
            switch (state){
                case RADIO_STATE_STATE_RxRu:
                case RADIO_STATE_STATE_RxIdle:
                case RADIO_STATE_STATE_Rx:
                    break;
                default:
                    btstack_assert(false);
                    break;
            }
            break;
        case RADIO_DISABLED:
            btstack_assert(state == RADIO_STATE_STATE_Disabled);
            // - Stop Timer0
            NRF_TIMER0->TASKS_STOP = 1;
            // - Clear Timer0
            NRF_TIMER0->TASKS_CLEAR = 1;
            // - Set CC for receive
            NRF_TIMER0->CC[1] = timeout_us;
            NRF_TIMER0->EVENTS_COMPARE[1] = 0;
            // - Timer0 CC[1] -> Radio Disable
            NRF_PPI->CHENSET = PPI_CHEN_CH22_Msk;
            // - Start Timer0
            NRF_TIMER0->TASKS_START = 1;
            // Start Receive
            radio_setup_rx();
            NRF_RADIO->TASKS_RXEN = 1;
            break;
        default:
            log_info("RX unexpected radio_state: state 0x%04x / phy state state 0x%04x", radio_state, state);
            log_info("cc[1] %" PRIu32 "events_compare[1] %u", NRF_TIMER0->CC[1], (int) NRF_TIMER0->EVENTS_COMPARE[1]);
            btstack_assert(false);
            break;
    }

    // Disable all interrupts
    NRF_RADIO->INTENCLR = 0xffffffff;

    NVIC_ClearPendingIRQ(RADIO_IRQn);
    NVIC_EnableIRQ(RADIO_IRQn);

    // Interrupt on DISABLED
    NRF_RADIO->INTENSET = 0x00000010;

    radio_state = RADIO_W4_RX_DONE;
}

void radio_stop(radio_callback_t callback){

    // log_info("Disable, state 0x%04x", (uint16_t) NRF_RADIO->STATE);

    radio_callback = callback;

    NRF_RADIO->SHORTS = 0;

    uint16_t state = (uint16_t) NRF_RADIO->STATE;
    switch (state){
        case RADIO_STATE_STATE_Disabled:
            (*callback)(RADIO_RESULT_OK);
            break;
        default:
            radio_state = RADIO_W4_DISABLED;
            NRF_RADIO->TASKS_DISABLE = 1;
            break;
    }
}

void RADIO_IRQHandler(void){
    uint16_t state = (uint16_t) NRF_RADIO->STATE;

#ifdef DEBUG_PIN_RADIO_IRQ
    nrf_gpio_pin_toggle(DEBUG_PIN_RADIO_IRQ);
#endif
#ifdef DEBUG_PIN_RX
    nrf_gpio_pin_clear(DEBUG_PIN_RX);
#endif
#ifdef DEBUG_PIN_TX
    nrf_gpio_pin_clear(DEBUG_PIN_TX);
#endif

    switch (radio_state){
        case RADIO_W4_TX_DONE:
            // TX Done, no transition to rx requested
            btstack_assert(state == RADIO_STATE_STATE_Disabled);
            NRF_RADIO->EVENTS_DISABLED = 0;
            radio_state = RADIO_DISABLED;
            (*radio_callback)(RADIO_RESULT_OK);
            break;
        case RADIO_W4_TX_TO_RX:
            // TX Done, transition to rx
            btstack_assert(state == RADIO_STATE_STATE_RxRu);
            NRF_RADIO->EVENTS_DISABLED = 0;
            radio_state = RADIO_W2_RX;
            radio_setup_rx();
            (*radio_callback)(RADIO_RESULT_OK);
            break;
        case RADIO_W4_RX_DONE:
            // RX Done
            btstack_assert(state == RADIO_STATE_STATE_TxRu);
            NRF_RADIO->EVENTS_DISABLED = 0;
            NRF_TIMER0->TASKS_STOP = 1;
            // check EVENTS_COMPARE[1]
            if (NRF_TIMER0->EVENTS_COMPARE[1]){
                // compare event -> timeout
                radio_state = RADIO_W4_RX_TIMEOUT;
                NRF_RADIO->SHORTS = 0;
                NRF_RADIO->TASKS_DISABLE = 1;
#ifdef DEBUG_PIN_RX
                // toggle twice for timeout
                nrf_gpio_pin_toggle(DEBUG_PIN_RX);
                nrf_gpio_pin_toggle(DEBUG_PIN_RX);
                nrf_gpio_pin_toggle(DEBUG_PIN_RX);
                nrf_gpio_pin_toggle(DEBUG_PIN_RX);
#endif
            } else {
                // no compare event -> packet with address received
                radio_state = RADIO_W2_TX;
                // RSSI is stored without sign but is negative (range: 0..127)
                if (rssi_buffer != NULL){
                    uint32_t rssi_sample = NRF_RADIO->RSSISAMPLE;
                    int8_t rssi;
                    if (rssi_sample < 128){
                        rssi = -rssi_sample;
                    } else {
                        rssi = -128;
                    }
                    *rssi_buffer = rssi;
                }
                // check CRC
                radio_result_t result = ((NRF_RADIO->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk) != 0) ? RADIO_RESULT_OK : RADIO_RESULT_CRC_ERROR;
#ifdef DEBUG_PIN_RX
                // toggle once for crc error
                if (result == RADIO_RESULT_CRC_ERROR){
                    nrf_gpio_pin_toggle(DEBUG_PIN_RX);
                    nrf_gpio_pin_toggle(DEBUG_PIN_RX);
                }
#endif
                (*radio_callback)(result);
            }
            break;
        case RADIO_W4_RX_TIMEOUT:
            // after RX Timeout, RX was started and stopped again
            btstack_assert(state == RADIO_STATE_STATE_Disabled);
            NRF_RADIO->EVENTS_DISABLED = 0;
            radio_state = RADIO_DISABLED;
            (*radio_callback)(RADIO_RESULT_TIMEOUT);
            break;
        case RADIO_W4_DISABLED:
            NRF_RADIO->EVENTS_DISABLED = 0;
            NRF_RADIO->INTENCLR = 0xffffffff;
            radio_state = RADIO_DISABLED;
            (*radio_callback)(RADIO_RESULT_OK);
            break;
        default:
            log_info("IRQ: our state = 0x%0x, radio_state 0x%04x", radio_state, state);
            btstack_assert(false);
            break;
    }
#ifdef DEBUG_PIN_RADIO_IRQ
    nrf_gpio_pin_toggle(DEBUG_PIN_RADIO_IRQ);
#endif
}
