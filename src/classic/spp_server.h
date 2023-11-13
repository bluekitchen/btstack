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

/**
 * @title SPP Server
 *
 * Create SPP SDP Records.
 *
 */

#ifndef SPP_SERVER_H
#define SPP_SERVER_H

#include <stdint.h>
 
#if defined __cplusplus
extern "C" {
#endif

/* API_START */

/**
 * @brief Create SDP record for SPP service with official SPP Service Class
 * @param service buffer - needs to large enough
 * @param service_record_handle
 * @param rfcomm_channel
 * @param name or NULL for default value. Provide "" (empty string) to skip attribute
 */
void spp_create_sdp_record(uint8_t *service, uint32_t service_record_handle, int rfcomm_channel, const char *name);

/**
 * @brief Create SDP record for SPP service with custom service UUID (e.g. for use with Android)
 * @param service buffer - needs to large enough
 * @param service_record_handle
 * @param service_uuid128 buffer
 * @param rfcomm_channel
 * @param name
 */
void spp_create_custom_sdp_record(uint8_t *service, uint32_t service_record_handle, const uint8_t * service_uuid128, int rfcomm_channel, const char *name);

/* API_END */

#if defined __cplusplus
}
#endif
#endif // SPP_SERVER_H
