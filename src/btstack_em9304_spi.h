/*
 * Copyright (C) 2017 BlueKitchen GmbH
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

/**
 * @title EM9304 SPI
 *
 * BTstack's Hardware Abstraction Layer for EM9304 connected via SPI with additional RDY Interrupt line.
 *
 */

#ifndef BTSTACK_EM9304_SPI_H
#define BTSTACK_EM9304_SPI_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

#include <stdint.h>
typedef struct {

    /**
     * @brief Open SPI
     */
    int (*open)(void);

    /**
     * @brief Close SPI
     */
    int (*close)(void);

    /**
     * @brief Check if full duplex operation via transceive is supported
     * @return 1 if supported
     */
    int  (*get_fullduplex_support)(void);

    /**
     * @brief Set callback for RDY
     * @param callback or NULL to disable callback
     */
    void (*set_ready_callback)(void (*callback)(void));

    /**
     * @brief Set callback for transfer complete
     * @param callback
     */
    void (*set_transfer_done_callback)(void (*callback)(void));

    /**
     * @brief Set Chip Selet
     * @param enable
     */
    void (*set_chip_select)(int enable);

    /**
     * @brief Poll READY state
     */
    int (*get_ready)(void);

    /**
     * @brief Transmit and Receive bytes via SPI
     * @param tx_data buffer to transmit
     * @param rx_data buffer to receive into
     * @param len 
     */
    void (*transceive)(const uint8_t * tx_data, uint8_t * rx_data, uint16_t len);

    /**
     * @brief Transmit bytes via SPI
     * @param tx_data buffer to transmit
     * @param len 
     */
    void (*transmit)(const uint8_t * tx_data, uint16_t len);

    /**
     * @brief Receive bytes via SPI
     * @param rx_data buffer to receive into
     * @param len 
     */
    void (*receive)(uint8_t * rx_data, uint16_t len);

} btstack_em9304_spi_t;

/**
 * @brief Get EM9304 SPI instance
 */
const btstack_em9304_spi_t * btstack_em9304_spi_embedded_instance(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif // BTSTACK_EM9304_SPI_H
