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
#include "hal_audio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/i2s.h"
#include <string.h>

#ifdef CONFIG_ESP_LYRAT_V4_3_BOARD
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "es8388.h"
#define IIC_DATA                    (GPIO_NUM_18)
#define IIC_CLK                     (GPIO_NUM_23)
#endif

#define DRIVER_POLL_INTERVAL_MS          5
#define DMA_BUFFER_COUNT                 3
#define DMA_BUFFER_SAMPLES               300
#define BYTES_PER_SAMPLE_STEREO          4

static int i2s_num = I2S_NUM_0;

typedef enum {
    BTSTACK_AUDIO_ESP32_OFF = 0,
    BTSTACK_AUDIO_ESP32_INITIALIZED,
    BTSTACK_AUDIO_ESP32_STREAMING
} btstack_audio_esp32_state_t;

static bool btstack_audio_esp32_i2s_installed;

// timer to fill output ring buffer
static btstack_timer_source_t  btstack_audio_esp32_driver_timer;

static uint8_t  btstack_audio_esp32_sink_num_channels;
static uint8_t  btstack_audio_esp32_sink_bytes_per_sample;
static uint32_t btstack_audio_esp32_sink_samplerate;

static btstack_audio_esp32_state_t btstack_audio_esp32_sink_state;

// client
static void (*btstack_audio_esp32_sink_playback_callback)(int16_t * buffer, uint16_t num_samples);
// static void (*btstack_audio_esp32_source_recording_callback)(const int16_t * buffer, uint16_t num_samples);

// queue for TX done events
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

static void btstack_audio_esp32_sink_fill_buffer(void){
    size_t bytes_written;
    uint8_t buffer[DMA_BUFFER_SAMPLES * BYTES_PER_SAMPLE_STEREO];
    if ((btstack_audio_esp32_sink_playback_callback != NULL) && (btstack_audio_esp32_sink_state == BTSTACK_AUDIO_ESP32_STREAMING)){
        (*btstack_audio_esp32_sink_playback_callback)((int16_t *) buffer, DMA_BUFFER_SAMPLES);
    } else {
        memset(buffer, 0, sizeof(buffer));
    }
    i2s_write(i2s_num, buffer, DMA_BUFFER_SAMPLES * btstack_audio_esp32_sink_bytes_per_sample, &bytes_written, portMAX_DELAY);
}

static void btstack_audio_esp32_driver_timer_handler(btstack_timer_source_t * ts){
    // read from i2s event queue
    i2s_event_t i2s_event;
    if( xQueueReceive( btstack_audio_esp32_i2s_event_queue, &i2s_event, 0) ){
        // fill buffer when DMA buffer was sent
        if (i2s_event.type == I2S_EVENT_TX_DONE){
            btstack_audio_esp32_sink_fill_buffer();
        }
    }

    // re-set timer
    btstack_run_loop_set_timer(ts, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(ts);
}

static void btstack_audio_esp32_init(void){

    // de-register driver if already installed
    if (btstack_audio_esp32_i2s_installed){
        i2s_driver_uninstall(i2s_num);
    }

    i2s_config_t config =
    {
        .mode                 = I2S_MODE_MASTER | I2S_MODE_TX, // Playback only
        .sample_rate          = btstack_audio_esp32_sink_samplerate,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .dma_buf_count        = DMA_BUFFER_COUNT,              // Number of DMA buffers. Max 128.
        .dma_buf_len          = DMA_BUFFER_SAMPLES,            // Size of each DMA buffer in samples. Max 1024.
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1
    };

#ifdef CONFIG_ESP_LYRAT_V4_3_BOARD
    i2s_pin_config_t pins =
    {
        .bck_io_num           = GPIO_NUM_5,
        .ws_io_num            = GPIO_NUM_25,
        .data_out_num         = GPIO_NUM_26,
        .data_in_num          = GPIO_NUM_35
    };
#else
    i2s_pin_config_t pins =
    {
        .bck_io_num           = 26,
        .ws_io_num            = 25,
        .data_out_num         = 22,
        .data_in_num          = I2S_PIN_NO_CHANGE
    };
#endif

#ifdef CONFIG_ESP_LYRAT_V4_3_BOARD
    btstack_audio_esp32_set_i2s0_mclk();
#endif

    i2s_driver_install(i2s_num, &config, DMA_BUFFER_COUNT, &btstack_audio_esp32_i2s_event_queue);
    i2s_set_pin(i2s_num, &pins);
    i2s_zero_dma_buffer(i2s_num);

#ifdef CONFIG_ESP_LYRAT_V4_3_BOARD
    btstack_audio_esp32_es8388_init();
#endif

    btstack_audio_esp32_i2s_installed = true;
}

static int btstack_audio_esp32_sink_init(
    uint8_t channels,
    uint32_t samplerate, 
    void (*playback)(int16_t * buffer, uint16_t num_samples)){

    // store config
    btstack_audio_esp32_sink_playback_callback  = playback;
    btstack_audio_esp32_sink_num_channels       = channels;
    btstack_audio_esp32_sink_bytes_per_sample   = channels * 2;  // 16-bit
    btstack_audio_esp32_sink_samplerate         = samplerate;

    btstack_audio_esp32_sink_state = BTSTACK_AUDIO_ESP32_INITIALIZED;

    // init i2s and codec
    btstack_audio_esp32_init();
    return 0;
}

static void btstack_audio_esp32_sink_set_volume(uint8_t gain) {
#ifdef CONFIG_ESP_LYRAT_V4_3_BOARD
    uint8_t volume_0_100 = (uint8_t) ((((uint16_t) gain) * 100) / 128);
    es8388_set_volume( volume_0_100 );
#else
    UNUSED(gain);
#endif
}

static void btstack_audio_esp32_sink_start_stream(void){

    if (btstack_audio_esp32_sink_playback_callback){
        // pre-fill HAL buffers
        log_info("start stream");
        uint16_t i;
        for (i=0;i<DMA_BUFFER_COUNT;i++){
            btstack_audio_esp32_sink_fill_buffer();
        }
    }

    // start i2s
    i2s_start(i2s_num);

    // start timer
    btstack_run_loop_set_timer_handler(&btstack_audio_esp32_driver_timer, &btstack_audio_esp32_driver_timer_handler);
    btstack_run_loop_set_timer(&btstack_audio_esp32_driver_timer, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(&btstack_audio_esp32_driver_timer);

    // state
    btstack_audio_esp32_sink_state = BTSTACK_AUDIO_ESP32_STREAMING;
}

static void btstack_audio_esp32_sink_stop_stream(void){

    if (btstack_audio_esp32_sink_state != BTSTACK_AUDIO_ESP32_STREAMING) return;

    // stop timer
    btstack_run_loop_remove_timer(&btstack_audio_esp32_driver_timer);

    // stop i2s
    i2s_stop(i2s_num);

    // state
    btstack_audio_esp32_sink_state = BTSTACK_AUDIO_ESP32_INITIALIZED;
}

static void btstack_audio_esp32_sink_close(void){

    if (btstack_audio_esp32_sink_state == BTSTACK_AUDIO_ESP32_STREAMING) {
        btstack_audio_esp32_sink_stop_stream();
    }

    btstack_audio_esp32_sink_state = BTSTACK_AUDIO_ESP32_OFF;

    // uninstall driver
    i2s_driver_uninstall(i2s_num);

    btstack_audio_esp32_i2s_installed = false;
}

static const btstack_audio_sink_t btstack_audio_sink_esp32 = {
    /* int (*init)(..);*/                                       &btstack_audio_esp32_sink_init,
    /* void (*set_volume)(uint8_t gain); */                     &btstack_audio_esp32_sink_set_volume,
    /* void (*start_stream(void));*/                            &btstack_audio_esp32_sink_start_stream,
    /* void (*stop_stream)(void)  */                            &btstack_audio_esp32_sink_stop_stream,
    /* void (*close)(void); */                                  &btstack_audio_esp32_sink_close
};

#if 0
static int btstack_audio_esp32_source_init(
    uint8_t channels,
    uint32_t samplerate, 
    void (*recording)(const int16_t * buffer, uint16_t num_samples)
){
    playback_callback  = playback;
    recording_callback = recording;
    num_channels       = channels;
    bytes_per_sample   = channels * 2;  // 16-bit
    ...
}
static const btstack_audio_source_t btstack_audio_source_esp32 = {
    /* int (*init)(..);*/                                       &btstack_audio_esp32_source_init,
    /* void (*set_gain)(uint8_t gain); */                       &btstack_audio_esp32_source_set_gain,
    /* void (*start_stream(void));*/                            &btstack_audio_esp32_source_start_stream,
    /* void (*stop_stream)(void)  */                            &btstack_audio_esp32_source_stop_stream,
    /* void (*close)(void); */                                  &btstack_audio_esp32_source_close
};
#endif

const btstack_audio_sink_t * btstack_audio_esp32_sink_get_instance(void){
    return &btstack_audio_sink_esp32;
}

#if 0
const btstack_audio_source_t * btstack_audio_esp32_source_get_instance(void){
    return &btstack_audio_esp32_source;
}
#endif
