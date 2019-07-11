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

#include <stdint.h>
#include "btstack_chipset.h"

const btstack_chipset_t * btstack_chipset_bcm_instance(void);


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

#if defined __cplusplus
}
#endif

#endif // BTSTACK_CHIPSET_BCM_H
