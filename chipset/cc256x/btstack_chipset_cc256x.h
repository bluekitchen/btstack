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
 *  btstack_chipset_cc256x.c
 *
 *  Adapter to use cc256x-based chipsets with BTstack
 */
 
#ifndef BTSTACK_CHIPSET_CC256X_H
#define BTSTACK_CHIPSET_CC256X_H

#if defined __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "btstack_chipset.h"

/** 
 * Configure output power before HCI POWER_ON
 */
void btstack_chipset_cc256x_set_power(int16_t power_in_dB);

/**
 * Configure output power for specific modulation before HCI POWER_ON using explicit power vector
 * @see https://processors.wiki.ti.com/index.php/CC256x_VS_HCI_Commands#HCI_VS_DRPb_Set_Power_Vector_.280xFD82.29
 * @note data is not copied, pointer has to stay valid
 * @note overrides setting from btstack_chipset_cc256x_set_power
 * @param modulation_type 0=GFSK, 1=EDR2, 2= EDR3
 * @param power_vector array of 16 uint8_t values as explained on TI wiki
 */
void btstack_chipset_cc256x_set_power_vector(uint8_t modulation_type, const uint8_t * power_vector);

/**
 * Get chipset instance for CC256x series
 */
const btstack_chipset_t * btstack_chipset_cc256x_instance(void);

/**
 * Get LMP Subversion of compile-time init script
 */
uint16_t btstack_chipset_cc256x_lmp_subversion(void);

/**
 * Set init script to use for next power up
 *
 * Note: Only needed when switching between different add-ons (BLE, AVRP, ANT+)
 * Note: Not supported by MSP430 "init script at 0x10000" workaround
 *
 * @param init_script_size or 0 for default script
 */
void btstack_chipset_cc256x_set_init_script(uint8_t * data, uint32_t size);

#if defined __cplusplus
}
#endif

#endif // BTSTACK_CHIPSET_CC256X_H
