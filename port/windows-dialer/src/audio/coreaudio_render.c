#include "audio_render.h"
#include "audio_common.h"
#include "diag_logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <AudioToolbox/AudioToolbox.h>

#define BUFFER_COUNT 3
#define BUFFER_FRAMES 960 // 20ms at 48kHz
#define DEVICE_SAMPLE_RATE 48000.0

static AudioQueueRef s_audio_queue = NULL;
static AudioQueueBufferRef s_buffers[BUFFER_COUNT];
static audio_ring_buffer_t s_render_ring_buf;
static bool s_is_running = false;
static float s_volume = 1.0f;
static volatile uint32_t s_src_sample_rate = 8000;

static float s_resample_phase = 0.0f;
static int16_t s_sample_curr = 0;
static int16_t s_sample_next = 0;

static void render_callback(void *user_data, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer) {
    UNUSED(user_data);

    if (!s_is_running) return;

    uint32_t frames_needed = inBuffer->mAudioDataBytesCapacity / (sizeof(int16_t) * 2);
    int16_t *out_ptr = (int16_t *)inBuffer->mAudioData;

    float src_rate = (float)s_src_sample_rate;
    if (src_rate <= 0.0f) src_rate = 8000.0f;
    float step = src_rate / (float)DEVICE_SAMPLE_RATE;

    for (uint32_t i = 0; i < frames_needed; i++) {
        while (s_resample_phase >= 1.0f) {
            s_resample_phase -= 1.0f;
            s_sample_curr = s_sample_next;
            if (audio_ring_buffer_pop(&s_render_ring_buf, &s_sample_next, 1) == 0) {
                s_sample_next = 0; // Underflow
            }
        }

        float interp = (float)s_sample_curr + s_resample_phase * (float)(s_sample_next - s_sample_curr);
        interp *= s_volume;
        int16_t out_val = audio_soft_clip(interp);

        out_ptr[i * 2]     = out_val; // Left
        out_ptr[i * 2 + 1] = out_val; // Right

        s_resample_phase += step;
    }

    inBuffer->mAudioDataByteSize = frames_needed * sizeof(int16_t) * 2;
    AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL);
}

int audio_render_init(void) {
    if (s_audio_queue) return 0;

    audio_ring_buffer_init(&s_render_ring_buf, 32768);

    AudioStreamBasicDescription format;
    memset(&format, 0, sizeof(format));
    format.mFormatID          = kAudioFormatLinearPCM;
    format.mSampleRate        = DEVICE_SAMPLE_RATE;
    format.mFormatFlags       = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    format.mBitsPerChannel    = 16;
    format.mChannelsPerFrame  = 2; // Stereo
    format.mBytesPerFrame     = sizeof(int16_t) * 2;
    format.mFramesPerPacket   = 1;
    format.mBytesPerPacket    = format.mBytesPerFrame;

    OSStatus status = AudioQueueNewOutput(&format, render_callback, NULL, NULL, NULL, 0, &s_audio_queue);
    if (status != noErr) {
        diag_log("[COREAUDIO_RENDER] ERROR: AudioQueueNewOutput failed with status %d", (int)status);
        return -1;
    }

    uint32_t buffer_byte_size = BUFFER_FRAMES * format.mBytesPerFrame;
    for (int i = 0; i < BUFFER_COUNT; i++) {
        status = AudioQueueAllocateBuffer(s_audio_queue, buffer_byte_size, &s_buffers[i]);
        if (status != noErr) {
            diag_log("[COREAUDIO_RENDER] ERROR: AudioQueueAllocateBuffer[%d] failed with status %d", i, (int)status);
            return -2;
        }
        memset(s_buffers[i]->mAudioData, 0, buffer_byte_size);
        s_buffers[i]->mAudioDataByteSize = buffer_byte_size;
        AudioQueueEnqueueBuffer(s_audio_queue, s_buffers[i], 0, NULL);
    }

    diag_log("[COREAUDIO_RENDER] CoreAudio render engine initialized successfully (48 kHz Stereo)");
    return 0;
}

int audio_render_start(void) {
    if (!s_audio_queue) return -1;
    if (s_is_running) return 0;

    s_is_running = true;
    OSStatus status = AudioQueueStart(s_audio_queue, NULL);
    if (status != noErr) {
        diag_log("[COREAUDIO_RENDER] ERROR: AudioQueueStart failed with status %d", (int)status);
        s_is_running = false;
        return -1;
    }
    diag_log("[COREAUDIO_RENDER] Speaker playback started");
    return 0;
}

void audio_render_stop(void) {
    if (!s_audio_queue || !s_is_running) return;

    s_is_running = false;
    AudioQueueStop(s_audio_queue, false);
    diag_log("[COREAUDIO_RENDER] Speaker playback stopped");
}

void audio_render_push_samples(const int16_t *samples, int count) {
    if (!samples || count <= 0) return;
    audio_ring_buffer_push(&s_render_ring_buf, samples, count);
}

void audio_render_set_source_sample_rate(uint32_t sample_rate) {
    s_src_sample_rate = sample_rate;
}

void audio_render_set_volume(float volume) {
    s_volume = volume;
}

void audio_render_shutdown(void) {
    if (s_audio_queue) {
        audio_render_stop();
        AudioQueueDispose(s_audio_queue, true);
        s_audio_queue = NULL;
    }
    audio_ring_buffer_free(&s_render_ring_buf);
    diag_log("[COREAUDIO_RENDER] CoreAudio render engine shutdown complete");
}
