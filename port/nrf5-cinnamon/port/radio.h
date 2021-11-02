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

/*
 *  radio.h
 */

#ifndef RADIO_H
#define RADIO_H

#include "btstack_bool.h"
#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

typedef struct {
    void (*tx_done)(void);
    void (*rx_done)(void);
} radio_callbacks_t;

typedef enum {
    RADIO_TRANSITION_TX_ONLY,
    RADIO_TRANSITION_TX_TO_RX,
} radio_transition_t;

typedef enum {
    RADIO_RESULT_OK,
    RADIO_RESULT_CRC_ERROR,
    RADIO_RESULT_TIMEOUT,
} radio_result_t;

typedef void (*radio_callback_t)(radio_result_t result);

/**
 * Init radio
 */
void radio_init(void);

/**
 * Set Access Address
 * @param access_address
 */
void radio_set_access_address(uint32_t access_address);

/**
 * Enable RF CLock
 * @param wait_until_ready if true, waits until HF clock is ready
 */
void radio_hf_clock_enable(bool wait_until_ready);

/**
 * Disable RF CLock
 */
void radio_hf_clock_disable(void);

/**
 * Set CRC Init value
 * @param crc 24-bit init value
 */
void radio_set_crc_init(uint32_t crc);

/**
 * Set Channel: frequency and whitening
 * @param channel 0..39
 */
void radio_set_channel(uint8_t channel);

/**
 * Transmit packet.
 * @param callback
 * @param transition - on RADIO_TRANSITION_TX_TO_RX, radio transitions to RX
 * @param packet
 * @param len
 */
void radio_transmit(radio_callback_t callback, radio_transition_t transition, const uint8_t * packet, uint16_t len);

/**
 * Receive packet
 * @note automatic transition to TX
 * @param callback
 * @param timeout_us if radio was disabled before (i.e. not in tx -> rx transition)
 * @param buffer
 * @param len
 * @param rssi (out)
 */
void radio_receive(radio_callback_t callback, uint32_t timeout_us, uint8_t * buffer, uint16_t len, int8_t * rssi);

/**
 * Stop active transmission, e.g. tx after rx
 * @param callback
 */
void radio_stop(radio_callback_t callback);

/* API_END */

#if defined __cplusplus
}
#endif

#endif // LL_H
