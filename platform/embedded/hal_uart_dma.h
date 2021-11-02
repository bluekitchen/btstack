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

/*
 *  hal_uart_dma.h
 *
 *  Hardware abstraction layer that provides
 *  - block wise IRQ-driven read/write
 *  - baud control
 *  - wake-up on CTS pulse (BTSTACK_UART_SLEEP_RTS_HIGH_WAKE_ON_CTS_PULSE)
 *
 * If HAVE_HAL_UART_DMA_SLEEP_MODES is defined, different sleeps modes can be provided and used
 *
 */

#ifndef HAL_UART_DMA_H
#define HAL_UART_DMA_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

/**
 * @brief Init and open device
 */
void hal_uart_dma_init(void);

/**
 * @brief Set callback for block received - can be called from ISR context
 * @param callback
 */
void hal_uart_dma_set_block_received( void (*callback)(void));

/**
 * @brief Set callback for block sent - can be called from ISR context
 * @param callback
 */
void hal_uart_dma_set_block_sent( void (*callback)(void));

/**
 * @brief Set baud rate
 * @note During baud change, TX line should stay high and no data should be received on RX accidentally
 * @param baudrate
 */
int  hal_uart_dma_set_baud(uint32_t baud);

#ifdef HAVE_UART_DMA_SET_FLOWCONTROL
/**
 * @brief Set flowcontrol
 * @param flowcontrol enabled
 */
int  hal_uart_dma_set_flowcontrol(int flowcontrol);
#endif

/**
 * @brief Send block. When done, callback set by hal_uart_set_block_sent must be called
 * @param buffer
 * @param lengh
 */
void hal_uart_dma_send_block(const uint8_t *buffer, uint16_t length);

/**
 * @brief Receive block. When done, callback set by hal_uart_dma_set_block_received must be called
 * @param buffer
 * @param lengh
 */
void hal_uart_dma_receive_block(uint8_t *buffer, uint16_t len);

/**
 * @brief Set or clear callback for CSR pulse - can be called from ISR context
 * @param csr_irq_handler or NULL to disable IRQ handler
 */
void hal_uart_dma_set_csr_irq_handler( void (*csr_irq_handler)(void));

/**
 * @brief Set sleep mode
 * @param block_received callback
 */
void hal_uart_dma_set_sleep(uint8_t sleep);

#ifdef HAVE_HAL_UART_DMA_SLEEP_MODES

/**
 * @brief Set callback for block received - can be called from ISR context
 * @returns list of supported sleep modes
 */
int  hal_uart_dma_get_supported_sleep_modes(void);

/**
 * @brief Set sleep mode
 * @param sleep_mode
 */
void hal_uart_dma_set_sleep_mode(btstack_uart_sleep_mode_t sleep_mode);

#endif

#if defined __cplusplus
}
#endif
#endif // HAL_UART_DMA_H
