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
#include "classic/hfp.h"
#include "classic/hfp_msbc.h"

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
#define BYTES_PER_FRAME         2

// pre-buffer for CVSD and mSBC - also defines latency
#define SCO_PREBUFFER_MS      50
#define PREBUFFER_BYTES_8KHZ  (SCO_PREBUFFER_MS *  SAMPLE_RATE_8KHZ/1000 * BYTES_PER_FRAME)
#define PREBUFFER_BYTES_16KHZ (SCO_PREBUFFER_MS * SAMPLE_RATE_16KHZ/1000 * BYTES_PER_FRAME)

static uint16_t              audio_prebuffer_bytes;

// output
static int                   audio_output_paused  = 0;
static uint8_t               audio_output_ring_buffer_storage[2 * PREBUFFER_BYTES_16KHZ];
static btstack_ring_buffer_t audio_output_ring_buffer;

// input
#if SCO_DEMO_MODE == SCO_DEMO_MODE_MICROPHONE
#define USE_AUDIO_INPUT
#else
#define USE_ADUIO_GENERATOR
static void (*sco_demo_audio_generator)(uint16_t num_samples, int16_t * data);
#endif
static int                   audio_input_paused  = 0;
static uint8_t               audio_input_ring_buffer_storage[2 * PREBUFFER_BYTES_16KHZ];
static btstack_ring_buffer_t audio_input_ring_buffer;

static int count_sent = 0;
static int count_received = 0;

#ifdef ENABLE_HFP_WIDE_BAND_SPEECH
static btstack_sbc_decoder_state_t decoder_state;
#endif

static btstack_cvsd_plc_state_t cvsd_plc_state;

#define MAX_NUM_MSBC_SAMPLES (16*8)

int num_samples_to_write;
int num_audio_frames;

// generic codec support
typedef struct {
    void (*init)(void);
    void(*receive)(const uint8_t * packet, uint16_t size);
    void (*fill_payload)(uint8_t * payload_buffer, uint16_t sco_payload_length);
    void (*close)(void);
} codec_support_t;
static const codec_support_t * codec_current = NULL;

// sine generator

#ifdef USE_ADUIO_GENERATOR
static unsigned int phase;

// input signal: pre-computed sine wave, 266 Hz at 16000 kHz
static const int16_t sine_int16_at_16000hz[] = {
     0,   3135,   6237,   9270,  12202,  14999,  17633,  20073,  22294,  24270,
 25980,  27406,  28531,  29344,  29835,  30000,  29835,  29344,  28531,  27406,
 25980,  24270,  22294,  20073,  17633,  14999,  12202,   9270,   6237,   3135,
     0,  -3135,  -6237,  -9270, -12202, -14999, -17633, -20073, -22294, -24270,
-25980, -27406, -28531, -29344, -29835, -30000, -29835, -29344, -28531, -27406,
-25980, -24270, -22294, -20073, -17633, -14999, -12202,  -9270,  -6237,  -3135,
};

// 8 kHz samples for CVSD/SCO packets in little endian
static void sco_demo_sine_wave_int16_at_8000_hz_host_endian(uint16_t num_samples, int16_t * data){
    unsigned int i;
    for (i=0; i < num_samples; i++){
        data[i] = sine_int16_at_16000hz[phase];
        // ony use every second sample from 16khz table to get 8khz
        phase += 2;
        if (phase >= (sizeof(sine_int16_at_16000hz) / sizeof(int16_t))){
            phase = 0;
        }
    }
}

// 16 kHz samples for mSBC encoder in host endianess
#ifdef ENABLE_HFP_WIDE_BAND_SPEECH
static void sco_demo_sine_wave_int16_at_16000_hz_host_endian(uint16_t num_samples, int16_t * data){
    unsigned int i;
    for (i=0; i < num_samples; i++){
        data[i] = sine_int16_at_16000hz[phase++];
        if (phase >= (sizeof(sine_int16_at_16000hz) / sizeof(int16_t))){
            phase = 0;
        }
    }
}
#endif
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
    if (!audio_sink) return 0;

    audio_sink->init(1, sample_rate, &audio_playback_callback);
    audio_sink->start_stream();

    audio_output_paused  = 1;

    // -- input -- //

    // init buffers
    memset(audio_input_ring_buffer_storage, 0, sizeof(audio_input_ring_buffer_storage));
    btstack_ring_buffer_init(&audio_input_ring_buffer, audio_input_ring_buffer_storage, sizeof(audio_input_ring_buffer_storage));
    audio_input_paused  = 1;

#ifdef USE_AUDIO_INPUT
    // config and setup audio recording
    const btstack_audio_source_t * audio_source = btstack_audio_source_get_instance();
    if (!audio_source) return 0;

    audio_source->init(1, sample_rate, &audio_recording_callback);
    audio_source->start_stream();
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

    audio_prebuffer_bytes = PREBUFFER_BYTES_8KHZ;

#ifdef USE_ADUIO_GENERATOR
    sco_demo_audio_generator = &sco_demo_sine_wave_int16_at_8000_hz_host_endian;
#endif

#ifdef SCO_WAV_FILENAME
    num_samples_to_write = SAMPLE_RATE_8KHZ * SCO_WAV_DURATION_IN_SECONDS;
    wav_writer_open(SCO_WAV_FILENAME, 1, SAMPLE_RATE_8KHZ);
#endif

    audio_initialize(SAMPLE_RATE_8KHZ);
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
        .close        = &sco_demo_cvsd_close
};

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

    btstack_sbc_decoder_init(&decoder_state, SBC_MODE_mSBC, &handle_pcm_data, NULL);
    hfp_msbc_init();

    audio_prebuffer_bytes = PREBUFFER_BYTES_16KHZ;

#ifdef USE_ADUIO_GENERATOR
    sco_demo_audio_generator = &sco_demo_sine_wave_int16_at_16000_hz_host_endian;
#endif

#ifdef SCO_WAV_FILENAME
    num_samples_to_write = SAMPLE_RATE_16KHZ * SCO_WAV_DURATION_IN_SECONDS;
    wav_writer_open(SCO_WAV_FILENAME, 1, SAMPLE_RATE_16KHZ);
#endif

    audio_initialize(SAMPLE_RATE_16KHZ);
}

static void sco_demo_msbc_receive(const uint8_t * packet, uint16_t size){
    btstack_sbc_decoder_process_data(&decoder_state, (packet[1] >> 4) & 3, packet+3, size-3);
}

void sco_demo_msbc_fill_payload(uint8_t * payload_buffer, uint16_t sco_payload_length){
    if (!audio_input_paused){
        int num_samples = hfp_msbc_num_audio_samples_per_frame();
        btstack_assert(num_samples <= MAX_NUM_MSBC_SAMPLES);
        if (hfp_msbc_can_encode_audio_frame_now() && btstack_ring_buffer_bytes_available(&audio_input_ring_buffer) >= (unsigned int)(num_samples * BYTES_PER_FRAME)){
            int16_t sample_buffer[MAX_NUM_MSBC_SAMPLES];
            uint32_t bytes_read;
            btstack_ring_buffer_read(&audio_input_ring_buffer, (uint8_t*) sample_buffer, num_samples * BYTES_PER_FRAME, &bytes_read);
            hfp_msbc_encode_audio_frame(sample_buffer);
            num_audio_frames++;
        }
        btstack_assert(hfp_msbc_num_bytes_in_stream() >= sco_payload_length);
    }

    // get data from encoder, fill with 0 if not enough
    if (audio_input_paused || hfp_msbc_num_bytes_in_stream() < sco_payload_length){
        // just send '0's
        memset(payload_buffer, 0, sco_payload_length);
        audio_input_paused = 1;
    } else {
        hfp_msbc_read_from_stream(payload_buffer, sco_payload_length);
    }
}

static void sco_demo_msbc_close(void){
    printf("Used mSBC with PLC, number of processed frames: \n - %d good frames, \n - %d zero frames, \n - %d bad frames.\n", decoder_state.good_frames_nr, decoder_state.zero_frames_nr, decoder_state.bad_frames_nr);
}

static const codec_support_t codec_msbc = {
        .init         = &sco_demo_msbc_init,
        .receive      = &sco_demo_msbc_receive,
        .fill_payload = &sco_demo_msbc_fill_payload,
        .close        = &sco_demo_msbc_close
};

#endif /* ENABLE_HFP_WIDE_BAND_SPEECH */

void sco_demo_init(void){

#ifdef ENABLE_CLASSIC_LEGACY_CONNECTIONS_FOR_SCO_DEMOS
    printf("Disable BR/EDR Secure Connctions due to incompatibilities with SCO connections\n");
    gap_secure_connections_enable(false);
#endif

	// status
#if SCO_DEMO_MODE == SCO_DEMO_MODE_MICROPHONE
    printf("SCO Demo: Sending and receiving audio via btstack_audio.\n");
#endif
#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE
    if (btstack_audio_sink_get_instance()){
        printf("SCO Demo: Sending sine wave, audio output via btstack_audio.\n");
    } else {
        printf("SCO Demo: Sending sine wave, hexdump received data.\n");
    }
#endif

    // Set SCO for CVSD (mSBC or other codecs automatically use 8-bit transparent mode)
    hci_set_sco_voice_setting(0x60);    // linear, unsigned, 16-bit, CVSD
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
        default:
            btstack_assert(false);
            break;
    }

    codec_current->init();
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

    int sco_packet_length = hci_get_sco_packet_length();
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
