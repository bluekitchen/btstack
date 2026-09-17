#ifndef AUDIO_CAPTURE_H
#define AUDIO_CAPTURE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*audio_capture_pcm_callback_t)(const int16_t *samples, int count);

/**
 * @brief Initialize microphone capture engine (WASAPI on Windows, CoreAudio on macOS).
 * @param pcm_callback Callback invoked with 8kHz / 16kHz 16-bit mono PCM microphone samples.
 * @return 0 on success, negative error on failure.
 */
int audio_capture_init(audio_capture_pcm_callback_t pcm_callback);

/**
 * @brief Start microphone capture.
 */
int audio_capture_start(void);

/**
 * @brief Stop microphone capture.
 */
void audio_capture_stop(void);

/**
 * @brief Set the target output sample rate (8000 Hz for CVSD, 16000 Hz for mSBC).
 */
void audio_capture_set_target_sample_rate(uint32_t sample_rate);

/**
 * @brief Set microphone digital gain multiplier [0.0 .. 8.0].
 */
void audio_capture_set_gain(float gain);

/**
 * @brief Shutdown and release microphone capture resources.
 */
void audio_capture_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_CAPTURE_H
