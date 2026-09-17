#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "audio/flac_encoder.h"
#include "classic/btstack_sbc.h"
#include "classic/btstack_sbc_bluedroid.h"
#include "classic/btstack_sbc_plc.h"


void hci_dump_log(int log_level, const char * format, ...) {
    (void)log_level;
    (void)format;
}


#pragma pack(push, 1)
typedef struct {
    char riff_id[4];        // "RIFF"
    uint32_t riff_size;
    char wave_id[4];        // "WAVE"
    char fmt_id[4];         // "fmt "
    uint32_t fmt_size;      // 16
    uint16_t audio_format;  // 1 = PCM
    uint16_t num_channels;  // 1 or 2
    uint32_t sample_rate;   // 16000
    uint32_t byte_rate;     // sample_rate * num_channels * (bits/8)
    uint16_t block_align;   // num_channels * (bits/8)
    uint16_t bits_per_sample; // 16
    char data_id[4];        // "data"
    uint32_t data_size;
} wav_header_t;
#pragma pack(pop)

static void write_wav_header(FILE *f, uint32_t sample_rate, uint16_t channels, uint32_t num_samples) {
    wav_header_t hdr;
    memcpy(hdr.riff_id, "RIFF", 4);
    hdr.riff_size = 36 + num_samples * channels * sizeof(int16_t);
    memcpy(hdr.wave_id, "WAVE", 4);
    memcpy(hdr.fmt_id, "fmt ", 4);
    hdr.fmt_size = 16;
    hdr.audio_format = 1;
    hdr.num_channels = channels;
    hdr.sample_rate = sample_rate;
    hdr.bits_per_sample = 16;
    hdr.byte_rate = sample_rate * channels * sizeof(int16_t);
    hdr.block_align = channels * sizeof(int16_t);
    memcpy(hdr.data_id, "data", 4);
    hdr.data_size = num_samples * channels * sizeof(int16_t);

    fseek(f, 0, SEEK_SET);
    fwrite(&hdr, sizeof(hdr), 1, f);
}

typedef struct {
    int16_t *samples;
    size_t count;
    size_t capacity;
} sample_buffer_t;

static void sample_buffer_init(sample_buffer_t *sb) {
    sb->capacity = 1024 * 1024;
    sb->samples = (int16_t *)malloc(sb->capacity * sizeof(int16_t));
    sb->count = 0;
}

static void sample_buffer_push(sample_buffer_t *sb, const int16_t *data, int num) {
    if (sb->count + num > sb->capacity) {
        sb->capacity = (sb->capacity + num) * 2;
        sb->samples = (int16_t *)realloc(sb->samples, sb->capacity * sizeof(int16_t));
    }
    memcpy(&sb->samples[sb->count], data, num * sizeof(int16_t));
    sb->count += num;
}

static sample_buffer_t s_rx_buf;
static sample_buffer_t s_tx_buf;

static void rx_pcm_callback(int16_t *data, int num_samples, int num_channels, int sample_rate, void *context) {
    (void)num_channels;
    (void)sample_rate;
    (void)context;
    sample_buffer_push(&s_rx_buf, data, num_samples);
}

static void tx_pcm_callback(int16_t *data, int num_samples, int num_channels, int sample_rate, void *context) {
    (void)num_channels;
    (void)sample_rate;
    (void)context;
    sample_buffer_push(&s_tx_buf, data, num_samples);
}

static uint32_t read_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

int main(int argc, char *argv[]) {
    const char *input_pklg = "logs/hci_dump_20260911_170410.pklg";
    if (argc > 1) {
        input_pklg = argv[1];
    }

    printf("======================================================================\n");
    printf("   DIALER-BTSTACK AUDIO EXTRACTOR & CONVERTER                        \n");
    printf("   Input: %s\n", input_pklg);
    printf("======================================================================\n");

    FILE *f = fopen(input_pklg, "rb");
    if (!f) {
        fprintf(stderr, "Error: Could not open %s\n", input_pklg);
        return 1;
    }

    sample_buffer_init(&s_rx_buf);
    sample_buffer_init(&s_tx_buf);

    btstack_sbc_decoder_bluedroid_t rx_decoder_ctx;
    const btstack_sbc_decoder_t *rx_decoder = btstack_sbc_decoder_bluedroid_init_instance(&rx_decoder_ctx);
    rx_decoder->configure(&rx_decoder_ctx, SBC_MODE_mSBC, &rx_pcm_callback, NULL);

    btstack_sbc_decoder_bluedroid_t tx_decoder_ctx;
    const btstack_sbc_decoder_t *tx_decoder = btstack_sbc_decoder_bluedroid_init_instance(&tx_decoder_ctx);
    tx_decoder->configure(&tx_decoder_ctx, SBC_MODE_mSBC, &tx_pcm_callback, NULL);

    uint8_t header[13];
    uint32_t total_packets = 0;
    uint32_t rx_sco_packets = 0;
    uint32_t tx_sco_packets = 0;

    while (fread(header, 1, 13, f) == 13) {
        uint32_t length = read_be32(&header[0]);
        uint32_t tv_sec = read_be32(&header[4]);
        uint32_t tv_us  = read_be32(&header[8]);
        uint8_t pklg_type = header[12];

        if (length < 9) break;
        uint32_t payload_len = length - 9;

        uint8_t *payload = (uint8_t *)malloc(payload_len);
        if (fread(payload, 1, payload_len, f) != payload_len) {
            free(payload);
            break;
        }

        total_packets++;

        if (pklg_type == 0x09 && payload_len >= 3) {
            // HCI SCO RX (from phone -> speaker)
            rx_sco_packets++;
            uint8_t packet_status_flag = (payload[1] >> 4) & 0x03;
            rx_decoder->decode_signed_16(&rx_decoder_ctx, packet_status_flag, payload + 3, payload_len - 3);
        } else if (pklg_type == 0x08 && payload_len >= 3) {
            // HCI SCO TX (from mic -> to phone)
            tx_sco_packets++;
            uint8_t packet_status_flag = (payload[1] >> 4) & 0x03;
            tx_decoder->decode_signed_16(&tx_decoder_ctx, packet_status_flag, payload + 3, payload_len - 3);
        }

        free(payload);
    }

    fclose(f);

    printf("Extracted Packet Summary:\n");
    printf("  Total Trace Packets : %u\n", total_packets);
    printf("  SCO RX (Speaker)    : %u packets -> %zu audio samples (%.2f seconds)\n",
           rx_sco_packets, s_rx_buf.count, (double)s_rx_buf.count / 16000.0);
    printf("  SCO TX (Microphone) : %u packets -> %zu audio samples (%.2f seconds)\n",
           tx_sco_packets, s_tx_buf.count, (double)s_tx_buf.count / 16000.0);

    // Create recordings directory if not exists
    system("mkdir recordings 2>nul");

    // Extract base name from input_pklg (e.g., "logs/hci_dump_20260911_170410.pklg" -> "call_20260911_170410")
    char base_tag[128] = "call";
    const char *slash = strrchr(input_pklg, '/');
    if (!slash) slash = strrchr(input_pklg, '\\');
    const char *filename = slash ? slash + 1 : input_pklg;
    if (strncmp(filename, "hci_dump_", 9) == 0) {
        snprintf(base_tag, sizeof(base_tag), "call_%s", filename + 9);
        char *dot = strrchr(base_tag, '.');
        if (dot) *dot = '\0';
    } else {
        snprintf(base_tag, sizeof(base_tag), "%s", filename);
        char *dot = strrchr(base_tag, '.');
        if (dot) *dot = '\0';
    }

    char rx_wav_path[256], rx_flac_path[256];
    char tx_wav_path[256], tx_flac_path[256];
    char stereo_wav_path[256], stereo_flac_path[256];

    snprintf(rx_wav_path, sizeof(rx_wav_path), "recordings/%s_speaker_caller.wav", base_tag);
    snprintf(rx_flac_path, sizeof(rx_flac_path), "recordings/%s_speaker_caller.flac", base_tag);
    snprintf(tx_wav_path, sizeof(tx_wav_path), "recordings/%s_microphone_you.wav", base_tag);
    snprintf(tx_flac_path, sizeof(tx_flac_path), "recordings/%s_microphone_you.flac", base_tag);
    snprintf(stereo_wav_path, sizeof(stereo_wav_path), "recordings/%s_full_duplex_stereo.wav", base_tag);
    snprintf(stereo_flac_path, sizeof(stereo_flac_path), "recordings/%s_full_duplex_stereo.flac", base_tag);

    // 1. Write Speaker RX WAV & FLAC
    FILE *f_rx = fopen(rx_wav_path, "wb");
    if (f_rx) {
        write_wav_header(f_rx, 16000, 1, (uint32_t)s_rx_buf.count);
        fwrite(s_rx_buf.samples, sizeof(int16_t), s_rx_buf.count, f_rx);
        fclose(f_rx);
        printf("[SUCCESS] Generated Speaker WAV   : %s\n", rx_wav_path);
    }
    if (flac_encode_file(rx_flac_path, s_rx_buf.samples, (uint32_t)s_rx_buf.count, 1, 16000)) {
        printf("[SUCCESS] Generated Speaker FLAC  : %s\n", rx_flac_path);
    }

    // 2. Write Microphone TX WAV & FLAC
    FILE *f_tx = fopen(tx_wav_path, "wb");
    if (f_tx) {
        write_wav_header(f_tx, 16000, 1, (uint32_t)s_tx_buf.count);
        fwrite(s_tx_buf.samples, sizeof(int16_t), s_tx_buf.count, f_tx);
        fclose(f_tx);
        printf("[SUCCESS] Generated Mic WAV       : %s\n", tx_wav_path);
    }
    if (flac_encode_file(tx_flac_path, s_tx_buf.samples, (uint32_t)s_tx_buf.count, 1, 16000)) {
        printf("[SUCCESS] Generated Mic FLAC      : %s\n", tx_flac_path);
    }

    // 3. Write Combined Full-Duplex Stereo WAV & FLAC (Left = Caller, Right = You)
    size_t max_samples = s_rx_buf.count > s_tx_buf.count ? s_rx_buf.count : s_tx_buf.count;
    int16_t *stereo_buf = (int16_t *)malloc(max_samples * 2 * sizeof(int16_t));
    if (stereo_buf) {
        for (size_t i = 0; i < max_samples; i++) {
            stereo_buf[i * 2 + 0] = (i < s_rx_buf.count) ? s_rx_buf.samples[i] : 0; // Left: Caller
            stereo_buf[i * 2 + 1] = (i < s_tx_buf.count) ? s_tx_buf.samples[i] : 0; // Right: You
        }

        FILE *f_stereo = fopen(stereo_wav_path, "wb");
        if (f_stereo) {
            write_wav_header(f_stereo, 16000, 2, (uint32_t)max_samples);
            fwrite(stereo_buf, sizeof(int16_t), max_samples * 2, f_stereo);
            fclose(f_stereo);
            printf("[SUCCESS] Generated Stereo WAV    : %s\n", stereo_wav_path);
        }

        if (flac_encode_file(stereo_flac_path, stereo_buf, (uint32_t)max_samples, 2, 16000)) {
            printf("[SUCCESS] Generated Stereo FLAC   : %s\n", stereo_flac_path);
        }

        free(stereo_buf);
    }

    free(s_rx_buf.samples);
    free(s_tx_buf.samples);

    printf("Done!\n");
    return 0;
}


