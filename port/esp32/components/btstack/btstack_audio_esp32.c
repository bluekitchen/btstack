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
#include "driver/i2s.h"

#define DRIVER_POLL_INTERVAL_MS          5
#define DMA_BUFFER_COUNT                 2
#define DMA_BUFFER_SAMPLES               512
#define BYTES_PER_SAMPLE_STEREO          4

static int num_channels;
static int bytes_per_sample;

// client
static void (*playback_callback)(int16_t * buffer, uint16_t num_samples);
// static void (*recording_callback)(const int16_t * buffer, uint16_t num_samples);

// timer to fill output ring buffer
static btstack_timer_source_t  driver_timer;

// queue for TX done events
static QueueHandle_t i2s_event_queue;

static int i2s_num = I2S_NUM_0;

static int sink_streaming;

static void fill_buffer(void){
    size_t bytes_written;
    uint8_t buffer[DMA_BUFFER_SAMPLES * BYTES_PER_SAMPLE_STEREO];
    (*playback_callback)((int16_t*)buffer, DMA_BUFFER_SAMPLES);
    i2s_write(i2s_num, buffer, DMA_BUFFER_SAMPLES * bytes_per_sample, &bytes_written, portMAX_DELAY);
}

static void driver_timer_handler(btstack_timer_source_t * ts){

    // playback buffer ready to fill
    if (playback_callback){
        // read from i2s event queue
        i2s_event_t i2s_event;
        if( xQueueReceive( i2s_event_queue, &i2s_event, 0) ){
            // fill buffer when DMA buffer was sent            
            if (i2s_event.type == I2S_EVENT_TX_DONE){
                fill_buffer();
            }
        }
    }

    // re-set timer
    btstack_run_loop_set_timer(ts, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(ts);
}

static int btstack_audio_esp32_sink_init(
    uint8_t channels,
    uint32_t samplerate, 
    void (*playback)(int16_t * buffer, uint16_t num_samples)){

    playback_callback  = playback;
    num_channels       = channels;
    bytes_per_sample   = channels * 2;  // 16-bit

    i2s_config_t config = 
    {
        .mode                 = I2S_MODE_MASTER | I2S_MODE_TX, // Playback only
        .sample_rate          = samplerate,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB,
        .dma_buf_count        = DMA_BUFFER_COUNT,              // Number of DMA buffers. Max 128.
        .dma_buf_len          = DMA_BUFFER_SAMPLES,            // Size of each DMA buffer in samples. Max 1024.
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1
    };
    i2s_pin_config_t pins = 
    {
        .bck_io_num           = 26,
        .ws_io_num            = 25,
        .data_out_num         = 22,
        .data_in_num          = I2S_PIN_NO_CHANGE
    };

    i2s_driver_install(i2s_num, &config, DMA_BUFFER_COUNT, &i2s_event_queue);
    i2s_set_pin(i2s_num, &pins);
    i2s_zero_dma_buffer(i2s_num);

    return 0;
}

static void btstack_audio_esp32_sink_start_stream(void){

    if (playback_callback){
        // pre-fill HAL buffers
        log_info("start stream");
        uint16_t i;
        for (i=0;i<DMA_BUFFER_COUNT;i++){
            fill_buffer();
        }
    }

    // TODO: setup input

    // start i2s
    i2s_start(i2s_num);

    // start timer
    btstack_run_loop_set_timer_handler(&driver_timer, &driver_timer_handler);
    btstack_run_loop_set_timer(&driver_timer, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(&driver_timer);

    // state
    sink_streaming = 1;
}

static void btstack_audio_esp32_sink_stop_stream(void){

    if (!sink_streaming) return;

    // stop timer
    btstack_run_loop_remove_timer(&driver_timer);

    // stop i2s
     i2s_stop(i2s_num);

    // state
    sink_streaming = 0;
}

static void btstack_audio_esp32_sink_close(void){

    if (sink_streaming) {
        btstack_audio_esp32_sink_stop_stream();
    }

    // uninstall driver
    i2s_driver_uninstall(i2s_num);
}

static const btstack_audio_sink_t btstack_audio_sink_esp32 = {
    /* int (*init)(..);*/                                       &btstack_audio_esp32_sink_init,
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
