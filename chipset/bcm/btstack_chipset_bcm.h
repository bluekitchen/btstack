/*
 * Copyright (C) 2009-2012 by Matthias Ringwald
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
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
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
 * Please inquire about commercial licensing options at contact@bluekitchen-gmbh.com
 *
 */

/*
 *  btstack_control_bcm.h
 *
 *  Adapter to use Broadcom-based chipsets with BTstack
 */
 
#ifndef BTSTACK_CHIPSET_BCM_H
#define BTSTACK_CHIPSET_BCM_H

#if defined __cplusplus
extern "C" {
#endif

#include <btstack_lc3.h>
#include <stdint.h>
#include "btstack_chipset.h"

const btstack_chipset_t * btstack_chipset_bcm_instance(void);

// Support for loading .hci init files from Flash
// actual data provided by separate patchram.c
extern const uint8_t brcm_patchram_buf[];
extern const int     brcm_patch_ram_length;
extern const char    brcm_patch_version[];


// Support for loading .hcd init files on POSIX systems

/** 
 * @brief Set path to .hcd init file
 * @param path
 */
void btstack_chipset_bcm_set_hcd_file_path(const char * path);

/**
 * @brief Set folder to look for .hcd init files
 * @param path
 */
void btstack_chipset_bcm_set_hcd_folder_path(const char * path);

/**
 * @brief Look for .hcd init file based on device name
 * @param device_name e.g. BCM43430A1
 */
void btstack_chipset_bcm_set_device_name(const char * path);

/**
 * @brief Enable init file - needed by btstack_chipset_bcm_download_firmware when using h5
 * @param enabled
 */
void btstack_chipset_bcm_enable_init_script(int enabled);

/**
 * @Brief Identify Broadcom/Cypress/Infineon chipset by lmp_subversion
 * If identified, device name is returned and can be used for HCI_OPCODE_HCI_READ_LOCAL_VERSION_INFORMATION
 *
 * @note Required for newer Controllers that require AIROC Download Mode for PatchRAM Upload
 *
 * @param lmp_subversion
 * @returns device name if identified, otherwise NULL
 */
const char * btstack_chipset_bcm_identify_controller(uint16_t lmp_subversion);

/**
 * @brief Setup Codec Config for LC3 Offloading
 * Asserts if size is smaller than 11
 * @param buffer
 * @param size of buffer
 * @param sampling_frequency_hz
 * @param frame_duration
 * @param octets_per_frame
 * @return size of config
 */
uint8_t btstack_chipset_bcm_create_lc3_offloading_config(
    uint8_t * buffer,
    uint8_t size,
    uint16_t sampling_frequency_hz,
    btstack_lc3_frame_duration_t frame_duration,
    uint16_t octets_per_frame);

#if defined __cplusplus
}
#endif

#endif // BTSTACK_CHIPSET_BCM_H
