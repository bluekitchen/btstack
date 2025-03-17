/*
 * Copyright (C) 2024 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "broadcast_audio_uri_builder.c"

#include "le-audio/broadcast_audio_uri_builder.h"
#include "btstack_base64_encoder.h"
#include "btstack_util.h"

#include <stdint.h>
#include <string.h>
#include <inttypes.h>

static void broadcast_audio_uri_builder_string_hexdump(uint8_t * buffer, const uint8_t * data, uint16_t size){
    uint8_t i;
    for (i = 0; i < size ; i++) {
        uint8_t byte = data[i];
        buffer[2*i+0] = char_for_nibble(byte >> 4);
        buffer[2*i+1] = char_for_nibble(byte & 0x0f);
    }
}

static inline bool broadcast_audio_uri_builder_have_space(const broadcast_audio_uri_builder_t * builder, uint16_t len){
    return builder->len + len <= builder->size;
}

static inline char * broadcast_audio_uri_builder_get_ptr( const broadcast_audio_uri_builder_t * builder ) {
   return &builder->buffer[builder->len];
}

bool broadcast_audio_uri_builder_init(broadcast_audio_uri_builder_t * builder, char * buffer, uint16_t size){
    builder->buffer = buffer;
    builder->size = size;
    builder->len = 0;
    return broadcast_audio_uri_builder_append_string(builder, "BLUETOOTH:UUID:184F;");
}

bool broadcast_audio_uri_builder_finalize(broadcast_audio_uri_builder_t * builder){
    bool ok = broadcast_audio_uri_builder_append_string(builder, ";");
    uint16_t remaining;
    void *ptr;
    if(ok) {
        remaining = broadcast_audio_uri_builder_get_remaining_space(builder);
        ptr = broadcast_audio_uri_builder_get_ptr(builder);
        ok = (remaining > 0);
    }
    if(ok) {
        memset(ptr, 0, remaining);
    }
    return ok;
}

uint16_t broadcast_audio_uri_builder_get_remaining_space(const broadcast_audio_uri_builder_t * builder){
    return builder->size - builder->len;
}

uint16_t broadcast_audio_uri_builder_get_size(const broadcast_audio_uri_builder_t * builder){
    return builder->len;
}

bool broadcast_audio_uri_builder_append_bytes(broadcast_audio_uri_builder_t * builder, const uint8_t * data, uint16_t len){
    bool ok = broadcast_audio_uri_builder_have_space(builder, len);
    if (ok){
        memcpy(&builder->buffer[builder->len], data, len);
        builder->len += len;
    }
    return ok;
}

bool broadcast_audio_uri_builder_append_string(broadcast_audio_uri_builder_t * builder, const char * text){
    uint16_t len = (uint16_t) strlen(text);
    return broadcast_audio_uri_builder_append_bytes(builder, (const uint8_t *) text, len);
}

bool broadcast_audio_uri_builder_append_broadcast_name(broadcast_audio_uri_builder_t * builder, const char * broadcast_name){
    uint16_t len = builder->len;
    bool ok = broadcast_audio_uri_builder_append_string(builder,"BN:");
    if( !ok ) {
        return ok;
    }
    char *ptr = broadcast_audio_uri_builder_get_ptr(builder);
    size_t remaining = broadcast_audio_uri_builder_get_remaining_space( builder );
    ok = btstack_base64_encoder_process_block(broadcast_name, strlen(broadcast_name), ptr, &remaining);
    if( ok ) {
        builder->len += remaining;
        ok = broadcast_audio_uri_builder_append_string( builder, ";" );
    }
    if( !ok ) {
        builder->len = len;
    }
    return ok;
}

bool broadcast_audio_uri_builder_append_advertiser_address_type(broadcast_audio_uri_builder_t * builder, bd_addr_type_t advertiser_address_type){
    char buffer[10];
    btstack_snprintf_assert_complete(buffer, sizeof(buffer), "AT:%u;",advertiser_address_type == BD_ADDR_TYPE_LE_RANDOM ? 1 : 0);
    return broadcast_audio_uri_builder_append_string(builder, buffer);
}

bool broadcast_audio_uri_builder_append_standard_quality(broadcast_audio_uri_builder_t * builder, bool standard_quality){
    char buffer[10];
    btstack_snprintf_assert_complete(buffer, sizeof(buffer), "SQ:%u;", (int) standard_quality);
    return broadcast_audio_uri_builder_append_string(builder, buffer);
}

bool broadcast_audio_uri_builder_append_high_quality(broadcast_audio_uri_builder_t * builder, bool high_quality){
    char buffer[10];
    btstack_snprintf_assert_complete(buffer, sizeof(buffer), "HQ:%u;", (int) high_quality);
    return broadcast_audio_uri_builder_append_string(builder, buffer);
}

bool broadcast_audio_uri_builder_append_advertiser_address(broadcast_audio_uri_builder_t * builder, bd_addr_t advertiser_address){
    char buffer[13];
    uint16_t len = builder->len;
    bool ok = broadcast_audio_uri_builder_append_string(builder,"AD:");
    broadcast_audio_uri_builder_string_hexdump((uint8_t *)buffer, (const uint8_t *) advertiser_address, 6);
    buffer[12] = ';';
    if(ok){
        ok = broadcast_audio_uri_builder_append_bytes(builder, (const uint8_t *) buffer, sizeof(buffer));
    }
    if(!ok){
        builder->len = len;
    }
    return ok;
}

bool broadcast_audio_uri_builder_append_broadcast_id(broadcast_audio_uri_builder_t * builder, uint32_t broadcast_id){
    uint8_t buffer[7];
    uint16_t len = builder->len;
    bool ok = broadcast_audio_uri_builder_append_string(builder,"BI:");

    uint8_t big_endian_id[3];
    big_endian_store_24(big_endian_id, 0, broadcast_id);
    broadcast_audio_uri_builder_string_hexdump(buffer, big_endian_id, 3);
    buffer[6] = ';';

    if(ok){
        ok = broadcast_audio_uri_builder_append_bytes(builder, (const uint8_t *) buffer, sizeof(buffer));
    }
    if(!ok) {
        builder->len = len;
    }
    return ok;
}

bool broadcast_audio_uri_builder_append_broadcast_code_as_bytes(broadcast_audio_uri_builder_t * builder, const uint8_t * broadcast_code){
    uint16_t len = builder->len;
    bool ok = broadcast_audio_uri_builder_append_string(builder,"BC:");
    if (!ok){
        return ok;
    }
    char *ptr = broadcast_audio_uri_builder_get_ptr(builder);
    size_t remaining = broadcast_audio_uri_builder_get_remaining_space(builder);
    ok = btstack_base64_encoder_process_block(broadcast_code, 16, ptr, &remaining);
    if(ok) {
        builder->len += remaining;
        ok = broadcast_audio_uri_builder_append_string( builder, ";" );
    }
    if(!ok){
        builder->len = len;
    }
    return ok;
}

bool broadcast_audio_uri_builder_append_broadcast_code_as_string(broadcast_audio_uri_builder_t * builder, const char *broadcast_code){
    uint16_t len = builder->len;
    bool ok = broadcast_audio_uri_builder_append_string(builder,"BC:");
    if (!ok){
        return ok;
    }
    char *ptr = broadcast_audio_uri_builder_get_ptr(builder);
    size_t remaining = broadcast_audio_uri_builder_get_remaining_space(builder);
    size_t block_size = btstack_min(strlen(broadcast_code), 16);
    ok = btstack_base64_encoder_process_block(broadcast_code, block_size, ptr, &remaining);
    if(ok) {
        builder->len += remaining;
        ok = broadcast_audio_uri_builder_append_string( builder, ";" );
    }
    if(!ok){
        builder->len = len;
    }
    return ok;
}

bool broadcast_audio_uri_builder_append_vendor_specific(broadcast_audio_uri_builder_t * builder, uint16_t vendor_id, const uint8_t * data, uint16_t data_len){
    UNUSED(data);
    UNUSED(data_len);
    uint16_t len = builder->len;
    bool ok = broadcast_audio_uri_builder_append_string(builder, "VS:");
    size_t remaining, olen;
    btstack_base64_state_t state;
    if(ok) {
        char buffer[9] = "ID:0000;";
        uint8_t vendor_id_big_endian_id[2];
        big_endian_store_16(vendor_id_big_endian_id, 0, vendor_id);
        broadcast_audio_uri_builder_string_hexdump((uint8_t*)&buffer[3], vendor_id_big_endian_id, 2);
        char *ptr = broadcast_audio_uri_builder_get_ptr(builder);
        remaining = broadcast_audio_uri_builder_get_remaining_space(builder);
        olen = remaining;
        btstack_base64_encoder_stream_init( &state );
        ssize_t ret = btstack_base64_encoder_stream( &state, buffer, strlen(buffer), ptr, &olen );
        ok = ((ret>0) && !(olen>remaining));
    }
    if(ok) {
        builder->len += olen;
        remaining -= olen;
        olen = remaining;
        char *ptr = broadcast_audio_uri_builder_get_ptr(builder);
        ssize_t ret = btstack_base64_encoder_stream( &state, data, data_len, ptr, &olen );
        ok = ((ret>0) && !(olen>remaining));
    }
    if(ok) {
        builder->len += olen;
        remaining -= olen;
        olen = remaining;
        char *ptr = broadcast_audio_uri_builder_get_ptr(builder);
        ssize_t ret = btstack_base64_encoder_stream_final(&state, ptr, &olen);
        ok = ((ret>=0) && (olen<remaining));
    }
    if (ok){
        builder->len += olen;
        ok = broadcast_audio_uri_builder_append_string(builder, ";");
    }
    if (!ok){
        builder->len = len;
    }
    return ok;
}

bool broadcast_audio_uri_builder_append_advertising_sid(broadcast_audio_uri_builder_t * builder, uint8_t advertising_sid){
    char buffer[10];
    btstack_snprintf_assert_complete(buffer, sizeof(buffer), "AS:%X;", advertising_sid);
    return broadcast_audio_uri_builder_append_string(builder, buffer);
}

bool broadcast_audio_uri_builder_append_pa_interval(broadcast_audio_uri_builder_t * builder, uint16_t pa_interval){
    char buffer[10];
    btstack_snprintf_assert_complete(buffer, sizeof(buffer), "PI:%" PRIX16 ";", pa_interval);
    return broadcast_audio_uri_builder_append_string(builder, buffer);
}

bool broadcast_audio_uri_builder_append_num_subgroups(broadcast_audio_uri_builder_t * builder, uint8_t num_subgroups){
    char buffer[10];
    btstack_snprintf_assert_complete(buffer, sizeof(buffer), "NS:%X;", num_subgroups);
    return broadcast_audio_uri_builder_append_string(builder, buffer);
}

bool broadcast_audio_uri_builder_append_bis_sync(broadcast_audio_uri_builder_t * builder, uint32_t bis_sync){
    char buffer[10];
    btstack_snprintf_assert_complete(buffer, sizeof(buffer), "BS:%" PRIX32 ";", bis_sync);
    return broadcast_audio_uri_builder_append_string(builder, buffer);
}

bool broadcast_audio_uri_builder_append_sg_number_of_bises(broadcast_audio_uri_builder_t * builder, uint32_t sg_number_of_bises){
    char buffer[10];
    btstack_snprintf_assert_complete(buffer, sizeof(buffer), "NB:%" PRIX32 ";", sg_number_of_bises);
    return broadcast_audio_uri_builder_append_string(builder, buffer);
}

bool broadcast_audio_uri_builder_sg_metadata_begin(broadcast_audio_uri_builder_t * builder){
    uint16_t len = builder->len;
    bool ok = broadcast_audio_uri_builder_append_string(builder, "SM:");
    if (ok) {
        btstack_base64_encoder_stream_init(&builder->state);
    }
    if(!ok){
        builder->len = len;
    }
    return ok;
}

bool broadcast_audio_uri_builder_sg_metadata_append_ltv(broadcast_audio_uri_builder_t * builder, uint8_t metadata_len, const uint8_t type, const uint8_t * metadata){
    if(metadata_len == 0) {
        return true;
    }
    uint16_t len = builder->len;
    char *ptr = broadcast_audio_uri_builder_get_ptr(builder);
    size_t remaining = broadcast_audio_uri_builder_get_remaining_space(builder);
    size_t olen = remaining;
    uint8_t l = metadata_len + 1; // account for type octet
    ssize_t ret = btstack_base64_encoder_stream(&builder->state, &l, 1, ptr, &olen);
    bool ok = (ret>0) && !(olen>remaining);
    if (ok) {
        builder->len += olen;
        ptr = broadcast_audio_uri_builder_get_ptr(builder);
        remaining = broadcast_audio_uri_builder_get_remaining_space(builder);
        olen = remaining;
        ret = btstack_base64_encoder_stream(&builder->state, &type, 1, ptr, &olen);
        ok = (ret>0) && !(olen>remaining);
    }
    if (ok) {
        builder->len += olen;
        ptr = broadcast_audio_uri_builder_get_ptr(builder);
        remaining = broadcast_audio_uri_builder_get_remaining_space(builder);
        olen = remaining;
        ret = btstack_base64_encoder_stream(&builder->state, metadata, metadata_len, ptr, &olen);
        ok = (ret>0) && !(olen>remaining);
    }
    if(ok) {
        builder->len += olen;
    } else {
        builder->len = len;
    }
    return ok;
}

bool broadcast_audio_uri_builder_sg_metadata_end(broadcast_audio_uri_builder_t * builder){
    uint16_t len = builder->len;

    char *ptr = broadcast_audio_uri_builder_get_ptr(builder);
    size_t remaining = broadcast_audio_uri_builder_get_remaining_space(builder);
    size_t olen = remaining;
    ssize_t ret = btstack_base64_encoder_stream_final(&builder->state, ptr, &olen);
    bool ok = (ret>=0) && !(olen>remaining);

    if (ok) {
        builder->len += olen;
        ok = broadcast_audio_uri_builder_append_string(builder, ";");
    }
    if(!ok){
        builder->len = len;
    }
    return ok;
}

bool broadcast_audio_uri_builder_append_public_broadcast_announcement_metadata(broadcast_audio_uri_builder_t * builder, const uint8_t * metadata, uint16_t metadata_len){
    if(metadata_len == 0) {
        return true;
    }
    uint16_t len = builder->len;
    bool ok = broadcast_audio_uri_builder_append_string(builder, "PM:");
    if (ok) {
        char *ptr = broadcast_audio_uri_builder_get_ptr(builder);
        size_t remaining = broadcast_audio_uri_builder_get_remaining_space(builder);
        ok = btstack_base64_encoder_process_block(metadata, metadata_len, ptr, &remaining);
    }
    if (ok) {
        ok = broadcast_audio_uri_builder_append_string(builder, ";");
    }
    if(!ok){
        builder->len = len;
    }
    return ok;
}
