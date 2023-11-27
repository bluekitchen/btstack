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

#define BTSTACK_FILE__ "sco_demo_util.c"
 
/*
 * sco_demo_util.c - send/receive test data via SCO, used by hfp_*_demo and hsp_*_demo
 */

#include <stdio.h>

#include "sco_demo_util.h"

#include "btstack_audio.h"
#include "btstack_debug.h"
#include "btstack_ring_buffer.h"
#include "classic/btstack_cvsd_plc.h"
#include "classic/btstack_sbc.h"
#include "classic/btstack_sbc_bluedroid.h"
#include "classic/hfp.h"
#include "classic/hfp_codec.h"

#ifdef ENABLE_HFP_SUPER_WIDE_BAND_SPEECH
#include "btstack_lc3.h"
#include "btstack_lc3_google.h"
#endif


#ifdef _MSC_VER
// ignore deprecated warning for fopen
#pragma warning(disable : 4996)
#endif

#ifdef HAVE_POSIX_FILE_IO
#include "wav_util.h"
#endif

// test modes
#define SCO_DEMO_MODE_SINE		 0
#define SCO_DEMO_MODE_MICROPHONE 1
#define SCO_DEMO_MODE_MODPLAYER  2

// SCO demo configuration
#define SCO_DEMO_MODE               SCO_DEMO_MODE_MICROPHONE

// number of sco packets until 'report' on console
#define SCO_REPORT_PERIOD           100


#ifdef HAVE_POSIX_FILE_IO
// length and name of wav file on disk
#define SCO_WAV_DURATION_IN_SECONDS 15
#define SCO_WAV_FILENAME            "sco_input.wav"
#endif

// constants
#define NUM_CHANNELS            1
#define SAMPLE_RATE_8KHZ        8000
#define SAMPLE_RATE_16KHZ       16000
#define SAMPLE_RATE_32KHZ       32000
#define BYTES_PER_FRAME         2

// audio pre-buffer - also defines latency
#define SCO_PREBUFFER_MS      50
#define PREBUFFER_BYTES_8KHZ  (SCO_PREBUFFER_MS *  SAMPLE_RATE_8KHZ/1000 * BYTES_PER_FRAME)
#define PREBUFFER_BYTES_16KHZ (SCO_PREBUFFER_MS * SAMPLE_RATE_16KHZ/1000 * BYTES_PER_FRAME)
#define PREBUFFER_BYTES_32KHZ (SCO_PREBUFFER_MS * SAMPLE_RATE_32KHZ/1000 * BYTES_PER_FRAME)

#if defined(ENABLE_HFP_SUPER_WIDE_BAND_SPEECH)
#define PREBUFFER_BYTES_MAX PREBUFFER_BYTES_32KHZ
#define SAMPLES_PER_FRAME_MAX 240
#elif defined(ENABLE_HFP_WIDE_BAND_SPEECH)
#define PREBUFFER_BYTES_MAX PREBUFFER_BYTES_16KHZ
#define SAMPLES_PER_FRAME_MAX 120
#else
#define PREBUFFER_BYTES_MAX PREBUFFER_BYTES_8KHZ
#define SAMPLES_PER_FRAME_MAX 60
#endif

static uint16_t              audio_prebuffer_bytes;

// output
static int                   audio_output_paused  = 0;
static uint8_t               audio_output_ring_buffer_storage[2 * PREBUFFER_BYTES_MAX];
static btstack_ring_buffer_t audio_output_ring_buffer;

// input
#if SCO_DEMO_MODE == SCO_DEMO_MODE_MICROPHONE
#define USE_AUDIO_INPUT
#else
#define USE_ADUIO_GENERATOR
static void (*sco_demo_audio_generator)(uint16_t num_samples, int16_t * data);
#endif

static int                   audio_input_paused  = 0;
static uint8_t               audio_input_ring_buffer_storage[2 * PREBUFFER_BYTES_MAX];
static btstack_ring_buffer_t audio_input_ring_buffer;

// mod player
#if SCO_DEMO_MODE == SCO_DEMO_MODE_MODPLAYER
#include "hxcmod.h"
#include "mods/mod.h"
static modcontext mod_context;
#endif

static int count_sent = 0;
static int count_received = 0;

static btstack_cvsd_plc_state_t cvsd_plc_state;

#ifdef ENABLE_HFP_WIDE_BAND_SPEECH
static const btstack_sbc_decoder_t *   sbc_decoder_instance;
static btstack_sbc_decoder_bluedroid_t sbc_decoder_context;
static const btstack_sbc_encoder_t *   sbc_encoder_instance;
static btstack_sbc_encoder_bluedroid_t sbc_encoder_context;
#endif

#ifdef ENABLE_HFP_SUPER_WIDE_BAND_SPEECH
static const btstack_lc3_decoder_t * lc3_decoder;
static btstack_lc3_decoder_google_t lc3_decoder_context;
static btstack_lc3_encoder_google_t lc3_encoder_context;
static hfp_h2_sync_t    hfp_h2_sync;
#endif

int num_samples_to_write;
int num_audio_frames;

// generic codec support
typedef struct {
    void (*init)(void);
    void(*receive)(const uint8_t * packet, uint16_t size);
    void (*fill_payload)(uint8_t * payload_buffer, uint16_t sco_payload_length);
    void (*close)(void);
    //
    uint16_t sample_rate;
} codec_support_t;

// current configuration
static const codec_support_t * codec_current = NULL;

// hfp_codec
#if defined(ENABLE_HFP_WIDE_BAND_SPEECH) || defined(ENABLE_HFP_SUPER_WIDE_BAND_SPEECH)
static hfp_codec_t hfp_codec;
#endif

// Sine Wave

#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE
static uint16_t sine_wave_phase;
static uint16_t sine_wave_steps_per_sample;
#define SINE_WAVE_SAMPLE_RATE SAMPLE_RATE_32KHZ

// input signal: pre-computed int16 sine wave, 32000 Hz at 266 Hz
static const int16_t sine_int16[] = {
     0,   1715,   3425,   5126,   6813,   8481,  10126,  11743,  13328,  14876,
 16383,  17846,  19260,  20621,  21925,  23170,  24351,  25465,  26509,  27481,
 28377,  29196,  29934,  30591,  31163,  31650,  32051,  32364,  32587,  32722,
 32767,  32722,  32587,  32364,  32051,  31650,  31163,  30591,  29934,  29196,
 28377,  27481,  26509,  25465,  24351,  23170,  21925,  20621,  19260,  17846,
 16383,  14876,  13328,  11743,  10126,   8481,   6813,   5126,   3425,   1715,
     0,  -1715,  -3425,  -5126,  -6813,  -8481, -10126, -11743, -13328, -14876,
-16384, -17846, -19260, -20621, -21925, -23170, -24351, -25465, -26509, -27481,
-28377, -29196, -29934, -30591, -31163, -31650, -32051, -32364, -32587, -32722,
-32767, -32722, -32587, -32364, -32051, -31650, -31163, -30591, -29934, -29196,
-28377, -27481, -26509, -25465, -24351, -23170, -21925, -20621, -19260, -17846,
-16384, -14876, -13328, -11743, -10126,  -8481,  -6813,  -5126,  -3425,  -1715,
};

static void sco_demo_sine_wave_host_endian(uint16_t num_samples, int16_t * data){
    unsigned int i;
    for (i=0; i < num_samples; i++){
        data[i] = sine_int16[sine_wave_phase];
        sine_wave_phase += sine_wave_steps_per_sample;
        if (sine_wave_phase >= (sizeof(sine_int16) / sizeof(int16_t))){
            sine_wave_phase = 0;
        }
    }
}
#endif

// Mod Player
#if SCO_DEMO_MODE == SCO_DEMO_MODE_MODPLAYER
#define NUM_SAMPLES_GENERATOR_BUFFER 30
static void sco_demo_modplayer(uint16_t num_samples, int16_t * data){
    // mix down stereo
    signed short samples[NUM_SAMPLES_GENERATOR_BUFFER * 2];
    while (num_samples > 0){
        uint16_t next_samples = btstack_min(num_samples, NUM_SAMPLES_GENERATOR_BUFFER);
    	hxcmod_fillbuffer(&mod_context, (unsigned short *) samples, next_samples, NULL);
        num_samples -= next_samples;
        uint16_t i;
        for (i=0;i<next_samples;i++){
            int32_t left  = samples[2*i + 0];
            int32_t right = samples[2*i + 1];
            data[i] = (int16_t)((left + right) / 2);
        }
    }
}
#endif

// Audio Playback / Recording

static void audio_playback_callback(int16_t * buffer, uint16_t num_samples){

    // fill with silence while paused
    if (audio_output_paused){
        if (btstack_ring_buffer_bytes_available(&audio_output_ring_buffer) < audio_prebuffer_bytes){
            memset(buffer, 0, num_samples * BYTES_PER_FRAME);
           return;
        } else {
            // resume playback
            audio_output_paused = 0;
        }
    }

    // get data from ringbuffer
    uint32_t bytes_read = 0;
    btstack_ring_buffer_read(&audio_output_ring_buffer, (uint8_t *) buffer, num_samples * BYTES_PER_FRAME, &bytes_read);
    num_samples -= bytes_read / BYTES_PER_FRAME;
    buffer      += bytes_read / BYTES_PER_FRAME;

    // fill with 0 if not enough
    if (num_samples){
        memset(buffer, 0, num_samples * BYTES_PER_FRAME);
        audio_output_paused = 1;
    }
}

#ifdef USE_AUDIO_INPUT
static void audio_recording_callback(const int16_t * buffer, uint16_t num_samples){
    btstack_ring_buffer_write(&audio_input_ring_buffer, (uint8_t *)buffer, num_samples * 2);
}
#endif

// return 1 if ok
static int audio_initialize(int sample_rate){

    // -- output -- //

    // init buffers
    memset(audio_output_ring_buffer_storage, 0, sizeof(audio_output_ring_buffer_storage));
    btstack_ring_buffer_init(&audio_output_ring_buffer, audio_output_ring_buffer_storage, sizeof(audio_output_ring_buffer_storage));

    // config and setup audio playback
    const btstack_audio_sink_t * audio_sink = btstack_audio_sink_get_instance();
    if (audio_sink != NULL){
        audio_sink->init(1, sample_rate, &audio_playback_callback);
        audio_sink->start_stream();

        audio_output_paused  = 1;
    }

    // -- input -- //

    // init buffers
    memset(audio_input_ring_buffer_storage, 0, sizeof(audio_input_ring_buffer_storage));
    btstack_ring_buffer_init(&audio_input_ring_buffer, audio_input_ring_buffer_storage, sizeof(audio_input_ring_buffer_storage));
    audio_input_paused  = 1;

#ifdef USE_AUDIO_INPUT
    // config and setup audio recording
    const btstack_audio_source_t * audio_source = btstack_audio_source_get_instance();
    if (audio_source != NULL){
        audio_source->init(1, sample_rate, &audio_recording_callback);
        audio_source->start_stream();
    }
#endif

    return 1;
}

static void audio_terminate(void){
    const btstack_audio_sink_t * audio_sink = btstack_audio_sink_get_instance();
    if (!audio_sink) return;
    audio_sink->close();

#ifdef USE_AUDIO_INPUT
    const btstack_audio_source_t * audio_source= btstack_audio_source_get_instance();
    if (!audio_source) return;
    audio_source->close();
#endif
}


// CVSD - 8 kHz

static void sco_demo_cvsd_init(void){
    printf("SCO Demo: Init CVSD\n");
    btstack_cvsd_plc_init(&cvsd_plc_state);
}

static void sco_demo_cvsd_receive(const uint8_t * packet, uint16_t size){

    int16_t audio_frame_out[128];    //

    if (size > sizeof(audio_frame_out)){
        printf("sco_demo_cvsd_receive: SCO packet larger than local output buffer - dropping data.\n");
        return;
    }

    const int audio_bytes_read = size - 3;
    const int num_samples = audio_bytes_read / BYTES_PER_FRAME;

    // convert into host endian
    int16_t audio_frame_in[128];
    int i;
    for (i=0;i<num_samples;i++){
        audio_frame_in[i] = little_endian_read_16(packet, 3 + i * 2);
    }

    // treat packet as bad frame if controller does not report 'all good'
    bool bad_frame = (packet[1] & 0x30) != 0;

    btstack_cvsd_plc_process_data(&cvsd_plc_state, bad_frame, audio_frame_in, num_samples, audio_frame_out);

#ifdef SCO_WAV_FILENAME
    // Samples in CVSD SCO packet are in little endian, ready for wav files (take shortcut)
    const int samples_to_write = btstack_min(num_samples, num_samples_to_write);
    wav_writer_write_le_int16(samples_to_write, audio_frame_out);
    num_samples_to_write -= samples_to_write;
    if (num_samples_to_write == 0){
        wav_writer_close();
    }
#endif

    btstack_ring_buffer_write(&audio_output_ring_buffer, (uint8_t *)audio_frame_out, audio_bytes_read);
}

static void sco_demo_cvsd_fill_payload(uint8_t * payload_buffer, uint16_t sco_payload_length){
    uint16_t bytes_to_copy = sco_payload_length;

    // get data from ringbuffer
    uint16_t pos = 0;
    if (!audio_input_paused){
        uint16_t samples_to_copy = sco_payload_length / 2;
        uint32_t bytes_read = 0;
        btstack_ring_buffer_read(&audio_input_ring_buffer, payload_buffer, bytes_to_copy, &bytes_read);
        // flip 16 on big endian systems
        // @note We don't use (uint16_t *) casts since all sample addresses are odd which causes crahses on some systems
        if (btstack_is_big_endian()){
            uint16_t i;
            for (i=0;i<samples_to_copy/2;i+=2){
                uint8_t tmp           = payload_buffer[i*2];
                payload_buffer[i*2]   = payload_buffer[i*2+1];
                payload_buffer[i*2+1] = tmp;
            }
        }
        bytes_to_copy -= bytes_read;
        pos           += bytes_read;
    }

    // fill with 0 if not enough
    if (bytes_to_copy){
        memset(payload_buffer + pos, 0, bytes_to_copy);
        audio_input_paused = 1;
    }
}

static void sco_demo_cvsd_close(void){
    printf("Used CVSD with PLC, number of proccesed frames: \n - %d good frames, \n - %d bad frames.\n", cvsd_plc_state.good_frames_nr, cvsd_plc_state.bad_frames_nr);
}

static const codec_support_t codec_cvsd = {
        .init         = &sco_demo_cvsd_init,
        .receive      = &sco_demo_cvsd_receive,
        .fill_payload = &sco_demo_cvsd_fill_payload,
        .close        = &sco_demo_cvsd_close,
        .sample_rate = SAMPLE_RATE_8KHZ
};

// encode using hfp_codec
#if defined(ENABLE_HFP_WIDE_BAND_SPEECH) || defined(ENABLE_HFP_SUPER_WIDE_BAND_SPEECH)
static void sco_demo_codec_fill_payload(uint8_t * payload_buffer, uint16_t sco_payload_length){
    if (!audio_input_paused){
        int num_samples = hfp_codec_num_audio_samples_per_frame(&hfp_codec);
        btstack_assert(num_samples <= SAMPLES_PER_FRAME_MAX);
        uint16_t samples_available = btstack_ring_buffer_bytes_available(&audio_input_ring_buffer) / BYTES_PER_FRAME;
        if (hfp_codec_can_encode_audio_frame_now(&hfp_codec) && samples_available >= num_samples){
            int16_t sample_buffer[SAMPLES_PER_FRAME_MAX];
            uint32_t bytes_read;
            btstack_ring_buffer_read(&audio_input_ring_buffer, (uint8_t*) sample_buffer, num_samples * BYTES_PER_FRAME, &bytes_read);
            hfp_codec_encode_audio_frame(&hfp_codec, sample_buffer);
            num_audio_frames++;
        }
    }
    // get data from encoder, fill with 0 if not enough
    if (audio_input_paused || hfp_codec_num_bytes_available(&hfp_codec) < sco_payload_length){
        // just send '0's
        memset(payload_buffer, 0, sco_payload_length);
        audio_input_paused = 1;
    } else {
        hfp_codec_read_from_stream(&hfp_codec, payload_buffer, sco_payload_length);
    }
}
#endif

// mSBC - 16 kHz

#ifdef ENABLE_HFP_WIDE_BAND_SPEECH

static void handle_pcm_data(int16_t * data, int num_samples, int num_channels, int sample_rate, void * context){
    UNUSED(context);
    UNUSED(sample_rate);
    UNUSED(data);
    UNUSED(num_samples);
    UNUSED(num_channels);

    // samples in callback in host endianess, ready for playback
    btstack_ring_buffer_write(&audio_output_ring_buffer, (uint8_t *)data, num_samples*num_channels*2);

#ifdef SCO_WAV_FILENAME
    if (!num_samples_to_write) return;
    num_samples = btstack_min(num_samples, num_samples_to_write);
    num_samples_to_write -= num_samples;
    wav_writer_write_int16(num_samples, data);
    if (num_samples_to_write == 0){
        wav_writer_close();
    }
#endif /* SCO_WAV_FILENAME */
}

static void sco_demo_msbc_init(void){
    printf("SCO Demo: Init mSBC\n");
    sbc_decoder_instance = btstack_sbc_decoder_bluedroid_init_instance(&sbc_decoder_context);
    sbc_decoder_instance->configure(&sbc_decoder_context, SBC_MODE_mSBC, &handle_pcm_data, NULL);
    sbc_encoder_instance = btstack_sbc_encoder_bluedroid_init_instance(&sbc_encoder_context);
    hfp_codec_init_msbc_with_codec(&hfp_codec, sbc_encoder_instance, &sbc_encoder_context);
}

static void sco_demo_msbc_receive(const uint8_t * packet, uint16_t size){
    sbc_decoder_instance->decode_signed_16(&sbc_decoder_context, (packet[1] >> 4) & 3, packet + 3, size - 3);
}

static void sco_demo_msbc_close(void){
    printf("Used mSBC with PLC, number of processed frames: \n - %d good frames, \n - %d zero frames, \n - %d bad frames.\n", sbc_decoder_context.good_frames_nr, sbc_decoder_context.zero_frames_nr, sbc_decoder_context.bad_frames_nr);
}

static const codec_support_t codec_msbc = {
        .init         = &sco_demo_msbc_init,
        .receive      = &sco_demo_msbc_receive,
        .fill_payload = &sco_demo_codec_fill_payload,
        .close        = &sco_demo_msbc_close,
        .sample_rate = SAMPLE_RATE_16KHZ
};

#endif /* ENABLE_HFP_WIDE_BAND_SPEECH */

#ifdef ENABLE_HFP_SUPER_WIDE_BAND_SPEECH

#define LC3_SWB_SAMPLES_PER_FRAME 240
#define LC3_SWB_OCTETS_PER_FRAME   58

static bool sco_demo_lc3swb_frame_callback(bool bad_frame, const uint8_t * frame_data, uint16_t frame_len){

    // skip H2 header for good frames
    if (bad_frame == false){
        btstack_assert(frame_data != NULL);
        frame_data += 2;
    }

    uint8_t tmp_BEC_detect = 0;
    uint8_t BFI = bad_frame ? 1 : 0;
    int16_t samples[LC3_SWB_SAMPLES_PER_FRAME];
    (void) lc3_decoder->decode_signed_16(&lc3_decoder_context, frame_data, BFI,
                                         samples, 1, &tmp_BEC_detect);

    // samples in callback in host endianess, ready for playback
    btstack_ring_buffer_write(&audio_output_ring_buffer, (uint8_t *)samples, LC3_SWB_SAMPLES_PER_FRAME*2);

#ifdef SCO_WAV_FILENAME
    if (num_samples_to_write > 0){
        uint16_t num_samples = btstack_min(LC3_SWB_SAMPLES_PER_FRAME, num_samples_to_write);
        num_samples_to_write -= num_samples;
        wav_writer_write_int16(num_samples, samples);
        if (num_samples_to_write == 0){
            wav_writer_close();
        }
    }
#endif /* SCO_WAV_FILENAME */

    // frame is good, if it isn't a bad frame and we didn't detect other errors
    return (bad_frame == false) && (tmp_BEC_detect == 0);
}

static void sco_demo_lc3swb_init(void){

    printf("SCO Demo: Init LC3-SWB\n");

    hfp_codec.lc3_encoder_context = &lc3_encoder_context;
    const btstack_lc3_encoder_t * lc3_encoder = btstack_lc3_encoder_google_init_instance( &lc3_encoder_context);
    hfp_codec_init_lc3_swb(&hfp_codec, lc3_encoder, &lc3_encoder_context);

    // init lc3 decoder
    lc3_decoder = btstack_lc3_decoder_google_init_instance(&lc3_decoder_context);
    lc3_decoder->configure(&lc3_decoder_context, SAMPLE_RATE_32KHZ, BTSTACK_LC3_FRAME_DURATION_7500US, LC3_SWB_OCTETS_PER_FRAME);

    // init HPF H2 framing
    hfp_h2_sync_init(&hfp_h2_sync, &sco_demo_lc3swb_frame_callback);
}

static void sco_demo_lc3swb_receive(const uint8_t * packet, uint16_t size){
    uint8_t packet_status = (packet[1] >> 4) & 3;
    bool bad_frame = packet_status != 0;
    hfp_h2_sync_process(&hfp_h2_sync, bad_frame, &packet[3], size-3);
}

static void sco_demo_lc3swb_close(void){
    // TODO: report
}

static const codec_support_t codec_lc3swb = {
        .init         = &sco_demo_lc3swb_init,
        .receive      = &sco_demo_lc3swb_receive,
        .fill_payload = &sco_demo_codec_fill_payload,
        .close        = &sco_demo_lc3swb_close,
        .sample_rate = SAMPLE_RATE_32KHZ
};
#endif

void sco_demo_init(void){

#ifdef ENABLE_CLASSIC_LEGACY_CONNECTIONS_FOR_SCO_DEMOS
    printf("Disable BR/EDR Secure Connctions due to incompatibilities with SCO connections\n");
    gap_secure_connections_enable(false);
#endif

    // Set SCO for CVSD (mSBC or other codecs automatically use 8-bit transparent mode)
    hci_set_sco_voice_setting(0x60);    // linear, unsigned, 16-bit, CVSD

    // status
#if SCO_DEMO_MODE == SCO_DEMO_MODE_MICROPHONE
    printf("SCO Demo: Sending and receiving audio via btstack_audio.\n");
#endif
#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE
    printf("SCO Demo: Sending sine wave, audio output via btstack_audio.\n");
#endif
#if SCO_DEMO_MODE == SCO_DEMO_MODE_MODPLAYER
    printf("SCO Demo: Sending modplayer wave, audio output via btstack_audio.\n");
    // init mod
    int hxcmod_initialized = hxcmod_init(&mod_context);
    btstack_assert(hxcmod_initialized != 0);
#endif
}

void sco_demo_set_codec(uint8_t negotiated_codec){
    switch (negotiated_codec){
        case HFP_CODEC_CVSD:
            codec_current = &codec_cvsd;
            break;
#ifdef ENABLE_HFP_WIDE_BAND_SPEECH
        case HFP_CODEC_MSBC:
            codec_current = &codec_msbc;
            break;
#endif
#ifdef ENABLE_HFP_SUPER_WIDE_BAND_SPEECH
        case HFP_CODEC_LC3_SWB:
            codec_current = &codec_lc3swb;
            break;
#endif
        default:
            btstack_assert(false);
            break;
    }

    codec_current->init();

    audio_initialize(codec_current->sample_rate);

    audio_prebuffer_bytes = SCO_PREBUFFER_MS * (codec_current->sample_rate/1000) * BYTES_PER_FRAME;

#ifdef SCO_WAV_FILENAME
    num_samples_to_write = codec_current->sample_rate * SCO_WAV_DURATION_IN_SECONDS;
    wav_writer_open(SCO_WAV_FILENAME, 1, codec_current->sample_rate);
#endif

#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE
    sine_wave_steps_per_sample = SINE_WAVE_SAMPLE_RATE / codec_current->sample_rate;
    sco_demo_audio_generator = &sco_demo_sine_wave_host_endian;
#endif

#if SCO_DEMO_MODE == SCO_DEMO_MODE_MODPLAYER
    // load mod
    hxcmod_setcfg(&mod_context, codec_current->sample_rate, 16, 1, 1, 1);
    hxcmod_load(&mod_context, (void *) &mod_data, mod_len);
    sco_demo_audio_generator = &sco_demo_modplayer;
#endif
}

void sco_demo_receive(uint8_t * packet, uint16_t size){
    static uint32_t packets = 0;
    static uint32_t crc_errors = 0;
    static uint32_t data_received = 0;
    static uint32_t byte_errors = 0;

    count_received++;

    data_received += size - 3;
    packets++;
    if (data_received > 100000){
        printf("Summary: data %07u, packets %04u, packet with crc errors %0u, byte errors %04u\n",  (unsigned int) data_received,  (unsigned int) packets, (unsigned int) crc_errors, (unsigned int) byte_errors);
        crc_errors = 0;
        byte_errors = 0;
        data_received = 0;
        packets = 0;
    }

    codec_current->receive(packet, size);
}

void sco_demo_send(hci_con_handle_t sco_handle){

    if (sco_handle == HCI_CON_HANDLE_INVALID) return;

    int sco_packet_length = hci_get_sco_packet_length_for_connection(sco_handle);
    int sco_payload_length = sco_packet_length - 3;

    hci_reserve_packet_buffer();
    uint8_t * sco_packet = hci_get_outgoing_packet_buffer();

#ifdef USE_ADUIO_GENERATOR
    #define REFILL_SAMPLES 16
    // re-fill audio buffer
    uint16_t samples_free = btstack_ring_buffer_bytes_free(&audio_input_ring_buffer) / 2;
    while (samples_free > 0){
        int16_t samples_buffer[REFILL_SAMPLES];
        uint16_t samples_to_add = btstack_min(samples_free, REFILL_SAMPLES);
        (*sco_demo_audio_generator)(samples_to_add, samples_buffer);
        btstack_ring_buffer_write(&audio_input_ring_buffer, (uint8_t *)samples_buffer, samples_to_add * 2);
        samples_free -= samples_to_add;
    }
#endif

    // resume if pre-buffer is filled
    if (audio_input_paused){
        if (btstack_ring_buffer_bytes_available(&audio_input_ring_buffer) >= audio_prebuffer_bytes){
            // resume sending
            audio_input_paused = 0;
        }
    }

    // fill payload by codec
    codec_current->fill_payload(&sco_packet[3], sco_payload_length);

    // set handle + flags
    little_endian_store_16(sco_packet, 0, sco_handle);
    // set len
    sco_packet[2] = sco_payload_length;
    // finally send packet
    hci_send_sco_packet_buffer(sco_packet_length);

    // request another send event
    hci_request_sco_can_send_now_event();

    count_sent++;
    if ((count_sent % SCO_REPORT_PERIOD) == 0) {
        printf("SCO: sent %u, received %u\n", count_sent, count_received);
    }
}

void sco_demo_close(void){
    printf("SCO demo close\n");

    printf("SCO demo statistics: ");
    codec_current->close();
    codec_current = NULL;

#if defined(SCO_WAV_FILENAME)
    wav_writer_close();
#endif

    audio_terminate();
}
