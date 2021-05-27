/*
 * Copyright (C) 2017 BlueKitchen GmbH
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
 */

/**
 * @title Tag-Value-Length Persistent Storage (TLV)
 *
 * Inteface for BTstack's Tag Value Length Persistent Storage implementations
 * used to store pairing/bonding data.
 *
 */

#ifndef BTSTACK_TLV_H
#define BTSTACK_TLV_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

typedef struct {

	/**
	 * Get Value for Tag
	 * @param context
	 * @param tag
	 * @param buffer
	 * @param buffer_size
	 * @returns size of value
	 */
	int (*get_tag)(void * context, uint32_t tag, uint8_t * buffer, uint32_t buffer_size);

	/**
	 * Store Tag 
	 * @param context
	 * @param tag
	 * @param data
	 * @param data_size
	 * @returns 0 on success
	 */
	int (*store_tag)(void * context, uint32_t tag, const uint8_t * data, uint32_t data_size);

	/**
	 * Delete Tag
     *  @note it is not expected that delete operation fails, please use at least log_error in case of errors
	 * @param context
	 * @param tag
	 */
	void (*delete_tag)(void * context,  uint32_t tag);

} btstack_tlv_t;

/** 
 * @brief Make TLV implementation available to BTstack components via Singleton
 * @note Usually called by port after BD_ADDR was retrieved from Bluetooth Controller
 * @param tlv_impl
 * @param tlv_context
 */
void btstack_tlv_set_instance(const btstack_tlv_t * tlv_impl, void * tlv_context);

/**
 * @brief Get current TLV implementation. Used for bonding information, but can be used by application, too.
 * @param tlv_impl
 * @param tlv_context
 */
void btstack_tlv_get_instance(const btstack_tlv_t ** tlv_impl, void ** tlv_context);

/* API_END */

#if defined __cplusplus
}
#endif
#endif // BTSTACK_TLV_H
