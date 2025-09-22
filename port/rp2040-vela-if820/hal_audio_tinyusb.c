/*
 * Copyright (C) 2025 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "hal_audio_tinyusb.c"

#include <stddef.h>

#include "btstack_bool.h"
#include "btstack_config.h"
#include "btstack_chunk_buffer.h"
#include "btstack_debug.h"
#include "btstack_resample.h"
#include "btstack_run_loop.h"
#include "btstack_util.h"

#include <stdint.h>

#include "hal_audio.h"

#include "tusb.h"
#include "tusb_config.h"

/**
 * USB Soundcard using TinyUSB
 *
 * Full speed USB requires to provide audio data at 1ms intervals, which is too short for BTstack's Audio Embedded implementation
 * To align this, we provide two 10ms buffers and split these internally into 1 ms buffers
 *
 * The USB sample rate is controlled by the host while BTstack assumes it can set the sample rate, too.
 * work around this, we only support 48 kHz over USB and upsample if needed
 *
 * note: some code in the resampling logic assumes 16 bit samples and stereo. If you change that, please review the code
 */

#define NUM_APP_BUFFERS 2
#define NUM_USB_BUFFERS_PER_APP_BUFFER 10

#define NUM_SAMPLES_PER_USB_BUFFER (CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE / 1000)
#define NUM_INT32_PER_USB_BUFFER (NUM_SAMPLES_PER_USB_BUFFER * CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_TX * CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX / 4)

#define NUM_SAMPLES_PER_AUDIO_BUFFER (NUM_USB_BUFFERS_PER_APP_BUFFER * NUM_SAMPLES_PER_USB_BUFFER)

// align output buffer to 4 byte boundary, double buffered
static int32_t output_buffer[NUM_APP_BUFFERS * NUM_USB_BUFFERS_PER_APP_BUFFER * NUM_INT32_PER_USB_BUFFER];

// store index into output_buffer for each application buffer
static uint16_t app_buffer_indices[NUM_APP_BUFFERS];

// Buffer for received data -- not used currently
static int32_t spk_buf[CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ / 4];

// Local static structs to store init arguments
static struct {
    uint8_t channels;
    uint32_t samplerate;
    void (*playback)(uint8_t buffer_index);
    bool     initialized;
    uint16_t samples_per_application_buffer;
    uint16_t samples_per_usb_buffer;
    uint16_t playback_pos_index;
    uint8_t  playback_pos_ms;
    bool     playback_active;
    bool     mic_active;
    btstack_timer_source_t usb_simulation_timer;
    uint32_t usb_simulation_next_ms;
} audio_sink_state;

static struct {
    uint8_t channels;
    uint32_t samplerate;
    void (*recording)(const int16_t * buffer, uint16_t num_samples);
    uint8_t gain;
    int initialized;
} audio_source_state;

// Speaker data size received in the last frame
static int spk_data_size;

// resample buffer for 44.1 -> 48 kHz (a few more samples)
static uint8_t resample_chunk_buffer_storage[(NUM_SAMPLES_PER_USB_BUFFER + 4) * 2 * sizeof(int16_t)];
static btstack_chunk_buffer_t resample_chunk_buffer;
static btstack_resample_t resampler;

// simulate open usb microphone stream to allow for normal playback behaviour
static void hal_audio_sink_usb_simulation_set_next_timer(void) {
    audio_sink_state.usb_simulation_next_ms += NUM_USB_BUFFERS_PER_APP_BUFFER;
    btstack_run_loop_set_timer(&audio_sink_state.usb_simulation_timer,
        audio_sink_state.usb_simulation_next_ms - btstack_run_loop_get_time_ms() );
    btstack_run_loop_add_timer(&audio_sink_state.usb_simulation_timer);
}

static void hal_audio_sink_usb_simulation_handler(btstack_timer_source_t * ts);

static void hal_audio_sink_usb_simulation_start(void) {
    log_info("Start USB MIC simulation");
    audio_sink_state.usb_simulation_next_ms = btstack_run_loop_get_time_ms();
    btstack_run_loop_set_timer_handler(&audio_sink_state.usb_simulation_timer, hal_audio_sink_usb_simulation_handler);
    hal_audio_sink_usb_simulation_set_next_timer();
}

static void hal_audio_sink_usb_simulation_stop(void) {
    log_info("Stop USB MIC simulation");
    btstack_run_loop_remove_timer(&audio_sink_state.usb_simulation_timer);
}

// Sink functions
void hal_audio_sink_init(uint8_t channels, uint32_t samplerate, void (*playback)(uint8_t buffer_index)) {

    log_info("hal_audio_sink_init, sample rate %u hz, channels %u", samplerate, channels);

    // validate configuration: 8 kHz <= samplerate <= 48 kHz, 2 channels
    btstack_assert(channels == CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX);
    btstack_assert((samplerate >= 8000) || (samplerate <= 48000));

    audio_sink_state.channels = channels;
    audio_sink_state.samplerate = samplerate;
    audio_sink_state.playback = playback;
    audio_sink_state.initialized = true;
    audio_sink_state.playback_active = false;

    // note: sizeof(int32_t) == 4 == num channels * num bytes per sample
    audio_sink_state.samples_per_usb_buffer = samplerate / 1000;
    audio_sink_state.samples_per_application_buffer = audio_sink_state.samples_per_usb_buffer * NUM_USB_BUFFERS_PER_APP_BUFFER;

    if (audio_sink_state.samplerate != 48000){
        // setup resample buffer
        btstack_resample_init(&resampler, 2);
        uint32_t resampling_factor = (samplerate << 16) / 48000;
        log_info("Resampling factor 0x%08x", resampling_factor);
        btstack_resample_set_factor( &resampler,  resampling_factor);
        btstack_chunk_buffer_init(&resample_chunk_buffer, resample_chunk_buffer_storage, 0);
    }

    // init app buffer indices
    uint16_t pos = 0;
    for (int i=0;i<NUM_APP_BUFFERS ;i++){
        app_buffer_indices[i] = pos;
        pos += audio_sink_state.samples_per_application_buffer;
    }
}

uint32_t hal_audio_sink_get_frequency(void) {
    return audio_sink_state.samplerate;
}

void hal_audio_sink_set_volume(uint8_t volume) {
    UNUSED(volume);
    log_info("hal_audio_sink_set_volume %u -- not implemented", volume);
}

void hal_audio_sink_start(void) {
    log_info("hal_audio_sink_start");
    audio_sink_state.playback_active = true;
    audio_sink_state.playback_pos_ms = 0;
    audio_sink_state.playback_pos_index = 0;

    if (audio_sink_state.mic_active == false) {
        hal_audio_sink_usb_simulation_start();
    }
}

void hal_audio_sink_stop(void) {
    log_info("hal_audio_sink_start");
    audio_sink_state.playback_active = false;
    hal_audio_sink_usb_simulation_stop();
}

void hal_audio_sink_close(void) {
    log_info("hal_audio_sink_close");
    audio_sink_state.initialized = false;
}

uint16_t hal_audio_sink_get_num_output_buffers(void) {
    return NUM_APP_BUFFERS;
}

uint16_t hal_audio_sink_get_num_output_buffer_samples(void) {
    return audio_sink_state.samples_per_application_buffer;
}

int16_t * hal_audio_sink_get_output_buffer(uint8_t buffer_index) {
    btstack_assert(buffer_index < NUM_APP_BUFFERS);
    return (int16_t*) &output_buffer[app_buffer_indices[buffer_index]];
}


// Source functions -- not implemented
void hal_audio_source_init(uint8_t channels, uint32_t samplerate, void (*recording)(const int16_t *, uint16_t)) {
    audio_source_state.channels = channels;
    audio_source_state.samplerate = samplerate;
    audio_source_state.recording = recording;
    audio_source_state.initialized = 1;
}

uint32_t hal_audio_source_get_samplerate(void) {
    return audio_source_state.samplerate;
}

void hal_audio_source_set_gain(uint8_t gain) {
    audio_source_state.gain = gain;
}

void hal_audio_source_start_stream(void) {
}

void hal_audio_source_stop_stream(void) {
}

void hal_audio_source_close(void) {
    audio_source_state.initialized = 0;
}
#if CFG_TUD_AUDIO
// USB Audio RX Complete Callback
bool tud_audio_rx_done_pre_read_cb(uint8_t rhport, uint16_t n_bytes_received, uint8_t func_id, uint8_t ep_out, uint8_t cur_alt_setting)
{
    (void)rhport;
    (void)func_id;
    (void)ep_out;
    (void)cur_alt_setting;
    spk_data_size = tud_audio_read(spk_buf, n_bytes_received);
    return true;
}

// USB Audio TX Complete Callback

static void hal_audio_sink_handle_usb_buffer_processed(void) {
    // cache buffer index of application buffer that has just been completed
    uint8_t buffer_index = audio_sink_state.playback_pos_ms / NUM_USB_BUFFERS_PER_APP_BUFFER;

    // go to next usb buffer
    audio_sink_state.playback_pos_ms++;
    audio_sink_state.playback_pos_index += audio_sink_state.samples_per_usb_buffer;
    if (audio_sink_state.playback_pos_ms >= NUM_APP_BUFFERS * NUM_USB_BUFFERS_PER_APP_BUFFER){
        // wrap around
        audio_sink_state.playback_pos_ms = 0;
        audio_sink_state.playback_pos_index = 0;
    }

    // report as played if all USB buffers of an application buffer have been sent
    if (audio_sink_state.playback_pos_ms % NUM_USB_BUFFERS_PER_APP_BUFFER == 0) {
        log_debug("Playback %u - mic active %u", buffer_index, audio_sink_state.mic_active );
        (*audio_sink_state.playback)(buffer_index);
    }
}

static void hal_audio_sink_handle_tx_done(void) {
    if (audio_sink_state.playback_active) {
        // send 1ms of audio data
        if (audio_sink_state.samplerate == 48000) {
            // send buffer in expected sample rate
            tud_audio_write(&output_buffer[audio_sink_state.playback_pos_ms * NUM_SAMPLES_PER_USB_BUFFER],
                NUM_SAMPLES_PER_USB_BUFFER * CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_TX * CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX);
            hal_audio_sink_handle_usb_buffer_processed();
        } else {
            // use resampler to convert to 48 kHz on the fly
            uint16_t samples_needed = NUM_SAMPLES_PER_USB_BUFFER;
            uint32_t usb_buffer[NUM_SAMPLES_PER_USB_BUFFER];
            uint8_t * usb_data = (uint8_t *) usb_buffer;
            while (samples_needed > 0) {
                // copy remaining resampled data from previous round
                uint16_t bytes_needed = samples_needed * CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_TX * CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX;
                uint16_t bytes_read = btstack_chunk_buffer_read(&resample_chunk_buffer, usb_data, bytes_needed);
                usb_data += bytes_read;
                uint16_t samples_read = bytes_read / (CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_TX * CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX);
                samples_needed -= samples_read;

                // resample next buffer
                if (btstack_chunk_buffer_bytes_available(&resample_chunk_buffer) == 0) {
                    int16_t * input_buffer = (int16_t*) &output_buffer[audio_sink_state.playback_pos_index];
                    uint16_t num_frames = btstack_resample_block(&resampler, input_buffer,
                        audio_sink_state.samples_per_usb_buffer, (int16_t*) resample_chunk_buffer_storage);
                    btstack_chunk_buffer_init(&resample_chunk_buffer, resample_chunk_buffer_storage, num_frames * 2 * sizeof(int16_t));
                   hal_audio_sink_handle_usb_buffer_processed();
                }
            }

            tud_audio_write(usb_buffer,NUM_SAMPLES_PER_USB_BUFFER * CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_TX * CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX);
        }

    } else {

        // playback not active - send silence
        uint32_t bytes_to_write = NUM_INT32_PER_USB_BUFFER * sizeof(int32_t);
        memset(output_buffer, 0, bytes_to_write);
        tud_audio_write(output_buffer, bytes_to_write);
    }
}

// USB Audio TX Simulation - only one call every NUM_USB_BUFFERS_PER_APP_BUFFER ms
static void hal_audio_sink_usb_simulation_handler(btstack_timer_source_t * ts) {
    for (int i = 0 ; i < NUM_USB_BUFFERS_PER_APP_BUFFER; i++) {
        hal_audio_sink_handle_usb_buffer_processed();
    }
    hal_audio_sink_usb_simulation_set_next_timer();
}

// tud_audio_tx_done_pre_load_cb was replaced by tud_audio_tx_done_isr after the TinyUSB v0.18.0 release
// Support old interface for backwards compatibility
// Note: re-opening the audio stream fails on the 7th time with TinyUSB v0.17.0 and on the second with v0.18.0

// provide old and new function prototype to avoid warning
bool tud_audio_tx_done_pre_load_cb(uint8_t rhport, uint8_t itf, uint8_t ep_in, uint8_t cur_alt_setting);
bool tud_audio_tx_done_isr(uint8_t rhport, uint16_t n_bytes_sent, uint8_t func_id, uint8_t ep_in, uint8_t cur_alt_setting);

bool tud_audio_tx_done_pre_load_cb(uint8_t rhport, uint8_t itf, uint8_t ep_in, uint8_t cur_alt_setting) {
    (void)rhport;
    (void)itf;
    (void)ep_in;
    (void)cur_alt_setting;
    hal_audio_sink_handle_tx_done();
    return true;
}

bool tud_audio_tx_done_isr(uint8_t rhport, uint16_t n_bytes_sent, uint8_t func_id, uint8_t ep_in, uint8_t cur_alt_setting) {
    (void)rhport;
    (void)n_bytes_sent;
    (void)func_id;
    (void)ep_in;
    (void)cur_alt_setting;
    hal_audio_sink_handle_tx_done();
    return true;
}

bool tud_audio_set_itf_close_EP_cb(uint8_t rhport, tusb_control_request_t const * p_request)
{
    (void)rhport;

    uint8_t const itf = tu_u16_low(tu_le16toh(p_request->wIndex));
    uint8_t const alt = tu_u16_low(tu_le16toh(p_request->wValue));

    if (ITF_NUM_AUDIO_STREAMING_MIC == itf && alt == 0) {
        // speaker streaming stopped
        audio_sink_state.mic_active = false;
        hal_audio_sink_usb_simulation_start();
    }

    return true;
}

bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const * p_request)
{
    (void)rhport;
    uint8_t const itf = tu_u16_low(tu_le16toh(p_request->wIndex));
    uint8_t const alt = tu_u16_low(tu_le16toh(p_request->wValue));

    if (ITF_NUM_AUDIO_STREAMING_MIC == itf && alt != 0) {
        // speaker streaming started
        audio_sink_state.mic_active = true;
        hal_audio_sink_usb_simulation_stop();
    }
    return true;
}

#endif