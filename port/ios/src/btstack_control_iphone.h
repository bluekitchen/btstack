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
 *  btstack_control_iphone.h
 *
 *  BT Control API implementation for the iPhone and the iPod Touch
 *
 *  Created by Matthias Ringwald on 5/19/09.
 */
#ifndef __BTSTACK_CONTROL_IPHONE_H
#define __BTSTACK_CONTROL_IPHONE_H
 
#include "btstack_control.h"

#if defined __cplusplus
extern "C" {
#endif

extern btstack_control_t btstack_control_iphone;

// power management
int      btstack_control_iphone_power_management_enabled(void);
int      btstack_control_iphone_power_management_supported(void);

// system bluetooth on/off
int      btstack_control_iphone_bt_enabled(void);
void     btstack_control_iphone_bt_set_enabled(int enabled);

// does device has Bluetooth support
int      btstack_control_iphone_is_valid(void);

// get default transport speed
uint32_t btstack_control_iphone_get_transport_speed(void);

#if defined __cplusplus
}
#endif

#endif // __BTSTACK_CONTROL_IPHONE_H
