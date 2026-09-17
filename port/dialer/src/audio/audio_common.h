#ifndef AUDIO_COMMON_H
#define AUDIO_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

#ifdef _WIN32
#include <windows.h>
typedef CRITICAL_SECTION audio_mutex_t;

static inline void audio_mutex_init(audio_mutex_t *m) {
    InitializeCriticalSection(m);
}

static inline void audio_mutex_destroy(audio_mutex_t *m) {
    DeleteCriticalSection(m);
}

static inline void audio_mutex_lock(audio_mutex_t *m) {
    EnterCriticalSection(m);
}

static inline void audio_mutex_unlock(audio_mutex_t *m) {
    LeaveCriticalSection(m);
}
#else
#include <pthread.h>
typedef pthread_mutex_t audio_mutex_t;

static inline void audio_mutex_init(audio_mutex_t *m) {
    pthread_mutex_init(m, NULL);
}

static inline void audio_mutex_destroy(audio_mutex_t *m) {
    pthread_mutex_destroy(m);
}

static inline void audio_mutex_lock(audio_mutex_t *m) {
    pthread_mutex_lock(m);
}

static inline void audio_mutex_unlock(audio_mutex_t *m) {
    pthread_mutex_unlock(m);
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Cross-platform thread-safe ring buffer for 16-bit PCM audio samples
typedef struct {
    int16_t *buffer;
    int capacity;
    int read_pos;
    int write_pos;
    audio_mutex_t mutex;
} audio_ring_buffer_t;

static inline void audio_ring_buffer_init(audio_ring_buffer_t *rb, int capacity) {
    rb->capacity = capacity;
    rb->buffer = (int16_t *)malloc(capacity * sizeof(int16_t));
    rb->read_pos = 0;
    rb->write_pos = 0;
    audio_mutex_init(&rb->mutex);
}

static inline void audio_ring_buffer_free(audio_ring_buffer_t *rb) {
    audio_mutex_destroy(&rb->mutex);
    if (rb->buffer) {
        free(rb->buffer);
        rb->buffer = NULL;
    }
}

static inline void audio_ring_buffer_push(audio_ring_buffer_t *rb, const int16_t *samples, int count) {
    audio_mutex_lock(&rb->mutex);
    for (int i = 0; i < count; i++) {
        int next_write = (rb->write_pos + 1) % rb->capacity;
        if (next_write != rb->read_pos) {
            rb->buffer[rb->write_pos] = samples[i];
            rb->write_pos = next_write;
        } else {
            // Buffer overrun: drop oldest sample to maintain low latency
            rb->read_pos = (rb->read_pos + 1) % rb->capacity;
            rb->buffer[rb->write_pos] = samples[i];
            rb->write_pos = next_write;
        }
    }
    audio_mutex_unlock(&rb->mutex);
}

static inline int audio_ring_buffer_pop(audio_ring_buffer_t *rb, int16_t *out_samples, int count) {
    audio_mutex_lock(&rb->mutex);
    int popped = 0;
    while (popped < count && rb->read_pos != rb->write_pos) {
        out_samples[popped++] = rb->buffer[rb->read_pos];
        rb->read_pos = (rb->read_pos + 1) % rb->capacity;
    }
    audio_mutex_unlock(&rb->mutex);
    return popped;
}

static inline int audio_ring_buffer_available(audio_ring_buffer_t *rb) {
    audio_mutex_lock(&rb->mutex);
    int count = (rb->write_pos - rb->read_pos + rb->capacity) % rb->capacity;
    audio_mutex_unlock(&rb->mutex);
    return count;
}

static inline void audio_ring_buffer_clear(audio_ring_buffer_t *rb) {
    audio_mutex_lock(&rb->mutex);
    rb->read_pos = 0;
    rb->write_pos = 0;
    audio_mutex_unlock(&rb->mutex);
}

// Soft limiter to prevent digital clipping when boosting microphone or speaker gain
static inline int16_t audio_soft_clip(float sample) {
    if (sample > 32767.0f) sample = 32767.0f;
    if (sample < -32768.0f) sample = -32768.0f;
    return (int16_t)sample;
}

#ifdef __cplusplus
}
#endif

#endif // AUDIO_COMMON_H
