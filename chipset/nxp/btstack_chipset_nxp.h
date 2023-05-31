/*
 * Copyright (C) 2023 BlueKitchen GmbH
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
 *  btstack_chipset_nxp
 *
 *  Adapter to use NXP/Marvel-based chipsets with BTstack
 */

#ifndef BTSTACK_CHIPSET_MARVEL_H
#define BTSTACK_CHIPSET_MARVEL_H

#if defined __cplusplus
extern "C" {
#endif

#include "bluetooth.h"
#include "btstack_chipset.h"
#include "btstack_uart.h"
#include <stdint.h>

/**
 * @brief Set path to firmware file for Controller with v1 bootloader, e.g. 88W8997
 * @note used for systems with HAVE_POSIX_FILE_IO
 * @param path
 */
void btstack_chipset_nxp_set_v1_firmware_path(const char * firmware_path);

/**
 * @brieft Provide firmware as data
 * @note used for systems without HAVE_POSIX_FILE_IO
 * @param fw_data
 * @param fw_size
 */
void btstack_chipset_nxp_set_firmware(const uint8_t * fw_data, uint32_t fw_size);

/**
* @brief Download firmware via uart_driver
* @param uart_driver -- already initialized
* @param callback. 0 = Success
*/
void btstack_chipset_nxp_download_firmware_with_uart(const btstack_uart_t *uart_driver, void (*callback)(uint8_t status));

/**
 * @brief Get baudrate after firmware download
 * @note iw612 with current firmware is set to 3mbps instead of 115200. to be called after firmware download complete
 */
uint32_t btstack_chipset_nxp_get_initial_baudrate(void);

/**
 * Get chipset instance for NXP chipsets
 */
const btstack_chipset_t *btstack_chipset_nxp_instance(void);

#if defined __cplusplus
}
#endif

#endif  // BTSTACK_CHIPSET_NXP_H
