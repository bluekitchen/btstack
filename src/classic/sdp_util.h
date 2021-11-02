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
 * @title SDP Utils
 *
 */

#ifndef SDP_UTIL_H
#define SDP_UTIL_H

#include "btstack_bool.h"

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif


typedef enum {
    SDP_Invalid = 0,
    SDP_ErrorResponse = 1,
    SDP_ServiceSearchRequest,
    SDP_ServiceSearchResponse,
    SDP_ServiceAttributeRequest,
    SDP_ServiceAttributeResponse,
    SDP_ServiceSearchAttributeRequest,
    SDP_ServiceSearchAttributeResponse
} sdp_pdu_id_t;


/* API_START */
    
// OFFSETS FOR LOCALIZED ATTRIBUTES - BLUETOOTH_ATTRIBUTE_LANGUAGE_BASE_ATTRIBUTE_ID_LIST
#define SDP_Offset_ServiceName        0x0000
#define SDP_Offset_ServiceDescription 0x0001
#define SDP_Offset_ProviderName       0x0002

typedef enum {
    DE_NIL = 0,
    DE_UINT,
    DE_INT,
    DE_UUID,
    DE_STRING,
    DE_BOOL,
    DE_DES,
    DE_DEA,
    DE_URL
} de_type_t;

typedef enum {
    DE_SIZE_8 = 0,
    DE_SIZE_16,
    DE_SIZE_32,
    DE_SIZE_64,
    DE_SIZE_128,
    DE_SIZE_VAR_8,
    DE_SIZE_VAR_16,
    DE_SIZE_VAR_32
} de_size_t;

// MARK: DateElement
void      de_dump_data_element(const uint8_t * record);
int       de_get_len(const uint8_t * header);


// @note returned "string" is not NULL terminated
const uint8_t * de_get_string(const uint8_t * element);

de_size_t de_get_size_type(const uint8_t * header);
de_type_t de_get_element_type(const uint8_t * header);
uint32_t  de_get_header_size(const uint8_t * header);
int       de_element_get_uint16(const uint8_t * element, uint16_t * value);
uint32_t  de_get_data_size(const uint8_t * header);
uint32_t  de_get_uuid32(const uint8_t * element);
int       de_get_normalized_uuid(uint8_t *uuid128, const uint8_t *element);
void      de_create_sequence(uint8_t * header);
void      de_store_descriptor_with_len(uint8_t * header, de_type_t type, de_size_t size, uint32_t len);
uint8_t * de_push_sequence(uint8_t *header);
void      de_pop_sequence(uint8_t * parent, uint8_t * child);
void      de_add_number(uint8_t *seq, de_type_t type, de_size_t size, uint32_t value);
void      de_add_data( uint8_t *seq, de_type_t type, uint16_t size, uint8_t *data);

void      de_add_uuid128(uint8_t * seq, uint8_t * uuid);

// returns data element  len if date element is smaller than size
uint32_t de_get_len_safe(const uint8_t * header, uint32_t size);

// MARK: DES iterator
typedef struct {
    uint8_t * element;
    uint16_t pos;
    uint16_t length;
} des_iterator_t;

bool des_iterator_init(des_iterator_t * it, uint8_t * element);
bool des_iterator_has_more(des_iterator_t * it);
de_type_t des_iterator_get_type (des_iterator_t * it);
uint16_t des_iterator_get_size (des_iterator_t * it);
uint8_t * des_iterator_get_element(des_iterator_t * it);
void des_iterator_next(des_iterator_t * it);

// MARK: SDP
uint16_t  sdp_append_attributes_in_attributeIDList(uint8_t *record, uint8_t *attributeIDList, uint16_t startOffset, uint16_t maxBytes, uint8_t *buffer);
uint8_t * sdp_get_attribute_value_for_attribute_id(uint8_t * record, uint16_t attributeID);
uint8_t   sdp_set_attribute_value_for_attribute_id(uint8_t * record, uint16_t attributeID, uint32_t value);
int       sdp_record_matches_service_search_pattern(uint8_t *record, uint8_t *serviceSearchPattern);
int       spd_get_filtered_size(uint8_t *record, uint8_t *attributeIDList);
int       sdp_filter_attributes_in_attributeIDList(uint8_t *record, uint8_t *attributeIDList, uint16_t startOffset, uint16_t maxBytes, uint16_t *usedBytes, uint8_t *buffer);  
int       sdp_attribute_list_constains_id(uint8_t *attributeIDList, uint16_t attributeID);
int       sdp_traversal_match_pattern(uint8_t * element, de_type_t attributeType, de_size_t size, void *my_context);

/*
 * @brief Returns service search pattern for given UUID-16
 * @note Uses fixed buffer
 */
uint8_t* sdp_service_search_pattern_for_uuid16(uint16_t uuid16);

/*
 * @brief Returns service search pattern for given UUID-128
 * @note Uses fixed buffer
 */
uint8_t* sdp_service_search_pattern_for_uuid128(const uint8_t * uuid128);

/* API_END */

#if defined __cplusplus
}
#endif

#endif // SDP_UTIL_H
