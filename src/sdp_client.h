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
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

/*
 *  sdp_client.h
 */

#ifndef __SDP_CLIENT_H
#define __SDP_CLIENT_H

#include "btstack-config.h"

#include <btstack/utils.h>

#if defined __cplusplus
extern "C" {
#endif

/* API_START */
 
/** 
 * @brief Queries the SDP service of the remote device given a service search pattern and a list of attribute IDs. The remote data is handled by the SDP parser. The SDP parser delivers attribute values and done event via a registered callback.
 */
void sdp_client_query(bd_addr_t remote, uint8_t * des_serviceSearchPattern, uint8_t * des_attributeIDList);

#ifdef HAVE_SDP_EXTRA_QUERIES
void sdp_client_service_attribute_search(bd_addr_t remote, uint32_t search_serviceRecordHandle, uint8_t * des_attributeIDList);
void sdp_client_service_search(bd_addr_t remote, uint8_t * des_serviceSearchPattern);
#endif
/* API_END */

#if defined __cplusplus
}
#endif

#endif // __SDP_CLIENT_H
