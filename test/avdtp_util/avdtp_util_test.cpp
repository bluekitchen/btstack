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


#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "classic/avdtp_util.h"
#include "classic/avdtp_initiator.h"
#include "btstack_event.h"

// mock start
static uint8_t emitted_events[16][64];
static uint16_t emitted_event_sizes[16];
static uint16_t emitted_events_count;

extern "C" uint16_t l2cap_get_remote_mtu_for_local_cid(uint16_t local_cid){
    UNUSED(local_cid);
    return 1024;
}
extern "C" void avdtp_emit_sink_and_source(uint8_t * packet, uint16_t size){
    if (emitted_events_count >= 16) return;
    uint16_t copy_size = size;
    if (copy_size > sizeof(emitted_events[0])){
        copy_size = sizeof(emitted_events[0]);
    }
    memcpy(emitted_events[emitted_events_count], packet, copy_size);
    emitted_event_sizes[emitted_events_count] = copy_size;
    emitted_events_count++;
}
extern "C" void avdtp_emit_source(uint8_t * packet, uint16_t size){
    UNUSED(packet);
    UNUSED(size);
}
extern "C" uint8_t l2cap_request_can_send_now_event(uint16_t local_cid){
    UNUSED(local_cid);
    return ERROR_CODE_SUCCESS;
}
extern "C" btstack_packet_handler_t avdtp_packet_handler_for_stream_endpoint(const avdtp_stream_endpoint_t * stream_endpoint){
    UNUSED(stream_endpoint);
    return NULL;
}
extern "C" uint8_t l2cap_send(uint16_t local_cid, uint8_t *data, uint16_t len){
    UNUSED(local_cid);
    UNUSED(data);
    UNUSED(len);
    return ERROR_CODE_SUCCESS;
}
extern "C" uint8_t l2cap_create_channel(btstack_packet_handler_t packet_handler, bd_addr_t address, uint16_t psm, uint16_t mtu, uint16_t * out_local_cid){
    UNUSED(packet_handler);
    UNUSED(address);
    UNUSED(psm);
    UNUSED(mtu);
    UNUSED(out_local_cid);
    return ERROR_CODE_SUCCESS;
}
extern "C" uint8_t l2cap_disconnect(uint16_t local_cid){
    UNUSED(local_cid);
    return ERROR_CODE_SUCCESS;
}
extern "C" void l2cap_reserve_packet_buffer(void){
}
extern "C" uint8_t * l2cap_get_outgoing_buffer(void){
    static uint8_t buffer[128];
    return buffer;
}
extern "C" uint8_t l2cap_send_prepared(uint16_t local_cid, uint16_t len){
    UNUSED(local_cid);
    UNUSED(len);
    return ERROR_CODE_SUCCESS;
}
extern "C" avdtp_stream_endpoint_t * avdtp_get_stream_endpoint_for_seid(uint16_t seid){
    UNUSED(seid);
    return NULL;
}
extern "C" uint8_t avdtp_get_next_transaction_label(void){
    return 1;
}
extern "C" void avdtp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(packet);
    UNUSED(size);
}
// mock end

static uint8_t sbc_codec_capabilities[] = {
    0xFF, // (AVDTP_SBC_44100 << 4) | AVDTP_SBC_STEREO,
    0xFF, //(AVDTP_SBC_BLOCK_LENGTH_16 << 4) | (AVDTP_SBC_SUBBANDS_8 << 2) | AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS,
    2, 53
};

static uint16_t configuration_bitmap = (1 << AVDTP_MEDIA_TRANSPORT) | (1 << AVDTP_MEDIA_CODEC) | (1 << AVDTP_DELAY_REPORTING);

typedef struct {
    bool saw_media_transport;
    bool saw_reporting;
    bool saw_sbc_codec;
    bool saw_recovery;
    bool saw_header_compression;
    bool saw_multiplexing;
    bool saw_delay_reporting;
    bool saw_content_protection;
    bool saw_done;
} cabability_event_tracking_t;


TEST_GROUP(AvdtpUtil){
    avdtp_connection_t connection;
    avdtp_capabilities_t caps;
    avdtp_signaling_packet_t signaling_packet;
    avdtp_connection_t reassembly_connection;
    cabability_event_tracking_t event_info;
    uint16_t filtered_count;
    uint16_t filtered_indices[16];

    void setup(){
        emitted_events_count = 0;
        memset(emitted_events, 0, sizeof(emitted_events));
        memset(emitted_event_sizes, 0, sizeof(emitted_event_sizes));

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

    void reassembly_connection_init(const avdtp_signaling_packet_t *prepared_capabilities_packet, const avdtp_signal_identifier_t &identifier, uint16_t chunk_size) {
        memset(&reassembly_connection, 0, sizeof(reassembly_connection));
        reassembly_connection.avdtp_cid = 0x1234;
        reassembly_connection.initiator_remote_seid = 0x07;
        reassembly_connection.initiator_signaling_packet.message_type = AVDTP_RESPONSE_ACCEPT_MSG;
        reassembly_connection.initiator_signaling_packet.signal_identifier = identifier;
        reassembly_connection.initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_ANSWER;

        int remaining_chunk = (prepared_capabilities_packet->size % chunk_size) > 0 ? 1 : 0;
        reassembly_connection.initiator_signaling_packet.num_packets = (prepared_capabilities_packet->size / chunk_size) + remaining_chunk;
    }

    void send_valid_capabilities_packets(avdtp_signaling_packet_t * prepared_capabilities_packet, avdtp_signal_identifier_t identifier, uint16_t chunk_size){
        if (chunk_size == 0){
            return;
        }
        reassembly_connection_init(prepared_capabilities_packet, identifier, chunk_size);

        // send single packet
        if (reassembly_connection.initiator_signaling_packet.num_packets == 1){
            reassembly_connection.initiator_signaling_packet.packet_type = AVDTP_SINGLE_PACKET;
            reassembly_connection.initiator_signaling_packet.num_packets = 0;
            avdtp_initiator_stream_config_subsm(&reassembly_connection, &prepared_capabilities_packet->command[0], prepared_capabilities_packet->size, 0);
            return;
        }

        // send fragmented
        uint16_t num_packets = reassembly_connection.initiator_signaling_packet.num_packets;
        int offset = 0;
        for (int i = 0; i < num_packets; i++){
            uint16_t bytes_left = prepared_capabilities_packet->size - offset;
            uint16_t current_chunk_size = btstack_min(chunk_size, bytes_left);

            if (offset == 0){
                reassembly_connection.initiator_signaling_packet.packet_type = AVDTP_START_PACKET;
            } else if (bytes_left == current_chunk_size){
                reassembly_connection.initiator_signaling_packet.num_packets--;
                reassembly_connection.initiator_signaling_packet.packet_type = AVDTP_END_PACKET;
            } else {
                reassembly_connection.initiator_signaling_packet.num_packets--;
                reassembly_connection.initiator_signaling_packet.packet_type = AVDTP_CONTINUE_PACKET;
            }
            avdtp_initiator_stream_config_subsm(&reassembly_connection, &prepared_capabilities_packet->command[offset], current_chunk_size, 0);
            offset += current_chunk_size;
        }
    }

    void validate_prepared_capapbilities_use_sbc(avdtp_signaling_packet_t * prepared_capabilities_packet){
        bool prepared_uses_sbc = false;
        for (uint16_t i = 0; i + 3u < prepared_capabilities_packet->size; ){
            uint8_t category = prepared_capabilities_packet->command[i];
            uint8_t cap_len = prepared_capabilities_packet->command[i + 1u];
            if (category == AVDTP_MEDIA_CODEC){
                prepared_uses_sbc = (prepared_capabilities_packet->command[i + 3u] == AVDTP_CODEC_SBC);
                break;
            }
            i = i + 2u + cap_len;
        }
        CHECK_TRUE(prepared_uses_sbc);
    }

    void track_events(uint16_t categories_bitmap) {
        filtered_count = 0;
        memset(filtered_indices, 0, sizeof(filtered_indices));

        event_info.saw_media_transport = false;
        event_info.saw_reporting = false;
        event_info.saw_sbc_codec = false;
        event_info.saw_recovery = false;
        event_info.saw_header_compression = false;
        event_info.saw_multiplexing = false;
        event_info.saw_delay_reporting = false;
        event_info.saw_content_protection = false;
        event_info.saw_done = false;

        for (uint16_t i = 0; i < emitted_events_count; i++) {
            uint8_t subevent_code = emitted_events[i][2];
            if (subevent_code == AVDTP_SUBEVENT_SIGNALING_ACCEPT) {
                continue;
            }
            filtered_indices[filtered_count++] = i;
            switch (subevent_code) {
                case AVDTP_SUBEVENT_SIGNALING_MEDIA_TRANSPORT_CAPABILITY:
                    event_info.saw_media_transport = true;
                    break;
                case AVDTP_SUBEVENT_SIGNALING_REPORTING_CAPABILITY:
                    event_info.saw_reporting = true;
                    break;
                case AVDTP_SUBEVENT_SIGNALING_HEADER_COMPRESSION_CAPABILITY:
                    event_info.saw_header_compression = true;
                    break;
                case AVDTP_SUBEVENT_SIGNALING_MULTIPLEXING_CAPABILITY:
                    event_info.saw_multiplexing = true;
                    break;
                case AVDTP_SUBEVENT_SIGNALING_DELAY_REPORTING_CAPABILITY:
                    event_info.saw_delay_reporting = true;
                    break;
                case AVDTP_SUBEVENT_SIGNALING_RECOVERY_CAPABILITY:
                    event_info.saw_recovery = true;
                    break;
                case AVDTP_SUBEVENT_SIGNALING_CONTENT_PROTECTION_CAPABILITY:
                    event_info.saw_content_protection = true;
                    break;
                case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CAPABILITY:
                    event_info.saw_sbc_codec = true;
                    CHECK_EQUAL(0x1234, avdtp_subevent_signaling_media_codec_sbc_capability_get_avdtp_cid(emitted_events[i]));
                    CHECK_EQUAL(0x07, avdtp_subevent_signaling_media_codec_sbc_capability_get_remote_seid(emitted_events[i]));
                    CHECK_EQUAL(AVDTP_AUDIO, avdtp_subevent_signaling_media_codec_sbc_capability_get_media_type(emitted_events[i]));
                    break;
                case AVDTP_SUBEVENT_SIGNALING_CAPABILITIES_DONE:
                    event_info.saw_done = true;
                    CHECK_EQUAL(0x1234, avdtp_subevent_signaling_capabilities_done_get_avdtp_cid(emitted_events[i]));
                    CHECK_EQUAL(0x07, avdtp_subevent_signaling_capabilities_done_get_remote_seid(emitted_events[i]));
                    break;
                default:
                    break;
            }
        }
    }

    void validate_events(uint16_t categories_bitmap){
        track_events(categories_bitmap);

        if ((categories_bitmap & (1 << AVDTP_MEDIA_TRANSPORT)) > 0){
            CHECK_TRUE(event_info.saw_media_transport);
        }
        if ((categories_bitmap & (1 << AVDTP_REPORTING)) > 0){
            CHECK_TRUE(event_info.saw_reporting);
        }
        if ((categories_bitmap & (1 << AVDTP_RECOVERY)) > 0){
            CHECK_TRUE(event_info.saw_recovery);
        }
        if ((categories_bitmap & (1 << AVDTP_CONTENT_PROTECTION)) > 0){
            CHECK_TRUE(event_info.saw_content_protection);
        }
        if ((categories_bitmap & (1 << AVDTP_HEADER_COMPRESSION)) > 0){
            CHECK_TRUE(event_info.saw_header_compression);
        }
        if ((categories_bitmap & (1 << AVDTP_MULTIPLEXING)) > 0){
            CHECK_TRUE(event_info.saw_multiplexing);
        }
        if ((categories_bitmap & (1 << AVDTP_MEDIA_CODEC)) > 0){
            CHECK_TRUE(event_info.saw_sbc_codec);
        }
        if ((categories_bitmap & (1 << AVDTP_DELAY_REPORTING)) > 0){
            CHECK_TRUE(event_info.saw_delay_reporting);
        }
        CHECK_TRUE(event_info.saw_done);
        CHECK_EQUAL(AVDTP_SUBEVENT_SIGNALING_CAPABILITIES_DONE, emitted_events[filtered_indices[filtered_count - 1]][2]);
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
    static uint8_t packed_capabilities[] = {
            0x01, 0x00,
            0x07, 0x06, 0x00, 0x00, 0xff, 0xff, 0x02, 0x35,
            0x08, 0x00
    };
    avdtp_prepare_capabilities(&signaling_packet, 0x01, configuration_bitmap, caps, AVDTP_SI_GET_ALL_CAPABILITIES);
    MEMCMP_EQUAL(packed_capabilities, signaling_packet.command, sizeof(packed_capabilities));
}

TEST(AvdtpUtil, avdtp_unpack_service_capabilities_test){
    avdtp_capabilities_t capabilities;
    static uint8_t packed_capabilities[] = {
            0x01, 0x00,
            0x07, 0x06, 0x00, 0x00, 0xff, 0xff, 0x02, 0x35,
            0x08, 0x00
    };
    CHECK_EQUAL(configuration_bitmap, avdtp_unpack_service_capabilities(&connection, AVDTP_SI_GET_CONFIGURATION, &capabilities, &packed_capabilities[0], sizeof(packed_capabilities)));
    // media codec:
    CHECK_EQUAL(AVDTP_CODEC_SBC, capabilities.media_codec.media_codec_type);
    CHECK_EQUAL(4,  capabilities.media_codec.media_codec_information_len);
    MEMCMP_EQUAL(sbc_codec_capabilities, capabilities.media_codec.media_codec_information, 4);
}


TEST(AvdtpUtil, avdtp_initiator_get_capabilities_reassembly_from_prepared_array){
    avdtp_signaling_packet_t prepared_capabilities_packet;
    memset(&prepared_capabilities_packet, 0, sizeof(prepared_capabilities_packet));

    avdtp_signal_identifier_t identifier = AVDTP_SI_GET_ALL_CAPABILITIES;
    uint16_t basic_categories_bitmap = (1 << AVDTP_MEDIA_TRANSPORT) | (1 << AVDTP_REPORTING) | (1 << AVDTP_DELAY_REPORTING);
    avdtp_prepare_capabilities(&prepared_capabilities_packet, 0x01, basic_categories_bitmap, caps, identifier);
    CHECK_EQUAL(6, prepared_capabilities_packet.size);

    send_valid_capabilities_packets(&prepared_capabilities_packet, identifier, sizeof(prepared_capabilities_packet));
    validate_events(basic_categories_bitmap);
}

TEST(AvdtpUtil, avdtp_initiator_get_all_capabilities_reassembly_all_categories_sbc_single_packet){
    avdtp_signaling_packet_t prepared_capabilities_packet;
    memset(&prepared_capabilities_packet, 0, sizeof(prepared_capabilities_packet));

    avdtp_signal_identifier_t identifier = AVDTP_SI_GET_ALL_CAPABILITIES;
    uint16_t all_categories_bitmap = 0;
    for (int i = AVDTP_MEDIA_TRANSPORT; i <= AVDTP_DELAY_REPORTING; i++){
        all_categories_bitmap |= (1 << i);
    }
    avdtp_prepare_capabilities(&prepared_capabilities_packet, 0x01, all_categories_bitmap, caps, identifier);
    CHECK_EQUAL(46, prepared_capabilities_packet.size);

    send_valid_capabilities_packets(&prepared_capabilities_packet, identifier, sizeof(prepared_capabilities_packet));
    validate_events(all_categories_bitmap);
}

TEST(AvdtpUtil, avdtp_initiator_get_all_capabilities_reassembly_all_categories_sbc_chunked_20){
    avdtp_signaling_packet_t prepared_capabilities_packet;
    memset(&prepared_capabilities_packet, 0, sizeof(prepared_capabilities_packet));

    avdtp_signal_identifier_t identifier = AVDTP_SI_GET_ALL_CAPABILITIES;
    uint16_t all_categories_bitmap = 0;
    for (int i = AVDTP_MEDIA_TRANSPORT; i <= AVDTP_DELAY_REPORTING; i++){
        all_categories_bitmap |= (1 << i);
    }
    avdtp_prepare_capabilities(&prepared_capabilities_packet, 0x01, all_categories_bitmap, caps, identifier);
    CHECK_EQUAL(46, prepared_capabilities_packet.size);
    validate_prepared_capapbilities_use_sbc(&prepared_capabilities_packet);

    send_valid_capabilities_packets(&prepared_capabilities_packet, identifier, 20);
    validate_events(all_categories_bitmap);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
