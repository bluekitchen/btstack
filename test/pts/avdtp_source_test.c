/*
 * Copyright (C) 2016 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "avdtp_source_test.c"

/*
 * avdtp_source_test.c : Tool for testig AVDTP source with PTS, see avdtp_source_test.md and a2dp_source.md for PTS tests command sequences
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "btstack.h"

#ifdef HAVE_AAC_FDK
#include <fdk-aac/aacenc_lib.h>
#endif

#ifdef HAVE_LDAC_ENCODER
#include <ldac/ldacBT.h>
#endif

#define A2DP_CODEC_VENDOR_ID_SONY 0x12d
#define A2DP_SONY_CODEC_LDAC 0xaa

#ifdef HAVE_APTX
#include <openaptx.h>
#endif
#define A2DP_CODEC_VENDOR_ID_APT_LTD 0x4f
#define A2DP_CODEC_VENDOR_ID_QUALCOMM 0xd7
#define A2DP_APT_LTD_CODEC_APTX 0x1
#define A2DP_QUALCOMM_CODEC_APTX_HD 0x24

#define AVDTP_MAX_SEP_NUM 10
#define AVDTP_MAX_MEDIA_CODEC_CONFIG_LEN 16
#define AVDTP_MAX_MEDIA_CODEC_CAPABILITES_EVENT_LEN 100

#define VOLUME_REDUCTION 2

#define AUDIO_TIMEOUT_MS            5
#define TABLE_SIZE_441HZ            100

static const int16_t sine_int16[] = {
     0,    2057,    4107,    6140,    8149,   10126,   12062,   13952,   15786,   17557,
 19260,   20886,   22431,   23886,   25247,   26509,   27666,   28714,   29648,   30466,
 31163,   31738,   32187,   32509,   32702,   32767,   32702,   32509,   32187,   31738,
 31163,   30466,   29648,   28714,   27666,   26509,   25247,   23886,   22431,   20886,
 19260,   17557,   15786,   13952,   12062,   10126,    8149,    6140,    4107,    2057,
     0,   -2057,   -4107,   -6140,   -8149,  -10126,  -12062,  -13952,  -15786,  -17557,
-19260,  -20886,  -22431,  -23886,  -25247,  -26509,  -27666,  -28714,  -29648,  -30466,
-31163,  -31738,  -32187,  -32509,  -32702,  -32767,  -32702,  -32509,  -32187,  -31738,
-31163,  -30466,  -29648,  -28714,  -27666,  -26509,  -25247,  -23886,  -22431,  -20886,
-19260,  -17557,  -15786,  -13952,  -12062,  -10126,   -8149,   -6140,   -4107,   -2057,
};

typedef struct {
    // bitmaps
    uint8_t sampling_frequency_bitmap;
    uint8_t channel_mode_bitmap;
    uint8_t block_length_bitmap;
    uint8_t subbands_bitmap;
    uint8_t allocation_method_bitmap;
    uint8_t min_bitpool_value;
    uint8_t max_bitpool_value;
} media_codec_information_sbc_t;

typedef struct {
    int reconfigure;
    int num_channels;
    int sampling_frequency;
    int block_length;
    int subbands;
    int min_bitpool_value;
    int max_bitpool_value;

    int channel_mode;
    int allocation_method;
} avdtp_media_codec_configuration_sbc_t;

typedef struct {
    int reconfigure;
    int sampling_frequency_bitmap;
    int object_type_bitmap;
    int bit_rate;
    int channels_bitmap;
    int vbr;
} avdtp_media_codec_capabilities_aac_t;

typedef struct {
    int reconfigure;
    int channel_mode;
    int num_channels;
    int num_samples;
    int sampling_frequency;
} avdtp_media_codec_configuration_ldac_t;

typedef struct {
    int reconfigure;
    int channel_mode;
    int num_channels;
    int sampling_frequency;
} avdtp_media_codec_configuration_aptx_t;

#ifdef HAVE_BTSTACK_STDIN
// mac 2011:    static const char * device_addr_string = "04:0C:CE:E4:85:D3";
// pts:         static const char * device_addr_string = "00:1B:DC:08:E2:72";
// mac 2013:    static const char * device_addr_string = "84:38:35:65:d1:15";
// phone 2013:  static const char * device_addr_string = "D8:BB:2C:DF:F0:F2";
// minijambox:  static const char * device_addr_string = "00:21:3C:AC:F7:38";
// head phones: static const char * device_addr_string = "00:18:09:28:50:18";
// BT dongle:   static const char * device_addr_string = "00:15:83:5F:9D:46";
// BT dongle:   static const char * device_addr_string = "00:1A:7D:DA:71:0A";

static const char * device_addr_string = "00:1B:DC:08:E2:5C";
#endif

typedef struct {
    uint16_t avdtp_cid;
    uint8_t  local_seid;
    uint8_t  remote_seid;

    uint32_t time_audio_data_sent; // ms
    uint32_t acc_num_missed_samples;
    uint32_t samples_ready;
    btstack_timer_source_t audio_timer;
    uint8_t  streaming;
    int      max_media_payload_size;

    uint8_t  codec_storage[1030];
    uint16_t codec_storage_count;
    uint8_t  codec_ready_to_send;
    uint16_t codec_num_frames;
} a2dp_media_sending_context_t;


#ifdef HAVE_AAC_FDK
static HANDLE_AACENCODER handleAAC;
static AACENC_InfoStruct aacinf;
#endif

#ifdef HAVE_LDAC_ENCODER
HANDLE_LDAC_BT handleLDAC;
#endif

static struct aptx_context *aptx_handle;

static a2dp_media_sending_context_t media_tracker;

static int current_sample_rate = 44100;

static bd_addr_t device_addr;
static int sine_phase;

static uint8_t sdp_avdtp_source_service_buffer[150];

static uint16_t     num_remote_seps;

static uint16_t                  media_codec_config_len;
static uint8_t                   media_codec_config_data[AVDTP_MAX_MEDIA_CODEC_CONFIG_LEN];

static struct {
    avdtp_sep_t  sep;
    bool have_media_codec_apabilities;
    uint8_t                  media_codec_event[AVDTP_MAX_MEDIA_CODEC_CAPABILITES_EVENT_LEN];
} remote_seps[AVDTP_MAX_SEP_NUM];
static uint8_t selected_remote_sep_index;

static struct {
    avdtp_stream_endpoint_t * local_stream_endpoint;
    uint32_t sampling_frequency;
}  sc;

static btstack_sbc_encoder_state_t sbc_encoder_state;
static uint8_t is_cmd_triggered_localy = 0;

static btstack_packet_callback_registration_t hci_event_callback_registration;

static const uint8_t media_sbc_codec_capabilities[] = {
    0xFF,//(AVDTP_SBC_44100 << 4) | AVDTP_SBC_STEREO,
    0xFF,//(AVDTP_SBC_BLOCK_LENGTH_16 << 4) | (AVDTP_SBC_SUBBANDS_8 << 2) | AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS,
    2, 53
};

#ifdef HAVE_AAC_FDK
static uint8_t media_aac_codec_capabilities[] = {
        0xF0,
        0xFF,
        0xFC,
        0x80,
        0,
        0
};
#endif

static uint8_t media_ldac_codec_capabilities[] = {
        0x2D, 0x1, 0x0, 0x0,
        0xAA, 0,
        0x3C,
        0x7
};

static uint8_t media_aptx_codec_capabilities[] = {
        0x4F, 0x0, 0x0, 0x0,
        0x1, 0,
        0xFF,
};

static uint8_t media_aptxhd_codec_capabilities[] = {
        0xD7, 0x0, 0x0, 0x0,
        0x24, 0,
        0xFF,
        0, 0, 0, 0
};

// configurations for local stream endpoints
static uint8_t local_stream_endpoint_sbc_media_codec_configuration[4];
#ifdef HAVE_AAC_FDK
static uint8_t local_stream_endpoint_aac_media_codec_configuration[6];
#endif
static uint8_t local_stream_endpoint_ldac_media_codec_configuration[8];
static avdtp_media_codec_configuration_ldac_t ldac_configuration;
static uint8_t local_stream_endpoint_aptx_media_codec_configuration[7];
static avdtp_media_codec_configuration_aptx_t aptx_configuration;
static uint8_t local_stream_endpoint_aptxhd_media_codec_configuration[11];
static avdtp_media_codec_configuration_aptx_t aptxhd_configuration;

static int a2dp_sample_rate(void){
    return current_sample_rate;
}

static void configure_sample_rate(int sampling_frequency){
    switch (sampling_frequency){
        case AVDTP_SBC_48000:
            current_sample_rate = 48000;
            break;
        case AVDTP_SBC_44100:
            current_sample_rate = 44100;
            break;
        case AVDTP_SBC_32000:
            current_sample_rate = 32000;
            break;
        case AVDTP_SBC_16000:
            current_sample_rate = 16000;
            break;
        default:
            break;
    }

    media_tracker.codec_storage_count = 0;
    media_tracker.samples_ready = 0;
}

static const char * codec_name_for_type(avdtp_media_codec_type_t codec_type){
    switch (codec_type){
        case AVDTP_CODEC_SBC:            return "SBC";
        case AVDTP_CODEC_MPEG_1_2_AUDIO: return "MPEG_1_2_AUDIO";
        case AVDTP_CODEC_MPEG_2_4_AAC:   return "MPEG_2_4_AAC";
        case AVDTP_CODEC_ATRAC_FAMILY:   return "ATRAC_FAMILY";
        case AVDTP_CODEC_NON_A2DP:       return "NON_A2DP/Vendor";
        default:                         return "Unknown";
    }
}

static void a2dp_demo_send_media_packet_sbc(void){
    int num_bytes_in_frame = btstack_sbc_encoder_sbc_buffer_length();
    int bytes_in_storage = media_tracker.codec_storage_count;
    uint8_t num_frames = bytes_in_storage / num_bytes_in_frame;
    
    avdtp_source_stream_send_media_payload(media_tracker.avdtp_cid, media_tracker.local_seid, media_tracker.codec_storage, bytes_in_storage, num_frames, 0);
    media_tracker.codec_storage_count = 0;
    media_tracker.codec_ready_to_send = 0;
}

static void a2dp_demo_send_media_packet_aac(void) {
    int bytes_to_send;
    if (media_tracker.codec_storage_count > media_tracker.max_media_payload_size)
        bytes_to_send = media_tracker.max_media_payload_size;
    else
        bytes_to_send = media_tracker.codec_storage_count;
    a2dp_source_stream_send_media_payload_rtp(media_tracker.avdtp_cid, media_tracker.local_seid, 0, media_tracker.codec_storage, bytes_to_send);
    media_tracker.codec_storage_count -= bytes_to_send;
    memcpy(media_tracker.codec_storage, &media_tracker.codec_storage[bytes_to_send], media_tracker.codec_storage_count);
    media_tracker.codec_ready_to_send = 0;
}

static void a2dp_demo_send_media_packet_ldac(void) {
    uint8_t num_frames = media_tracker.codec_num_frames;
    media_tracker.codec_storage[0] = num_frames; // frames in first byte
    a2dp_source_stream_send_media_payload_rtp(media_tracker.avdtp_cid, media_tracker.local_seid, 0, media_tracker.codec_storage, media_tracker.codec_storage_count);
    media_tracker.codec_storage_count = 0;
    media_tracker.codec_ready_to_send = 0;
}

static void a2dp_send_aptx(void) {
    a2dp_source_stream_send_media_packet(media_tracker.avdtp_cid, media_tracker.local_seid, media_tracker.codec_storage, media_tracker.codec_storage_count);
    media_tracker.codec_storage_count = 0;
    media_tracker.codec_ready_to_send = 0;
}

static void a2dp_send_aptx_hd(void) {
    a2dp_source_stream_send_media_payload_rtp(media_tracker.avdtp_cid, media_tracker.local_seid, 0, media_tracker.codec_storage, media_tracker.codec_storage_count);
    media_tracker.codec_storage_count = 0;
    media_tracker.codec_ready_to_send = 0;
}

static uint32_t get_vendor_id(const uint8_t *codec_info) {
    uint32_t vendor_id = 0;
    vendor_id |= codec_info[0];
    vendor_id |= codec_info[1] << 8;
    vendor_id |= codec_info[2] << 16;
    vendor_id |= codec_info[3] << 24;
    return vendor_id;
}

static uint16_t get_codec_id(const uint8_t *codec_info) {
    uint16_t codec_id = 0;
    codec_id |= codec_info[4];
    codec_id |= codec_info[5] << 8;
    return codec_id;
}

static void a2dp_demo_send_media_packet(void) {
    adtvp_media_codec_capabilities_t local_cap;
    switch (remote_seps[selected_remote_sep_index].sep.capabilities.media_codec.media_codec_type){
        case AVDTP_CODEC_SBC:
            a2dp_demo_send_media_packet_sbc();
            break;
        case AVDTP_CODEC_MPEG_2_4_AAC:
            a2dp_demo_send_media_packet_aac();
            break;
        case AVDTP_CODEC_NON_A2DP:
            local_cap = sc.local_stream_endpoint->sep.capabilities.media_codec;
            uint32_t local_vendor_id = get_vendor_id(local_cap.media_codec_information);
            uint16_t local_codec_id = get_codec_id(local_cap.media_codec_information);
            if (local_vendor_id == A2DP_CODEC_VENDOR_ID_SONY && local_codec_id == A2DP_SONY_CODEC_LDAC)
                a2dp_demo_send_media_packet_ldac();
            else if (local_vendor_id == A2DP_CODEC_VENDOR_ID_APT_LTD && local_codec_id == A2DP_APT_LTD_CODEC_APTX)
                a2dp_send_aptx();
            else if (local_vendor_id == A2DP_CODEC_VENDOR_ID_QUALCOMM && local_codec_id == A2DP_QUALCOMM_CODEC_APTX_HD)
                a2dp_send_aptx_hd();
            break;
        default:
            // TODO:
            printf("Send media payload for %s not implemented yet\n", codec_name_for_type(sc.local_stream_endpoint->media_codec_type));
            break;
    }
}

static void produce_sine_audio(int16_t * pcm_buffer, int num_samples_to_write){
    int count;
    for (count = 0; count < num_samples_to_write ; count++){
        pcm_buffer[count * 2]     = sine_int16[sine_phase] >> VOLUME_REDUCTION;
        pcm_buffer[count * 2 + 1] = sine_int16[sine_phase] >> VOLUME_REDUCTION;
        sine_phase++;
        if (sine_phase >= TABLE_SIZE_441HZ){
            sine_phase -= TABLE_SIZE_441HZ;
        }
    }
}

static int fill_sbc_audio_buffer(a2dp_media_sending_context_t * context){
    // perform sbc encodin
    int total_num_bytes_read = 0;
    int num_audio_samples_per_sbc_buffer = btstack_sbc_encoder_num_audio_frames();
    
    while (context->samples_ready >= num_audio_samples_per_sbc_buffer
        && (context->max_media_payload_size - context->codec_storage_count) >= btstack_sbc_encoder_sbc_buffer_length()){

        // uint8_t pcm_frame[ 256 * bytes_per_audio_sample()];
        int16_t pcm_frame[256*2];

        produce_sine_audio((int16_t *) pcm_frame, num_audio_samples_per_sbc_buffer);
        btstack_sbc_encoder_process_data((int16_t *) pcm_frame);
        
        uint16_t sbc_frame_size = btstack_sbc_encoder_sbc_buffer_length(); 
        uint8_t * sbc_frame = btstack_sbc_encoder_sbc_buffer();
        
        total_num_bytes_read += num_audio_samples_per_sbc_buffer;
        memcpy(&context->codec_storage[context->codec_storage_count], sbc_frame, sbc_frame_size);
        context->codec_storage_count += sbc_frame_size;
        context->samples_ready -= num_audio_samples_per_sbc_buffer;
    }
    return total_num_bytes_read;
}

#ifdef HAVE_AAC_FDK
static int fill_aac_audio_buffer(a2dp_media_sending_context_t *context) {
    int          total_samples_read               = 0;
    unsigned int num_audio_samples_per_aac_buffer = aacinf.frameLength;

    btstack_assert(num_audio_samples_per_aac_buffer <= 1024);
    int16_t  pcm_frame[2048];
    unsigned required_bytes = num_audio_samples_per_aac_buffer * aacinf.inputChannels;

    AACENC_BufDesc in_buf   = { 0 }, out_buf = { 0 };
    AACENC_InArgs  in_args  = { 0 };
    AACENC_OutArgs out_args = { 0 };
    int in_identifier = IN_AUDIO_DATA;
    int in_size, in_elem_size;
    int out_identifier = OUT_BITSTREAM_DATA;
    int out_size, out_elem_size;
    void *in_ptr, *out_ptr;

    in_ptr                   = pcm_frame;
    in_size                  = required_bytes;
    in_elem_size             = 2;
    in_buf.numBufs           = 1;
    in_buf.bufs              = &in_ptr;
    in_buf.bufferIdentifiers = &in_identifier;
    in_buf.bufSizes          = &in_size;
    in_buf.bufElSizes        = &in_elem_size;

    out_elem_size             = 1;
    out_buf.numBufs           = 1;
    out_buf.bufs              = &out_ptr;
    out_buf.bufferIdentifiers = &out_identifier;
    out_buf.bufSizes          = &out_size;
    out_buf.bufElSizes        = &out_elem_size;

    while (context->samples_ready >= num_audio_samples_per_aac_buffer &&
           (context->max_media_payload_size - context->codec_storage_count) > 0) {
        produce_sine_audio((int16_t *) pcm_frame, num_audio_samples_per_aac_buffer);
        in_args.numInSamples = required_bytes;
        out_ptr              = &context->codec_storage[context->codec_storage_count];
        out_size             = sizeof(context->codec_storage) - context->codec_storage_count;
        out_buf.bufs         = &out_ptr;
        out_buf.bufSizes     = &out_size;
        AACENC_ERROR err;

        if ((err = aacEncEncode(handleAAC, &in_buf, &out_buf, &in_args, &out_args)) != AACENC_OK) {
            printf("Error in AAC encoding %d. Check if codec storage size is sufficient\n", err);
        }

        total_samples_read += num_audio_samples_per_aac_buffer;
        context->codec_storage_count += out_args.numOutBytes;
        context->samples_ready -= num_audio_samples_per_aac_buffer;
    }
    return total_samples_read;
}
#endif

#ifdef HAVE_LDAC_ENCODER
static int a2dp_demo_fill_ldac_audio_buffer(a2dp_media_sending_context_t *context) {
    int          total_samples_read                = 0;
    unsigned int num_audio_samples_per_ldac_buffer = LDACBT_ENC_LSU;
    int          consumed;
	int          encoded = 0;
	int          frames;

    // reserve first byte for number of frames
    if (context->codec_storage_count == 0)
        context->codec_storage_count = 1;

    int16_t pcm_frame[LDACBT_MAX_LSU * 2];
    while (context->samples_ready >= num_audio_samples_per_ldac_buffer && encoded == 0) {
        produce_sine_audio((int16_t *) pcm_frame, num_audio_samples_per_ldac_buffer);

        if (ldacBT_encode(handleLDAC, pcm_frame, &consumed, &context->codec_storage[context->codec_storage_count], &encoded, &frames) != 0) {
            printf("LDAC encoding error: %d\n", ldacBT_get_error_code(handleLDAC));
        }
        consumed = consumed / (2 * ldac_configuration.num_channels);
        total_samples_read += consumed;
        context->codec_storage_count += encoded;
        context->codec_num_frames += frames;
        context->samples_ready -= consumed;
    }
    return total_samples_read;
}
#endif

#ifdef HAVE_APTX
static int a2dp_demo_fill_aptx_audio_buffer(a2dp_media_sending_context_t *context) {
    int          total_num_bytes_read              = 0;
    unsigned int num_audio_samples_per_aptx_buffer = 4;
    uint8_t      out_size;
    int          channels;

    adtvp_media_codec_capabilities_t local_cap = sc.local_stream_endpoint->sep.capabilities.media_codec;
    uint32_t local_vendor_id = get_vendor_id(local_cap.media_codec_information);
    uint16_t local_codec_id = get_codec_id(local_cap.media_codec_information);
    if (local_vendor_id == A2DP_CODEC_VENDOR_ID_APT_LTD && local_codec_id == A2DP_APT_LTD_CODEC_APTX) {
        out_size = 4;
        channels = aptx_configuration.num_channels;
    } else {
        out_size = 6; // aptx hd
        channels = aptxhd_configuration.num_channels;
    }
    uint16_t required_samples = num_audio_samples_per_aptx_buffer * channels;
    int16_t pcm_frame16[4 * 2];
    unsigned char pcm_frame8[4 * 2 * 3];

    while (context->samples_ready >= num_audio_samples_per_aptx_buffer &&
           (context->max_media_payload_size - context->codec_storage_count) >= out_size) {
        produce_sine_audio((int16_t *) pcm_frame16, num_audio_samples_per_aptx_buffer);

        // convert to 24-bit stored in bytes
        for (int i = 0; i < 4 * 2; i++) {
            pcm_frame8[i * 3 + 2] = (pcm_frame16[i] & 0xFF00) >> 8;
            pcm_frame8[i * 3 + 1] = pcm_frame16[i] & 0xFF;
            pcm_frame8[i * 3] = 0;
        }

        size_t written;
        aptx_encode(aptx_handle, pcm_frame8, 4 * 3 * 2, &context->codec_storage[context->codec_storage_count], 6, &written);

        total_num_bytes_read += num_audio_samples_per_aptx_buffer;
        context->codec_storage_count += out_size;  // LLRR  for aptx or LLLRRR for aptx hd
        context->samples_ready -= num_audio_samples_per_aptx_buffer;
    }
    return total_num_bytes_read;
}
#endif

static void avdtp_audio_timeout_handler(btstack_timer_source_t * timer){
    adtvp_media_codec_capabilities_t local_cap;
    a2dp_media_sending_context_t * context = (a2dp_media_sending_context_t *) btstack_run_loop_get_timer_context(timer);
    btstack_run_loop_set_timer(&context->audio_timer, AUDIO_TIMEOUT_MS); 
    btstack_run_loop_add_timer(&context->audio_timer);
    uint32_t now = btstack_run_loop_get_time_ms();

    uint32_t update_period_ms = AUDIO_TIMEOUT_MS;
    if (context->time_audio_data_sent > 0){
        update_period_ms = now - context->time_audio_data_sent;
    } 

    uint32_t num_samples = (update_period_ms * a2dp_sample_rate()) / 1000;
    context->acc_num_missed_samples += (update_period_ms * a2dp_sample_rate()) % 1000;
    
    while (context->acc_num_missed_samples >= 1000){
        num_samples++;
        context->acc_num_missed_samples -= 1000;
    }
    context->time_audio_data_sent = now;
    context->samples_ready += num_samples;

    if (context->codec_ready_to_send) return;

    avdtp_media_codec_type_t codec_type = sc.local_stream_endpoint->remote_configuration.media_codec.media_codec_type;
    switch (codec_type){
        case AVDTP_CODEC_SBC:
            fill_sbc_audio_buffer(context);
            if ((context->codec_storage_count + btstack_sbc_encoder_sbc_buffer_length()) > context->max_media_payload_size){
                // schedule sending
                context->codec_ready_to_send = 1;
                a2dp_source_stream_endpoint_request_can_send_now(context->avdtp_cid, context->local_seid);
            }
            break;
        case AVDTP_CODEC_MPEG_1_2_AUDIO:
            break;
#ifdef HAVE_AAC_FDK
        case AVDTP_CODEC_MPEG_2_4_AAC:
            if (context->codec_storage_count == 0){
                fill_aac_audio_buffer(context);
            }

            if (context->codec_storage_count > 0) {
                // schedule sending
                context->codec_ready_to_send = 1;
                a2dp_source_stream_endpoint_request_can_send_now(context->avdtp_cid, context->local_seid);
            }
            break;
#endif
        case AVDTP_CODEC_ATRAC_FAMILY:
            break;
        case AVDTP_CODEC_NON_A2DP:
            local_cap = sc.local_stream_endpoint->sep.capabilities.media_codec;
            uint32_t local_vendor_id = get_vendor_id(local_cap.media_codec_information);
            uint16_t local_codec_id = get_codec_id(local_cap.media_codec_information);
            // LDAC
#ifdef HAVE_LDAC_ENCODER
            if (local_vendor_id == A2DP_CODEC_VENDOR_ID_SONY && local_codec_id == A2DP_SONY_CODEC_LDAC) {
                if (context->codec_ready_to_send)
                    return;
                a2dp_demo_fill_ldac_audio_buffer(context);

                if (context->codec_storage_count > 1) {
                    // schedule sending
                    context->codec_ready_to_send = 1;
                    a2dp_source_stream_endpoint_request_can_send_now(context->avdtp_cid, context->local_seid);
                }
            }
#endif
#ifdef HAVE_APTX
            // APTX / APTX HD
            if ((local_vendor_id == A2DP_CODEC_VENDOR_ID_APT_LTD && local_codec_id == A2DP_APT_LTD_CODEC_APTX) ||
                    (local_vendor_id == A2DP_CODEC_VENDOR_ID_QUALCOMM && local_codec_id == A2DP_QUALCOMM_CODEC_APTX_HD)) {
                if (context->codec_ready_to_send)
                    return;
                a2dp_demo_fill_aptx_audio_buffer(context);

                if ((context->codec_storage_count + 6) > context->max_media_payload_size) {
                    // schedule sending
                    context->codec_ready_to_send = 1;
                    a2dp_source_stream_endpoint_request_can_send_now(context->avdtp_cid, context->local_seid);
                }
            }
#endif
            break;
        default:
            break;
    }
}

static void a2dp_demo_timer_start(a2dp_media_sending_context_t * context){
    context->max_media_payload_size = 0x290;// avdtp_max_media_payload_size(context->local_seid);
    context->codec_storage_count = 0;
    context->codec_ready_to_send = 0;
    context->streaming = 1;
    btstack_run_loop_remove_timer(&context->audio_timer);
    btstack_run_loop_set_timer_handler(&context->audio_timer, avdtp_audio_timeout_handler);
    btstack_run_loop_set_timer_context(&context->audio_timer, context);
    btstack_run_loop_set_timer(&context->audio_timer, AUDIO_TIMEOUT_MS); 
    btstack_run_loop_add_timer(&context->audio_timer);
}

static void a2dp_demo_timer_stop(a2dp_media_sending_context_t * context){
    context->time_audio_data_sent = 0;
    context->acc_num_missed_samples = 0;
    context->samples_ready = 0;
    context->streaming = 1;
    context->codec_storage_count = 0;
    context->codec_ready_to_send = 0;
    btstack_run_loop_remove_timer(&context->audio_timer);
} 

static void a2dp_demo_timer_pause(a2dp_media_sending_context_t * context){
    btstack_run_loop_remove_timer(&context->audio_timer);
} 


static void dump_sbc_capability(media_codec_information_sbc_t media_codec_sbc){
    printf("    - sampling_frequency: 0x%02x\n", media_codec_sbc.sampling_frequency_bitmap);
    printf("    - channel_mode: 0x%02x\n", media_codec_sbc.channel_mode_bitmap);
    printf("    - block_length: 0x%02x\n", media_codec_sbc.block_length_bitmap);
    printf("    - subbands: 0x%02x\n", media_codec_sbc.subbands_bitmap);
    printf("    - allocation_method: 0x%02x\n", media_codec_sbc.allocation_method_bitmap);
    printf("    - bitpool_value [%d, %d] \n", media_codec_sbc.min_bitpool_value, media_codec_sbc.max_bitpool_value);
}

static void dump_sbc_configuration(avdtp_media_codec_configuration_sbc_t configuration){
    printf("Received media codec configuration:\n");
    printf("    - num_channels: %d\n", configuration.num_channels);
    printf("    - sampling_frequency: %d\n", configuration.sampling_frequency);
    printf("    - channel_mode: %d\n", configuration.channel_mode);
    printf("    - block_length: %d\n", configuration.block_length);
    printf("    - subbands: %d\n", configuration.subbands);
    printf("    - allocation_method: %d\n", configuration.allocation_method);
    printf("    - bitpool_value [%d, %d] \n", configuration.min_bitpool_value, configuration.max_bitpool_value);
}

static void dump_remote_sink_endpoints(void){
    printf("Remote Endpoints:\n");
    int i;
    for (i=0;i<num_remote_seps;i++) {
        printf("- %u. remote seid %u\n", i, remote_seps[i].sep.seid);
    }
}

static int find_remote_seid(uint8_t remote_seid){
    int i;
    for (i=0;i<num_remote_seps;i++){
        if (remote_seps[i].sep.seid == remote_seid){
            return i;
        }
    }
    return -1;
}

static void setup_sbc_codec_config(uint8_t local_remote_seid_index){
    // choose SBC config params
    const uint8_t * packet = remote_seps[local_remote_seid_index].media_codec_event;

    // choose SBC config params
    avdtp_configuration_sbc_t configuration;
    configuration.sampling_frequency = avdtp_choose_sbc_sampling_frequency(sc.local_stream_endpoint, avdtp_subevent_signaling_media_codec_sbc_capability_get_sampling_frequency_bitmap(packet));
    configuration.channel_mode       = avdtp_choose_sbc_channel_mode(sc.local_stream_endpoint, avdtp_subevent_signaling_media_codec_sbc_capability_get_channel_mode_bitmap(packet));
    configuration.block_length       = avdtp_choose_sbc_block_length(sc.local_stream_endpoint, avdtp_subevent_signaling_media_codec_sbc_capability_get_block_length_bitmap(packet));
    configuration.subbands           = avdtp_choose_sbc_subbands(sc.local_stream_endpoint, avdtp_subevent_signaling_media_codec_sbc_capability_get_subbands_bitmap(packet));
    configuration.allocation_method  = avdtp_choose_sbc_allocation_method(sc.local_stream_endpoint, avdtp_subevent_signaling_media_codec_sbc_capability_get_allocation_method_bitmap(packet));
    configuration.max_bitpool_value  = avdtp_choose_sbc_max_bitpool_value(sc.local_stream_endpoint, avdtp_subevent_signaling_media_codec_sbc_capability_get_max_bitpool_value(packet));
    configuration.min_bitpool_value  = avdtp_choose_sbc_min_bitpool_value(sc.local_stream_endpoint, avdtp_subevent_signaling_media_codec_sbc_capability_get_min_bitpool_value(packet));

    // setup SBC configuration
    avdtp_config_sbc_store(media_codec_config_data, &configuration);
    media_codec_config_len = 4;
}

static void setup_mpeg_audio_codec_config(uint8_t local_remote_seid_index) {
    UNUSED(local_remote_seid_index);
    // setup MPEG Audio configuration (layer 3, no crc, joint stereo, mpf = 1, 48 khz, vbr, bit rate index 14);
    avdtp_configuration_mpeg_audio_t configuration;
    configuration.layer = AVDTP_MPEG_LAYER_3;
    configuration.crc = 0;
    configuration.channel_mode = AVDTP_CHANNEL_MODE_JOINT_STEREO;
    configuration.media_payload_format = 0;
    configuration.sampling_frequency = 48000;
    configuration.vbr = 0;
    configuration.bit_rate_index = 14;
    avdtp_config_mpeg_audio_store(media_codec_config_data, &configuration);
    media_codec_config_len = 4;
}

static void setup_mpeg_aac_codec_config(uint8_t local_remote_seid_index) {
    UNUSED(local_remote_seid_index);
    // setup MPEG AAC configuration (MPEG 2 LC, 48 kHz, 2 channels, 300 kbps, no vbr)
    avdtp_configuration_mpeg_aac_t configuration;
    configuration.object_type = AVDTP_AAC_MPEG2_LC;
    configuration.sampling_frequency = 48000;
    configuration.channels = 2;
    configuration.bit_rate = 300000;
    configuration.vbr = 0;
    avdtp_config_mpeg_aac_store(media_codec_config_data, &configuration);
    media_codec_config_len = 6;
}

static void setup_atrac_codec_config(uint8_t local_remote_seid_index) {
    UNUSED(local_remote_seid_index);
    // setup ATRAC configuration (ATRAC-3, joint stereo, no vbr, bit rate index 0x10, max sul 100)
    avdtp_configuration_atrac_t configuration;
    configuration.version = AVDTP_ATRAC_VERSION_3;
    configuration.channel_mode = AVDTP_CHANNEL_MODE_JOINT_STEREO;
    configuration.sampling_frequency = 48000;
    configuration.bit_rate_index = 0x10;
    configuration.maximum_sul = 100;
    avdtp_config_atrac_store(media_codec_config_data, &configuration);
    media_codec_config_len = 4;
}

static void setup_non_a2dp_codec_config(uint8_t local_remote_seid_index) {
    const uint8_t * packet = remote_seps[local_remote_seid_index].media_codec_event;
    uint8_t remote_seid = avdtp_subevent_signaling_media_codec_sbc_capability_get_remote_seid(packet);
    const uint8_t *media_info = a2dp_subevent_signaling_media_codec_other_capability_get_media_codec_information(packet);
    uint32_t vendor_id = get_vendor_id(media_info);
    uint16_t codec_id = get_codec_id(media_info);
    // LDAC
    if (vendor_id == A2DP_CODEC_VENDOR_ID_SONY && codec_id == A2DP_SONY_CODEC_LDAC) {
        printf("Setup LDAC\n");
        media_codec_config_data[0] = 0x2D;
        media_codec_config_data[1] = 0x1;
        media_codec_config_data[2] = 0x0;
        media_codec_config_data[3] = 0x0;
        media_codec_config_data[4] = 0xAA;
        media_codec_config_data[5] = 0;
        media_codec_config_data[6] = 0x4;
        media_codec_config_data[7] = 1;
        media_codec_config_len = 8;
    } else if (vendor_id == A2DP_CODEC_VENDOR_ID_APT_LTD && codec_id == A2DP_APT_LTD_CODEC_APTX) {
        printf("Setup aptx\n");
        media_codec_config_data[0] = 0x4f;
        media_codec_config_data[1] = 0x0;
        media_codec_config_data[2] = 0x0;
        media_codec_config_data[3] = 0x0;
        media_codec_config_data[4] = 0x1;
        media_codec_config_data[5] = 0x0;
        media_codec_config_data[6] = 0x12;
        media_codec_config_len = 7;
    } else if (vendor_id == A2DP_CODEC_VENDOR_ID_QUALCOMM && codec_id == A2DP_QUALCOMM_CODEC_APTX_HD) {
        printf("Setup aptx hd\n");
        media_codec_config_data[0] = 0xD7;
        media_codec_config_data[1] = 0x0;
        media_codec_config_data[2] = 0x0;
        media_codec_config_data[3] = 0x0;
        media_codec_config_data[4] = 0x24;
        media_codec_config_data[5] = 0x0;
        media_codec_config_data[6] = 0x12;
        media_codec_config_data[7] = 0x0;
        media_codec_config_data[8] = 0x0;
        media_codec_config_data[9] = 0x0;
        media_codec_config_data[10] = 0x0;
        media_codec_config_len = 11;
    }
}

static bool setup_remote_config(uint8_t local_remote_seid_index){
    avdtp_media_codec_type_t codec_type = remote_seps[local_remote_seid_index].sep.capabilities.media_codec.media_codec_type;
    if (codec_type == AVDTP_CODEC_NON_A2DP) {
        const uint8_t * packet = remote_seps[local_remote_seid_index].media_codec_event;
        const uint8_t *media_info = a2dp_subevent_signaling_media_codec_other_capability_get_media_codec_information(packet);
        uint32_t vendor_id = get_vendor_id(media_info);
        uint16_t codec_id = get_codec_id(media_info);
        sc.local_stream_endpoint = avdtp_get_source_stream_endpoint_for_media_codec_other(vendor_id, codec_id);
    } else {
        sc.local_stream_endpoint = avdtp_get_source_stream_endpoint_for_media_codec(codec_type);
    }
    if (sc.local_stream_endpoint == NULL){
        printf("No local endpoint provided with codec %s\n", codec_name_for_type(codec_type));
        return false;
    }
    // store local seid
    media_tracker.local_seid  = avdtp_local_seid(sc.local_stream_endpoint);
    // set media configuration
    sc.local_stream_endpoint->remote_configuration_bitmap = store_bit16(sc.local_stream_endpoint->remote_configuration_bitmap, AVDTP_MEDIA_CODEC, 1);
    sc.local_stream_endpoint->remote_configuration.media_codec.media_type = AVDTP_AUDIO;
    sc.local_stream_endpoint->remote_configuration.media_codec.media_codec_type = codec_type;
    switch (codec_type){
        case AVDTP_CODEC_SBC:
            setup_sbc_codec_config(local_remote_seid_index);
            break;
        case AVDTP_CODEC_MPEG_1_2_AUDIO:
            setup_mpeg_audio_codec_config(local_remote_seid_index);
            break;
        case AVDTP_CODEC_MPEG_2_4_AAC:
            setup_mpeg_aac_codec_config(local_remote_seid_index);
            break;
        case AVDTP_CODEC_ATRAC_FAMILY:
            setup_atrac_codec_config(local_remote_seid_index);
            break;
        case AVDTP_CODEC_NON_A2DP:
            setup_non_a2dp_codec_config(local_remote_seid_index);
            break;
        default:
            printf("Setup config for codec %s not implemented yet\n", codec_name_for_type(codec_type));
            return false;
    }
    return true;
}

#ifdef HAVE_AAC_FDK
static int convert_aac_object_type(int bitmap) {
    switch (bitmap) {
        case AVDTP_AAC_MPEG4_SCALABLE:
            return AOT_AAC_SCAL;
        case AVDTP_AAC_MPEG4_LTP:
            return AOT_AAC_LTP;
        case AVDTP_AAC_MPEG4_LC:
            return AOT_AAC_LC;
        case AVDTP_AAC_MPEG2_LC:
            // https://lists.freedesktop.org/archives/gstreamer-commits/2016-September/096332.html
            return AOT_AAC_LC;
        default:
            printf("invalid aac aot config %d\n", bitmap);
            return 0;
    }
}

static int convert_aac_vbr(int vbr) {
    if (vbr)
        return 4;
    else
        return 0;
}
#endif

static int convert_ldac_sampling_frequency(uint8_t frequency_bitmap) {
    switch (frequency_bitmap) {
    case 1 << 0:
        return 192000;
    case 1 << 1:
        return 176400;
    case 1 << 2:
        return 96000;
    case 1 << 3:
        return 88200;
    case 1 << 4:
        return 48000;
    case 1 << 5:
        return 44100;
    default:
        printf("invalid ldac sampling frequency %d\n", frequency_bitmap);
        return 0;
    }
}

static int convert_ldac_num_channels(uint8_t channel_mode) {
    switch (channel_mode) {
    case 1 << 0: // stereo
    case 1 << 1: // dual channel
        return 2;
    case 1 << 2:
        return 1;
    default:
        printf("error ldac channel mode\n");
        return 0;
    }
}

static int convert_aptx_sampling_frequency(uint8_t frequency_bitmap) {
    switch (frequency_bitmap) {
    case 1 << 4:
        return 48000;
    case 1 << 5:
        return 44100;
    case 1 << 6:
        return 32000;
    case 1 <<7:
        return 16000;
    default:
        printf("invalid aptx sampling frequency %d\n", frequency_bitmap);
        return 0;
    }
}

static int convert_aptx_num_channels(uint8_t channel_mode) {
    switch (channel_mode) {
    case 1 << 0:
    case 1 << 1:
    case 1 << 2:
        return 2;
    case 1 << 3:
        return 1;
    default:
        printf("invalid aptx channel mode %d\n", channel_mode);
        return 0;
    }
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_AVDTP_META) return;
    UNUSED(channel);
    UNUSED(size);

    uint8_t signal_identifier;
    uint8_t status;
    avdtp_sep_t sep;
    uint16_t avdtp_cid;
    uint8_t remote_seid;
    int local_remote_seid_index;
    uint32_t vendor_id;
    uint16_t codec_id;

    switch (packet[2]){
        case AVDTP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED:
            avdtp_cid = avdtp_subevent_signaling_connection_established_get_avdtp_cid(packet);
            status = avdtp_subevent_signaling_connection_established_get_status(packet);
            if (status != 0){
                printf("AVDTP source signaling connection failed: status %d\n", status);
                break;
            }
            media_tracker.avdtp_cid = avdtp_subevent_signaling_connection_established_get_avdtp_cid(packet);
            printf("AVDTP source signaling connection established: avdtp_cid 0x%02x\n", avdtp_cid);
            break;
        
        case AVDTP_SUBEVENT_STREAMING_CONNECTION_ESTABLISHED:
            status = avdtp_subevent_streaming_connection_established_get_status(packet);
            if (status != 0){
                printf("Streaming connection failed: status %d\n", status);
                break;
            }
            avdtp_cid = avdtp_subevent_streaming_connection_established_get_avdtp_cid(packet);
            printf("Streaming connection established, avdtp_cid 0x%02x\n", avdtp_cid);
            break;

        case AVDTP_SUBEVENT_SIGNALING_SEP_FOUND:
            memset(&sep, 0, sizeof(avdtp_sep_t));
            sep.seid = avdtp_subevent_signaling_sep_found_get_remote_seid(packet);;
            sep.in_use = avdtp_subevent_signaling_sep_found_get_in_use(packet);
            sep.media_type = avdtp_subevent_signaling_sep_found_get_media_type(packet);
            sep.type = avdtp_subevent_signaling_sep_found_get_sep_type(packet);
            printf("Found sep: seid %u, in_use %d, media type %d, sep type %d (1-SNK)\n", sep.seid, sep.in_use, sep.media_type, sep.type);
            if (sep.type == AVDTP_SINK) {
                remote_seps[num_remote_seps].sep = sep;
                num_remote_seps++;
            }
            break;

        case AVDTP_SUBEVENT_SIGNALING_SEP_DICOVERY_DONE:
            // select remote if there's only a single remote
            if (num_remote_seps == 1){
                media_tracker.remote_seid = remote_seps[0].sep.seid;
                printf("Only one remote Stream Endpoint with SEID %u, select it for initiator commands\n", media_tracker.remote_seid);
            }
            break;

        case AVDTP_SUBEVENT_SIGNALING_MEDIA_TRANSPORT_CAPABILITY:
            printf("CAPABILITY - MEDIA_TRANSPORT supported on remote.\n");
            break;
        case AVDTP_SUBEVENT_SIGNALING_REPORTING_CAPABILITY:
            printf("CAPABILITY - REPORTING supported on remote.\n");
            break;
        case AVDTP_SUBEVENT_SIGNALING_RECOVERY_CAPABILITY:
            printf("CAPABILITY - RECOVERY supported on remote: \n");
            printf("    - recovery_type                %d\n", avdtp_subevent_signaling_recovery_capability_get_recovery_type(packet));
            printf("    - maximum_recovery_window_size %d\n", avdtp_subevent_signaling_recovery_capability_get_maximum_recovery_window_size(packet));
            printf("    - maximum_number_media_packets %d\n", avdtp_subevent_signaling_recovery_capability_get_maximum_number_media_packets(packet));
            break;
        case AVDTP_SUBEVENT_SIGNALING_CONTENT_PROTECTION_CAPABILITY:
            printf("CAPABILITY - CONTENT_PROTECTION supported on remote: \n");
            printf("    - cp_type           %d\n", avdtp_subevent_signaling_content_protection_capability_get_cp_type(packet));
            printf("    - cp_type_value_len %d\n", avdtp_subevent_signaling_content_protection_capability_get_cp_type_value_len(packet));
            printf("    - cp_type_value     \'%s\'\n", avdtp_subevent_signaling_content_protection_capability_get_cp_type_value(packet));
            break;
        case AVDTP_SUBEVENT_SIGNALING_MULTIPLEXING_CAPABILITY:
            printf("CAPABILITY - MULTIPLEXING supported on remote: \n");
            printf("    - fragmentation                  %d\n", avdtp_subevent_signaling_multiplexing_capability_get_fragmentation(packet));
            printf("    - transport_identifiers_num      %d\n", avdtp_subevent_signaling_multiplexing_capability_get_transport_identifiers_num(packet));
            printf("    - transport_session_identifier_1 %d\n", avdtp_subevent_signaling_multiplexing_capability_get_transport_session_identifier_1(packet));
            printf("    - transport_session_identifier_2 %d\n", avdtp_subevent_signaling_multiplexing_capability_get_transport_session_identifier_2(packet));
            printf("    - transport_session_identifier_3 %d\n", avdtp_subevent_signaling_multiplexing_capability_get_transport_session_identifier_3(packet));
            printf("    - tcid_1                         %d\n", avdtp_subevent_signaling_multiplexing_capability_get_tcid_1(packet));
            printf("    - tcid_2                         %d\n", avdtp_subevent_signaling_multiplexing_capability_get_tcid_2(packet));
            printf("    - tcid_3                         %d\n", avdtp_subevent_signaling_multiplexing_capability_get_tcid_3(packet));
            break;
        case AVDTP_SUBEVENT_SIGNALING_DELAY_REPORTING_CAPABILITY:
            printf("CAPABILITY - DELAY_REPORTING supported on remote.\n");
            break;
        case AVDTP_SUBEVENT_SIGNALING_DELAY_REPORT:
            printf("DELAY_REPORT received: %d.%0d ms, local seid %d\n", 
                avdtp_subevent_signaling_delay_report_get_delay_100us(packet)/10, avdtp_subevent_signaling_delay_report_get_delay_100us(packet)%10,
                avdtp_subevent_signaling_delay_report_get_local_seid(packet));
            break;
        case AVDTP_SUBEVENT_SIGNALING_HEADER_COMPRESSION_CAPABILITY:
            printf("CAPABILITY - HEADER_COMPRESSION supported on remote: \n");
            printf("    - back_ch   %d\n", avdtp_subevent_signaling_header_compression_capability_get_back_ch(packet));
            printf("    - media     %d\n", avdtp_subevent_signaling_header_compression_capability_get_media(packet));
            printf("    - recovery  %d\n", avdtp_subevent_signaling_header_compression_capability_get_recovery(packet));
            break;

        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CAPABILITY:{
            // cache
            remote_seid = avdtp_subevent_signaling_media_codec_sbc_capability_get_remote_seid(packet);
            local_remote_seid_index = find_remote_seid(remote_seid);
            btstack_assert(local_remote_seid_index >= 0);
            (void) memcpy(remote_seps[local_remote_seid_index].media_codec_event, packet, size);
            remote_seps[local_remote_seid_index].sep.capabilities.media_codec.media_codec_type = AVDTP_CODEC_SBC;
            remote_seps[local_remote_seid_index].have_media_codec_apabilities = true;

            printf("CAPABILITY - MEDIA_CODEC: SBC, remote seid %u: \n", remote_seid);

            media_codec_information_sbc_t   sbc_capability;
            sbc_capability.sampling_frequency_bitmap = avdtp_subevent_signaling_media_codec_sbc_capability_get_sampling_frequency_bitmap(packet);
            sbc_capability.channel_mode_bitmap = avdtp_subevent_signaling_media_codec_sbc_capability_get_channel_mode_bitmap(packet);
            sbc_capability.block_length_bitmap = avdtp_subevent_signaling_media_codec_sbc_capability_get_block_length_bitmap(packet);
            sbc_capability.subbands_bitmap = avdtp_subevent_signaling_media_codec_sbc_capability_get_subbands_bitmap(packet);
            sbc_capability.allocation_method_bitmap = avdtp_subevent_signaling_media_codec_sbc_capability_get_allocation_method_bitmap(packet);
            sbc_capability.min_bitpool_value = avdtp_subevent_signaling_media_codec_sbc_capability_get_min_bitpool_value(packet);
            sbc_capability.max_bitpool_value = avdtp_subevent_signaling_media_codec_sbc_capability_get_max_bitpool_value(packet);
            dump_sbc_capability(sbc_capability);

            // pre-select SBC codec, can be overwritten for other codecs
            selected_remote_sep_index = local_remote_seid_index;
            break;
        }

        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AUDIO_CAPABILITY:
            // cache
            remote_seid = avdtp_subevent_signaling_media_codec_sbc_capability_get_remote_seid(packet);
            local_remote_seid_index = find_remote_seid(remote_seid);
            btstack_assert(local_remote_seid_index >= 0);
            (void) memcpy(remote_seps[local_remote_seid_index].media_codec_event, packet, size);
            remote_seps[local_remote_seid_index].sep.capabilities.media_codec.media_codec_type = AVDTP_CODEC_MPEG_1_2_AUDIO;
            remote_seps[local_remote_seid_index].have_media_codec_apabilities = true;
            printf("CAPABILITY - MEDIA_CODEC: MPEG AUDIO, remote seid %u: \n", remote_seid);
            break;

        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AAC_CAPABILITY: {
            // cache
            remote_seid = avdtp_subevent_signaling_media_codec_sbc_capability_get_remote_seid(packet);
            local_remote_seid_index = find_remote_seid(remote_seid);
            btstack_assert(local_remote_seid_index >= 0);
            (void) memcpy(remote_seps[local_remote_seid_index].media_codec_event, packet, size);
            remote_seps[local_remote_seid_index].sep.capabilities.media_codec.media_codec_type = AVDTP_CODEC_MPEG_2_4_AAC;
            remote_seps[local_remote_seid_index].have_media_codec_apabilities = true;
            printf("CAPABILITY - MEDIA_CODEC: MPEG AAC, remote seid %u: \n", remote_seid);

            avdtp_media_codec_capabilities_aac_t aac_capabilities;
            aac_capabilities.sampling_frequency_bitmap = a2dp_subevent_signaling_media_codec_mpeg_aac_capability_get_sampling_frequency_bitmap(packet);
            aac_capabilities.object_type_bitmap = a2dp_subevent_signaling_media_codec_mpeg_aac_capability_get_object_type_bitmap(packet);
            aac_capabilities.channels_bitmap = a2dp_subevent_signaling_media_codec_mpeg_aac_capability_get_channels_bitmap(packet);
            aac_capabilities.bit_rate = a2dp_subevent_signaling_media_codec_mpeg_aac_capability_get_bit_rate(packet);
            aac_capabilities.vbr = a2dp_subevent_signaling_media_codec_mpeg_aac_capability_get_vbr(packet);
            printf("A2DP Source: Received AAC capabilities! Sampling frequency bitmap: 0x%04x, object type %u, channel mode %u, bitrate %u, vbr: %u\n",
                   aac_capabilities.sampling_frequency_bitmap, aac_capabilities.object_type_bitmap, aac_capabilities.channels_bitmap,
                   aac_capabilities.bit_rate, aac_capabilities.vbr);
            break;
        }

        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_ATRAC_CAPABILITY:
            // cache
            remote_seid = avdtp_subevent_signaling_media_codec_sbc_capability_get_remote_seid(packet);
            local_remote_seid_index = find_remote_seid(remote_seid);
            btstack_assert(local_remote_seid_index >= 0);
            (void) memcpy(remote_seps[local_remote_seid_index].media_codec_event, packet, size);
            remote_seps[local_remote_seid_index].sep.capabilities.media_codec.media_codec_type = AVDTP_CODEC_ATRAC_FAMILY;
            remote_seps[local_remote_seid_index].have_media_codec_apabilities = true;
            printf("CAPABILITY - MEDIA_CODEC: ATRAC, remote seid %u: \n", remote_seid);
            break;

        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CAPABILITY:
            // cache
            remote_seid = avdtp_subevent_signaling_media_codec_sbc_capability_get_remote_seid(packet);
            local_remote_seid_index = find_remote_seid(remote_seid);
            btstack_assert(local_remote_seid_index >= 0);
            (void) memcpy(remote_seps[local_remote_seid_index].media_codec_event, packet, size);
            remote_seps[local_remote_seid_index].sep.capabilities.media_codec.media_codec_type  = AVDTP_CODEC_NON_A2DP;
            remote_seps[local_remote_seid_index].have_media_codec_apabilities = true;
            const uint8_t *media_info = avdtp_subevent_signaling_media_codec_other_capability_get_media_codec_information(packet);
            vendor_id = get_vendor_id(media_info);
            codec_id = get_codec_id(media_info);

            if (vendor_id == A2DP_CODEC_VENDOR_ID_SONY && codec_id == A2DP_SONY_CODEC_LDAC)
                printf("CAPABILITY - LDAC, remote seid %u\n", remote_seid);
            else if (vendor_id == A2DP_CODEC_VENDOR_ID_APT_LTD && codec_id == A2DP_APT_LTD_CODEC_APTX)
                printf("CAPABILITY - APTX, remote seid %u\n", remote_seid);
            else if (vendor_id == A2DP_CODEC_VENDOR_ID_QUALCOMM && codec_id == A2DP_QUALCOMM_CODEC_APTX_HD)
                printf("CAPABILITY - APTX HD, remote seid %u\n", remote_seid);
            else
                printf("CAPABILITY - MEDIA_CODEC: OTHER, remote seid %u: \n", remote_seid);

            break;

        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION:{
            printf("Set configuration and init encoder\n");
            avdtp_media_codec_configuration_sbc_t sbc_configuration;
            sbc_configuration.reconfigure = avdtp_subevent_signaling_media_codec_sbc_configuration_get_reconfigure(packet);
            sbc_configuration.num_channels = avdtp_subevent_signaling_media_codec_sbc_configuration_get_num_channels(packet);
            sbc_configuration.sampling_frequency = avdtp_subevent_signaling_media_codec_sbc_configuration_get_sampling_frequency(packet);
            sbc_configuration.block_length = avdtp_subevent_signaling_media_codec_sbc_configuration_get_block_length(packet);
            sbc_configuration.subbands = avdtp_subevent_signaling_media_codec_sbc_configuration_get_subbands(packet);
            sbc_configuration.min_bitpool_value = avdtp_subevent_signaling_media_codec_sbc_configuration_get_min_bitpool_value(packet);
            sbc_configuration.max_bitpool_value = avdtp_subevent_signaling_media_codec_sbc_configuration_get_max_bitpool_value(packet);
            
            avdtp_channel_mode_t channel_mode = (avdtp_channel_mode_t) avdtp_subevent_signaling_media_codec_sbc_configuration_get_channel_mode(packet);
            avdtp_sbc_allocation_method_t allocation_method = (avdtp_sbc_allocation_method_t) avdtp_subevent_signaling_media_codec_sbc_configuration_get_allocation_method(packet);
            
            // Map Bluetooth spec definition to SBC Encoder expected input
            sbc_configuration.allocation_method = (btstack_sbc_allocation_method_t)(((uint8_t) allocation_method) - 1);
            switch (channel_mode){
                case AVDTP_CHANNEL_MODE_JOINT_STEREO:
                    sbc_configuration.channel_mode = SBC_CHANNEL_MODE_JOINT_STEREO;
                    sbc_configuration.num_channels = 2;
                    break;
                case AVDTP_CHANNEL_MODE_STEREO:
                    sbc_configuration.channel_mode = SBC_CHANNEL_MODE_STEREO;
                    sbc_configuration.num_channels = 2;
                    break;
                case AVDTP_CHANNEL_MODE_DUAL_CHANNEL:
                    sbc_configuration.channel_mode = SBC_CHANNEL_MODE_DUAL_CHANNEL;
                    sbc_configuration.num_channels = 2;
                    break;
                case AVDTP_CHANNEL_MODE_MONO:
                    sbc_configuration.channel_mode = SBC_CHANNEL_MODE_MONO;
                    sbc_configuration.num_channels = 1;
                    break;
                default:
                    btstack_assert(false);
                    break;
            }
            dump_sbc_configuration(sbc_configuration);

            configure_sample_rate(sc.sampling_frequency);
            btstack_sbc_encoder_init(&sbc_encoder_state, SBC_MODE_STANDARD, 
                sbc_configuration.block_length, sbc_configuration.subbands, 
                sbc_configuration.allocation_method, sbc_configuration.sampling_frequency, 
                sbc_configuration.max_bitpool_value,
                sbc_configuration.channel_mode);

            break;
        }

        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AAC_CONFIGURATION: {
            printf("Set configuration and init encoder\n");
            avdtp_configuration_mpeg_aac_t aac_configuration;
            // avdtp_media_codec_configuration_aac_t aac_configuration;
            aac_configuration.sampling_frequency = a2dp_subevent_signaling_media_codec_mpeg_aac_configuration_get_sampling_frequency(
                    packet);
            aac_configuration.object_type = a2dp_subevent_signaling_media_codec_mpeg_aac_configuration_get_object_type(
                    packet);
            aac_configuration.channels = a2dp_subevent_signaling_media_codec_mpeg_aac_configuration_get_num_channels(
                    packet);
            aac_configuration.bit_rate = a2dp_subevent_signaling_media_codec_mpeg_aac_configuration_get_bit_rate(
                    packet);
            aac_configuration.vbr = a2dp_subevent_signaling_media_codec_mpeg_aac_configuration_get_vbr(packet);

            printf("A2DP Source: Received AAC configuration! Sampling frequency: %u, object type %u, channel mode %u, bitrate %u, vbr: %u\n",
                   aac_configuration.sampling_frequency, aac_configuration.object_type, aac_configuration.channels,
                   aac_configuration.bit_rate, aac_configuration.vbr);

            int aot = convert_aac_object_type(aac_configuration.object_type);
            int vbr = convert_aac_vbr(aac_configuration.vbr);

            // init encoder
            AACENC_ERROR err;
            if ((err = aacEncOpen(&handleAAC, 0x07, aac_configuration.channels)) != AACENC_OK) {
                printf("Couldn't open AAC encoder: %d\n", err);
                break;
            }
            if ((err = aacEncoder_SetParam(handleAAC, AACENC_AOT, aot)) != AACENC_OK) {
                printf("Couldn't set audio object type: %d\n", err);
                break;
            }
            if ((err = aacEncoder_SetParam(handleAAC, AACENC_BITRATE, aac_configuration.bit_rate)) != AACENC_OK) {
                printf("Couldn't set bitrate: %d\n", err);
                break;
            }
            if ((err = aacEncoder_SetParam(handleAAC, AACENC_SAMPLERATE, aac_configuration.sampling_frequency)) !=
                AACENC_OK) {
                printf("Couldn't set sampling rate: %d\n", err);
                break;
            }
            if ((err = aacEncoder_SetParam(handleAAC, AACENC_CHANNELMODE, aac_configuration.channels)) != AACENC_OK) {
                printf("Couldn't set channel mode: %d\n", err);
                break;
            }
            if (aac_configuration.vbr) {
                if ((err = aacEncoder_SetParam(handleAAC, AACENC_BITRATEMODE, vbr)) != AACENC_OK) {
                    printf("Couldn't set VBR bitrate mode %u: %d\n", aac_configuration.vbr, err);
                    break;
                }
            }
            if ((err = aacEncoder_SetParam(handleAAC, AACENC_AFTERBURNER, false)) != AACENC_OK) {
                printf("Couldn't enable afterburner: %d\n", err);
                break;
            }
            if ((err = aacEncoder_SetParam(handleAAC, AACENC_TRANSMUX, TT_MP4_LATM_MCP1)) != AACENC_OK) {
                printf("Couldn't enable LATM transport type: %d\n", err);
                break;
            }
            if ((err = aacEncoder_SetParam(handleAAC, AACENC_HEADER_PERIOD, 1)) != AACENC_OK) {
                printf("Couldn't set LATM header period: %d\n", err);
                break;
            }
            if ((err = aacEncoder_SetParam(handleAAC, AACENC_AUDIOMUXVER, 1)) != AACENC_OK) {
                printf("Couldn't set LATM version: %d\n", err);
                break;
            }
            if ((err = aacEncEncode(handleAAC, NULL, NULL, NULL, NULL)) != AACENC_OK) {
                printf("Couldn't initialize AAC encoder: %d\n", err);
                break;
            }
            if ((err = aacEncInfo(handleAAC, &aacinf)) != AACENC_OK) {
                printf("Couldn't get encoder info: %d\n", err);
                break;
            }
            current_sample_rate = aac_configuration.sampling_frequency;
            break;
        }

        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AUDIO_CONFIGURATION:
        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_ATRAC_CONFIGURATION:
            // TODO: handle other configuration event
            printf("Config not handled for %s\n", codec_name_for_type(remote_seps[selected_remote_sep_index].sep.capabilities.media_codec.media_codec_type));
            break;
        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CONFIGURATION:
            printf("Received other configuration\n");
            uint8_t *codec_info = (uint8_t *) a2dp_subevent_signaling_media_codec_other_configuration_get_media_codec_information(packet);

            vendor_id = get_vendor_id(codec_info);
            codec_id = get_codec_id(codec_info);

            // LDAC
            if (vendor_id == A2DP_CODEC_VENDOR_ID_SONY && codec_id == A2DP_SONY_CODEC_LDAC) {
#ifdef HAVE_LDAC_ENCODER
                ldac_configuration.reconfigure = a2dp_subevent_signaling_media_codec_other_configuration_get_reconfigure(packet);
                ldac_configuration.sampling_frequency = codec_info[6];
                ldac_configuration.channel_mode = codec_info[7];
                ldac_configuration.sampling_frequency = convert_ldac_sampling_frequency(ldac_configuration.sampling_frequency);
                ldac_configuration.num_channels = convert_ldac_num_channels(ldac_configuration.channel_mode);
                printf("A2DP Source: Received LDAC configuration! Sampling frequency: %d, channel mode: %d channels: %d\n",
                        ldac_configuration.sampling_frequency, ldac_configuration.channel_mode, ldac_configuration.num_channels);

                handleLDAC = ldacBT_get_handle();
                if (handleLDAC == NULL) {
                    printf("Failed to get LDAC handle\n");
                    break;
                }

                // init ldac encoder
                int mtu = 679; // minimal required mtu
                if (ldacBT_init_handle_encode(handleLDAC, mtu, LDACBT_EQMID_SQ, ldac_configuration.channel_mode,
                            LDACBT_SMPL_FMT_S16, ldac_configuration.sampling_frequency) == -1) {
                    printf("Couldn't initialize LDAC encoder: %d\n", ldacBT_get_error_code(handleLDAC));
                    break;
                }

                current_sample_rate = ldac_configuration.sampling_frequency;
#endif
#ifdef HAVE_APTX
            } else if (vendor_id == A2DP_CODEC_VENDOR_ID_APT_LTD && codec_id == A2DP_APT_LTD_CODEC_APTX) {
                aptx_configuration.reconfigure = a2dp_subevent_signaling_media_codec_other_configuration_get_reconfigure(packet);
                aptx_configuration.sampling_frequency = codec_info[6] & 0xF0;
                aptx_configuration.channel_mode = codec_info[6] & 0x0F;
                aptx_configuration.sampling_frequency = convert_aptx_sampling_frequency(aptx_configuration.sampling_frequency);
                aptx_configuration.num_channels = convert_aptx_num_channels(aptx_configuration.channel_mode);
                printf("A2DP Source: Received APTX configuration! Sampling frequency: %d, channel mode: %d channels: %d\n",
                        aptx_configuration.sampling_frequency, aptx_configuration.channel_mode, aptx_configuration.num_channels);

                aptx_handle = aptx_init(0);
                current_sample_rate = aptx_configuration.sampling_frequency;
            } else if (vendor_id == A2DP_CODEC_VENDOR_ID_QUALCOMM && codec_id == A2DP_QUALCOMM_CODEC_APTX_HD) {
                aptxhd_configuration.reconfigure = a2dp_subevent_signaling_media_codec_other_configuration_get_reconfigure(packet);
                aptxhd_configuration.sampling_frequency = codec_info[6] & 0xF0;
                aptxhd_configuration.channel_mode = codec_info[6] & 0x0F;
                aptxhd_configuration.sampling_frequency = convert_aptx_sampling_frequency(aptxhd_configuration.sampling_frequency);
                aptxhd_configuration.num_channels = convert_aptx_num_channels(aptxhd_configuration.channel_mode);
                printf("A2DP Source: Received APTX HD configuration! Sampling frequency: %d, channel mode: %d channels: %d\n",
                        aptxhd_configuration.sampling_frequency, aptxhd_configuration.channel_mode, aptxhd_configuration.num_channels);

                aptx_handle = aptx_init(1);
                current_sample_rate = aptxhd_configuration.sampling_frequency;
#endif
            } else {
                printf("Config not handled for %s\n", codec_name_for_type(remote_seps[selected_remote_sep_index].sep.capabilities.media_codec.media_codec_type));
            }
            break;

        case AVDTP_SUBEVENT_STREAMING_CAN_SEND_MEDIA_PACKET_NOW:
            a2dp_demo_send_media_packet();
            break;  


        case AVDTP_SUBEVENT_SIGNALING_ACCEPT:
            // TODO check cid
            signal_identifier = avdtp_subevent_signaling_accept_get_signal_identifier(packet);
            if (is_cmd_triggered_localy){
                is_cmd_triggered_localy = 0;
                printf("AVDTP Source command accepted\n");
            }
            
            switch (signal_identifier){
                case AVDTP_SI_OPEN:
                    break;
                case AVDTP_SI_SET_CONFIGURATION:
                    break;
                case  AVDTP_SI_START:
                    printf("Stream started.\n");
                    a2dp_demo_timer_start(&media_tracker);
                    break;
                case AVDTP_SI_SUSPEND:
                    printf("Stream paused.\n");
                    a2dp_demo_timer_pause(&media_tracker);
                    break;
                case AVDTP_SI_ABORT:
                case AVDTP_SI_CLOSE:
                    printf("Stream released.\n");
                    a2dp_demo_timer_stop(&media_tracker);
                    break;
                default:
                    break;
            }
            break;

        case AVDTP_SUBEVENT_SIGNALING_REJECT:
            signal_identifier = avdtp_subevent_signaling_reject_get_signal_identifier(packet);
            printf("Rejected %s\n", avdtp_si2str(signal_identifier));
            break;
        case AVDTP_SUBEVENT_SIGNALING_GENERAL_REJECT:
            signal_identifier = avdtp_subevent_signaling_general_reject_get_signal_identifier(packet);
            printf("Rejected %s\n", avdtp_si2str(signal_identifier));
            break;
        case AVDTP_SUBEVENT_STREAMING_CONNECTION_RELEASED:
            a2dp_demo_timer_stop(&media_tracker);
            printf("Streaming connection released.\n");
            break;
        case AVDTP_SUBEVENT_SIGNALING_CONNECTION_RELEASED:
            a2dp_demo_timer_stop(&media_tracker);
            printf("Signaling connection released.\n");
            break;
        default:
            break;
    }
}

#ifdef HAVE_BTSTACK_STDIN

static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- Bluetooth AVDTP SOURCE Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("c      - create connection to addr %s\n", device_addr_string);
    printf("C      - disconnect\n");
    printf("d      - discover stream endpoints\n");
    printf("z      - select remote endpoint\n");
    printf("g      - get capabilities     for remote seid %u\n", media_tracker.remote_seid);
    printf("a      - get all capabilities for remote seid %u\n", media_tracker.remote_seid);
    printf("s      - set configuration    for remote seid %u\n", media_tracker.remote_seid);
    printf("f      - get configuration    for remote seid %u\n", media_tracker.remote_seid);
    printf("R      - reconfigure stream   for remote seid %u\n", media_tracker.remote_seid);
    printf("o      - open stream          for remote seid %u\n", media_tracker.remote_seid);
    printf("m      - start stream         for remote seid %u\n", media_tracker.remote_seid);
    printf("A      - abort stream         for remote seid %u\n", media_tracker.remote_seid);
    printf("S      - stop stream          for remote seid %u\n", media_tracker.remote_seid);
    printf("P      - suspend stream       for remote seid %u\n", media_tracker.remote_seid);
    printf("X      - stop streaming sine\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}


static void stdin_process(char cmd){
    uint8_t status = ERROR_CODE_SUCCESS;
    is_cmd_triggered_localy = 1;
    static bool enter_remote_seid_index = false;

    if (enter_remote_seid_index){
        if ((cmd < '0') || (cmd > '9')) return;
        uint8_t index = cmd - '0';
        if (index >= num_remote_seps){
            printf("Index too high, try again\n");
            return;
        }
        enter_remote_seid_index = false;
        selected_remote_sep_index = index;
        media_tracker.remote_seid = remote_seps[selected_remote_sep_index].sep.seid;
        printf("Selected Remote Stream Endpoint with SEID %u\n",  media_tracker.remote_seid);
        return;
    }

    switch (cmd){
        case 'c':
            printf("Establish AVDTP Source connection to %s\n", device_addr_string);
            status = avdtp_source_connect(device_addr, &media_tracker.avdtp_cid);
            break;
        case 'C':
            printf("Disconnect AVDTP Source\n");
            status = avdtp_source_disconnect(media_tracker.avdtp_cid);
            break;
        case 'd':
            printf("Discover stream endpoints of %s\n", device_addr_string);
            num_remote_seps = 0;
            selected_remote_sep_index = 0;
            status = avdtp_source_discover_stream_endpoints(media_tracker.avdtp_cid);
            break;
        case 'g':
            if (num_remote_seps == 0){
                printf("Remote Stream Endpoints not discovered yet, please discover stream endpoints first\n");
                break;
            }
            printf("Get capabilities of stream endpoint with seid %d\n", media_tracker.remote_seid);
            status = avdtp_source_get_capabilities(media_tracker.avdtp_cid, media_tracker.remote_seid);
            break;
        case 'a':
            printf("Get all capabilities of stream endpoint with seid %d\n", media_tracker.remote_seid);
            status = avdtp_source_get_all_capabilities(media_tracker.avdtp_cid, media_tracker.remote_seid);
            break;
        case 'f':
            printf("Get configuration of stream endpoint with seid %d\n", media_tracker.remote_seid);
            status = avdtp_source_get_configuration(media_tracker.avdtp_cid, media_tracker.remote_seid);
            break;
        case 'z':
            if (num_remote_seps == 0){
                printf("Remote Stream Endpoints not discovered yet, please discover stream endpoints first\n");
                break;
            }
            dump_remote_sink_endpoints();
            printf("Please enter index of remote stream endpoint:\n");
            enter_remote_seid_index = true;
            break;

        case 's':{
            if (num_remote_seps == 0){
                printf("Remote Stream Endpoints not discovered yet, please discover stream endpoints first\n");
                break;
            }
            if (remote_seps[selected_remote_sep_index].have_media_codec_apabilities == false){
                printf("Remote Stream Endpoints Media Codec Capabilities not received yet, please get (all) capabilities for stream endpoint with seid %u first\n",  media_tracker.remote_seid);
                break;
            }
            setup_remote_config(selected_remote_sep_index);
            printf("Set configuration of stream endpoint with seid %d\n", media_tracker.remote_seid);
            avdtp_capabilities_t new_configuration;
            new_configuration.media_codec.media_type = AVDTP_AUDIO;
            new_configuration.media_codec.media_codec_type = remote_seps[selected_remote_sep_index].sep.capabilities.media_codec.media_codec_type ;
            new_configuration.media_codec.media_codec_information_len = media_codec_config_len;
            new_configuration.media_codec.media_codec_information = media_codec_config_data;
            status = avdtp_source_set_configuration(media_tracker.avdtp_cid, media_tracker.local_seid, media_tracker.remote_seid, 1 << AVDTP_MEDIA_CODEC, new_configuration);
            break;
        }
        case 'R':{
            if (num_remote_seps == 0){
                printf("Remote Stream Endpoints not discovered yet, please discover stream endpoints first\n");
                break;
            }
            if (remote_seps[selected_remote_sep_index].have_media_codec_apabilities == false){
                printf("Remote Stream Endpoints Media Codec Capabilities not received yet, please get (all) capabilities for stream endpoint with seid %u first\n",  media_tracker.remote_seid);
                break;
            }
            printf("Reconfigure stream endpoint with seid %d\n", media_tracker.remote_seid);
            avdtp_media_codec_type_t media_codec_type = remote_seps[selected_remote_sep_index].sep.capabilities.media_codec.media_codec_type;
            avdtp_capabilities_t new_configuration;
            new_configuration.media_codec.media_type = AVDTP_AUDIO;

            uint16_t new_sampling_frequency = 44100;
            switch (current_sample_rate){
                case 44100:
                    new_sampling_frequency = 48000;
                    break;
                default:
                    break;
            }

            switch (media_codec_type){
                case AVDTP_CODEC_SBC:
                    avdtp_config_sbc_set_sampling_frequency(media_codec_config_data, new_sampling_frequency);
                    break;
                case AVDTP_CODEC_MPEG_1_2_AUDIO:
                    avdtp_config_mpeg_audio_set_sampling_frequency(media_codec_config_data, new_sampling_frequency);
                    break;
                case AVDTP_CODEC_MPEG_2_4_AAC:
                    avdtp_config_mpeg_aac_set_sampling_frequency(media_codec_config_data, new_sampling_frequency);
                    break;
                case AVDTP_CODEC_ATRAC_FAMILY:
                    avdtp_config_atrac_set_sampling_frequency(media_codec_config_data, new_sampling_frequency);
                    break;
                default:
                    printf("Reconfigure not implemented for %s\n", codec_name_for_type(media_codec_type));
                    return;
            }
            new_configuration.media_codec.media_type = AVDTP_AUDIO;
            new_configuration.media_codec.media_codec_type = media_codec_type;
            new_configuration.media_codec.media_codec_information_len = media_codec_config_len;
            new_configuration.media_codec.media_codec_information = media_codec_config_data;
            status = avdtp_source_reconfigure(media_tracker.avdtp_cid, media_tracker.local_seid, media_tracker.remote_seid, 1 << AVDTP_MEDIA_CODEC, new_configuration);
            break;
        }
        case 'o':
            printf("Establish stream between local %d and remote %d seid\n", media_tracker.local_seid, media_tracker.remote_seid);
            status = avdtp_source_open_stream(media_tracker.avdtp_cid, media_tracker.local_seid, media_tracker.remote_seid);
            break;
        case 'm': 
            printf("Start stream between local %d and remote %d seid, \n", media_tracker.local_seid, media_tracker.remote_seid);
            status = avdtp_source_start_stream(media_tracker.avdtp_cid, media_tracker.local_seid);
            break;
        case 'A':
            printf("Abort stream between local %d and remote %d seid\n", media_tracker.local_seid, media_tracker.remote_seid);
            status = avdtp_source_abort_stream(media_tracker.avdtp_cid, media_tracker.local_seid);
            break;
        case 'S':
            printf("Release stream between local %d and remote %d seid\n", media_tracker.local_seid, media_tracker.remote_seid);
            status = avdtp_source_stop_stream(media_tracker.avdtp_cid, media_tracker.local_seid);
            break;
        case 'P':
            printf("Susspend stream between local %d and remote %d seid\n", media_tracker.local_seid, media_tracker.remote_seid);
            status = avdtp_source_suspend(media_tracker.avdtp_cid, media_tracker.local_seid);
            break;
        case 'X':
            printf("Stop streaming\n");
            status = avdtp_source_stop_stream(media_tracker.avdtp_cid, media_tracker.local_seid);
            break;
            
        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;

    }
    if (status != ERROR_CODE_SUCCESS){
        printf("AVDTP Sink cmd \'%c\' failed, status 0x%02x\n", cmd, status);
    }
}
#endif


int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    UNUSED(argc);
    (void)argv;

    /* Register for HCI events */
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    l2cap_init();
    // Initialize AVDTP Sink
    avdtp_source_init();
    avdtp_source_register_packet_handler(&packet_handler);

    // setup stream endpoints
    avdtp_stream_endpoint_t * stream_endpoint;

    // - SBC
    stream_endpoint = a2dp_source_create_stream_endpoint(AVDTP_AUDIO, AVDTP_CODEC_SBC, (uint8_t *) media_sbc_codec_capabilities, sizeof(media_sbc_codec_capabilities), (uint8_t*) local_stream_endpoint_sbc_media_codec_configuration, sizeof(local_stream_endpoint_sbc_media_codec_configuration));
    btstack_assert(stream_endpoint != NULL);
    stream_endpoint->media_codec_configuration_info = local_stream_endpoint_sbc_media_codec_configuration;
    stream_endpoint->media_codec_configuration_len  = sizeof(local_stream_endpoint_sbc_media_codec_configuration);
    avdtp_source_register_delay_reporting_category(avdtp_local_seid(stream_endpoint));
    avdtp_set_preferred_sampling_frequency(stream_endpoint, 44100);
    avdtp_set_preferred_channel_mode(stream_endpoint, AVDTP_SBC_STEREO);

#ifdef HAVE_AAC_FDK
    // - AAC
    stream_endpoint = a2dp_source_create_stream_endpoint(AVDTP_AUDIO, AVDTP_CODEC_MPEG_2_4_AAC, (uint8_t *) media_aac_codec_capabilities, sizeof(media_aac_codec_capabilities), (uint8_t*) local_stream_endpoint_aac_media_codec_configuration, sizeof(local_stream_endpoint_aac_media_codec_configuration));
    btstack_assert(stream_endpoint != NULL);
    stream_endpoint->media_codec_configuration_info = local_stream_endpoint_aac_media_codec_configuration;
    stream_endpoint->media_codec_configuration_len  = sizeof(local_stream_endpoint_aac_media_codec_configuration);
    avdtp_source_register_delay_reporting_category(avdtp_local_seid(stream_endpoint));
#endif

    // - MPEG1/2 Layer 3
    // ..

#ifdef HAVE_LDAC_ENCODER
    // - LDAC
    stream_endpoint = a2dp_source_create_stream_endpoint(AVDTP_AUDIO, AVDTP_CODEC_NON_A2DP, (uint8_t *) media_ldac_codec_capabilities, sizeof(media_ldac_codec_capabilities), (uint8_t*) local_stream_endpoint_ldac_media_codec_configuration, sizeof(local_stream_endpoint_ldac_media_codec_configuration));
    btstack_assert(stream_endpoint != NULL);
    stream_endpoint->media_codec_configuration_info = local_stream_endpoint_ldac_media_codec_configuration;
    stream_endpoint->media_codec_configuration_len  = sizeof(local_stream_endpoint_ldac_media_codec_configuration);
    avdtp_source_register_delay_reporting_category(avdtp_local_seid(stream_endpoint));
#endif
    
#ifdef HAVE_APTX
    // - APTX
    stream_endpoint = a2dp_source_create_stream_endpoint(AVDTP_AUDIO, AVDTP_CODEC_NON_A2DP, (uint8_t *) media_aptx_codec_capabilities, sizeof(media_aptx_codec_capabilities), (uint8_t*) local_stream_endpoint_aptx_media_codec_configuration, sizeof(local_stream_endpoint_aptx_media_codec_configuration));
    btstack_assert(stream_endpoint != NULL);
    stream_endpoint->media_codec_configuration_info = local_stream_endpoint_aptx_media_codec_configuration;
    stream_endpoint->media_codec_configuration_len  = sizeof(local_stream_endpoint_aptx_media_codec_configuration);
    avdtp_source_register_delay_reporting_category(avdtp_local_seid(stream_endpoint));

    // - APTX HD
    stream_endpoint = a2dp_source_create_stream_endpoint(AVDTP_AUDIO, AVDTP_CODEC_NON_A2DP, (uint8_t *) media_aptxhd_codec_capabilities, sizeof(media_aptxhd_codec_capabilities), (uint8_t*) local_stream_endpoint_aptxhd_media_codec_configuration, sizeof(local_stream_endpoint_aptxhd_media_codec_configuration));
    btstack_assert(stream_endpoint != NULL);
    stream_endpoint->media_codec_configuration_info = local_stream_endpoint_aptxhd_media_codec_configuration;
    stream_endpoint->media_codec_configuration_len  = sizeof(local_stream_endpoint_aptxhd_media_codec_configuration);
    avdtp_source_register_delay_reporting_category(avdtp_local_seid(stream_endpoint));
#endif

    // Initialize SDP 
    sdp_init();
    memset(sdp_avdtp_source_service_buffer, 0, sizeof(sdp_avdtp_source_service_buffer));
    a2dp_source_create_sdp_record(sdp_avdtp_source_service_buffer, 0x10002, AVDTP_SOURCE_FEATURE_MASK_PLAYER, NULL, NULL);
    sdp_register_service(sdp_avdtp_source_service_buffer);
    
    gap_set_local_name("BTstack AVDTP Source PTS 00:00:00:00:00:00");
    gap_discoverable_control(1);
    gap_set_class_of_device(0x200408);

#ifdef HAVE_BTSTACK_STDIN
    // parse human readable Bluetooth address
    sscanf_bd_addr(device_addr_string, device_addr);
    btstack_stdin_setup(stdin_process);
#endif

    // turn on!
    hci_power_control(HCI_POWER_ON);
    return 0;
}
