/*
 * Copyright (C) 2018 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "btstack_audio_esp32.c"

/*
 *  btstack_audio_esp32.c
 *
 *  Implementation of btstack_audio.h using polling ESP32 I2S driver
 *
 */

#include "btstack_config.h"
#include "btstack_debug.h"
#include "btstack_audio.h"
#include "btstack_run_loop.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "driver/i2s.h"
#include "esp_log.h"

#include <inttypes.h>
#include <string.h>

#define LOG_TAG "AUDIO"

#ifdef CONFIG_ESP_LYRAT_V4_3_BOARD
#include "driver/i2c.h"
#include "es8388.h"
#define IIC_DATA                    (GPIO_NUM_18)
#define IIC_CLK                     (GPIO_NUM_23)
#endif

#if CONFIG_IDF_TARGET_ESP32C3
// Arbitrary choice - Strapping Pins 2,8,9 are used as outputs
#define BTSTACK_AUDIO_I2S_BCK GPIO_NUM_2
#define BTSTACK_AUDIO_I2S_WS  GPIO_NUM_8
#define BTSTACK_AUDIO_I2S_OUT GPIO_NUM_9
#define BTSTACK_AUDIO_I2S_IN  GPIO_NUM_10
#elif CONFIG_IDF_TARGET_ESP32S3
// ESP32-S3-Korvo-2 V3.0
#define BTSTACK_AUDIO_I2S_BCK GPIO_NUM_9
#define BTSTACK_AUDIO_I2S_WS  GPIO_NUM_45
#define BTSTACK_AUDIO_I2S_OUT GPIO_NUM_8
#define BTSTACK_AUDIO_I2S_IN  GPIO_NUM_10
#else
// ESP32-LyraT V4
#define BTSTACK_AUDIO_I2S_BCK GPIO_NUM_5
#define BTSTACK_AUDIO_I2S_WS  GPIO_NUM_25
#define BTSTACK_AUDIO_I2S_OUT GPIO_NUM_26
#define BTSTACK_AUDIO_I2S_IN  GPIO_NUM_35
#endif

// prototypes
static void btstack_audio_esp32_sink_fill_buffer(void);
static void btstack_audio_esp32_source_process_buffer(void);

#define BTSTACK_AUDIO_I2S_NUM  (I2S_NUM_0)

#define DRIVER_POLL_INTERVAL_MS          5
#define DMA_BUFFER_COUNT                 2
#define BYTES_PER_SAMPLE_STEREO          4

// one DMA buffer for max sample rate
#define MAX_DMA_BUFFER_SAMPLES (48000 * 2 * DRIVER_POLL_INTERVAL_MS/ 1000)

typedef enum {
    BTSTACK_AUDIO_ESP32_OFF = 0,
    BTSTACK_AUDIO_ESP32_INITIALIZED,
    BTSTACK_AUDIO_ESP32_STREAMING
} btstack_audio_esp32_state_t;

static bool btstack_audio_esp32_i2s_installed;
static bool btstack_audio_esp32_i2s_streaming;
static uint32_t btstack_audio_esp32_i2s_samplerate;
static uint16_t btstack_audio_esp32_samples_per_dma_buffer;

// timer to fill output ring buffer
static btstack_timer_source_t  btstack_audio_esp32_driver_timer;

static uint8_t  btstack_audio_esp32_sink_num_channels;
static uint32_t btstack_audio_esp32_sink_samplerate;

static uint8_t  btstack_audio_esp32_source_num_channels;
static uint32_t btstack_audio_esp32_source_samplerate;

static btstack_audio_esp32_state_t btstack_audio_esp32_sink_state;
static btstack_audio_esp32_state_t btstack_audio_esp32_source_state;

// client
static void (*btstack_audio_esp32_sink_playback_callback)(int16_t * buffer, uint16_t num_samples);
static void (*btstack_audio_esp32_source_recording_callback)(const int16_t * buffer, uint16_t num_samples);

// queue for RX/TX done events
static QueueHandle_t btstack_audio_esp32_i2s_event_queue;

#ifdef CONFIG_ESP_LYRAT_V4_3_BOARD

static bool btstack_audio_esp32_es8388_initialized;

static es8388_config_t es8388_i2c_cfg = AUDIO_CODEC_ES8388_DEFAULT();

static void btstack_audio_esp32_set_i2s0_mclk(void)
{
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
    WRITE_PERI_REG(PIN_CTRL, 0xFFF0);
}

void btstack_audio_esp32_es8388_init(void){
    if (btstack_audio_esp32_es8388_initialized) return;
    btstack_audio_esp32_es8388_initialized = true;

    es8388_init(&es8388_i2c_cfg);
    es8388_config_fmt(ES_MODULE_ADC_DAC, ES_I2S_NORMAL);
    es8388_set_bits_per_sample(ES_MODULE_ADC_DAC, BIT_LENGTH_16BITS);
    es8388_start(ES_MODULE_ADC_DAC);
    es8388_set_volume(70);
    es8388_set_mute(false);
}
#endif

static void btstack_audio_esp32_driver_timer_handler(btstack_timer_source_t * ts){
    // read max 2 events from i2s event queue
    i2s_event_t i2s_event;
    int i;
    for (i=0;i<2;i++){
        if( xQueueReceive( btstack_audio_esp32_i2s_event_queue, &i2s_event, 0) == false) break;
        switch (i2s_event.type){
            case I2S_EVENT_TX_DONE:
                log_debug("I2S_EVENT_TX_DONE");
                btstack_audio_esp32_sink_fill_buffer();
                break;
            case I2S_EVENT_RX_DONE:
                log_debug("I2S_EVENT_RX_DONE");
                btstack_audio_esp32_source_process_buffer();
                break;
            default:
                break;
        }
    }

    // re-set timer
    btstack_run_loop_set_timer(ts, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(ts);
}

static void btstack_audio_esp32_stream_start(void){
    if (btstack_audio_esp32_i2s_streaming) return;

    // start i2s
    log_info("i2s stream start");
    i2s_start(BTSTACK_AUDIO_I2S_NUM);

    // start timer
    btstack_run_loop_set_timer_handler(&btstack_audio_esp32_driver_timer, &btstack_audio_esp32_driver_timer_handler);
    btstack_run_loop_set_timer(&btstack_audio_esp32_driver_timer, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(&btstack_audio_esp32_driver_timer);

    btstack_audio_esp32_i2s_streaming = true;
}

static void btstack_audio_esp32_stream_stop(void){
    if (btstack_audio_esp32_i2s_streaming == false) return;

    // check if still needed
    bool still_needed = (btstack_audio_esp32_sink_state   == BTSTACK_AUDIO_ESP32_STREAMING)
                     || (btstack_audio_esp32_source_state == BTSTACK_AUDIO_ESP32_STREAMING);
    if (still_needed) return;

    // stop timer
    btstack_run_loop_remove_timer(&btstack_audio_esp32_driver_timer);

    // stop i2s
    log_info("i2s stream stop");
    i2s_stop(BTSTACK_AUDIO_I2S_NUM);

    btstack_audio_esp32_i2s_streaming = false;
}

static void btstack_audio_esp32_init(void){

    // de-register driver if already installed
    if (btstack_audio_esp32_i2s_installed){
        i2s_driver_uninstall(BTSTACK_AUDIO_I2S_NUM);
    }

    // set i2s mode, sample rate and pins based on sink / source config
    i2s_mode_t i2s_mode  = I2S_MODE_MASTER;
    int i2s_data_out_pin = I2S_PIN_NO_CHANGE;
    int i2s_data_in_pin  = I2S_PIN_NO_CHANGE;
    btstack_audio_esp32_i2s_samplerate = 0;

    if (btstack_audio_esp32_sink_state != BTSTACK_AUDIO_ESP32_OFF){
        i2s_mode |= I2S_MODE_TX; // playback
        i2s_data_out_pin = BTSTACK_AUDIO_I2S_OUT;
        if (btstack_audio_esp32_i2s_samplerate != 0){
            btstack_assert(btstack_audio_esp32_i2s_samplerate == btstack_audio_esp32_sink_samplerate);
        }
        btstack_audio_esp32_i2s_samplerate = btstack_audio_esp32_sink_samplerate;
        btstack_audio_esp32_samples_per_dma_buffer = btstack_audio_esp32_i2s_samplerate * 2 * DRIVER_POLL_INTERVAL_MS / 1000;
    }

    if (btstack_audio_esp32_source_state != BTSTACK_AUDIO_ESP32_OFF){
        i2s_mode |= I2S_MODE_RX; // recording
        i2s_data_in_pin = BTSTACK_AUDIO_I2S_IN;
        if (btstack_audio_esp32_i2s_samplerate != 0){
            btstack_assert(btstack_audio_esp32_i2s_samplerate == btstack_audio_esp32_source_samplerate);
        }
        btstack_audio_esp32_i2s_samplerate = btstack_audio_esp32_source_samplerate;
        btstack_audio_esp32_samples_per_dma_buffer = btstack_audio_esp32_i2s_samplerate * 2 * DRIVER_POLL_INTERVAL_MS / 1000;
    }

    btstack_assert(btstack_audio_esp32_samples_per_dma_buffer <= MAX_DMA_BUFFER_SAMPLES);

    i2s_config_t config =
    {
        .mode                 = i2s_mode,
        .sample_rate          = btstack_audio_esp32_i2s_samplerate,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .dma_buf_count        = DMA_BUFFER_COUNT,                           // Number of DMA buffers. Max 128.
        .dma_buf_len          = btstack_audio_esp32_samples_per_dma_buffer, // Size of each DMA buffer in samples. Max 1024.
        .use_apll             = true,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1
    };

    i2s_pin_config_t pins =
    {
        .bck_io_num           = BTSTACK_AUDIO_I2S_BCK,
        .ws_io_num            = BTSTACK_AUDIO_I2S_WS,
        .data_out_num         = i2s_data_out_pin,
        .data_in_num          = i2s_data_in_pin
    };

#ifdef CONFIG_ESP_LYRAT_V4_3_BOARD
    btstack_audio_esp32_set_i2s0_mclk();
#endif

    log_info("i2s init mode 0x%02x, samplerate %" PRIu32 ", samples per DMA buffer: %u", 
        i2s_mode, btstack_audio_esp32_sink_samplerate, btstack_audio_esp32_samples_per_dma_buffer);

    i2s_driver_install(BTSTACK_AUDIO_I2S_NUM, &config, DMA_BUFFER_COUNT, &btstack_audio_esp32_i2s_event_queue);
    i2s_set_pin(BTSTACK_AUDIO_I2S_NUM, &pins);

#ifdef CONFIG_ESP_LYRAT_V4_3_BOARD
    btstack_audio_esp32_es8388_init();
#endif

    btstack_audio_esp32_i2s_installed = true;
}

static void btstack_audio_esp32_deinit(void){
    if  (btstack_audio_esp32_i2s_installed == false) return;

    // check if still needed
    bool still_needed = (btstack_audio_esp32_sink_state   != BTSTACK_AUDIO_ESP32_OFF)
                    ||  (btstack_audio_esp32_source_state != BTSTACK_AUDIO_ESP32_OFF);
    if (still_needed) return;

    // uninstall driver
    log_info("i2s close");
    i2s_driver_uninstall(BTSTACK_AUDIO_I2S_NUM);

    btstack_audio_esp32_i2s_installed = false;
}

// SINK Implementation
// - with esp-idf v4.4.3, we occasionally get a TX_DONE but fail to write data without waiting for free buffers
//   it's unclera why this happens. this code assumes that the TX_DONE event has been received prematurely and
//   just retries the i2s_write the next time without blocking
static uint8_t btstack_audio_esp32_sink_buffer[MAX_DMA_BUFFER_SAMPLES * BYTES_PER_SAMPLE_STEREO];
static bool btstack_audio_esp32_sink_buffer_ready;
static void btstack_audio_esp32_sink_fill_buffer(void){

    btstack_assert(btstack_audio_esp32_samples_per_dma_buffer <= MAX_DMA_BUFFER_SAMPLES);

    // fetch new data
    size_t bytes_written;
    uint16_t data_len = btstack_audio_esp32_samples_per_dma_buffer * BYTES_PER_SAMPLE_STEREO;
    if (btstack_audio_esp32_sink_buffer_ready == false){
        if (btstack_audio_esp32_sink_state == BTSTACK_AUDIO_ESP32_STREAMING){
            (*btstack_audio_esp32_sink_playback_callback)((int16_t *) btstack_audio_esp32_sink_buffer, btstack_audio_esp32_samples_per_dma_buffer);
            // duplicate samples for mono
            if (btstack_audio_esp32_sink_num_channels == 1){
                int16_t i;
                int16_t * buffer16 = (int16_t *) btstack_audio_esp32_sink_buffer;
                for (i=btstack_audio_esp32_samples_per_dma_buffer-1;i >= 0; i--){
                    buffer16[2*i  ] = buffer16[i];
                    buffer16[2*i+1] = buffer16[i];
                }
            }
            btstack_audio_esp32_sink_buffer_ready = true;
        } else {
            memset(btstack_audio_esp32_sink_buffer, 0, data_len);
        }
    }

    i2s_write(BTSTACK_AUDIO_I2S_NUM, btstack_audio_esp32_sink_buffer, data_len, &bytes_written, 0);

    // check if all data has been written. tolerate writing zero bytes (->retry), but assert on partial write
    if (bytes_written == data_len){
        btstack_audio_esp32_sink_buffer_ready = false;
    } else if (bytes_written == 0){
        ESP_LOGW(LOG_TAG, "i2s_write: couldn't write after I2S_EVENT_TX_DONE\n");
    } else {
        ESP_LOGE(LOG_TAG, "i2s_write: only %u of %u!!!\n", (int) bytes_written, data_len);
        btstack_assert(false);
    }
}

static int btstack_audio_esp32_sink_init(
    uint8_t channels,
    uint32_t samplerate, 
    void (*playback)(int16_t * buffer, uint16_t num_samples)){

    btstack_assert(playback != NULL);
    btstack_assert((1 <= channels) && (channels <= 2));

    // store config
    btstack_audio_esp32_sink_playback_callback  = playback;
    btstack_audio_esp32_sink_num_channels       = channels;
    btstack_audio_esp32_sink_samplerate         = samplerate;

    btstack_audio_esp32_sink_state = BTSTACK_AUDIO_ESP32_INITIALIZED;
    
    // init i2s and codec
    btstack_audio_esp32_init();
    return 0;
}

static uint32_t btstack_audio_esp32_sink_get_samplerate(void) {
    return btstack_audio_esp32_sink_samplerate;
}
static void btstack_audio_esp32_sink_set_volume(uint8_t gain) {
#ifdef CONFIG_ESP_LYRAT_V4_3_BOARD
    if (!btstack_audio_esp32_es8388_initialized) return;
    uint8_t volume_0_100 = (uint8_t) ((((uint16_t) gain) * 100) / 128);
    es8388_set_volume( volume_0_100 );
#else
    UNUSED(gain);
#endif
}

static void btstack_audio_esp32_sink_start_stream(void){

    if (btstack_audio_esp32_sink_state != BTSTACK_AUDIO_ESP32_INITIALIZED) return;

    // validate samplerate
    btstack_assert(btstack_audio_esp32_sink_samplerate == btstack_audio_esp32_i2s_samplerate);

    // state
    btstack_audio_esp32_sink_state = BTSTACK_AUDIO_ESP32_STREAMING;
    btstack_audio_esp32_sink_buffer_ready = false;
    
    // note: conceptually, it would make sense to pre-fill all I2S buffers and then feed new ones when they are
    // marked as complete. However, it looks like we get additoinal events and then assert below, 
    // so we just don't pre-fill them here

    btstack_audio_esp32_stream_start();
}

static void btstack_audio_esp32_sink_stop_stream(void){

    if (btstack_audio_esp32_sink_state != BTSTACK_AUDIO_ESP32_STREAMING) return;

    // state
    btstack_audio_esp32_sink_state = BTSTACK_AUDIO_ESP32_INITIALIZED;

    btstack_audio_esp32_stream_stop();
}

static void btstack_audio_esp32_sink_close(void){

    if (btstack_audio_esp32_sink_state == BTSTACK_AUDIO_ESP32_STREAMING) {
        btstack_audio_esp32_sink_stop_stream();
    }

    // state
    btstack_audio_esp32_sink_state = BTSTACK_AUDIO_ESP32_OFF;

    btstack_audio_esp32_deinit();
}

static const btstack_audio_sink_t btstack_audio_esp32_sink = {
    .init           = &btstack_audio_esp32_sink_init,
    .get_samplerate = &btstack_audio_esp32_sink_get_samplerate,
    .set_volume     = &btstack_audio_esp32_sink_set_volume,
    .start_stream   = &btstack_audio_esp32_sink_start_stream,
    .stop_stream    = &btstack_audio_esp32_sink_stop_stream,
    .close          = &btstack_audio_esp32_sink_close
};

const btstack_audio_sink_t * btstack_audio_esp32_sink_get_instance(void){
    return &btstack_audio_esp32_sink;
}

// SOURCE Implementation

static void btstack_audio_esp32_source_process_buffer(void){
    size_t bytes_read;
    uint8_t buffer[MAX_DMA_BUFFER_SAMPLES * BYTES_PER_SAMPLE_STEREO];

    btstack_assert(btstack_audio_esp32_samples_per_dma_buffer <= MAX_DMA_BUFFER_SAMPLES);

    uint16_t data_len = btstack_audio_esp32_samples_per_dma_buffer * BYTES_PER_SAMPLE_STEREO;
    i2s_read(BTSTACK_AUDIO_I2S_NUM, buffer, data_len, &bytes_read, 0);

    // check if full buffer is ready. tolerate reading zero bytes (->retry), but assert on partial read
    if (bytes_read == 0){
        ESP_LOGW(LOG_TAG, "i2s_read: no data after I2S_EVENT_RX_DONE\n");
        return;
    } else if (bytes_read < data_len){
        ESP_LOGE(LOG_TAG, "i2s_read: only %u of %u!!!\n", (int) bytes_read, data_len);
        btstack_assert(false);
        return;
    }

    int16_t * buffer16 = (int16_t *) buffer;
    if (btstack_audio_esp32_source_state == BTSTACK_AUDIO_ESP32_STREAMING) {
        // drop second channel if configured for mono
        if (btstack_audio_esp32_source_num_channels == 1){
            uint16_t i;
            for (i=0;i<btstack_audio_esp32_samples_per_dma_buffer;i++){
                buffer16[i] = buffer16[2*i];
            }
        }
        (*btstack_audio_esp32_source_recording_callback)(buffer16, btstack_audio_esp32_samples_per_dma_buffer);
    }
}

static int btstack_audio_esp32_source_init(
    uint8_t channels,
    uint32_t samplerate, 
    void (*recording)(const int16_t * buffer, uint16_t num_samples)
){
    btstack_assert(recording != NULL);

    // store config
    btstack_audio_esp32_source_recording_callback = recording;
    btstack_audio_esp32_source_num_channels       = channels;
    btstack_audio_esp32_source_samplerate         = samplerate;

    btstack_audio_esp32_source_state = BTSTACK_AUDIO_ESP32_INITIALIZED;

    // init i2s and codec
    btstack_audio_esp32_init();
    return 0;
}

static uint32_t btstack_audio_esp32_source_get_samplerate(void) {
    return btstack_audio_esp32_source_samplerate;
}
static void btstack_audio_esp32_source_set_gain(uint8_t gain) {
#ifdef CONFIG_ESP_LYRAT_V4_3_BOARD
    if (!btstack_audio_esp32_es8388_initialized) return;
    // ES8388 supports 0..24 dB gain in 3 dB steps
    uint8_t gain_db = (uint8_t) ( ((uint16_t) gain) * 24 / 127);
    es8388_set_mic_gain( (es_codec_mic_gain_t) gain_db );
#else
    UNUSED(gain);
#endif
}

static void btstack_audio_esp32_source_start_stream(void){

    if (btstack_audio_esp32_source_state != BTSTACK_AUDIO_ESP32_INITIALIZED) return;

    // validate samplerate
    btstack_assert(btstack_audio_esp32_source_samplerate == btstack_audio_esp32_i2s_samplerate);

    // state
    btstack_audio_esp32_source_state = BTSTACK_AUDIO_ESP32_STREAMING;

    btstack_audio_esp32_stream_start();
}

static void btstack_audio_esp32_source_stop_stream(void){

    if (btstack_audio_esp32_source_state != BTSTACK_AUDIO_ESP32_STREAMING) return;

    // state
    btstack_audio_esp32_source_state = BTSTACK_AUDIO_ESP32_INITIALIZED;

    btstack_audio_esp32_stream_stop();
}

static void btstack_audio_esp32_source_close(void){

    if (btstack_audio_esp32_source_state == BTSTACK_AUDIO_ESP32_STREAMING) {
        btstack_audio_esp32_source_stop_stream();
    }

    // state
    btstack_audio_esp32_source_state = BTSTACK_AUDIO_ESP32_OFF;

    btstack_audio_esp32_deinit();
}

static const btstack_audio_source_t btstack_audio_esp32_source = {
    .init           = &btstack_audio_esp32_source_init,
    .get_samplerate = &btstack_audio_esp32_source_get_samplerate,
    .set_gain       = &btstack_audio_esp32_source_set_gain,
    .start_stream   = &btstack_audio_esp32_source_start_stream,
    .stop_stream    = &btstack_audio_esp32_source_stop_stream,
    .close          = &btstack_audio_esp32_source_close
};

const btstack_audio_source_t * btstack_audio_esp32_source_get_instance(void){
    return &btstack_audio_esp32_source;
}
