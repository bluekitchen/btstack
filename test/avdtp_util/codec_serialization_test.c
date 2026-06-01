/*
 * Test for serializing/deserializing
 * AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_D_USAC_CONFIGURATION
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "btstack_event.h"
#include "btstack_util.h"
#include "classic/avdtp.h"
#include "classic/avdtp_util.h"

uint16_t l2cap_get_remote_mtu_for_local_cid(uint16_t local_cid){
    UNUSED(local_cid);
    return 1024;
}
void avdtp_emit_sink_and_source(uint8_t * packet, uint16_t size){
    UNUSED(packet);
    UNUSED(size);
}
void avdtp_emit_source(uint8_t * packet, uint16_t size){
    UNUSED(packet);
    UNUSED(size);
}
uint8_t l2cap_request_can_send_now_event(uint16_t local_cid){
    UNUSED(local_cid);
    return ERROR_CODE_SUCCESS;
}
btstack_packet_handler_t avdtp_packet_handler_for_stream_endpoint(const avdtp_stream_endpoint_t * stream_endpoint){
    UNUSED(stream_endpoint);
    return NULL;
}

void btstack_assert_failed(const char * file, uint16_t line_nr);
void btstack_assert_failed(const char * file, uint16_t line_nr){
    printf("ASSERT FAILED in %s:%u\n", file, line_nr);
}

static int assert_equal_uint32(const char * label, uint32_t expected, uint32_t actual){
    if (expected == actual){
        return 1;
    }
    printf("FAIL: %s expected %u, got %u\n", label, (unsigned int) expected, (unsigned int) actual);
    return 0;
}

static int test_avdtp_setup_media_codec_config_event(void){
    avdtp_stream_endpoint_t stream_endpoint;
    memset(&stream_endpoint, 0, sizeof(stream_endpoint));

    // advertise broad USAC capabilities so our concrete config is accepted
    uint8_t codec_capabilities[] = { 0xBF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };
    stream_endpoint.sep.seid = 0x11;
    stream_endpoint.remote_sep.seid = 0x22;
    stream_endpoint.sep.capabilities.media_codec.media_codec_type = AVDTP_CODEC_MPEG_D_USAC;
    stream_endpoint.sep.capabilities.media_codec.media_codec_information = codec_capabilities;
    stream_endpoint.sep.capabilities.media_codec.media_codec_information_len = sizeof(codec_capabilities);

    avdtp_configuration_mpegd_usac_t usac_configuration = {
        .object_type = AVDTP_USAC_OBJECT_TYPE_MPEG_D_DRC,
        .sampling_frequency = 44100,
        .channels = 2,
        .bit_rate = 0xFF,
        .vbr = 1
    };

    uint8_t codec_configuration[7];
    memset(codec_configuration, 0, sizeof(codec_configuration));
    if (avdtp_config_mpegd_usac_store(codec_configuration, &usac_configuration) != ERROR_CODE_SUCCESS){
        printf("FAIL: avdtp_config_mpegd_usac_store failed\n");
        return 1;
    }

    adtvp_media_codec_capabilities_t media_codec;
    memset(&media_codec, 0, sizeof(media_codec));
    media_codec.media_type = AVDTP_AUDIO;
    media_codec.media_codec_type = AVDTP_CODEC_MPEG_D_USAC;
    media_codec.media_codec_information = codec_configuration;
    media_codec.media_codec_information_len = sizeof(codec_configuration);

    uint8_t event[AVDTP_MEDIA_CONFIG_MPEG_D_USAC_EVENT_LEN];
    memset(event, 0, sizeof(event));
    uint16_t out_size = 0;

    codec_specific_error_code_t status = avdtp_setup_media_codec_config_event(
            event, sizeof(event), &stream_endpoint, 0x3344, 1, &media_codec, &out_size);

    int ok = 1;
    ok &= assert_equal_uint32("setup_status", CODEC_SPECIFIC_ERROR_CODE_ACCEPT, status);
    ok &= assert_equal_uint32("out_size", AVDTP_MEDIA_CONFIG_MPEG_D_USAC_EVENT_LEN, out_size);
    ok &= assert_equal_uint32("event_type", HCI_EVENT_AVDTP_META, event[0]);
    ok &= assert_equal_uint32("subevent", AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_D_USAC_CONFIGURATION, event[2]);
    ok &= assert_equal_uint32("avdtp_cid", 0x3344, avdtp_subevent_signaling_media_codec_mpeg_d_usac_configuration_get_avdtp_cid(event));
    ok &= assert_equal_uint32("local_seid", stream_endpoint.sep.seid, avdtp_subevent_signaling_media_codec_mpeg_d_usac_configuration_get_local_seid(event));
    ok &= assert_equal_uint32("remote_seid", stream_endpoint.remote_sep.seid, avdtp_subevent_signaling_media_codec_mpeg_d_usac_configuration_get_remote_seid(event));
    ok &= assert_equal_uint32("reconfigure", 1, avdtp_subevent_signaling_media_codec_mpeg_d_usac_configuration_get_reconfigure(event));
    // accessor name says media_type but field stores codec type in this event layout
    ok &= assert_equal_uint32("media_type_field", AVDTP_CODEC_MPEG_D_USAC, avdtp_subevent_signaling_media_codec_mpeg_d_usac_configuration_get_media_type(event));
    ok &= assert_equal_uint32("object_type", usac_configuration.object_type, avdtp_subevent_signaling_media_codec_mpeg_d_usac_configuration_get_object_type(event));
    ok &= assert_equal_uint32("sampling_frequency", usac_configuration.sampling_frequency, avdtp_subevent_signaling_media_codec_mpeg_d_usac_configuration_get_sampling_frequency(event));
    ok &= assert_equal_uint32("num_channels", usac_configuration.channels, avdtp_subevent_signaling_media_codec_mpeg_d_usac_configuration_get_num_channels(event));
    ok &= assert_equal_uint32("vbr", usac_configuration.vbr, avdtp_subevent_signaling_media_codec_mpeg_d_usac_configuration_get_vbr(event));
    ok &= assert_equal_uint32("bit_rate", usac_configuration.bit_rate, avdtp_subevent_signaling_media_codec_mpeg_d_usac_configuration_get_bit_rate(event));

    if (!ok){
        return 1;
    }

    printf("PASS: avdtp_setup_media_codec_config_event MPEG-D USAC\n");
    return 0;
}

int main(void){
    if (test_avdtp_setup_media_codec_config_event()) return 1;
    return 0;
}
