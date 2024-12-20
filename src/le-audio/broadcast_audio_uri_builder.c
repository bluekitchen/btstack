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

void broadcast_audio_uri_builder_init(broadcast_audio_uri_builder_t * builder, uint8_t * buffer, uint16_t size){
    builder->buffer = buffer;
    builder->size = size;
    builder->len = 0;
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
    UNUSED(broadcast_name);
    // TODO: base64
    return broadcast_audio_uri_builder_append_string(builder, "BN:QnJvYWRjYXN0IE5hbWU=;");
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
    uint16_t len = builder->len;
    bool ok = broadcast_audio_uri_builder_append_string(builder,"AD:");
    if (ok){
        char buffer[13];
        broadcast_audio_uri_builder_string_hexdump((uint8_t *)buffer, (const uint8_t *) advertiser_address, 6);
        buffer[12] = ';';
        broadcast_audio_uri_builder_append_bytes(builder, (const uint8_t *) buffer, sizeof(buffer));
    } else {
        builder->len = len;
    }
    return ok;
}

bool broadcast_audio_uri_builder_append_broadcast_id(broadcast_audio_uri_builder_t * builder, uint32_t broadcast_id){
    uint16_t len = builder->len;
    bool ok = broadcast_audio_uri_builder_append_string(builder,"BI:");
    if (ok){
        uint8_t buffer[7];
        uint8_t big_endian_id[3];
        big_endian_store_24(big_endian_id, 0, broadcast_id);
        broadcast_audio_uri_builder_string_hexdump(buffer, big_endian_id, 3);
        buffer[6] = ';';
        broadcast_audio_uri_builder_append_bytes(builder, (const uint8_t *) buffer, sizeof(buffer));
    } else {
        builder->len = len;
    }
    return ok;

}

bool broadcast_audio_uri_builder_append_broadcast_code(broadcast_audio_uri_builder_t * builder, const uint8_t * broadcast_code){
    UNUSED(broadcast_code);
    uint16_t len = builder->len;
    bool ok = broadcast_audio_uri_builder_append_string(builder,"BC:");
    if (ok){
        uint8_t buffer[25];
        // TODO: base64
        memcpy(buffer, "MDEyMzQ1Njc4OWFiY2RlZg==", 24);
        buffer[24] = ';';
        broadcast_audio_uri_builder_append_bytes(builder, (const uint8_t *) buffer, sizeof(buffer));
    } else {
        builder->len = len;
    }
    return ok;
}

bool broadcast_audio_uri_builder_append_vendor_specific(broadcast_audio_uri_builder_t * builder, uint16_t vendor_id, const uint8_t * data, uint16_t data_len){
    UNUSED(data);
    UNUSED(data_len);
    uint16_t len = builder->len;
    bool ok = broadcast_audio_uri_builder_append_string(builder, "VS:");
    if (ok){
        uint8_t vendor_id_big_endian_id[2];
        big_endian_store_16(vendor_id_big_endian_id, 0, vendor_id);
        uint8_t vendor_id_hex[4];
        memset(vendor_id_hex, 0, sizeof(vendor_id_hex));
        broadcast_audio_uri_builder_string_hexdump(vendor_id_hex, vendor_id_hex, 2);
        // TODO: base64(vendor_id_hex + data)
        ok = broadcast_audio_uri_builder_append_string(builder, ";");
    }
    if (ok == false){
        builder->len = len;
    }
    return ok;
}

bool broadcast_audio_uri_builder_append_advertising_sid(broadcast_audio_uri_builder_t * builder, uint8_t advertising_sid){
    char buffer[10];
    btstack_snprintf_assert_complete(buffer, sizeof(buffer), "AS:%02X;", advertising_sid);
    return broadcast_audio_uri_builder_append_string(builder, buffer);
}

bool broadcast_audio_uri_builder_append_pa_interval(broadcast_audio_uri_builder_t * builder, uint16_t pa_interval){
    char buffer[10];
    btstack_snprintf_assert_complete(buffer, sizeof(buffer), "PI:%04" PRIX32 ";", pa_interval);
    return broadcast_audio_uri_builder_append_string(builder, buffer);
}

bool broadcast_audio_uri_builder_append_num_subgroups(broadcast_audio_uri_builder_t * builder, uint8_t num_subgroups){
    char buffer[10];
    btstack_snprintf_assert_complete(buffer, sizeof(buffer), "NS:%02X", num_subgroups);
    return broadcast_audio_uri_builder_append_string(builder, buffer);
}

bool broadcast_audio_uri_builder_append_bis_sync(broadcast_audio_uri_builder_t * builder, uint32_t bis_sync){
    char buffer[10];
    btstack_snprintf_assert_complete(buffer, sizeof(buffer), "BS:%04" PRIX32 ";", bis_sync);
    return broadcast_audio_uri_builder_append_string(builder, buffer);
}

bool broadcast_audio_uri_builder_append_sg_number_of_bises(broadcast_audio_uri_builder_t * builder, uint32_t sg_number_of_bises){
    char buffer[10];
    btstack_snprintf_assert_complete(buffer, sizeof(buffer), "NB:%04" PRIX32 ";", sg_number_of_bises);
    return broadcast_audio_uri_builder_append_string(builder, buffer);
}

bool broadcast_audio_uri_builder_append_sg_metadata(broadcast_audio_uri_builder_t * builder, const uint8_t * metadata, uint16_t metadata_len){
    uint16_t len = builder->len;
    bool ok = broadcast_audio_uri_builder_append_string(builder, "SM:");
    if (ok && (metadata_len > 0)) {
        // TODO: base64(data)
        ok = broadcast_audio_uri_builder_append_bytes(builder, metadata, metadata_len);
    }
    if (ok) {
        ok = broadcast_audio_uri_builder_append_string(builder, ";");
    } else {
        builder->len = len;
    }
    return ok;
}

bool broadcast_audio_uri_builder_append_public_broadcast_announcement_metadata(broadcast_audio_uri_builder_t * builder, const uint8_t * metadata, uint16_t metadata_len){
    uint16_t len = builder->len;
    bool ok = broadcast_audio_uri_builder_append_string(builder, "PM:");
    if (ok && metadata_len > 0) {
        // TODO: base64(data)
        ok = broadcast_audio_uri_builder_append_bytes(builder, metadata, metadata_len);
    }
    if (ok) {
        ok = broadcast_audio_uri_builder_append_string(builder, ";");
    } else {
        builder->len = len;
    }
    return ok;
}
