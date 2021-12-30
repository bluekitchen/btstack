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
 *  btstack_chipset_atwilc3000.c
 *
 *  Adapter to use atwilc3000-based chipsets with BTstack
 *  
 */

#ifndef BTSTACK_CHIPSET_ATWILC3000_H
#define BTSTACK_CHIPSET_ATWILC3000_H

#if defined __cplusplus
extern "C" {
#endif

#include "btstack_chipset.h"
#include "btstack_uart.h"
#include "btstack_uart_block.h"

#define HCI_DEFAULT_BAUDRATE 115200

/**
 * @brief get chipset instance
 */
const btstack_chipset_t * btstack_chipset_atwilc3000_instance(void);

/**
 * @brief Download firmware via btstack_uart_t implementation
 * @param uart_driver -- already initialized
 * @param baudrate for firmware update
 * @param flowcontrol after firmwware update
 * @param done callback. 0 = Success
 */
void btstack_chipset_atwilc3000_download_firmware_with_uart(const btstack_uart_t * uart_driver, uint32_t baudrate, int flowcontrol, const uint8_t * fw, uint32_t fw_size, void (*done)(int result));

/**
 * @brief Download firmware via btstack_uart_block_t implementation
 * @param uart_driver -- already initialized
 * @param baudrate for firmware update
 * @param flowcontrol after firmwware update
 * @param done callback. 0 = Success
 * @deprecated please use btstack_chipset_atwilc3000_download_firmware_with_uart instead
 */
void btstack_chipset_atwilc3000_download_firmware(const btstack_uart_block_t * uart_driver, uint32_t baudrate, int flowcontrol, const uint8_t * fw, uint32_t fw_size, void (*done)(int result));

#if defined __cplusplus
}
#endif

#endif // BTSTACK_CHIPSET_ATWILC3000_H
