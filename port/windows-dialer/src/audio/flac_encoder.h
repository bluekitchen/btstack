#ifndef FLAC_ENCODER_H
#define FLAC_ENCODER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// FLAC CRC-8 calculation (polynomial 0x07)
static inline uint8_t flac_calc_crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0;
    while (len--) {
        crc ^= *data++;
        for (int i = 0; i < 8; i++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// FLAC CRC-16 calculation (polynomial 0x8005)
static inline uint16_t flac_calc_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0;
    while (len--) {
        crc ^= ((uint16_t)*data++) << 8;
        for (int i = 0; i < 8; i++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x8005;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

static inline int flac_write_utf8_u32(uint8_t *out, uint32_t val) {
    if (val < 0x80) {
        out[0] = (uint8_t)val;
        return 1;
    } else if (val < 0x800) {
        out[0] = (uint8_t)(0xC0 | (val >> 6));
        out[1] = (uint8_t)(0x80 | (val & 0x3F));
        return 2;
    } else if (val < 0x10000) {
        out[0] = (uint8_t)(0xE0 | (val >> 12));
        out[1] = (uint8_t)(0x80 | ((val >> 6) & 0x3F));
        out[2] = (uint8_t)(0x80 | (val & 0x3F));
        return 3;
    } else if (val < 0x200000) {
        out[0] = (uint8_t)(0xF0 | (val >> 18));
        out[1] = (uint8_t)(0x80 | ((val >> 12) & 0x3F));
        out[2] = (uint8_t)(0x80 | ((val >> 6) & 0x3F));
        out[3] = (uint8_t)(0x80 | (val & 0x3F));
        return 4;
    } else {
        out[0] = (uint8_t)(0xF8 | (val >> 24));
        out[1] = (uint8_t)(0x80 | ((val >> 18) & 0x3F));
        out[2] = (uint8_t)(0x80 | ((val >> 12) & 0x3F));
        out[3] = (uint8_t)(0x80 | ((val >> 6) & 0x3F));
        out[4] = (uint8_t)(0x80 | (val & 0x3F));
        return 5;
    }
}

#define FLAC_DEFAULT_BLOCK_SIZE 4096

typedef struct {
    FILE *f;
    uint32_t sample_rate;
    uint16_t channels;
    uint32_t total_samples;
    uint32_t frame_number;
    int16_t *block_buffer; // capacity: channels * FLAC_DEFAULT_BLOCK_SIZE
    uint32_t block_samples;
    uint8_t *frame_buffer;
    size_t frame_buffer_capacity;
} flac_stream_encoder_t;

static inline bool flac_stream_encoder_open(flac_stream_encoder_t *enc, const char *filepath, uint32_t sample_rate, uint16_t channels) {
    memset(enc, 0, sizeof(*enc));
    enc->f = fopen(filepath, "wb");
    if (!enc->f) return false;

    enc->sample_rate = sample_rate;
    enc->channels = channels;
    enc->total_samples = 0;
    enc->frame_number = 0;
    enc->block_samples = 0;

    enc->block_buffer = (int16_t *)malloc(channels * FLAC_DEFAULT_BLOCK_SIZE * sizeof(int16_t));
    enc->frame_buffer_capacity = 32 + channels * FLAC_DEFAULT_BLOCK_SIZE * sizeof(int16_t) + 16;
    enc->frame_buffer = (uint8_t *)malloc(enc->frame_buffer_capacity);

    // 1. Write "fLaC" marker
    fwrite("fLaC", 1, 4, enc->f);

    // 2. Write METADATA_BLOCK_HEADER for STREAMINFO (Last block = 1, Type = 0, Length = 34)
    uint8_t meta_hdr[4] = { 0x80, 0x00, 0x00, 0x22 };
    fwrite(meta_hdr, 1, 4, enc->f);

    // 3. Write Placeholder STREAMINFO payload (34 bytes)
    uint8_t streaminfo[34];
    memset(streaminfo, 0, sizeof(streaminfo));

    // Min / Max block size (4096 = 0x1000)
    streaminfo[0] = (uint8_t)(FLAC_DEFAULT_BLOCK_SIZE >> 8);
    streaminfo[1] = (uint8_t)(FLAC_DEFAULT_BLOCK_SIZE & 0xFF);
    streaminfo[2] = (uint8_t)(FLAC_DEFAULT_BLOCK_SIZE >> 8);
    streaminfo[3] = (uint8_t)(FLAC_DEFAULT_BLOCK_SIZE & 0xFF);

    // Min / Max frame size (0 = unknown)
    // Sample rate (20 bits), channels-1 (3 bits), bits_per_sample-1 (5 bits), total_samples (36 bits)
    uint64_t sr_ch_bps_samples = ((uint64_t)(sample_rate & 0xFFFFF) << 44) |
                                 ((uint64_t)((channels - 1) & 0x07) << 41) |
                                 ((uint64_t)(15 & 0x1F) << 36) |
                                 ((uint64_t)0); // initial 0 total samples

    for (int i = 0; i < 8; i++) {
        streaminfo[10 + i] = (uint8_t)((sr_ch_bps_samples >> (56 - i * 8)) & 0xFF);
    }
    // MD5 = zeros
    fwrite(streaminfo, 1, 34, enc->f);

    return true;
}

static inline void flac_stream_encoder_write_frame(flac_stream_encoder_t *enc, const int16_t *interleaved_samples, uint32_t num_samples) {
    if (num_samples == 0) return;

    uint8_t *buf = enc->frame_buffer;
    size_t pos = 0;

    // Frame Header
    buf[pos++] = 0xFF;
    buf[pos++] = 0xF8; // Sync 14 bits + 0 reserved + 0 fixed blocking

    uint8_t bs_code;
    if (num_samples == FLAC_DEFAULT_BLOCK_SIZE) {
        bs_code = 0x0C; // 1100b = 4096 samples
    } else {
        bs_code = 0x07; // 0111b = explicit 16-bit blocksize-1 at end of header
    }

    uint8_t sr_code;
    if (enc->sample_rate == 8000) {
        sr_code = 0x04; // 8kHz
    } else if (enc->sample_rate == 16000) {
        sr_code = 0x09; // 16kHz
    } else {
        sr_code = 0x00; // get from STREAMINFO
    }

    buf[pos++] = (uint8_t)((bs_code << 4) | (sr_code & 0x0F));

    uint8_t ch_code = (enc->channels == 2) ? 0x01 : 0x00;
    uint8_t bps_code = 0x04; // 100b = 16-bit
    buf[pos++] = (uint8_t)((ch_code << 4) | (bps_code << 1) | 0x00);

    // Frame Number (UTF-8)
    pos += flac_write_utf8_u32(&buf[pos], enc->frame_number);

    // If explicit block size:
    if (bs_code == 0x07) {
        uint16_t bs_val = (uint16_t)(num_samples - 1);
        buf[pos++] = (uint8_t)(bs_val >> 8);
        buf[pos++] = (uint8_t)(bs_val & 0xFF);
    }

    // Header CRC-8
    uint8_t hdr_crc = flac_calc_crc8(buf, pos);
    buf[pos++] = hdr_crc;

    // Subframes (one for each channel)
    for (uint16_t c = 0; c < enc->channels; c++) {
        // Subframe Header: SUBFRAME_VERBATIM = 000001b -> (1 << 1) = 0x02
        buf[pos++] = 0x02;

        // Verbatim 16-bit samples big-endian
        for (uint32_t s = 0; s < num_samples; s++) {
            int16_t sample = interleaved_samples[s * enc->channels + c];
            buf[pos++] = (uint8_t)((sample >> 8) & 0xFF);
            buf[pos++] = (uint8_t)(sample & 0xFF);
        }
    }

    // Frame CRC-16 over entire frame bytes
    uint16_t frame_crc = flac_calc_crc16(buf, pos);
    buf[pos++] = (uint8_t)(frame_crc >> 8);
    buf[pos++] = (uint8_t)(frame_crc & 0xFF);

    fwrite(buf, 1, pos, enc->f);

    enc->frame_number++;
    enc->total_samples += num_samples;
}

static inline void flac_stream_encoder_push(flac_stream_encoder_t *enc, const int16_t *interleaved_samples, uint32_t num_samples) {
    if (!enc || !enc->f) return;

    while (num_samples > 0) {
        uint32_t needed = FLAC_DEFAULT_BLOCK_SIZE - enc->block_samples;
        uint32_t take = (num_samples < needed) ? num_samples : needed;

        memcpy(&enc->block_buffer[enc->block_samples * enc->channels],
               interleaved_samples,
               take * enc->channels * sizeof(int16_t));

        enc->block_samples += take;
        interleaved_samples += take * enc->channels;
        num_samples -= take;

        if (enc->block_samples == FLAC_DEFAULT_BLOCK_SIZE) {
            flac_stream_encoder_write_frame(enc, enc->block_buffer, FLAC_DEFAULT_BLOCK_SIZE);
            enc->block_samples = 0;
        }
    }
}

static inline void flac_stream_encoder_close(flac_stream_encoder_t *enc) {
    if (!enc || !enc->f) return;

    // Flush any pending samples
    if (enc->block_samples > 0) {
        flac_stream_encoder_write_frame(enc, enc->block_buffer, enc->block_samples);
        enc->block_samples = 0;
    }

    // Update STREAMINFO with actual total_samples
    fseek(enc->f, 8, SEEK_SET); // Offset of STREAMINFO payload

    uint8_t streaminfo[34];
    memset(streaminfo, 0, sizeof(streaminfo));

    streaminfo[0] = (uint8_t)(FLAC_DEFAULT_BLOCK_SIZE >> 8);
    streaminfo[1] = (uint8_t)(FLAC_DEFAULT_BLOCK_SIZE & 0xFF);
    streaminfo[2] = (uint8_t)(FLAC_DEFAULT_BLOCK_SIZE >> 8);
    streaminfo[3] = (uint8_t)(FLAC_DEFAULT_BLOCK_SIZE & 0xFF);

    uint64_t sr_ch_bps_samples = ((uint64_t)(enc->sample_rate & 0xFFFFF) << 44) |
                                 ((uint64_t)((enc->channels - 1) & 0x07) << 41) |
                                 ((uint64_t)(15 & 0x1F) << 36) |
                                 ((uint64_t)(enc->total_samples & 0xFFFFFFFFFLL));

    for (int i = 0; i < 8; i++) {
        streaminfo[10 + i] = (uint8_t)((sr_ch_bps_samples >> (56 - i * 8)) & 0xFF);
    }

    fwrite(streaminfo, 1, 34, enc->f);
    fclose(enc->f);
    enc->f = NULL;

    if (enc->block_buffer) {
        free(enc->block_buffer);
        enc->block_buffer = NULL;
    }
    if (enc->frame_buffer) {
        free(enc->frame_buffer);
        enc->frame_buffer = NULL;
    }
}

static inline bool flac_encode_file(const char *filepath, const int16_t *interleaved_samples, uint32_t num_samples, uint16_t channels, uint32_t sample_rate) {
    flac_stream_encoder_t enc;
    if (!flac_stream_encoder_open(&enc, filepath, sample_rate, channels)) {
        return false;
    }
    flac_stream_encoder_push(&enc, interleaved_samples, num_samples);
    flac_stream_encoder_close(&enc);
    return true;
}

#ifdef __cplusplus
}
#endif

#endif // FLAC_ENCODER_H
