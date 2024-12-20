/*
 * Copyright (C) 2023 BlueKitchen GmbH
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
#include "driver/i2s_std.h"
#include "esp_log.h"

#include <inttypes.h>
#include <stdatomic.h>
#include <string.h>

#define LOG_TAG "AUDIO"

#ifdef CONFIG_I2S_ISR_IRAM_SAFE
#error CONFIG_I2S_ISR_IRAM_SAFE not supported
#endif

i2s_chan_handle_t tx_handle = NULL;
i2s_chan_handle_t rx_handle = NULL;

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
#elif CONFIG_ESP_LYRAT_V4_3_BOARD
// ESP32-LyraT V4
#define BTSTACK_AUDIO_I2S_MCLK GPIO_NUM_0
#define BTSTACK_AUDIO_I2S_BCK  GPIO_NUM_5
#define BTSTACK_AUDIO_I2S_WS   GPIO_NUM_25
#define BTSTACK_AUDIO_I2S_OUT  GPIO_NUM_26
#define BTSTACK_AUDIO_I2S_IN   GPIO_NUM_35
#define HEADPHONE_DETECT       GPIO_NUM_19
#elif CONFIG_IDF_TARGET_ESP32
// Generic ESP32
#define BTSTACK_AUDIO_I2S_MCLK GPIO_NUM_0
#define BTSTACK_AUDIO_I2S_BCK  GPIO_NUM_5
#define BTSTACK_AUDIO_I2S_WS   GPIO_NUM_25
#define BTSTACK_AUDIO_I2S_OUT  GPIO_NUM_26
#define BTSTACK_AUDIO_I2S_IN   GPIO_NUM_35
#else
#error "No I2S configuration, if you don't use BTstack I2S audio please disable BTSTACK_AUDIO in Components->BTstack Configuration"
#endif

// set MCLK unused
#ifndef BTSTACK_AUDIO_I2S_MCLK
#define BTSTACK_AUDIO_I2S_MCLK GPIO_NUM_NC
#endif

#define BTSTACK_AUDIO_I2S_NUM  (I2S_NUM_0)

#define DRIVER_ISR_INTERVAL_MS          10 // dma interrupt cycle time in ms
#define DMA_BUFFER_COUNT                 6
#define BYTES_PER_SAMPLE_STEREO          4

// one DMA buffer for max sample rate
#define MAX_DMA_BUFFER_SAMPLES (48000 * 2 * DRIVER_ISR_INTERVAL_MS/ 1000)

typedef enum {
    BTSTACK_AUDIO_ESP32_OFF = 0,
    BTSTACK_AUDIO_ESP32_INITIALIZED,
    BTSTACK_AUDIO_ESP32_STREAMING
} btstack_audio_esp32_state_t;

static bool btstack_audio_esp32_i2s_installed;
static bool btstack_audio_esp32_i2s_streaming;
static uint32_t btstack_audio_esp32_i2s_samplerate;

static uint8_t  btstack_audio_esp32_sink_num_channels;
static uint32_t btstack_audio_esp32_sink_samplerate;

static uint8_t  btstack_audio_esp32_source_num_channels;
static uint32_t btstack_audio_esp32_source_samplerate;

static btstack_audio_esp32_state_t btstack_audio_esp32_sink_state;
static btstack_audio_esp32_state_t btstack_audio_esp32_source_state;
// client
static void (*btstack_audio_esp32_sink_playback_callback)(int16_t * buffer, uint16_t num_samples);
static void (*btstack_audio_esp32_source_recording_callback)(const int16_t * buffer, uint16_t num_samples);

#ifdef CONFIG_ESP_LYRAT_V4_3_BOARD

static bool btstack_audio_esp32_es8388_initialized;

static es8388_config_t es8388_i2c_cfg = AUDIO_CODEC_ES8388_DEFAULT();

void btstack_audio_esp32_es8388_init(void){
    if (btstack_audio_esp32_es8388_initialized) return;

    btstack_audio_esp32_es8388_initialized = true;

    ESP_ERROR_CHECK(es8388_init(&es8388_i2c_cfg));
    ESP_ERROR_CHECK(es8388_config_fmt(ES_MODULE_ADC_DAC, ES_I2S_NORMAL));
    ESP_ERROR_CHECK(es8388_set_bits_per_sample(ES_MODULE_ADC_DAC, BIT_LENGTH_16BITS));
    ESP_ERROR_CHECK(es8388_start(ES_MODULE_ADC_DAC));
    ESP_ERROR_CHECK(es8388_set_volume(70));
    ESP_ERROR_CHECK(es8388_set_mute(false));
}
#endif

// data source for integration with btstack run-loop
static btstack_data_source_t transport_data_source;
static volatile _Atomic uint32_t isr_bytes_read = 0;
static volatile _Atomic uint32_t isr_bytes_written = 0;

static void btstack_audio_esp32_stream_start(void){
    if (btstack_audio_esp32_i2s_streaming) return;

    // check if needed
    bool needed = (btstack_audio_esp32_sink_state   == BTSTACK_AUDIO_ESP32_STREAMING)
               || (btstack_audio_esp32_source_state == BTSTACK_AUDIO_ESP32_STREAMING);
    if (!needed) return;

    // clear isr exchange variables to avoid data source activation
    atomic_store_explicit(&isr_bytes_written, 0, memory_order_relaxed);
    atomic_store_explicit(&isr_bytes_read, 0, memory_order_relaxed);

    // start data source
    btstack_run_loop_enable_data_source_callbacks(&transport_data_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_add_data_source(&transport_data_source);

    btstack_audio_esp32_i2s_streaming = true;
}

static void btstack_audio_esp32_stream_stop(void){
    if (btstack_audio_esp32_i2s_streaming == false) return;

    // check if still needed
    bool still_needed = (btstack_audio_esp32_sink_state   == BTSTACK_AUDIO_ESP32_STREAMING)
                     || (btstack_audio_esp32_source_state == BTSTACK_AUDIO_ESP32_STREAMING);
    if (still_needed) return;

    // stop data source
    btstack_run_loop_disable_data_source_callbacks(&transport_data_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_remove_data_source(&transport_data_source);

    atomic_store_explicit(&isr_bytes_written, 0, memory_order_relaxed);
    atomic_store_explicit(&isr_bytes_read, 0, memory_order_relaxed);

    btstack_audio_esp32_i2s_streaming = false;
}

static IRAM_ATTR bool i2s_rx_callback(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx)
{
    size_t block_size = event->size;
    atomic_fetch_add_explicit(&isr_bytes_read, block_size, memory_order_relaxed);
    if( block_size > 0 ) {
        btstack_run_loop_poll_data_sources_from_irq();
        return true;
    }
    return false;
}

static IRAM_ATTR bool i2s_tx_callback(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx)
{
    size_t block_size = event->size;
    atomic_fetch_add_explicit(&isr_bytes_written, block_size, memory_order_relaxed);
    if( block_size > 0 ) {
        btstack_run_loop_poll_data_sources_from_irq();
        return true;
    }
    return false;
}

static uint8_t btstack_audio_esp32_buffer[MAX_DMA_BUFFER_SAMPLES * BYTES_PER_SAMPLE_STEREO * DMA_BUFFER_COUNT];
static void i2s_data_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type) {

    size_t bytes_written = 0;
    size_t bytes_read = 0;
    uint32_t write_block_size = atomic_load_explicit(&isr_bytes_written, memory_order_relaxed);
    uint32_t read_block_size = atomic_load_explicit(&isr_bytes_read, memory_order_relaxed);
    switch (callback_type){
        case DATA_SOURCE_CALLBACK_POLL: {
            uint16_t num_samples = write_block_size/BYTES_PER_SAMPLE_STEREO;
            if( num_samples > 0 ) {
                (*btstack_audio_esp32_sink_playback_callback)((int16_t *) btstack_audio_esp32_buffer, num_samples);
                // duplicate samples for mono
                if (btstack_audio_esp32_sink_num_channels == 1){
                    int16_t *buffer16 = (int16_t *) btstack_audio_esp32_buffer;
                    for (int16_t i=num_samples-1;i >= 0; i--){
                        buffer16[2*i  ] = buffer16[i];
                        buffer16[2*i+1] = buffer16[i];
                    }
                }
                esp_err_t ret = i2s_channel_write(tx_handle, btstack_audio_esp32_buffer, write_block_size, &bytes_written, 0 );
                if( ret == ESP_OK ) {
                    btstack_assert( bytes_written == write_block_size );
                } else if( ret == ESP_ERR_TIMEOUT ) {
                    ESP_LOGW(LOG_TAG, "audio output buffer underrun");
                }
                atomic_fetch_sub_explicit( &isr_bytes_written, write_block_size, memory_order_relaxed );
            }

            num_samples = read_block_size/BYTES_PER_SAMPLE_STEREO;
            if( num_samples > 0 ) {
                esp_err_t ret = i2s_channel_read(rx_handle, btstack_audio_esp32_buffer, read_block_size, &bytes_read, 0 );
                if( ret == ESP_OK ) {
                    btstack_assert( bytes_read == read_block_size );
                } else if( ret == ESP_ERR_TIMEOUT ) {
                    ESP_LOGW(LOG_TAG, "audio input buffer overrun");
                }
                // drop second channel if configured for mono
                int16_t * buffer16 = (int16_t *)btstack_audio_esp32_buffer;
                if (btstack_audio_esp32_source_num_channels == 1){
                    for (uint16_t i=0;i<num_samples;i++){
                        buffer16[i] = buffer16[2*i];
                    }
                }
                (*btstack_audio_esp32_source_recording_callback)((int16_t *) btstack_audio_esp32_buffer, num_samples);
                atomic_fetch_sub_explicit(&isr_bytes_read, read_block_size, memory_order_relaxed);
            }
            break;
        }
        default:
            break;
    }
}

/**
 * dma_frame_num * slot_num * data_bit_width / 8 = dma_buffer_size <= 4092
 * dma_frame_num <= 511
 * interrupt_interval = dma_frame_num / sample_rate = 511 / 144000 = 0.003549 s = 3.549 ms
 * dma_desc_num > polling_cycle / interrupt_interval = cell(10 / 3.549) = cell(2.818) = 3
 * recv_buffer_size > dma_desc_num * dma_buffer_size = 3 * 4092 = 12276 bytes
 *
 */
static void btstack_audio_esp32_init(void) {
    if (btstack_audio_esp32_i2s_installed) {
        return;
    }

    // set i2s mode, sample rate and pins based on sink / source config
    btstack_audio_esp32_i2s_samplerate = 0;

    if (btstack_audio_esp32_sink_state != BTSTACK_AUDIO_ESP32_OFF){
        if (btstack_audio_esp32_i2s_samplerate != 0){
            btstack_assert(btstack_audio_esp32_i2s_samplerate == btstack_audio_esp32_sink_samplerate);
        }
        btstack_audio_esp32_i2s_samplerate = btstack_audio_esp32_sink_samplerate;
    }

    if (btstack_audio_esp32_source_state != BTSTACK_AUDIO_ESP32_OFF){
        if (btstack_audio_esp32_i2s_samplerate != 0){
            btstack_assert(btstack_audio_esp32_i2s_samplerate == btstack_audio_esp32_source_samplerate);
        }
        btstack_audio_esp32_i2s_samplerate = btstack_audio_esp32_source_samplerate;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(BTSTACK_AUDIO_I2S_NUM, I2S_ROLE_MASTER);
    chan_cfg.dma_frame_num = DRIVER_ISR_INTERVAL_MS * btstack_audio_esp32_i2s_samplerate / 1000;
    chan_cfg.dma_desc_num = DMA_BUFFER_COUNT;
    btstack_assert( chan_cfg.dma_frame_num <= 4092 ); // as of https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2s.html
    chan_cfg.auto_clear = true; // Auto clear the legacy data in the DMA buffer
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle));
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = btstack_audio_esp32_i2s_samplerate,
#if SOC_I2S_SUPPORTS_APLL
            .clk_src = I2S_CLK_SRC_APLL,
#else
            .clk_src = I2S_CLK_SRC_DEFAULT,
#endif
            .mclk_multiple = I2S_MCLK_MULTIPLE_256, // for 16bit data
        },
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = BTSTACK_AUDIO_I2S_MCLK,
            .bclk = BTSTACK_AUDIO_I2S_BCK,
            .ws = BTSTACK_AUDIO_I2S_WS,
            .dout = BTSTACK_AUDIO_I2S_OUT,
            .din = BTSTACK_AUDIO_I2S_IN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));

    i2s_event_callbacks_t cbs = {
        .on_recv = i2s_rx_callback,
        .on_recv_q_ovf = NULL,
        .on_sent = i2s_tx_callback,
        .on_send_q_ovf = NULL,
    };
    ESP_ERROR_CHECK(i2s_channel_register_event_callback(rx_handle, &cbs, NULL));
    ESP_ERROR_CHECK(i2s_channel_register_event_callback(tx_handle, &cbs, NULL));

    log_info("i2s init samplerate %" PRIu32 ", samples per DMA buffer: %" PRIu32,
        btstack_audio_esp32_sink_samplerate, chan_cfg.dma_frame_num);

#ifdef CONFIG_ESP_LYRAT_V4_3_BOARD
    btstack_audio_esp32_es8388_init();
#endif

    btstack_audio_esp32_i2s_installed = true;

    btstack_run_loop_set_data_source_handler(&transport_data_source, i2s_data_process);
}

static void btstack_audio_esp32_deinit(void){
    if  (btstack_audio_esp32_i2s_installed == false) return;

    // check if still needed
    bool still_needed = (btstack_audio_esp32_sink_state   != BTSTACK_AUDIO_ESP32_OFF)
                    ||  (btstack_audio_esp32_source_state != BTSTACK_AUDIO_ESP32_OFF);
    if (still_needed) return;

    log_info("i2s close");

    ESP_ERROR_CHECK(i2s_del_channel(rx_handle));
    ESP_ERROR_CHECK(i2s_del_channel(tx_handle));

    btstack_run_loop_disable_data_source_callbacks(&transport_data_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_remove_data_source(&transport_data_source);

    btstack_audio_esp32_i2s_installed = false;
}

static int btstack_audio_esp32_sink_init(
    uint8_t channels,
    uint32_t samplerate,
    void (*playback)(int16_t * buffer, uint16_t num_samples)){

    btstack_assert(playback != NULL);
    btstack_assert((1 <= channels) && (channels <= 2));
    btstack_assert( samplerate <= 48000 );

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

    // note: conceptually, it would make sense to pre-fill all I2S buffers and then feed new ones when they are
    // marked as complete. But the corresponding function i2s_channel_preload_data is not in the v5.0.1 release
    // version

    btstack_audio_esp32_stream_start();
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
}

static void btstack_audio_esp32_sink_stop_stream(void){

    if (btstack_audio_esp32_sink_state != BTSTACK_AUDIO_ESP32_STREAMING) return;

    // state
    btstack_audio_esp32_sink_state = BTSTACK_AUDIO_ESP32_INITIALIZED;

    ESP_ERROR_CHECK(i2s_channel_disable(tx_handle));
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
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
}

static void btstack_audio_esp32_source_stop_stream(void){

    if (btstack_audio_esp32_source_state != BTSTACK_AUDIO_ESP32_STREAMING) return;

    // state
    btstack_audio_esp32_source_state = BTSTACK_AUDIO_ESP32_INITIALIZED;

    ESP_ERROR_CHECK(i2s_channel_disable(rx_handle));
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
