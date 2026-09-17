#ifndef AUDIO_RENDER_H
#define AUDIO_RENDER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize speaker playback engine (WASAPI on Windows, CoreAudio on macOS).
 * @return 0 on success, negative error on failure.
 */
int audio_render_init(void);

/**
 * @brief Start the speaker rendering worker thread or audio queue.
 */
int audio_render_start(void);

/**
 * @brief Stop speaker rendering.
 */
void audio_render_stop(void);

/**
 * @brief Push 16-bit mono PCM samples from Bluetooth SCO RX into the speaker buffer.
 */
void audio_render_push_samples(const int16_t *samples, int count);

/**
 * @brief Set the input sample rate for resampling (e.g., 16000 for mSBC, 8000 for CVSD).
 */
void audio_render_set_source_sample_rate(uint32_t sample_rate);

/**
 * @brief Adjust speaker digital gain factor [0.0 .. 4.0].
 */
void audio_render_set_volume(float volume);

/**
 * @brief Shutdown and release speaker render resources.
 */
void audio_render_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_RENDER_H
