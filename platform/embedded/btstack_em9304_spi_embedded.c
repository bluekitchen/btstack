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

#define BTSTACK_FILE__ "btstack_em9304_spi_embedded.c"

/*
 * btstack_em9304_spi_embedded.c
 * Integrate hal_em9304_spi.h with BTstack Run Loop
 */

#include "btstack_debug.h"
#include "btstack_run_loop.h"
#include "btstack_em9304_spi.h"
#include "hal_em9304_spi.h"
#include <stddef.h> // NULL

// data source for integration with BTstack Runloop
static btstack_data_source_t btstack_em9304_spi_embedded_data_source;

static void (*btstack_em9304_spi_embedded_ready_callback)(void);
static void (*btstack_em9304_spi_embedded_transfer_done_callback)(void);

static int btstack_em9304_spi_embedded_notify_ready;
static int btstack_em9304_spi_embedded_notify_transfer_done;

static void btstack_em9304_spi_embedded_ready(void){
    btstack_em9304_spi_embedded_notify_ready = 1;
    btstack_run_loop_poll_data_sources_from_irq();
}

static void btstack_em9304_spi_transfer_done(void){
    btstack_em9304_spi_embedded_notify_transfer_done = 1;
    btstack_run_loop_poll_data_sources_from_irq();
}

static void btstack_em9304_spi_embedded_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type) {
    UNUSED(ds);
    switch (callback_type){
        case DATA_SOURCE_CALLBACK_POLL:
            if (btstack_em9304_spi_embedded_notify_ready){
                btstack_em9304_spi_embedded_notify_ready = 0;
                if (btstack_em9304_spi_embedded_ready_callback){
                    (*btstack_em9304_spi_embedded_ready_callback)();
                }
            }
            if (btstack_em9304_spi_embedded_notify_transfer_done){
                btstack_em9304_spi_embedded_notify_transfer_done = 0;
                if (btstack_em9304_spi_embedded_transfer_done_callback){
                    (*btstack_em9304_spi_embedded_transfer_done_callback)();
                }
            }
            break;
        default:
            break;
    }
}

/**
 * @brief Initialize SPI
 */
static int btstack_em9304_spi_embedded_open(void){
    // set up polling data_source
    btstack_run_loop_set_data_source_handler(&btstack_em9304_spi_embedded_data_source, &btstack_em9304_spi_embedded_process);
    btstack_run_loop_enable_data_source_callbacks(&btstack_em9304_spi_embedded_data_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_add_data_source(&btstack_em9304_spi_embedded_data_source);

    // reset ready callback handler
    btstack_em9304_spi_embedded_ready_callback = NULL;

    // setup lower layer
    hal_em9304_spi_init();

    // setup callbacks with lower layer
    hal_em9304_spi_set_ready_callback(&btstack_em9304_spi_embedded_ready);
    hal_em9304_spi_set_transfer_done_callback(&btstack_em9304_spi_transfer_done);
    return 0;
}

/**
 * @brief Deinitialize SPI
 */
static int btstack_em9304_spi_embedded_close(void){
    // remove data source
    btstack_run_loop_disable_data_source_callbacks(&btstack_em9304_spi_embedded_data_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_remove_data_source(&btstack_em9304_spi_embedded_data_source);

    // stop lower layer
    hal_em9304_spi_deinit();
    return 0;
}

/**
 * @brief Set callback for RDY - can be called from ISR context
 * @param callback
 */
static void btstack_em9304_spi_embedded_set_ready_callback(void (*callback)(void)){
    if (callback){
        hal_em9304_spi_enable_ready_interrupt();
    } else {
        hal_em9304_spi_disable_ready_interrupt();
    }
    btstack_em9304_spi_embedded_ready_callback = callback;
}

/**
 * @brief Set callback for transfer complete - can be called from ISR context
 * @param callback
 */
static void btstack_em9304_spi_embedded_set_transfer_done_callback(void (*callback)(void)){
    btstack_em9304_spi_embedded_transfer_done_callback = callback;
}

/**
 * @brief Poll READY state
 */
static int btstack_em9304_spi_embedded_get_ready(){
    return hal_em9304_spi_get_ready();
}

/**
 * @brief Set Chip Selet
 * @param enable
 */
static void btstack_em9304_spi_embedded_set_chip_select(int enable){
    hal_em9304_spi_set_chip_select(enable);
}

/**
 * @brief Check if full duplex operation via btstack_em9304_spi_transceive is supported
 */
static int  btstack_em9304_spi_embedded_get_fullduplex_support(void){
    return hal_em9304_spi_get_fullduplex_support();
}

/**
 * @brief Transmit and Receive bytes via SPI
 * @param tx_data buffer to transmit
 * @param rx_data buffer to receive into
 * @param len 
 */
static void btstack_em9304_spi_embedded_transceive(const uint8_t * tx_data, uint8_t * rx_data, uint16_t len){
    hal_em9304_spi_transceive(tx_data, rx_data, len);
}

/**
 * @brief Transmit bytes via SPI
 * @param tx_data buffer to transmit
 * @param len 
 */
static void btstack_em9304_spi_embedded_transmit(const uint8_t * tx_data, uint16_t len){
    hal_em9304_spi_transmit(tx_data, len);
}

/**
 * @brief Receive bytes via SPI
 * @param rx_data buffer to receive into
 * @param len 
 */
static void btstack_em9304_spi_embedded_receive(uint8_t * rx_data, uint16_t len){
    hal_em9304_spi_receive(rx_data, len);
}

static const btstack_em9304_spi_t btstack_em9304_spi_embedded = {
    /* void (open)(void); */                        &btstack_em9304_spi_embedded_open,
    /* void (close)(void); */                       &btstack_em9304_spi_embedded_close,
    /* int  (get_fullduplex_support)(void); */      &btstack_em9304_spi_embedded_get_fullduplex_support,
    /* void (set_ready_callback)(..); */            &btstack_em9304_spi_embedded_set_ready_callback,
    /* void (set_transfer_done_callback)(..); */    &btstack_em9304_spi_embedded_set_transfer_done_callback,
    /* void (set_chip_select)(int enable); */       &btstack_em9304_spi_embedded_set_chip_select,
    /* int  (get_ready)(); */                       &btstack_em9304_spi_embedded_get_ready,
    /* void (transceive)(..); */                    &btstack_em9304_spi_embedded_transceive,
    /* void (transmit)(..); */                      &btstack_em9304_spi_embedded_transmit,
    /* void (receive)(..);     */                   &btstack_em9304_spi_embedded_receive,
};

const btstack_em9304_spi_t * btstack_em9304_spi_embedded_instance(void){
	return &btstack_em9304_spi_embedded;
}
