#include "audio/call_recorder.h"
#include "audio/flac_encoder.h"
#include "audio/audio_common.h"
#include "diag_logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

static audio_mutex_t s_recorder_mutex;
static bool s_recorder_mutex_initialized = false;

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

#pragma pack(push, 1)
typedef struct {
    char riff_id[4];        // "RIFF"
    uint32_t riff_size;     // File size - 8
    char wave_id[4];        // "WAVE"
    char fmt_id[4];         // "fmt "
    uint32_t fmt_size;      // 16
    uint16_t audio_format;  // 1 = PCM
    uint16_t num_channels;  // 1 or 2
    uint32_t sample_rate;   // 16000 or 8000
    uint32_t byte_rate;     // sample_rate * num_channels * 2
    uint16_t block_align;   // num_channels * 2
    uint16_t bits_per_sample; // 16
    char data_id[4];        // "data"
    uint32_t data_size;     // num_samples * num_channels * 2
} wav_file_header_t;
#pragma pack(pop)

typedef struct {
    FILE *f;
    uint32_t sample_rate;
    uint16_t channels;
    uint32_t num_samples;
} wav_stream_t;

static void wav_stream_open(wav_stream_t *w, const char *filepath, uint32_t sample_rate, uint16_t channels) {
    memset(w, 0, sizeof(*w));
    w->f = fopen(filepath, "wb");
    if (!w->f) return;

    w->sample_rate = sample_rate;
    w->channels = channels;
    w->num_samples = 0;

    wav_file_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    fwrite(&hdr, sizeof(hdr), 1, w->f);
}

static void wav_stream_write(wav_stream_t *w, const int16_t *samples, uint32_t num_samples) {
    if (!w || !w->f || num_samples == 0) return;
    fwrite(samples, sizeof(int16_t), num_samples * w->channels, w->f);
    w->num_samples += num_samples;
}

static void wav_stream_close(wav_stream_t *w) {
    if (!w || !w->f) return;

    wav_file_header_t hdr;
    memcpy(hdr.riff_id, "RIFF", 4);
    hdr.riff_size = 36 + w->num_samples * w->channels * sizeof(int16_t);
    memcpy(hdr.wave_id, "WAVE", 4);
    memcpy(hdr.fmt_id, "fmt ", 4);
    hdr.fmt_size = 16;
    hdr.audio_format = 1;
    hdr.num_channels = w->channels;
    hdr.sample_rate = w->sample_rate;
    hdr.bits_per_sample = 16;
    hdr.byte_rate = w->sample_rate * w->channels * sizeof(int16_t);
    hdr.block_align = w->channels * sizeof(int16_t);
    memcpy(hdr.data_id, "data", 4);
    hdr.data_size = w->num_samples * w->channels * sizeof(int16_t);

    fseek(w->f, 0, SEEK_SET);
    fwrite(&hdr, sizeof(hdr), 1, w->f);
    fclose(w->f);
    w->f = NULL;
}

// ---------------------------------------------------------------------------
// VAD Segmenter for Individual Channels ("in" = caller, "out" = mic)
// ---------------------------------------------------------------------------
typedef struct {
    char channel[8];           // "in" or "out"
    char dir[512];             // session directory
    uint32_t sample_rate;
    double silence_threshold;  // RMS threshold
    int pause_ms;              // e.g. 800 ms
    int max_chunk_ms;          // e.g. 20000 ms
    bool trim_silence;

    wav_stream_t cur_wav;
    bool is_open;
    char temp_path[512];
    double seg_start_sec;
    uint32_t seg_samples;
    int silence_ms;
    int seq;
    uint64_t total_samples_fed;
} vad_channel_t;

static double calc_rms(const int16_t *samples, int count) {
    if (count <= 0) return 0.0;
    double sum = 0;
    for (int i = 0; i < count; i++) {
        double s = (double)samples[i];
        sum += s * s;
    }
    return sqrt(sum / (double)count);
}

static void vad_channel_init(vad_channel_t *v, const char *channel, const char *dir, uint32_t sample_rate) {
    memset(v, 0, sizeof(*v));
    strncpy(v->channel, channel, sizeof(v->channel) - 1);
    strncpy(v->dir, dir, sizeof(v->dir) - 1);
    v->sample_rate = sample_rate;
    v->silence_threshold = 250.0; // sensitive voice detection threshold for 16-bit PCM
    v->pause_ms = 800;            // 800ms natural conversational pause
    v->max_chunk_ms = 20000;      // 20s max chunk cap
    v->trim_silence = true;
}

static call_recorder_segment_ready_callback_t s_segment_ready_cb = NULL;

static void vad_channel_open_seg(vad_channel_t *v) {
    v->seg_start_sec = (double)v->total_samples_fed / (double)v->sample_rate;
    v->seg_samples = 0;
    v->silence_ms = 0;
    snprintf(v->temp_path, sizeof(v->temp_path), "%s/.%s-%d.tmp.wav", v->dir, v->channel, v->seq++);
    wav_stream_open(&v->cur_wav, v->temp_path, v->sample_rate, 1);
    v->is_open = true;
}

static void vad_channel_close_seg(vad_channel_t *v, const char *reason) {
    if (!v->is_open) return;
    wav_stream_close(&v->cur_wav);
    v->is_open = false;

    if (v->seg_samples == 0) {
        remove(v->temp_path);
        return;
    }

    double start_sec = v->seg_start_sec;
    double end_sec = v->seg_start_sec + ((double)v->seg_samples / (double)v->sample_rate);
    int start = (int)floor(start_sec);
    int end = (int)ceil(end_sec);
    if (end <= start) end = start + 1;

    char dest_path[512];
    snprintf(dest_path, sizeof(dest_path), "%s/%s-%d-%d.wav", v->dir, v->channel, start, end);

    // Guard against collision
    int collision_idx = 2;
    while (1) {
        FILE *chk = fopen(dest_path, "rb");
        if (!chk) break;
        fclose(chk);
        snprintf(dest_path, sizeof(dest_path), "%s/%s-%d-%d_%d.wav", v->dir, v->channel, start, end, collision_idx++);
    }

    int ret = rename(v->temp_path, dest_path);
    if (ret != 0) {
        remove(dest_path);
        ret = rename(v->temp_path, dest_path);
    }

    if (ret == 0) {
        diag_log("[VAD:%s] %s -> saved %s (%.1fs)", v->channel, reason, dest_path, end_sec - start_sec);
        if (s_segment_ready_cb) {
            s_segment_ready_cb(dest_path, v->channel, start_sec, end_sec);
        }
    } else {
        diag_log("[VAD:%s] rename failed for %s", v->channel, v->temp_path);
    }
}

static void vad_channel_write(vad_channel_t *v, const int16_t *samples, int count) {
    if (count <= 0) return;
    double rms = calc_rms(samples, count);
    bool silent = (rms < v->silence_threshold);

    if (!v->is_open) {
        if (silent && v->trim_silence) {
            v->total_samples_fed += count;
            return;
        }
        vad_channel_open_seg(v);
    }

    wav_stream_write(&v->cur_wav, samples, (uint32_t)count);
    v->seg_samples += count;
    v->total_samples_fed += count;

    int frame_ms = (int)((count * 1000LL) / v->sample_rate);
    v->silence_ms = silent ? (v->silence_ms + frame_ms) : 0;
    int seg_ms = (int)(((uint64_t)v->seg_samples * 1000LL) / v->sample_rate);

    if (v->silence_ms >= v->pause_ms) {
        vad_channel_close_seg(v, "silence pause");
    } else if (seg_ms >= v->max_chunk_ms) {
        vad_channel_close_seg(v, "max chunk cap (20s)");
    }
}

// ---------------------------------------------------------------------------
// Call Recorder State
// ---------------------------------------------------------------------------
typedef struct {
    bool is_active;
    uint32_t sample_rate;
    char timestamp_str[64];
    char session_dir[512];
    char path_stereo_wav[512];
    char path_rx_wav[512];
    char path_tx_wav[512];

    // File streams
    flac_stream_encoder_t flac_rx;
    flac_stream_encoder_t flac_tx;
    flac_stream_encoder_t flac_stereo;

    wav_stream_t wav_rx;
    wav_stream_t wav_tx;
    wav_stream_t wav_stereo;

    // VAD Segmenters
    vad_channel_t vad_rx; // "in"  = speaker/caller
    vad_channel_t vad_tx; // "out" = microphone/you

    // Stereo interleave ring buffers
    #define STEREO_CHUNK_MAX 2048
    int16_t rx_queue[STEREO_CHUNK_MAX];
    uint32_t rx_q_count;
    int16_t tx_queue[STEREO_CHUNK_MAX];
    uint32_t tx_q_count;

    uint32_t total_rx_samples;
    uint32_t total_tx_samples;
    uint32_t total_stereo_frames;
} call_recorder_state_t;

static char s_recordings_root_dir[512] = "recordings";
static call_recorder_state_t s_recorder;
static call_recorder_started_callback_t s_started_cb = NULL;
static call_recorder_stopped_callback_t s_stopped_cb = NULL;

static void ensure_dir_exists(const char *dir) {
#ifdef _WIN32
    CreateDirectoryA(dir, NULL);
#else
    mkdir(dir, 0755);
#endif
}

void call_recorder_set_recordings_root_dir(const char *path) {
    if (path && strlen(path) > 0) {
        strncpy(s_recordings_root_dir, path, sizeof(s_recordings_root_dir) - 1);
        s_recordings_root_dir[sizeof(s_recordings_root_dir) - 1] = '\0';
        ensure_dir_exists(s_recordings_root_dir);
        diag_log("[RECORDER] Root recordings dir configured: %s", s_recordings_root_dir);
    }
}

void call_recorder_init(void) {
    if (!s_recorder_mutex_initialized) {
        audio_mutex_init(&s_recorder_mutex);
        s_recorder_mutex_initialized = true;
    }
    audio_mutex_lock(&s_recorder_mutex);
    memset(&s_recorder, 0, sizeof(s_recorder));
    ensure_dir_exists(s_recordings_root_dir);
    audio_mutex_unlock(&s_recorder_mutex);
}

void call_recorder_set_callbacks(call_recorder_started_callback_t on_started,
                                 call_recorder_stopped_callback_t on_stopped,
                                 call_recorder_segment_ready_callback_t on_segment_ready) {
    s_started_cb = on_started;
    s_stopped_cb = on_stopped;
    s_segment_ready_cb = on_segment_ready;
}

static void sanitize_filename(char *s) {
    for (char *p = s; *p; p++) {
        if (*p == '/' || *p == '\\' || *p == ':' || *p == '*' || *p == '?' || *p == '"' || *p == '<' || *p == '>' || *p == '|' || *p == ' ') {
            *p = '_';
        }
    }
}

void call_recorder_start(uint32_t sample_rate, const char *call_id_prefix) {
    if (!s_recorder_mutex_initialized) {
        audio_mutex_init(&s_recorder_mutex);
        s_recorder_mutex_initialized = true;
    }

    audio_mutex_lock(&s_recorder_mutex);
    if (s_recorder.is_active) {
        vad_channel_close_seg(&s_recorder.vad_rx, "call ended");
        vad_channel_close_seg(&s_recorder.vad_tx, "call ended");
        flac_stream_encoder_close(&s_recorder.flac_rx);
        flac_stream_encoder_close(&s_recorder.flac_tx);
        flac_stream_encoder_close(&s_recorder.flac_stereo);
        wav_stream_close(&s_recorder.wav_rx);
        wav_stream_close(&s_recorder.wav_tx);
        wav_stream_close(&s_recorder.wav_stereo);
        s_recorder.is_active = false;
    }

    ensure_dir_exists(s_recordings_root_dir);

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (!t) {
        snprintf(s_recorder.timestamp_str, sizeof(s_recorder.timestamp_str), "20260913_000000");
    } else {
        strftime(s_recorder.timestamp_str, sizeof(s_recorder.timestamp_str), "%Y%m%d_%H%M%S", t);
    }

    char clean_prefix[64] = "call";
    if (call_id_prefix && strlen(call_id_prefix) > 0) {
        strncpy(clean_prefix, call_id_prefix, sizeof(clean_prefix) - 1);
        clean_prefix[sizeof(clean_prefix) - 1] = '\0';
        sanitize_filename(clean_prefix);
    }

    snprintf(s_recorder.session_dir, sizeof(s_recorder.session_dir), "%s/call_%s_%s",
             s_recordings_root_dir, s_recorder.timestamp_str, clean_prefix);
    ensure_dir_exists(s_recorder.session_dir);

    char path_rx_flac[512], path_tx_flac[512], path_stereo_flac[512];

    snprintf(path_rx_flac, sizeof(path_rx_flac), "%s/speaker_caller.flac", s_recorder.session_dir);
    snprintf(path_tx_flac, sizeof(path_tx_flac), "%s/microphone_you.flac", s_recorder.session_dir);
    snprintf(path_stereo_flac, sizeof(path_stereo_flac), "%s/full_duplex_stereo.flac", s_recorder.session_dir);

    snprintf(s_recorder.path_rx_wav, sizeof(s_recorder.path_rx_wav), "%s/speaker_caller.wav", s_recorder.session_dir);
    snprintf(s_recorder.path_tx_wav, sizeof(s_recorder.path_tx_wav), "%s/microphone_you.wav", s_recorder.session_dir);
    snprintf(s_recorder.path_stereo_wav, sizeof(s_recorder.path_stereo_wav), "%s/full_duplex_stereo.wav", s_recorder.session_dir);

    s_recorder.sample_rate = sample_rate;
    s_recorder.total_rx_samples = 0;
    s_recorder.total_tx_samples = 0;
    s_recorder.total_stereo_frames = 0;
    s_recorder.rx_q_count = 0;
    s_recorder.tx_q_count = 0;

    // Initialize VAD segmenters for in/out channels
    vad_channel_init(&s_recorder.vad_rx, "in", s_recorder.session_dir, sample_rate);
    vad_channel_init(&s_recorder.vad_tx, "out", s_recorder.session_dir, sample_rate);

    // Open FLAC streams
    flac_stream_encoder_open(&s_recorder.flac_rx, path_rx_flac, sample_rate, 1);
    flac_stream_encoder_open(&s_recorder.flac_tx, path_tx_flac, sample_rate, 1);
    flac_stream_encoder_open(&s_recorder.flac_stereo, path_stereo_flac, sample_rate, 2);

    // Open WAV streams
    wav_stream_open(&s_recorder.wav_rx, s_recorder.path_rx_wav, sample_rate, 1);
    wav_stream_open(&s_recorder.wav_tx, s_recorder.path_tx_wav, sample_rate, 1);
    wav_stream_open(&s_recorder.wav_stereo, s_recorder.path_stereo_wav, sample_rate, 2);

    s_recorder.is_active = true;

    diag_log("[RECORDER] Started live call recording @ %u Hz (FLAC, WAV & VAD Chunks)", sample_rate);
    diag_log("[RECORDER] Session Dir: %s", s_recorder.session_dir);

    char session_dir[512], path_stereo[512], path_rx[512], path_tx[512];
    strncpy(session_dir, s_recorder.session_dir, sizeof(session_dir) - 1);
    session_dir[sizeof(session_dir) - 1] = '\0';
    strncpy(path_stereo, s_recorder.path_stereo_wav, sizeof(path_stereo) - 1);
    path_stereo[sizeof(path_stereo) - 1] = '\0';
    strncpy(path_rx, s_recorder.path_rx_wav, sizeof(path_rx) - 1);
    path_rx[sizeof(path_rx) - 1] = '\0';
    strncpy(path_tx, s_recorder.path_tx_wav, sizeof(path_tx) - 1);
    path_tx[sizeof(path_tx) - 1] = '\0';
    audio_mutex_unlock(&s_recorder_mutex);

    if (s_started_cb) {
        s_started_cb(session_dir, path_stereo, path_rx, path_tx);
    }
}

static void flush_stereo_buffer(void) {
    uint32_t ready_frames = (s_recorder.rx_q_count < s_recorder.tx_q_count) ? s_recorder.rx_q_count : s_recorder.tx_q_count;
    if (ready_frames == 0) return;
    if (ready_frames > STEREO_CHUNK_MAX) ready_frames = STEREO_CHUNK_MAX;

    int16_t stereo_chunk[STEREO_CHUNK_MAX * 2];
    for (uint32_t i = 0; i < ready_frames; i++) {
        stereo_chunk[i * 2 + 0] = s_recorder.rx_queue[i]; // Left = Caller
        stereo_chunk[i * 2 + 1] = s_recorder.tx_queue[i]; // Right = You
    }

    flac_stream_encoder_push(&s_recorder.flac_stereo, stereo_chunk, ready_frames);
    wav_stream_write(&s_recorder.wav_stereo, stereo_chunk, ready_frames);

    s_recorder.total_stereo_frames += ready_frames;

    // Shift queues
    if (ready_frames < s_recorder.rx_q_count) {
        memmove(&s_recorder.rx_queue[0], &s_recorder.rx_queue[ready_frames], (s_recorder.rx_q_count - ready_frames) * sizeof(int16_t));
    }
    s_recorder.rx_q_count -= ready_frames;

    if (ready_frames < s_recorder.tx_q_count) {
        memmove(&s_recorder.tx_queue[0], &s_recorder.tx_queue[ready_frames], (s_recorder.tx_q_count - ready_frames) * sizeof(int16_t));
    }
    s_recorder.tx_q_count -= ready_frames;
}

void call_recorder_write_rx(const int16_t *samples, int num_samples) {
    if (!s_recorder_mutex_initialized || num_samples <= 0) return;

    audio_mutex_lock(&s_recorder_mutex);
    if (!s_recorder.is_active) {
        audio_mutex_unlock(&s_recorder_mutex);
        return;
    }

    // 1. Continuous stream recording
    flac_stream_encoder_push(&s_recorder.flac_rx, samples, (uint32_t)num_samples);
    wav_stream_write(&s_recorder.wav_rx, samples, (uint32_t)num_samples);
    s_recorder.total_rx_samples += num_samples;

    // 2. VAD chunk segmentation
    vad_channel_write(&s_recorder.vad_rx, samples, num_samples);

    // 3. Append to stereo RX queue
    uint32_t space = (s_recorder.rx_q_count < STEREO_CHUNK_MAX) ? (STEREO_CHUNK_MAX - s_recorder.rx_q_count) : 0;
    uint32_t to_copy = (num_samples < (int)space) ? (uint32_t)num_samples : space;
    if (to_copy > 0) {
        memcpy(&s_recorder.rx_queue[s_recorder.rx_q_count], samples, to_copy * sizeof(int16_t));
        s_recorder.rx_q_count += to_copy;
    }

    flush_stereo_buffer();
    audio_mutex_unlock(&s_recorder_mutex);
}

void call_recorder_write_tx(const int16_t *samples, int num_samples) {
    if (!s_recorder_mutex_initialized || num_samples <= 0) return;

    audio_mutex_lock(&s_recorder_mutex);
    if (!s_recorder.is_active) {
        audio_mutex_unlock(&s_recorder_mutex);
        return;
    }

    // 1. Continuous stream recording
    flac_stream_encoder_push(&s_recorder.flac_tx, samples, (uint32_t)num_samples);
    wav_stream_write(&s_recorder.wav_tx, samples, (uint32_t)num_samples);
    s_recorder.total_tx_samples += num_samples;

    // 2. VAD chunk segmentation
    vad_channel_write(&s_recorder.vad_tx, samples, num_samples);

    // 3. Append to stereo TX queue
    uint32_t space = (s_recorder.tx_q_count < STEREO_CHUNK_MAX) ? (STEREO_CHUNK_MAX - s_recorder.tx_q_count) : 0;
    uint32_t to_copy = (num_samples < (int)space) ? (uint32_t)num_samples : space;
    if (to_copy > 0) {
        memcpy(&s_recorder.tx_queue[s_recorder.tx_q_count], samples, to_copy * sizeof(int16_t));
        s_recorder.tx_q_count += to_copy;
    }

    flush_stereo_buffer();
    audio_mutex_unlock(&s_recorder_mutex);
}

void call_recorder_stop(void) {
    if (!s_recorder_mutex_initialized) return;

    audio_mutex_lock(&s_recorder_mutex);
    if (!s_recorder.is_active) {
        audio_mutex_unlock(&s_recorder_mutex);
        return;
    }

    // Close any in-progress VAD chunks
    vad_channel_close_seg(&s_recorder.vad_rx, "call ended");
    vad_channel_close_seg(&s_recorder.vad_tx, "call ended");

    // Flush any remaining stereo samples
    uint32_t max_remaining = (s_recorder.rx_q_count > s_recorder.tx_q_count) ? s_recorder.rx_q_count : s_recorder.tx_q_count;
    if (max_remaining > STEREO_CHUNK_MAX) max_remaining = STEREO_CHUNK_MAX;
    if (max_remaining > 0) {
        int16_t stereo_chunk[STEREO_CHUNK_MAX * 2];
        for (uint32_t i = 0; i < max_remaining; i++) {
            stereo_chunk[i * 2 + 0] = (i < s_recorder.rx_q_count) ? s_recorder.rx_queue[i] : 0;
            stereo_chunk[i * 2 + 1] = (i < s_recorder.tx_q_count) ? s_recorder.tx_queue[i] : 0;
        }
        flac_stream_encoder_push(&s_recorder.flac_stereo, stereo_chunk, max_remaining);
        wav_stream_write(&s_recorder.wav_stereo, stereo_chunk, max_remaining);
        s_recorder.total_stereo_frames += max_remaining;
    }

    flac_stream_encoder_close(&s_recorder.flac_rx);
    flac_stream_encoder_close(&s_recorder.flac_tx);
    flac_stream_encoder_close(&s_recorder.flac_stereo);

    wav_stream_close(&s_recorder.wav_rx);
    wav_stream_close(&s_recorder.wav_tx);
    wav_stream_close(&s_recorder.wav_stereo);

    s_recorder.is_active = false;

    double duration_sec = call_recorder_get_duration_seconds();
    diag_log("[RECORDER] Call recording finalized (Duration: %.2f sec, %u samples)", duration_sec, s_recorder.total_rx_samples);
    diag_log("[RECORDER] Saved FLAC, WAV and VAD chunk files to %s", s_recorder.session_dir);

    char session_dir[512], path_stereo[512], path_rx[512], path_tx[512];
    strncpy(session_dir, s_recorder.session_dir, sizeof(session_dir) - 1);
    session_dir[sizeof(session_dir) - 1] = '\0';
    strncpy(path_stereo, s_recorder.path_stereo_wav, sizeof(path_stereo) - 1);
    path_stereo[sizeof(path_stereo) - 1] = '\0';
    strncpy(path_rx, s_recorder.path_rx_wav, sizeof(path_rx) - 1);
    path_rx[sizeof(path_rx) - 1] = '\0';
    strncpy(path_tx, s_recorder.path_tx_wav, sizeof(path_tx) - 1);
    path_tx[sizeof(path_tx) - 1] = '\0';
    audio_mutex_unlock(&s_recorder_mutex);

    if (s_stopped_cb) {
        s_stopped_cb(session_dir, path_stereo, path_rx, path_tx, duration_sec);
    }
}

bool call_recorder_is_active(void) {
    return s_recorder.is_active;
}

double call_recorder_get_duration_seconds(void) {
    if (s_recorder.sample_rate == 0) return 0.0;
    uint32_t max_samples = s_recorder.total_rx_samples > s_recorder.total_tx_samples ?
                           s_recorder.total_rx_samples : s_recorder.total_tx_samples;
    return (double)max_samples / (double)s_recorder.sample_rate;
}
