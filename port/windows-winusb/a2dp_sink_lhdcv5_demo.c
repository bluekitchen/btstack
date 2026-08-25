/*
 * BTstack Windows WinUSB A2DP Sink with LHDC V5 software decoding.
 *
 * LHDC V5 codec implementation:
 *   https://github.com/WillyBilly06/LHDC-V5-Decoder
 *
 * This example keeps SBC as the mandatory A2DP fallback and adds an LHDC V5
 * vendor-specific sink endpoint. Decoded audio is converted to stereo S16 PCM
 * for BTstack's audio sink (PortAudio on the Windows WinUSB port).
 */

#define BTSTACK_FILE__ "a2dp_sink_lhdcv5_demo.c"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "btstack.h"
#include "btstack_ring_buffer.h"
#include "lhdc_dec.h"

/* Host diagnostics required by the decoder test hooks. */
int g_nzc_formula = 0;
int g_pkframe = 0;

#define NUM_CHANNELS 2
#define PCM_BYTES_PER_FRAME 4

#define LHDCV5_VENDOR_ID 0x0000053au
#define LHDCV5_CODEC_ID  0x4c35u

#define LHDCV5_SR_192000 0x01
#define LHDCV5_SR_96000  0x04
#define LHDCV5_SR_48000  0x10
#define LHDCV5_SR_44100  0x20
#define LHDCV5_SR_MASK    0x35

#define LHDCV5_DEPTH_24   0x02
#define LHDCV5_DEPTH_16   0x04
#define LHDCV5_DEPTH_MASK 0x07

#define LHDCV5_VERSION_1  0x01
#define LHDCV5_FRAME_5MS  0x10
#define LHDCV5_FEATURE_LL 0x40

#define APTX_VENDOR_ID    0x0000004fu
#define APTX_CODEC_ID     0x0001u
#define APTX_HD_VENDOR_ID 0x000000d7u
#define APTX_HD_CODEC_ID  0x0024u
#define APTX_SR_48000     0x10
#define APTX_SR_44100     0x20
#define APTX_CHANNEL_STEREO 0x02
#define APTX_PCM_BUFFER_SIZE 16384
#define APTX_PCM_LATENCY_TARGET_MS 40
#define APTX_PCM_LATENCY_HIGH_MS   48

#define LHDCV5_FRAGMENT_BUFFER_SIZE 8192
#define LHDCV5_PCM_SAMPLES_PER_CH_MAX 1920
#define PCM_RING_BUFFER_SIZE (256 * 1024)

static btstack_packet_callback_registration_t hci_event_callback_registration;
static uint8_t sdp_a2dp_sink_service_buffer[150];
static uint8_t device_id_sdp_service_buffer[100];

static uint8_t media_sbc_codec_capabilities[] = { 0xff, 0xff, 2, 53 };
static uint8_t media_sbc_codec_configuration[4];

/*
 * BTstack wants only Media Codec Information here, not LOSC/media type/codec type.
 * vendor id LE (4), codec id LE (2), rate, depth/bitrate, version/frame,
 * features, lossless-raw features.
 */
static const uint8_t media_lhdcv5_codec_capabilities[] = {
    0x3a, 0x05, 0x00, 0x00,
    0x35, 0x4c,
    LHDCV5_SR_MASK,
    LHDCV5_DEPTH_24 | LHDCV5_DEPTH_16,
    LHDCV5_VERSION_1 | LHDCV5_FRAME_5MS,
    LHDCV5_FEATURE_LL,
    0x00
};
static uint8_t media_lhdcv5_codec_configuration[sizeof(media_lhdcv5_codec_capabilities)];

static const uint8_t media_aptx_codec_capabilities[] = {
    0x4f, 0x00, 0x00, 0x00, 0x01, 0x00,
    APTX_SR_44100 | APTX_SR_48000 | APTX_CHANNEL_STEREO
};
static uint8_t media_aptx_codec_configuration[sizeof(media_aptx_codec_capabilities)];

static const uint8_t media_aptx_hd_codec_capabilities[] = {
    0xd7, 0x00, 0x00, 0x00, 0x24, 0x00,
    APTX_SR_44100 | APTX_SR_48000 | APTX_CHANNEL_STEREO,
    0x00, 0x00, 0x00, 0x00
};
static uint8_t media_aptx_hd_codec_configuration[sizeof(media_aptx_hd_codec_capabilities)];

static uint8_t sbc_local_seid;
static uint8_t aptx_local_seid;
static uint8_t aptx_hd_local_seid;
static uint8_t lhdcv5_local_seid;
static uint32_t negotiated_sample_rate = 48000;
static uint8_t negotiated_bit_depth = 16;

static const btstack_sbc_decoder_t *sbc_decoder;
static btstack_sbc_decoder_bluedroid_t sbc_decoder_context;

static void *lhdcv5_workspace;
static size_t lhdcv5_workspace_size;
static lhdc_decoder_t *lhdcv5_decoder;
static uint8_t lhdcv5_fragment[LHDCV5_FRAGMENT_BUFFER_SIZE];
static size_t lhdcv5_fragment_size;

struct aptx_context;
typedef struct aptx_context *(__cdecl *aptx_init_fn)(int hd);
typedef void (__cdecl *aptx_reset_fn)(struct aptx_context *ctx);
typedef void (__cdecl *aptx_finish_fn)(struct aptx_context *ctx);
typedef size_t (__cdecl *aptx_decode_sync_fn)(
    struct aptx_context *ctx, const unsigned char *input, size_t input_size,
    unsigned char *output, size_t output_size, size_t *written,
    int *synced, size_t *dropped);

static HMODULE aptx_module;
static aptx_init_fn aptx_init_ptr;
static aptx_reset_fn aptx_reset_ptr;
static aptx_finish_fn aptx_finish_ptr;
static aptx_decode_sync_fn aptx_decode_sync_ptr;
static struct aptx_context *aptx_decoder;
static int aptx_decoder_is_hd;

static btstack_ring_buffer_t pcm_ring_buffer;
static uint8_t pcm_ring_storage[PCM_RING_BUFFER_SIZE];
static int audio_initialized;
static int audio_started;

static int is_lhdcv5_codec_info(const uint8_t *codec_info, uint16_t len) {
    if (codec_info == NULL || len < 11) return 0;
    return little_endian_read_32(codec_info, 0) == LHDCV5_VENDOR_ID &&
           little_endian_read_16(codec_info, 4) == LHDCV5_CODEC_ID;
}

static uint32_t lhdcv5_rate_from_codec_info(const uint8_t *codec_info) {
    uint8_t rate = codec_info[6] & LHDCV5_SR_MASK;
    if (rate & LHDCV5_SR_192000) return 192000;
    if (rate & LHDCV5_SR_96000)  return 96000;
    if (rate & LHDCV5_SR_48000)  return 48000;
    if (rate & LHDCV5_SR_44100)  return 44100;
    return 0;
}

static uint8_t lhdcv5_depth_from_codec_info(const uint8_t *codec_info) {
    uint8_t depth = codec_info[7] & LHDCV5_DEPTH_MASK;
    if (depth & LHDCV5_DEPTH_24) return 24;
    if (depth & LHDCV5_DEPTH_16) return 16;
    return 0;
}

static int is_aptx_codec_info(const uint8_t *codec_info, uint16_t len, int hd) {
    if (codec_info == NULL) return 0;
    if ((!hd && len < 7) || (hd && len < 11)) return 0;
    uint32_t vendor = little_endian_read_32(codec_info, 0);
    uint16_t codec = little_endian_read_16(codec_info, 4);
    return hd ? (vendor == APTX_HD_VENDOR_ID && codec == APTX_HD_CODEC_ID)
              : (vendor == APTX_VENDOR_ID && codec == APTX_CODEC_ID);
}

static uint32_t aptx_rate_from_codec_info(const uint8_t *codec_info, uint16_t len) {
    if (codec_info == NULL || len < 7) return 0;
    uint8_t rate = codec_info[6] & 0xf0;
    if (rate & APTX_SR_48000) return 48000;
    if (rate & APTX_SR_44100) return 44100;
    return 0;
}

static void pcm_queue_reset(void) {
    btstack_ring_buffer_init(&pcm_ring_buffer, pcm_ring_storage, sizeof(pcm_ring_storage));
}

static void pcm_queue_write(const int16_t *samples, uint32_t frames) {
    uint32_t bytes = frames * PCM_BYTES_PER_FRAME;
    if (btstack_ring_buffer_bytes_free(&pcm_ring_buffer) < bytes) return;
    (void)btstack_ring_buffer_write(&pcm_ring_buffer, (uint8_t *)samples, bytes);
}

static void playback_handler(int16_t *buffer, uint16_t num_audio_frames,
                             const btstack_audio_context_t *context) {
    UNUSED(context);
    uint32_t requested = (uint32_t)num_audio_frames * PCM_BYTES_PER_FRAME;
    if (aptx_decoder != NULL && negotiated_sample_rate != 0) {
        static uint8_t trim_scratch[2048];
        uint32_t queued = btstack_ring_buffer_bytes_available(&pcm_ring_buffer);
        uint32_t target = (negotiated_sample_rate * PCM_BYTES_PER_FRAME * APTX_PCM_LATENCY_TARGET_MS) / 1000;
        uint32_t high = (negotiated_sample_rate * PCM_BYTES_PER_FRAME * APTX_PCM_LATENCY_HIGH_MS) / 1000;
        target -= target % PCM_BYTES_PER_FRAME;
        high -= high % PCM_BYTES_PER_FRAME;
        if (queued > high) {
            uint32_t trim = queued - target;
            trim -= trim % PCM_BYTES_PER_FRAME;
            while (trim != 0) {
                uint32_t chunk = trim < sizeof(trim_scratch) ? trim : (uint32_t)sizeof(trim_scratch);
                chunk -= chunk % PCM_BYTES_PER_FRAME;
                uint32_t discarded = 0;
                btstack_ring_buffer_read(&pcm_ring_buffer, trim_scratch, chunk, &discarded);
                if (discarded == 0) break;
                trim -= discarded;
            }
        }
    }
    uint32_t read = 0;
    btstack_ring_buffer_read(&pcm_ring_buffer, (uint8_t *)buffer, requested, &read);
    if (read < requested) {
        memset(((uint8_t *)buffer) + read, 0, requested - read);
    }
}

static void audio_close(void) {
    const btstack_audio_sink_t *audio = btstack_audio_sink_get_instance();
    if (audio_initialized && audio != NULL) {
        if (audio_started) audio->stop_stream();
        audio->close();
    }
    audio_initialized = 0;
    audio_started = 0;
    pcm_queue_reset();
}

static void audio_open(uint32_t sample_rate) {
    audio_close();
    const btstack_audio_sink_t *audio = btstack_audio_sink_get_instance();
    if (audio == NULL) {
        printf("Audio output unavailable. Build/install PortAudio for live playback.\n");
        return;
    }
    int err = audio->init(NUM_CHANNELS, sample_rate, &playback_handler);
    if (err != 0) {
        printf("Audio output init FAILED for %" PRIu32 " Hz: %d\n", sample_rate, err);
        return;
    }
    audio_initialized = 1;
    uint32_t actual_rate = audio->get_samplerate ? audio->get_samplerate() : sample_rate;
    printf("Audio output opened: requested=%" PRIu32 " Hz actual=%" PRIu32 " Hz\n",
           sample_rate, actual_rate);
    audio->start_stream();
    audio_started = 1;
    printf("Audio output started: %" PRIu32 " Hz, stereo S16\n", actual_rate);
}

static void handle_sbc_pcm(int16_t *data, int num_audio_frames, int num_channels,
                           int sample_rate, void *context) {
    UNUSED(context);
    UNUSED(sample_rate);
    if (num_channels != NUM_CHANNELS) return;
    pcm_queue_write(data, (uint32_t)num_audio_frames);
}

static void aptx_decoder_close(void) {
    if (aptx_decoder != NULL && aptx_finish_ptr != NULL) aptx_finish_ptr(aptx_decoder);
    aptx_decoder = NULL;
    aptx_decoder_is_hd = 0;
}

static int aptx_load_library(void) {
    if (aptx_module != NULL) return 1;
    aptx_module = LoadLibraryA("openaptx0.dll");
    if (aptx_module == NULL) {
        printf("aptX: openaptx0.dll not found (error %lu)\n", (unsigned long)GetLastError());
        return 0;
    }
    aptx_init_ptr = (aptx_init_fn)GetProcAddress(aptx_module, "aptx_init");
    aptx_reset_ptr = (aptx_reset_fn)GetProcAddress(aptx_module, "aptx_reset");
    aptx_finish_ptr = (aptx_finish_fn)GetProcAddress(aptx_module, "aptx_finish");
    aptx_decode_sync_ptr = (aptx_decode_sync_fn)GetProcAddress(aptx_module, "aptx_decode_sync");
    if (!aptx_init_ptr || !aptx_reset_ptr || !aptx_finish_ptr || !aptx_decode_sync_ptr) {
        printf("aptX: decoder DLL missing required exports\n");
        FreeLibrary(aptx_module); aptx_module = NULL; return 0;
    }
    printf("aptX decoder backend loaded: openaptx0.dll\n");
    return 1;
}

static int aptx_decoder_open(int hd) {
    aptx_decoder_close();
    if (!aptx_load_library()) return 0;
    aptx_decoder = aptx_init_ptr(hd ? 1 : 0);
    if (aptx_decoder == NULL) return 0;
    aptx_decoder_is_hd = hd ? 1 : 0;
    printf("aptX%s decoder configured: %" PRIu32 " Hz, stereo\n",
           hd ? " HD" : "", negotiated_sample_rate);
    return 1;
}

static int32_t aptx_read_s24le(const uint8_t *q) {
    int32_t v = (int32_t)q[0] | ((int32_t)q[1] << 8) | ((int32_t)q[2] << 16);
    if (v & 0x00800000) v |= (int32_t)0xff000000;
    return v;
}

static void aptx_decode_payload(const uint8_t *payload, size_t payload_size) {
    static uint8_t pcm24[48];
    static int16_t s16[APTX_PCM_BUFFER_SIZE / 3];
    if (!aptx_decoder || !payload || payload_size == 0) return;

    const size_t block_size = aptx_decoder_is_hd ? 6 : 4;
    size_t offset = 0;
    size_t total_samples = 0;
    size_t total_dropped = 0;
    int final_synced = 0;

    while (payload_size - offset >= block_size) {
        size_t written = 0, dropped = 0;
        int synced = 0;
        size_t processed = aptx_decode_sync_ptr(aptx_decoder,
            payload + offset, block_size, pcm24, sizeof(pcm24),
            &written, &synced, &dropped);
        if (processed != block_size) break;
        offset += processed;
        total_dropped += dropped;
        final_synced = synced;

        size_t samples = written / 3;
        if (total_samples + samples > sizeof(s16) / sizeof(s16[0])) break;
        for (size_t i = 0; i < samples; i++)
            s16[total_samples + i] = (int16_t)(aptx_read_s24le(&pcm24[i * 3]) >> 8);
        total_samples += samples;
    }

    if (offset != payload_size || total_dropped != 0) {
        static uint32_t reports;
        if ((reports++ % 100) == 0)
            printf("aptX%s sync: processed=%zu/%zu samples=%zu synced=%d dropped=%zu\n",
                   aptx_decoder_is_hd ? " HD" : "", offset, payload_size,
                   total_samples, final_synced, total_dropped);
    }

    pcm_queue_write(s16, (uint32_t)(total_samples / NUM_CHANNELS));
}

static void lhdcv5_decoder_close(void) {
    lhdcv5_decoder = NULL;
    free(lhdcv5_workspace);
    lhdcv5_workspace = NULL;
    lhdcv5_workspace_size = 0;
    lhdcv5_fragment_size = 0;
}

static int lhdcv5_decoder_open(uint32_t sample_rate, uint8_t bit_depth) {
    lhdcv5_decoder_close();

    lhdc_dec_config_t config;
    memset(&config, 0, sizeof(config));
    config.sample_rate = (lhdc_dec_sample_rate_t)sample_rate;
    config.bit_depth = bit_depth == 24 ? LHDC_DEC_BITDEPTH_24 : LHDC_DEC_BITDEPTH_16;
    config.frame_duration = LHDC_DEC_FRAME_5MS;
    config.channels = NUM_CHANNELS;
    config.max_frame_bytes = LHDCV5_FRAGMENT_BUFFER_SIZE;
    config.lossless_enable = 0;

    lhdcv5_workspace_size = lhdc_dec_get_workspace_size(sample_rate, 5);
    lhdcv5_workspace = calloc(1, lhdcv5_workspace_size);
    if (lhdcv5_workspace == NULL) {
        printf("LHDC V5: cannot allocate %zu-byte decoder workspace\n", lhdcv5_workspace_size);
        return 0;
    }

    lhdcv5_decoder = lhdc_dec_init(lhdcv5_workspace, &config);
    if (lhdcv5_decoder == NULL) {
        printf("LHDC V5: decoder initialization failed\n");
        lhdcv5_decoder_close();
        return 0;
    }

    printf("LHDC V5 decoder configured: %" PRIu32 " Hz, %u-bit, 5 ms\n",
           sample_rate, bit_depth);
    return 1;
}

static void lhdcv5_emit_pcm(const void *pcm, uint32_t frames, uint8_t bit_depth) {
    static int16_t s16[LHDCV5_PCM_SAMPLES_PER_CH_MAX * NUM_CHANNELS];
    uint32_t total_samples = frames * NUM_CHANNELS;
    if (total_samples > (uint32_t)(sizeof(s16) / sizeof(s16[0]))) return;

    if (bit_depth == 16) {
        memcpy(s16, pcm, total_samples * sizeof(int16_t));
    } else {
        const int32_t *s32 = (const int32_t *)pcm;
        for (uint32_t i = 0; i < total_samples; i++) {
            /* Decoder emits 24 valid bits left-justified in a 32-bit slot. */
            s16[i] = (int16_t)(s32[i] >> 16);
        }
    }
    pcm_queue_write(s16, frames);
}

static void lhdcv5_decode_payload(const uint8_t *payload, size_t payload_size) {
    if (lhdcv5_decoder == NULL || payload == NULL || payload_size < 3) return;

    /*
     * Android/onyx prefixes each LHDC transport subpacket with two bytes.
     * At 48/96 kHz there is one prefix before the frame stream. At 192 kHz
     * the 20 ms A2DP payload contains two 10 ms subpackets:
     *   [tag, seq] + 2 frames + [tag, seq+1] + 2 frames
     * Skip the verified second prefix after frame 2; do not concatenate data
     * across A2DP packet boundaries.
     */
    const uint8_t prefix_tag = payload[0];
    const uint8_t prefix_seq = payload[1];
    size_t offset = 2;
    uint32_t frame_index = 0;

    while (offset + 2 <= payload_size) {
        if (negotiated_sample_rate == 192000 && frame_index != 0 &&
            (frame_index % 2u) == 0 && offset + 2 <= payload_size &&
            payload[offset] == prefix_tag &&
            payload[offset + 1] == (uint8_t)(prefix_seq + frame_index / 2u)) {
            offset += 2;
            if (offset + 2 > payload_size) break;
        }

        uint8_t pcm[LHDCV5_PCM_SAMPLES_PER_CH_MAX * NUM_CHANNELS * sizeof(int32_t)];
        size_t consumed = 0;
        uint32_t generated = 0;
        lhdc_dec_frame_info_t info;

        lhdc_dec_ret_t ret = lhdc_dec_decode_frame(
            lhdcv5_decoder, payload + offset, payload_size - offset,
            pcm, LHDCV5_PCM_SAMPLES_PER_CH_MAX,
            &consumed, &generated, &info);

        if (ret != LHDC_DEC_OK || consumed == 0) {
            static uint32_t decode_errors;
            if ((decode_errors++ % 100) == 0)
                printf("LHDC V5 decode error %d at %zu/%zu\n", (int)ret, offset, payload_size);
            break;
        }
        lhdcv5_emit_pcm(pcm, generated, info.bit_depth);
        offset += consumed;
        frame_index++;
    }
}

static int read_rtp_header(const uint8_t *packet, uint16_t size, uint16_t *offset) {
    if (size < 12) return 0;
    uint8_t csrc_count = packet[0] & 0x0f;
    uint16_t header_len = (uint16_t)(12 + 4 * csrc_count);
    if (size < header_len) return 0;
    *offset = header_len;
    return 1;
}

static void handle_lhdcv5_media(uint8_t *packet, uint16_t size) {
    uint16_t pos;
    if (!read_rtp_header(packet, size, &pos) || size <= pos) return;

    /* bit7 fragmented, bit6 first, bit5 last, low nibble fragment/frame count */
    uint8_t header = packet[pos++];
    int fragmented = (header & 0x80) != 0;
    int first = (header & 0x40) != 0;
    int last = (header & 0x20) != 0;

    const uint8_t *payload = packet + pos;
    size_t payload_size = size - pos;

    if (!fragmented) {
        lhdcv5_fragment_size = 0;
        lhdcv5_decode_payload(payload, payload_size);
        return;
    }

    if (first) lhdcv5_fragment_size = 0;
    if (payload_size > sizeof(lhdcv5_fragment) - lhdcv5_fragment_size) {
        printf("LHDC V5 fragment overflow, dropping packet\n");
        lhdcv5_fragment_size = 0;
        return;
    }

    memcpy(lhdcv5_fragment + lhdcv5_fragment_size, payload, payload_size);
    lhdcv5_fragment_size += payload_size;

    if (last) {
        lhdcv5_decode_payload(lhdcv5_fragment, lhdcv5_fragment_size);
        lhdcv5_fragment_size = 0;
    }
}

static void handle_sbc_media(uint8_t *packet, uint16_t size) {
    uint16_t pos;
    if (!read_rtp_header(packet, size, &pos) || size <= pos) return;
    pos++; /* SBC payload header */
    if (size <= pos) return;
    sbc_decoder->decode_signed_16(&sbc_decoder_context, 0, packet + pos, size - pos);
}

static void handle_aptx_media(uint8_t *packet, uint16_t size, int hd) {
    uint16_t pos = 0;
    /* Android aptX Classic omits RTP when SCMS-T is not enabled; aptX HD uses RTP. */
    if (hd && (!read_rtp_header(packet, size, &pos) || size <= pos)) return;
    aptx_decode_payload(packet + pos, size - pos);
}

static void handle_media_data(uint8_t local_seid, uint8_t *packet, uint16_t size) {
    if (local_seid == lhdcv5_local_seid) handle_lhdcv5_media(packet, size);
    else if (local_seid == aptx_hd_local_seid) handle_aptx_media(packet, size, 1);
    else if (local_seid == aptx_local_seid) handle_aptx_media(packet, size, 0);
    else if (local_seid == sbc_local_seid && sbc_decoder != NULL) handle_sbc_media(packet, size);
}

static uint8_t media_config_validator(const avdtp_stream_endpoint_t *stream_endpoint,
                                      const uint8_t *event, uint16_t size) {
    UNUSED(size);
    uint8_t local_seid = avdtp_local_seid(stream_endpoint);
    if (local_seid == sbc_local_seid) return ERROR_CODE_SUCCESS;

    const uint8_t *info =
        a2dp_subevent_signaling_media_codec_other_configuration_get_media_codec_information(event);
    uint16_t len =
        a2dp_subevent_signaling_media_codec_other_configuration_get_media_codec_information_len(event);

    if (local_seid == lhdcv5_local_seid) {
        if (!is_lhdcv5_codec_info(info, len)) return AVDTP_ERROR_CODE_UNSUPPORTED_CONFIGURATION;
        if (!lhdcv5_rate_from_codec_info(info) || !lhdcv5_depth_from_codec_info(info)) return AVDTP_ERROR_CODE_UNSUPPORTED_CONFIGURATION;
        if ((info[8] & LHDCV5_VERSION_1) == 0 || (info[8] & LHDCV5_FRAME_5MS) == 0) return AVDTP_ERROR_CODE_UNSUPPORTED_CONFIGURATION;
        return ERROR_CODE_SUCCESS;
    }
    int hd = local_seid == aptx_hd_local_seid;
    if (local_seid != aptx_local_seid && !hd) return ERROR_CODE_SUCCESS;
    if (!is_aptx_codec_info(info, len, hd)) return AVDTP_ERROR_CODE_UNSUPPORTED_CONFIGURATION;
    if (!aptx_rate_from_codec_info(info, len)) return AVDTP_ERROR_CODE_UNSUPPORTED_CONFIGURATION;
    if ((info[6] & APTX_CHANNEL_STEREO) == 0) return AVDTP_ERROR_CODE_UNSUPPORTED_CONFIGURATION;
    return ERROR_CODE_SUCCESS;
}

static void a2dp_packet_handler(uint8_t packet_type, uint16_t channel,
                                uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);
    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_A2DP_META) return;

    switch (hci_event_a2dp_meta_get_subevent_code(packet)) {
        case A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CONFIGURATION: {
            const uint8_t *info =
                a2dp_subevent_signaling_media_codec_other_configuration_get_media_codec_information(packet);
            uint16_t len =
                a2dp_subevent_signaling_media_codec_other_configuration_get_media_codec_information_len(packet);
            if (is_lhdcv5_codec_info(info, len)) {
                negotiated_sample_rate = lhdcv5_rate_from_codec_info(info);
                negotiated_bit_depth = lhdcv5_depth_from_codec_info(info);
                printf("A2DP: LHDC V5 selected, %" PRIu32 " Hz, %u-bit, LL=%s\n", negotiated_sample_rate, negotiated_bit_depth,
                       (len > 9 && (info[9] & LHDCV5_FEATURE_LL)) ? "ON" : "OFF");
            } else if (is_aptx_codec_info(info, len, 1)) {
                negotiated_sample_rate = aptx_rate_from_codec_info(info, len); negotiated_bit_depth = 24;
                printf("A2DP: aptX HD selected, %" PRIu32 " Hz, stereo\n", negotiated_sample_rate);
            } else if (is_aptx_codec_info(info, len, 0)) {
                negotiated_sample_rate = aptx_rate_from_codec_info(info, len); negotiated_bit_depth = 16;
                printf("A2DP: aptX selected, %" PRIu32 " Hz, stereo\n", negotiated_sample_rate);
            } else break;
            printf("A2DP vendor config (%u):", len);
            for (uint16_t i = 0; i < len; i++) printf(" %02x", info[i]);
            printf("\n");
            break;
        }

        case A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION:
            negotiated_sample_rate =
                a2dp_subevent_signaling_media_codec_sbc_configuration_get_sampling_frequency(packet);
            negotiated_bit_depth = 16;
            printf("A2DP: SBC fallback selected, %" PRIu32 " Hz\n", negotiated_sample_rate);
            break;

        case A2DP_SUBEVENT_STREAM_ESTABLISHED:
            if (a2dp_subevent_stream_established_get_status(packet) == ERROR_CODE_SUCCESS) {
                printf("A2DP stream established, local SEID %u\n",
                       a2dp_subevent_stream_established_get_local_seid(packet));
            } else {
                printf("A2DP stream establishment failed: 0x%02x\n",
                       a2dp_subevent_stream_established_get_status(packet));
            }
            break;

        case A2DP_SUBEVENT_STREAM_STARTED: {
            uint8_t local_seid = a2dp_subevent_stream_started_get_local_seid(packet);
            pcm_queue_reset();
            if (local_seid == lhdcv5_local_seid) {
                if (!lhdcv5_decoder_open(negotiated_sample_rate, negotiated_bit_depth)) break;
            } else if (local_seid == aptx_hd_local_seid || local_seid == aptx_local_seid) {
                int hd = local_seid == aptx_hd_local_seid;
                if (!aptx_decoder_open(hd)) break;
            } else if (local_seid == sbc_local_seid) {
                sbc_decoder = btstack_sbc_decoder_bluedroid_init_instance(&sbc_decoder_context);
                sbc_decoder->configure(&sbc_decoder_context, SBC_MODE_STANDARD, handle_sbc_pcm, NULL);
            }
            audio_open(negotiated_sample_rate);
            printf("A2DP stream started on SEID %u\n", local_seid);
            break;
        }

        case A2DP_SUBEVENT_STREAM_SUSPENDED: {
            const btstack_audio_sink_t *audio = btstack_audio_sink_get_instance();
            if (audio_started && audio != NULL) audio->stop_stream();
            audio_started = 0;
            pcm_queue_reset();
            if (lhdcv5_decoder != NULL) lhdc_dec_flush(lhdcv5_decoder);
            if (aptx_decoder != NULL && aptx_reset_ptr != NULL) aptx_reset_ptr(aptx_decoder);
            printf("A2DP stream suspended\n");
            break;
        }

        case A2DP_SUBEVENT_STREAM_STOPPED:
            audio_close();
            lhdcv5_decoder_close();
            aptx_decoder_close();
            printf("A2DP stream stopped\n");
            break;

        case A2DP_SUBEVENT_STREAM_RELEASED:
            audio_close();
            lhdcv5_decoder_close();
            aptx_decoder_close();
            printf("A2DP stream released\n");
            break;

        default:
            break;
    }
}

static void hci_packet_handler(uint8_t packet_type, uint16_t channel,
                               uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);
    if (packet_type != HCI_EVENT_PACKET) return;

    bd_addr_t address;
    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_PIN_CODE_REQUEST:
            hci_event_pin_code_request_get_bd_addr(packet, address);
            gap_pin_code_response(address, "0000");
            break;
        case HCI_EVENT_USER_CONFIRMATION_REQUEST:
            hci_event_user_confirmation_request_get_bd_addr(packet, address);
            printf("Pairing confirmation for %s, accepting\n", bd_addr_to_str(address));
            gap_ssp_confirmation_response(address);
            break;
        default:
            break;
    }
}

static void setup_receiver(void) {
    l2cap_init();
    sdp_init();
    a2dp_sink_init();

    a2dp_sink_register_packet_handler(&a2dp_packet_handler);
    a2dp_sink_register_media_handler(&handle_media_data);
    a2dp_sink_register_media_config_validator(&media_config_validator);

    avdtp_stream_endpoint_t *sbc_endpoint = a2dp_sink_create_stream_endpoint(
        AVDTP_AUDIO, AVDTP_CODEC_SBC,
        media_sbc_codec_capabilities, sizeof(media_sbc_codec_capabilities),
        media_sbc_codec_configuration, sizeof(media_sbc_codec_configuration));
    btstack_assert(sbc_endpoint != NULL);
    sbc_local_seid = avdtp_local_seid(sbc_endpoint);

    /* aptX decoding is optional. Only advertise the vendor codecs when an
     * external openaptx0.dll backend is available at runtime. */
    if (aptx_load_library()) {
        avdtp_stream_endpoint_t *aptx_endpoint = a2dp_sink_create_stream_endpoint(
            AVDTP_AUDIO, AVDTP_CODEC_NON_A2DP,
            media_aptx_codec_capabilities, sizeof(media_aptx_codec_capabilities),
            media_aptx_codec_configuration, sizeof(media_aptx_codec_configuration));
        btstack_assert(aptx_endpoint != NULL);
        aptx_local_seid = avdtp_local_seid(aptx_endpoint);

        avdtp_stream_endpoint_t *aptx_hd_endpoint = a2dp_sink_create_stream_endpoint(
            AVDTP_AUDIO, AVDTP_CODEC_NON_A2DP,
            media_aptx_hd_codec_capabilities, sizeof(media_aptx_hd_codec_capabilities),
            media_aptx_hd_codec_configuration, sizeof(media_aptx_hd_codec_configuration));
        btstack_assert(aptx_hd_endpoint != NULL);
        aptx_hd_local_seid = avdtp_local_seid(aptx_hd_endpoint);
    }

    avdtp_stream_endpoint_t *lhdc_endpoint = a2dp_sink_create_stream_endpoint(
        AVDTP_AUDIO, AVDTP_CODEC_NON_A2DP,
        media_lhdcv5_codec_capabilities, sizeof(media_lhdcv5_codec_capabilities),
        media_lhdcv5_codec_configuration, sizeof(media_lhdcv5_codec_configuration));
    btstack_assert(lhdc_endpoint != NULL);
    lhdcv5_local_seid = avdtp_local_seid(lhdc_endpoint);

    memset(sdp_a2dp_sink_service_buffer, 0, sizeof(sdp_a2dp_sink_service_buffer));
    a2dp_sink_create_sdp_record(sdp_a2dp_sink_service_buffer,
                                sdp_create_service_record_handle(),
                                AVDTP_SINK_FEATURE_MASK_HEADPHONE,
                                "BTstack Multi-Codec Receiver", "BTstack");
    sdp_register_service(sdp_a2dp_sink_service_buffer);

    memset(device_id_sdp_service_buffer, 0, sizeof(device_id_sdp_service_buffer));
    device_id_create_sdp_record(device_id_sdp_service_buffer,
                                sdp_create_service_record_handle(),
                                DEVICE_ID_VENDOR_ID_SOURCE_BLUETOOTH,
                                BLUETOOTH_COMPANY_ID_BLUEKITCHEN_GMBH, 1, 1);
    sdp_register_service(device_id_sdp_service_buffer);

    gap_set_local_name("BTstack Multi-Codec Receiver 00:00:00:00:00:00");
    gap_set_class_of_device(0x240400);
    gap_discoverable_control(1);
    gap_set_default_link_policy_settings(
        LM_LINK_POLICY_ENABLE_ROLE_SWITCH | LM_LINK_POLICY_ENABLE_SNIFF_MODE);
    gap_set_allow_role_switch(true);

    hci_event_callback_registration.callback = &hci_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    pcm_queue_reset();

    printf("A2DP endpoints: SBC SEID %u, aptX SEID %u, aptX HD SEID %u, LHDC V5 SEID %u\n",
           sbc_local_seid, aptx_local_seid, aptx_hd_local_seid, lhdcv5_local_seid);
    if (aptx_local_seid != 0) {
        printf("aptX capabilities: Classic + HD, 44.1/48 kHz stereo\n");
    } else {
        printf("aptX decoder backend unavailable; aptX endpoints disabled\n");
    }
    printf("LHDC V5 capabilities: 44.1/48/96/192 kHz, 16/24-bit, lossy\n");
}

int btstack_main(int argc, const char *argv[]) {
    UNUSED(argc);
    UNUSED(argv);
    setup_receiver();
    printf("Starting BTstack multi-codec receiver...\n");
    hci_power_control(HCI_POWER_ON);
    return 0;
}
