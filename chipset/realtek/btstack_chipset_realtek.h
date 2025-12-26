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
 *  btstack_chipset_realtek
 *
 *  Adapter to use Realtek-based chipsets with BTstack
 */

#ifndef BTSTACK_CHIPSET_REALTEK_H
#define BTSTACK_CHIPSET_REALTEK_H

#if defined __cplusplus
extern "C" {
#endif

#include "bluetooth.h"
#include "btstack_chipset.h"
#include "btstack_lc3.h"
#include <stdint.h>

/**
 * @brief Set path to firmware file
 * @param path
 */
void btstack_chipset_realtek_set_firmware_file_path(const char *path);

/**
 * @brief Set path to firmware folder
 * @param path
 */
void btstack_chipset_realtek_set_firmware_folder_path(const char *path);

/**
 * @brief Set path to config file
 * @param path
 */
void btstack_chipset_realtek_set_config_file_path(const char *path);

/**
 * @brief Set path to config folder
 * @param path
 */
void btstack_chipset_realtek_set_config_folder_path(const char *path);

/**
 * @brief Set USB Product id
 * @param id
 */
void btstack_chipset_realtek_set_product_id(uint16_t id);

/**
 * @brief Get num USB Controllers
 * @return num controllers
 */
uint16_t btstack_chipset_realtek_get_num_usb_controllers(void);

/**
 * @brief Get Vendor/Product ID for Controller with index
 * @param index
 * @param out_vendor_id
 * @param out_product_id
 */
void btstack_chipset_realtek_get_vendor_product_id(uint16_t index, uint16_t * out_vendor_id, uint16_t * out_product_id);

/**
 * @brief Set LMP Subversion for UART Controller
 */
void btstack_chipset_realtek_set_local_info(uint8_t hci_version, uint16_t hci_revision, uint16_t lmp_subversion);

 /**
  * @brief Get Baudrate from Config File
  */
uint32_t btstack_chipset_realtek_get_config_baudrate(void);

/**
 * Get chipset instance for REALTEK chipsets
 */
const btstack_chipset_t *btstack_chipset_realtek_instance(void);

#ifdef ENABLE_LE_AUDIO_CODEC_OFFLOAD
/**
 * @brief Setup Codec Config for LC3 Offloading
 * Asserts if size is smaller than 17
 * @param buffer
 * @param size of buffer
 * @param sampling_frequency_hz
 * @param frame_duration
 * @param channel_allocation,
 * @param octets_per_frame
 * @return size of config
 */
uint8_t btstack_chipset_realtek_create_lc3_offloading_config(
    uint8_t * buffer,
    uint8_t size,
    uint16_t sampling_frequency_hz,
    btstack_lc3_frame_duration_t frame_duration,
    uint32_t channel_allocation,
    uint16_t octets_per_frame);
#endif

#if defined __cplusplus
}
#endif

#endif  // BTSTACK_CHIPSET_REALTEK_H
