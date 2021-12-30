/*
 * Copyright (C) 2021 BlueKitchen GmbH
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

/**
 * @title SCO Transport
 *
 * Hardware abstraction for PCM/I2S Inteface used for HSP/HFP profiles.
 *
 */

#ifndef BTSTACK_SCO_TRANSPORT_H
#define BTSTACK_SCO_TRANSPORT_H

#include "btstack_config.h"

#include "bluetooth.h"
#include "btstack_defines.h"

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

typedef enum {
    SCO_FORMAT_8_BIT,
    SCO_FORMAT_16_BIT,
} sco_format_t;

/* API_START */

typedef struct {

    /**
     * register packet handler for SCO HCI packets
     */
    void (*register_packet_handler)(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size));

    /**
     * open transport
     * @param con_handle of SCO connection
     * @param sco_format
     */
    void (*open)(hci_con_handle_t con_handle, sco_format_t sco_format);

    /**
     * send SCO packet
     */
    void (*send_packet)(const uint8_t *buffer, uint16_t length);

    /**
     * close transport
     * @param con_handle of SCO connection
     */
    void (*close)(hci_con_handle_t con_handle);

} btstack_sco_transport_t;

/* API_END */

#if defined __cplusplus
}
#endif

#endif