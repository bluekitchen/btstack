/*
 * Copyright (C) {copyright_year} BlueKitchen GmbH
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

#define BTSTACK_FILE__ "le_audio_demo_util_source.c"


#include "le_audio_demo_util_source.h"

// #include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>

#include "btstack_config.h"
#include "btstack_debug.h"

#include "hci.h"

#ifdef ESP_PLATFORM
#include "esp_timer.h"
#include "esp_cpu.h"
#include "freertos/FreeRTOS.h"
// use bit 1 for le_audio_util_source, see EVENT_GROUP_FLAG_RUN_LOOP in btststack_run_loop_freertos.c
#define EVENT_GROUP_FLAG_LE_AUDIO_UTIL_SOURCE 2
#endif

// #define LE_AUDIO_SOURCE_RECORDING
#ifdef LE_AUDIO_SOURCE_RECORDING
// live recording
#define RECORDING_PREBUFFER__MS          20
#define RECORDING_BUFFER_MS              40

static uint8_t le_audio_demo_source_num_channels;

// recording / portaudio
#define SAMPLES_PER_CHANNEL_RECORDING (MAX_SAMPLES_PER_10MS_FRAME * RECORDING_BUFFER_MS / 10)
static uint8_t recording_storage[LE_AUDIO_DEMO_SOURCE_MAX_CHANNELS * 2 * SAMPLES_PER_CHANNEL_RECORDING];
static btstack_audio_generator_bridge_t * le_audio_demo_source_recording_bridge;

// recording callback has channels interleaved, collect per channel
static void le_audio_demo_util_source_recording_callback(const int16_t * buffer, uint16_t num_samples, const btstack_audio_context_t * context){
    UNUSED(context);
    log_info("store %u samples per channel", num_samples);
    btstack_audio_generator_bridge_push(le_audio_demo_source_recording_bridge, buffer, num_samples);
}

static bool le_audio_demo_util_source_recording_start(le_audio_demo_source_generator_t *me){
    bool ok = false;
    if (me->audio_source != NULL){
        int err = me->audio_source->init(me->num_channels, me->sampling_frequency_hz,
                                         &le_audio_demo_util_source_recording_callback);
        log_info("recording initialized, err %u", err);
        if (err){
            // configure audio bridge
            uint32_t start_threshold = me->sampling_frequency_hz * RECORDING_PREBUFFER__MS / 1000;
            btstack_audio_generator_bridge_init(&me->bridge, me->sampling_frequency_hz,
                me->num_channels, recording_storage, sizeof(recording_storage), start_threshold);
            le_audio_demo_source_recording_bridge = &me->bridge;
            me->initialized = true;
            // start audio stream
            me->audio_source->start_stream();
            log_info("recording start, %" PRIu32 " prebuffer samples per channel", start_threshold);
            ok = true;
        }
    }
    return ok;
}

static void le_audio_demo_util_source_recording_stop(le_audio_demo_source_generator_t *me) {
    me->audio_source->stop_stream();
}
#endif

// Multicore Processing
#ifdef CONFIG_IDF_TARGET_ESP32S3
#define ENABLE_ENCODER_TASK
// task handles
static TaskHandle_t main_task_handle = NULL;
static TaskHandle_t encoder_task_handle = NULL;
// task to perform
static le_audio_demo_source_generator_t *encoder_task_generator;
static uint8_t encoder_task_channel;

static void encoder_task(void * args) {
    UNUSED(args);
    while (true) {
        // wati for notification
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // collect input
        le_audio_demo_source_generator_t *me = encoder_task_generator;
        lc3_codec_t *codec = &me->codec;
        int16_t *pcm = me->pcm;
        uint8_t i = encoder_task_channel;
        codec->encoder->encode_signed_16(&codec->contexts[i], &pcm[i], me->num_channels, &me->iso_payload[i * MAX_LC3_FRAME_BYTES]);

        /* Notify Task A that work is done */
        xTaskNotify(main_task_handle, EVENT_GROUP_FLAG_LE_AUDIO_UTIL_SOURCE, eSetBits);
    }
}
#endif

void le_audio_demo_util_source_init(le_audio_demo_source_generator_t *me) {
    me->audio_source = btstack_audio_source_get_instance();
#ifdef ENABLE_ENCODER_TASK
    // current task handle
    main_task_handle = xTaskGetCurrentTaskHandle();
    // create encoder task on second core
    xTaskCreatePinnedToCore(
        encoder_task,
        "LC3_Encoder",
        8192,
        NULL,
        10,
        &encoder_task_handle,
        1);   // Core 1
#endif
}

static void le_audio_demo_util_source_setup_lc3_encoder(le_audio_demo_source_generator_t *gen){
    lc3_codec_t *me = &gen->codec;
    uint8_t channel;
    for (channel = 0 ; channel < gen->num_channels ; channel++){
        btstack_lc3_encoder_google_t * context = &me->contexts[channel];
        me->encoder = btstack_lc3_encoder_google_init_instance(context);
        me->encoder->configure(context, gen->sampling_frequency_hz, gen->frame_duration, gen->octets_per_frame);
    }

    printf("LC3 Encoder config: %" PRIu32 " hz, frame duration %s ms, num samples %u, num octets %u\n",
           gen->sampling_frequency_hz, gen->frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ? "7.5" : "10",
           gen->num_samples_per_frame, gen->octets_per_frame);
}

void le_audio_demo_util_source_configure(le_audio_demo_source_generator_t* me, uint8_t num_streams, uint8_t num_channels_per_stream, uint32_t sampling_frequency_hz,
                                         btstack_lc3_frame_duration_t frame_duration, uint16_t octets_per_frame) {
    me->sampling_frequency_hz = sampling_frequency_hz;
    me->frame_duration = frame_duration;
    me->octets_per_frame = octets_per_frame;
    me->num_streams = num_streams;
    me->num_channels_per_stream = num_channels_per_stream;

    me->num_channels = num_streams * num_channels_per_stream;
    btstack_assert((me->num_channels > 0) || (me->num_channels <= LE_AUDIO_DEMO_SOURCE_MAX_CHANNELS));

    me->num_samples_per_frame = btstack_lc3_samples_per_frame(sampling_frequency_hz, frame_duration);
    btstack_assert(me->num_samples_per_frame <= MAX_SAMPLES_PER_10MS_FRAME);
}

void le_audio_demo_util_source_generate_iso_frame(le_audio_demo_source_generator_t *me) {
    btstack_assert(me->octets_per_frame != 0);

#ifdef ESP_PLATFORM
    static int counter = 0;
    static int64_t time_generator_us = 0;
    static int64_t time_encodder_us = 0;
#endif

    if (me->data_generator != NULL) {
        for (uint8_t i = 0; i < me->num_channels; i++) {
            (*me->data_generator)(me->data_generator_context, &me->iso_payload[i * MAX_LC3_FRAME_BYTES],
                me->octets_per_frame, i, me->packet_sequence_numbers[i]++);
        }
    } else if (me->audio_generator != NULL) {
        int16_t *pcm = me->pcm;

#ifdef ESP_PLATFORM
        int64_t start = esp_timer_get_time();
#endif

        // generate audio
        btstack_audio_generator_generate(me->audio_generator, pcm, me->num_samples_per_frame);

#ifdef ESP_PLATFORM
        int64_t generator_end = esp_timer_get_time();
        time_generator_us += generator_end - start;
#endif

        // encode lc3
        lc3_codec_t *codec = &me->codec;

#ifdef ENABLE_ENCODER_TASK
        // start encoder for second channel
        if (me->num_channels > 1) {
            encoder_task_generator = me;
            encoder_task_channel = 1;
            xTaskNotifyGive(encoder_task_handle);
        }
        // process first channel ourselves
        uint8_t i = 0;
        codec->encoder->encode_signed_16(&codec->contexts[i], &pcm[i], me->num_channels, &me->iso_payload[i * MAX_LC3_FRAME_BYTES]);
#if 0
        // wait for second channel complete
        if (me->num_channels > 1) {
            while (true) {
                // xTaskNotifyWait returns on any bit set, wait until encoder reports done
                uint32_t value;
                xTaskNotifyWait(0, EVENT_GROUP_FLAG_LE_AUDIO_UTIL_SOURCE, &value, portMAX_DELAY);
                if (value & EVENT_GROUP_FLAG_LE_AUDIO_UTIL_SOURCE) break;
            }
        }
#endif

#else
        for (uint8_t i=0;i<me->num_channels;i++){
            codec->encoder->encode_signed_16(&codec->contexts[i], &pcm[i], me->num_channels, &me->iso_payload[i * MAX_LC3_FRAME_BYTES]);
        }
#endif

#ifdef ESP_PLATFORM
        int64_t encoder_end = esp_timer_get_time();
        time_encodder_us += encoder_end - generator_end;

        // report
        counter++;
        if (counter == 100) {
            printf("Generator %lld us, Encoder %lld us per frame, Core %u\n", time_generator_us/100, time_encodder_us/100, esp_cpu_get_core_id());
            // reset
            counter = 0;
            time_generator_us = 0;
            time_encodder_us = 0;
            encoder_end = 0;
        }
#endif

    } else {
        // send zeros
        memset(me->iso_payload, 0, sizeof(me->iso_payload));
    }
}

void le_audio_demo_util_source_send(le_audio_demo_source_generator_t *me, uint8_t stream_index, hci_con_handle_t con_handle){

    uint16_t octets_per_frame = me->octets_per_frame;
    uint8_t num_channels_per_stream = me->num_channels_per_stream;
    uint16_t *packet_sequence_numbers = me->packet_sequence_numbers;
    btstack_assert(octets_per_frame != 0);

    hci_reserve_packet_buffer();
    uint8_t * buffer = hci_get_outgoing_packet_buffer();
    // complete SDU, no TimeStamp
    little_endian_store_16(buffer, 0, ((uint16_t) con_handle) | (2 << 12));
    // len
    little_endian_store_16(buffer, 2, 0 + 4 + num_channels_per_stream * octets_per_frame);
    // TimeStamp if TS flag is set
    // packet seq nr
    little_endian_store_16(buffer, 4, packet_sequence_numbers[stream_index]);
    // iso sdu len
    little_endian_store_16(buffer, 6, num_channels_per_stream * octets_per_frame);
    uint16_t offset = 8;
    // copy encoded payload
    uint8_t i;
    for (i=0; i<num_channels_per_stream;i++) {
        uint8_t effective_channel = (stream_index * num_channels_per_stream) + i;
        memcpy(&buffer[offset], &me->iso_payload[effective_channel * MAX_LC3_FRAME_BYTES], octets_per_frame);
        offset += octets_per_frame;
    }
    // send
    hci_send_iso_packet_buffer(offset);

    packet_sequence_numbers[stream_index]++;
}

void le_audio_demo_util_source_close(le_audio_demo_source_generator_t *me){
    if (me->audio_source != NULL){
        me->audio_source->close();
    }
}

void le_audio_demo_util_source_set_data_generator(le_audio_demo_source_generator_t *gen, le_audio_demo_util_source_data_generator_t * data_generator, void * context) {
    gen->data_generator = data_generator;
    gen->data_generator_context = context;
    gen->audio_generator = NULL;
}

void le_audio_demo_util_source_set_audio_generator(le_audio_demo_source_generator_t *gen, btstack_audio_generator_t *audio_generator) {
    if (gen->audio_generator == NULL) {
        le_audio_demo_util_source_setup_lc3_encoder(gen);
    }
    gen->audio_generator = audio_generator;
    gen->data_generator = NULL;
    gen->data_generator_context = NULL;
}
