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
 
// *****************************************************************************
//
// test rfcomm query tests
//
// *****************************************************************************


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "classic/avdtp_util.h"
#include "btstack.h"
#include "classic/avdtp.h"
#include "classic/avdtp_util.h"


// mock start
extern "C" uint16_t l2cap_get_remote_mtu_for_local_cid(uint16_t local_cid){
    return 1024;
}
extern "C" void avdtp_emit_sink_and_source(uint8_t * packet, uint16_t size){}
extern "C" void avdtp_emit_source(uint8_t * packet, uint16_t size){}
extern "C" void l2cap_request_can_send_now_event(uint16_t local_cid){}
extern "C" btstack_packet_handler_t avdtp_packet_handler_for_stream_endpoint(const avdtp_stream_endpoint_t * stream_endpoint){
    UNUSED(stream_endpoint);
    return NULL;
}
// mock end

static uint8_t sbc_codec_capabilities[] = {
    0xFF, // (AVDTP_SBC_44100 << 4) | AVDTP_SBC_STEREO,
    0xFF, //(AVDTP_SBC_BLOCK_LENGTH_16 << 4) | (AVDTP_SBC_SUBBANDS_8 << 2) | AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS,
    2, 53
};

static uint8_t packed_capabilities[] = {
    0x01, 0x00,
    0x07, 0x06, 0x00, 0x00, 0xff, 0xff, 0x02, 0x35,
    0x08, 0x00 
};

static uint16_t configuration_bitmap = (1 << AVDTP_MEDIA_TRANSPORT) | (1 << AVDTP_MEDIA_CODEC) | (1 << AVDTP_DELAY_REPORTING);

TEST_GROUP(AvdtpUtil){
    avdtp_connection_t connection;
    avdtp_capabilities_t caps;
    avdtp_signaling_packet_t signaling_packet;

    void setup(){
        caps.recovery.recovery_type = 0x01;                  // 0x01 = RFC2733
        caps.recovery.maximum_recovery_window_size = 0x01;   // 0x01 to 0x18, for a Transport Packet
        caps.recovery.maximum_number_media_packets = 0x01;

        caps.content_protection.cp_type = 0x1111;
        caps.content_protection.cp_type_value_len = AVDTP_MAX_CONTENT_PROTECTION_TYPE_VALUE_LEN;

        int i;
        for (i=0; i<AVDTP_MAX_CONTENT_PROTECTION_TYPE_VALUE_LEN; i++){
            caps.content_protection.cp_type_value[i] = i;
        }
        
        caps.header_compression.back_ch = 0x01;  
        caps.header_compression.media = 0x01;    
        caps.header_compression.recovery = 0x01;
       
        caps.multiplexing_mode.fragmentation = 0x11; 
        caps.multiplexing_mode.transport_identifiers_num = 3;
        
        for (i=0; i<3; i++){
            caps.multiplexing_mode.transport_session_identifiers[i] = i;   
            caps.multiplexing_mode.tcid[i] = (i | 0x20);  
        }

        caps.media_codec.media_type = AVDTP_AUDIO;
        caps.media_codec.media_codec_type = AVDTP_CODEC_SBC;
        caps.media_codec.media_codec_information_len = 4;
        caps.media_codec.media_codec_information = sbc_codec_capabilities;
    }
};


TEST(AvdtpUtil, avdtp_pack_service_capabilities_test){
    uint8_t packet[200];
    {
        // 0 - packet length
        uint8_t packed_capabilities[] = {0x00};
        memset(packet, 0, sizeof(packet));
        uint16_t packet_length = avdtp_pack_service_capabilities(packet, sizeof(packet), caps, AVDTP_SERVICE_CATEGORY_INVALID_0);
        CHECK_EQUAL(sizeof(packed_capabilities), packet_length);
        MEMCMP_EQUAL(packed_capabilities, packet, packet_length);
    }
    
    {
        uint8_t packed_capabilities[] = {0x00};
        memset(packet, 0, sizeof(packet));
        uint16_t packet_length = avdtp_pack_service_capabilities(packet, sizeof(packet), caps, AVDTP_MEDIA_TRANSPORT);
        CHECK_EQUAL(sizeof(packed_capabilities), packet_length);
        MEMCMP_EQUAL(packed_capabilities, packet, packet_length);
    }

    {
        uint8_t packed_capabilities[] = {0x00};
        memset(packet, 0, sizeof(packet));
        uint16_t packet_length = avdtp_pack_service_capabilities(packet, sizeof(packet), caps, AVDTP_MEDIA_TRANSPORT);
        CHECK_EQUAL(sizeof(packed_capabilities), packet_length);
        MEMCMP_EQUAL(packed_capabilities, packet, packet_length);
    }

    {
        uint8_t packed_capabilities[] = {0x00};
        memset(packet, 0, sizeof(packet));
        uint16_t packet_length = avdtp_pack_service_capabilities(packet, sizeof(packet), caps, AVDTP_REPORTING);
        CHECK_EQUAL(sizeof(packed_capabilities), packet_length);
        MEMCMP_EQUAL(packed_capabilities, packet, packet_length);
    }
    
    {
        uint8_t packed_capabilities[] = {0x03, 0x01, 0x01, 0x01};
        memset(packet, 0, sizeof(packet));
        uint16_t packet_length = avdtp_pack_service_capabilities(packet, sizeof(packet), caps, AVDTP_RECOVERY);
        CHECK_EQUAL(sizeof(packed_capabilities), packet_length);
        MEMCMP_EQUAL(packed_capabilities, packet, packet_length);
    }
    
    {
        uint8_t packed_capabilities[] = {0x0D, 0x0A + 2, 0x11, 0x11, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};
        memset(packet, 0, sizeof(packet));
        uint16_t packet_length = avdtp_pack_service_capabilities(packet, sizeof(packet), caps, AVDTP_CONTENT_PROTECTION);
        CHECK_EQUAL(sizeof(packed_capabilities), packet_length);
        MEMCMP_EQUAL(packed_capabilities, packet, packet_length);
    }

    {
        uint8_t packed_capabilities[] = {0x01, 0xE0};
        memset(packet, 0, sizeof(packet));
        uint16_t packet_length = avdtp_pack_service_capabilities(packet, sizeof(packet), caps, AVDTP_HEADER_COMPRESSION);
        CHECK_EQUAL(sizeof(packed_capabilities), packet_length);
        MEMCMP_EQUAL(packed_capabilities, packet, packet_length);
    }

    {
        uint8_t packed_capabilities[] = {0x07, (uint8_t)(0x11 << 7), (uint8_t)(0x00 << 7), (uint8_t)(0x20 << 7), (uint8_t)(0x01 << 7), (uint8_t)(0x21 << 7), (uint8_t)(0x02 << 7), (uint8_t)(0x22 << 7)};
        memset(packet, 0, sizeof(packet));
        uint16_t packet_length = avdtp_pack_service_capabilities(packet, sizeof(packet), caps, AVDTP_MULTIPLEXING);
        CHECK_EQUAL(sizeof(packed_capabilities), packet_length);
        MEMCMP_EQUAL(packed_capabilities, packet, packet_length);
    }

    {
        uint8_t packed_capabilities[] = {0x06, 0x00, 0x00, 0xff, 0xff, 0x02, 0x35};
        memset(packet, 0, sizeof(packet));
        uint16_t packet_length = avdtp_pack_service_capabilities(packet, sizeof(packet), caps, AVDTP_MEDIA_CODEC);
        CHECK_EQUAL(sizeof(packed_capabilities), packet_length);
        MEMCMP_EQUAL(packed_capabilities, packet, packet_length);
    }
    
    {
        uint8_t packed_capabilities[] = {0x00};
        memset(packet, 0, sizeof(packet));
        uint16_t packet_length = avdtp_pack_service_capabilities(packet, sizeof(packet), caps, AVDTP_DELAY_REPORTING);
        CHECK_EQUAL(sizeof(packed_capabilities), packet_length);
        MEMCMP_EQUAL(packed_capabilities, packet, packet_length);
    }
}

TEST(AvdtpUtil, avdtp_prepare_capabilities){
    avdtp_prepare_capabilities(&signaling_packet, 0x01, configuration_bitmap, caps, AVDTP_SI_GET_ALL_CAPABILITIES);
    MEMCMP_EQUAL(packed_capabilities, signaling_packet.command, sizeof(packed_capabilities));
}


TEST(AvdtpUtil, avdtp_unpack_service_capabilities_test){
    avdtp_capabilities_t capabilities;

    CHECK_EQUAL(configuration_bitmap, avdtp_unpack_service_capabilities(&connection, AVDTP_SI_GET_CONFIGURATION, &capabilities, &packed_capabilities[0], sizeof(packed_capabilities)));
    // media codec:
    CHECK_EQUAL(AVDTP_CODEC_SBC, capabilities.media_codec.media_codec_type);
    CHECK_EQUAL(4,  capabilities.media_codec.media_codec_information_len);
    MEMCMP_EQUAL(sbc_codec_capabilities, capabilities.media_codec.media_codec_information, 4);

    
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
