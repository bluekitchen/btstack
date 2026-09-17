#include "audio_capture.h"
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
static bool s_is_running = false;
static volatile uint32_t s_target_sample_rate = 8000;
static float s_mic_gain = 2.5f; // Default digital gain boost (2.5x)
static audio_capture_pcm_callback_t s_capture_cb = NULL;

static float s_resample_phase = 0.0f;
static float s_mono_accum = 0.0f;
static int s_accum_count = 0;

static void capture_callback(void *user_data,
                             AudioQueueRef inAQ,
                             AudioQueueBufferRef inBuffer,
                             const AudioTimeStamp *inStartTime,
                             UInt32 inNumberPacketDescriptions,
                             const AudioStreamPacketDescription *inPacketDescs) {
    UNUSED(user_data);
    UNUSED(inStartTime);
    UNUSED(inNumberPacketDescriptions);
    UNUSED(inPacketDescs);

    if (!s_is_running || inBuffer->mAudioDataByteSize == 0) {
        AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL);
        return;
    }

    uint32_t num_frames = inBuffer->mAudioDataByteSize / sizeof(int16_t);
    const int16_t *in_samples = (const int16_t *)inBuffer->mAudioData;

    float target_rate = (float)s_target_sample_rate;
    if (target_rate <= 0.0f) target_rate = 8000.0f;
    float step = (float)DEVICE_SAMPLE_RATE / target_rate;

    int16_t downsample_buffer[2048];
    int out_count = 0;

    for (uint32_t i = 0; i < num_frames; i++) {
        s_mono_accum += (float)in_samples[i];
        s_accum_count++;
        s_resample_phase += 1.0f;

        if (s_resample_phase >= step) {
            s_resample_phase -= step;
            float avg = (s_accum_count > 0) ? (s_mono_accum / (float)s_accum_count) : 0.0f;
            avg *= s_mic_gain;

            if (out_count < 2048) {
                downsample_buffer[out_count++] = audio_soft_clip(avg);
            }
            s_mono_accum = 0.0f;
            s_accum_count = 0;
        }
    }

    if (out_count > 0 && s_capture_cb) {
        s_capture_cb(downsample_buffer, out_count);
    }

    AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL);
}

int audio_capture_init(audio_capture_pcm_callback_t pcm_callback) {
    s_capture_cb = pcm_callback;
    if (s_audio_queue) return 0;

    AudioStreamBasicDescription format;
    memset(&format, 0, sizeof(format));
    format.mFormatID          = kAudioFormatLinearPCM;
    format.mSampleRate        = DEVICE_SAMPLE_RATE;
    format.mFormatFlags       = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    format.mBitsPerChannel    = 16;
    format.mChannelsPerFrame  = 1; // Mono
    format.mBytesPerFrame     = sizeof(int16_t);
    format.mFramesPerPacket   = 1;
    format.mBytesPerPacket    = format.mBytesPerFrame;

    OSStatus status = AudioQueueNewInput(&format, capture_callback, NULL, NULL, NULL, 0, &s_audio_queue);
    if (status != noErr) {
        diag_log("[COREAUDIO_CAPTURE] ERROR: AudioQueueNewInput failed with status %d", (int)status);
        return -1;
    }

    uint32_t buffer_byte_size = BUFFER_FRAMES * format.mBytesPerFrame;
    for (int i = 0; i < BUFFER_COUNT; i++) {
        status = AudioQueueAllocateBuffer(s_audio_queue, buffer_byte_size, &s_buffers[i]);
        if (status != noErr) {
            diag_log("[COREAUDIO_CAPTURE] ERROR: AudioQueueAllocateBuffer[%d] failed with status %d", i, (int)status);
            return -2;
        }
        AudioQueueEnqueueBuffer(s_audio_queue, s_buffers[i], 0, NULL);
    }

    diag_log("[COREAUDIO_CAPTURE] CoreAudio capture engine initialized successfully (48 kHz Mono -> %u Hz)", s_target_sample_rate);
    return 0;
}

int audio_capture_start(void) {
    if (!s_audio_queue) return -1;
    if (s_is_running) return 0;

    s_is_running = true;
    OSStatus status = AudioQueueStart(s_audio_queue, NULL);
    if (status != noErr) {
        diag_log("[COREAUDIO_CAPTURE] ERROR: AudioQueueStart failed with status %d", (int)status);
        s_is_running = false;
        return -1;
    }
    diag_log("[COREAUDIO_CAPTURE] Microphone capture started");
    return 0;
}

void audio_capture_stop(void) {
    if (!s_audio_queue || !s_is_running) return;

    s_is_running = false;
    AudioQueueStop(s_audio_queue, false);
    diag_log("[COREAUDIO_CAPTURE] Microphone capture stopped");
}

void audio_capture_set_target_sample_rate(uint32_t sample_rate) {
    s_target_sample_rate = sample_rate;
}

void audio_capture_set_gain(float gain) {
    s_mic_gain = gain;
}

void audio_capture_shutdown(void) {
    if (s_audio_queue) {
        audio_capture_stop();
        AudioQueueDispose(s_audio_queue, true);
        s_audio_queue = NULL;
    }
    diag_log("[COREAUDIO_CAPTURE] CoreAudio capture engine shutdown complete");
}
