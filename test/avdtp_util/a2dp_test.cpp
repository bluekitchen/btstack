/*
 * Copyright (C) 2026 BlueKitchen GmbH
 */

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "classic/a2dp.h"
#include "classic/avdtp.h"
#include "classic/avdtp_util.h"
#include "classic/sdp_util.h"
#include "btstack_event.h"
#include "btstack_linked_list.h"
#include "btstack_run_loop.h"

static avdtp_connection_t test_connection;

extern "C" avdtp_connection_t * avdtp_get_connection_for_avdtp_cid(uint16_t avdtp_cid){
    return avdtp_cid == test_connection.avdtp_cid ? &test_connection : NULL;
}

extern "C" uint8_t avdtp_discover_stream_endpoints(uint16_t avdtp_cid){
    UNUSED(avdtp_cid);
    return ERROR_CODE_SUCCESS;
}

extern "C" uint8_t avdtp_get_all_capabilities(uint16_t avdtp_cid, uint8_t remote_seid, avdtp_role_t role){
    UNUSED(avdtp_cid);
    UNUSED(remote_seid);
    UNUSED(role);
    return ERROR_CODE_SUCCESS;
}

extern "C" avdtp_stream_endpoint_t * avdtp_get_stream_endpoint_for_seid(uint16_t seid){
    UNUSED(seid);
    return NULL;
}

extern "C" uint8_t avdtp_stream_endpoint_seid(avdtp_stream_endpoint_t * stream_endpoint){
    UNUSED(stream_endpoint);
    return 0;
}

extern "C" void avdtp_set_preferred_sampling_frequency(avdtp_stream_endpoint_t * stream_endpoint, uint32_t sampling_frequency){
    UNUSED(stream_endpoint);
    UNUSED(sampling_frequency);
}

extern "C" void avdtp_set_preferred_channel_mode(avdtp_stream_endpoint_t * stream_endpoint, uint8_t channel_mode){
    UNUSED(stream_endpoint);
    UNUSED(channel_mode);
}

extern "C" uint8_t avdtp_set_configuration(uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid, uint16_t configured_services_bitmap, avdtp_capabilities_t configuration){
    UNUSED(avdtp_cid);
    UNUSED(local_seid);
    UNUSED(remote_seid);
    UNUSED(configured_services_bitmap);
    UNUSED(configuration);
    return ERROR_CODE_SUCCESS;
}

extern "C" uint8_t avdtp_reconfigure(uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid, uint16_t configured_services_bitmap, avdtp_capabilities_t configuration){
    UNUSED(avdtp_cid);
    UNUSED(local_seid);
    UNUSED(remote_seid);
    UNUSED(configured_services_bitmap);
    UNUSED(configuration);
    return ERROR_CODE_SUCCESS;
}

extern "C" void avdtp_emit_source(uint8_t * packet, uint16_t size){
    UNUSED(packet);
    UNUSED(size);
}

extern "C" void avdtp_emit_sink(uint8_t * packet, uint16_t size){
    UNUSED(packet);
    UNUSED(size);
}

extern "C" btstack_linked_list_t * avdtp_get_connections(void){ return NULL; }
extern "C" avdtp_stream_endpoint_t * avdtp_get_source_stream_endpoint_for_media_codec_and_type(avdtp_media_codec_type_t codec_type, avdtp_sep_type_t sep_type){ UNUSED(codec_type); UNUSED(sep_type); return NULL; }
extern "C" uint8_t avdtp_open_stream(uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid){ UNUSED(avdtp_cid); UNUSED(local_seid); UNUSED(remote_seid); return ERROR_CODE_SUCCESS; }
extern "C" const char * avdtp_si2str(uint16_t index){ UNUSED(index); return ""; }
extern "C" avdtp_channel_mode_t avdtp_choose_sbc_channel_mode(avdtp_stream_endpoint_t * stream_endpoint, uint8_t value){ UNUSED(stream_endpoint); UNUSED(value); return AVDTP_CHANNEL_MODE_MONO; }
extern "C" avdtp_sbc_allocation_method_t avdtp_choose_sbc_allocation_method(avdtp_stream_endpoint_t * stream_endpoint, uint8_t value){ UNUSED(stream_endpoint); UNUSED(value); return AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS; }
extern "C" uint16_t avdtp_choose_sbc_sampling_frequency(avdtp_stream_endpoint_t * stream_endpoint, uint8_t value){ UNUSED(stream_endpoint); UNUSED(value); return 0; }
extern "C" avdtp_sbc_subbands_t avdtp_choose_sbc_subbands(avdtp_stream_endpoint_t * stream_endpoint, uint8_t value){ UNUSED(stream_endpoint); UNUSED(value); return AVDTP_SBC_SUBBANDS_4; }
extern "C" avdtp_sbc_block_length_t avdtp_choose_sbc_block_length(avdtp_stream_endpoint_t * stream_endpoint, uint8_t value){ UNUSED(stream_endpoint); UNUSED(value); return AVDTP_SBC_BLOCK_LENGTH_4; }
extern "C" uint8_t avdtp_choose_sbc_max_bitpool_value(avdtp_stream_endpoint_t * stream_endpoint, uint8_t value){ UNUSED(stream_endpoint); return value; }
extern "C" uint8_t avdtp_choose_sbc_min_bitpool_value(avdtp_stream_endpoint_t * stream_endpoint, uint8_t value){ UNUSED(stream_endpoint); return value; }
extern "C" uint8_t avdtp_config_sbc_store(uint8_t * config, const avdtp_configuration_sbc_t * configuration){ UNUSED(config); UNUSED(configuration); return ERROR_CODE_SUCCESS; }
extern "C" uint8_t avdtp_config_mpeg_audio_store(uint8_t * config, const avdtp_configuration_mpeg_audio_t * configuration){ UNUSED(config); UNUSED(configuration); return ERROR_CODE_SUCCESS; }
extern "C" uint8_t avdtp_config_mpeg_aac_store(uint8_t * config, const avdtp_configuration_mpeg_aac_t * configuration){ UNUSED(config); UNUSED(configuration); return ERROR_CODE_SUCCESS; }
extern "C" uint8_t avdtp_config_atrac_store(uint8_t * config, const avdtp_configuration_atrac_t * configuration){ UNUSED(config); UNUSED(configuration); return ERROR_CODE_SUCCESS; }
extern "C" uint8_t avdtp_config_mpegd_usac_store(uint8_t * config, const avdtp_configuration_mpegd_usac_t * configuration){ UNUSED(config); UNUSED(configuration); return ERROR_CODE_SUCCESS; }
extern "C" uint16_t store_bit16(uint16_t bitmap, int position, uint8_t value){ return value ? (uint16_t)(bitmap | (1u << position)) : (uint16_t)(bitmap & ~(1u << position)); }
extern "C" void btstack_linked_list_iterator_init(btstack_linked_list_iterator_t * it, btstack_linked_list_t * list){ UNUSED(it); UNUSED(list); }
extern "C" bool btstack_linked_list_iterator_has_next(btstack_linked_list_iterator_t * it){ UNUSED(it); return false; }
extern "C" btstack_linked_item_t * btstack_linked_list_iterator_next(btstack_linked_list_iterator_t * it){ UNUSED(it); return NULL; }
extern "C" void btstack_run_loop_set_timer(btstack_timer_source_t * timer, uint32_t timeout){ UNUSED(timer); UNUSED(timeout); }
extern "C" void btstack_run_loop_set_timer_handler(btstack_timer_source_t * timer, void (*handler)(btstack_timer_source_t *)){ UNUSED(timer); UNUSED(handler); }
extern "C" void btstack_run_loop_set_timer_context(btstack_timer_source_t * timer, void * context){ UNUSED(timer); UNUSED(context); }
extern "C" void * btstack_run_loop_get_timer_context(btstack_timer_source_t * timer){ UNUSED(timer); return NULL; }
extern "C" void btstack_run_loop_add_timer(btstack_timer_source_t * timer){ UNUSED(timer); }
extern "C" int btstack_run_loop_remove_timer(btstack_timer_source_t * timer){ UNUSED(timer); return 0; }
extern "C" void de_create_sequence(uint8_t * sequence){ UNUSED(sequence); }
extern "C" uint8_t * de_push_sequence(uint8_t * sequence){ return sequence; }
extern "C" void de_pop_sequence(uint8_t * parent, uint8_t * child){ UNUSED(parent); UNUSED(child); }
extern "C" void de_add_number(uint8_t * sequence, de_type_t type, de_size_t size, uint32_t value){ UNUSED(sequence); UNUSED(type); UNUSED(size); UNUSED(value); }
extern "C" void de_add_data(uint8_t * sequence, de_type_t type, uint16_t size, uint8_t * data){ UNUSED(sequence); UNUSED(type); UNUSED(size); UNUSED(data); }

TEST_GROUP(A2dp){
    void setup(){
        memset(&test_connection, 0, sizeof(test_connection));
        test_connection.avdtp_cid = 0x1234;
        test_connection.a2dp_source_config_process.state = A2DP_W4_DISCOVER_SEPS;
        a2dp_init();
    }
};

TEST(A2dp, ignore_seps_beyond_discovery_capacity){
    uint8_t event[] = {
        HCI_EVENT_AVDTP_META, 7, AVDTP_SUBEVENT_SIGNALING_SEP_FOUND,
        0x34, 0x12, 0, 0, AVDTP_AUDIO, AVDTP_SINK
    };

    for (uint8_t seid = 1; seid <= 11; seid++){
        event[5] = seid;
        a2dp_config_process_avdtp_event_handler(AVDTP_ROLE_SOURCE, event, sizeof(event));
    }

    CHECK(true);
}

int main(int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
