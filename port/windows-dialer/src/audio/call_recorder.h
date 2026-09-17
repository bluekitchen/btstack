#ifndef CALL_RECORDER_H
#define CALL_RECORDER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the call recorder subsystem.
 */
void call_recorder_init(void);

/**
 * @brief Start recording a new call session.
 * @param sample_rate Audio sample rate (e.g., 16000 for mSBC, 8000 for CVSD).
 * @param call_id_prefix Optional identifier prefix (e.g., phone number or "call").
 */
void call_recorder_start(uint32_t sample_rate, const char *call_id_prefix);

/**
 * @brief Push incoming audio (speaker/caller) PCM samples.
 * @param samples 16-bit linear PCM samples.
 * @param num_samples Number of samples.
 */
void call_recorder_write_rx(const int16_t *samples, int num_samples);

/**
 * @brief Push outgoing audio (microphone/you) PCM samples.
 * @param samples 16-bit linear PCM samples.
 * @param num_samples Number of samples.
 */
void call_recorder_write_tx(const int16_t *samples, int num_samples);

/**
 * @brief Stop current call recording and finalize all FLAC/WAV files.
 */
void call_recorder_stop(void);

/**
 * @brief Check if recording is currently active.
 */
bool call_recorder_is_active(void);

/**
 * @brief Get total recorded duration in seconds for current/last call.
 */
double call_recorder_get_duration_seconds(void);

void call_recorder_set_recordings_root_dir(const char *path);

typedef void (*call_recorder_started_callback_t)(const char *session_dir, const char *stereo_wav, const char *rx_wav, const char *tx_wav);
typedef void (*call_recorder_stopped_callback_t)(const char *session_dir, const char *stereo_wav, const char *rx_wav, const char *tx_wav, double duration_sec);
typedef void (*call_recorder_segment_ready_callback_t)(const char *wav_path, const char *channel, double start_sec, double end_sec);

/**
 * @brief Register lifecycle and chunk event callbacks for call recording.
 */
void call_recorder_set_callbacks(call_recorder_started_callback_t on_started,
                                 call_recorder_stopped_callback_t on_stopped,
                                 call_recorder_segment_ready_callback_t on_segment_ready);

#ifdef __cplusplus
}
#endif

#endif // CALL_RECORDER_H
