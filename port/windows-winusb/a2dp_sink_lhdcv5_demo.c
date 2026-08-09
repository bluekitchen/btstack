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

static uint8_t sbc_local_seid;
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

static btstack_ring_buffer_t pcm_ring_buffer;
static uint8_t pcm_ring_storage[PCM_RING_BUFFER_SIZE];
static int audio_initialized;
static int audio_started;
static uint32_t dropped_pcm_bytes;
static uint64_t pcm_written_frames;
static uint64_t pcm_read_frames;
static uint32_t pcm_underflow_count;
static uint32_t pcm_playback_callbacks;
static FILE *lhdcv5_media_dump;
static uint32_t lhdcv5_diag_packets;

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

static void pcm_queue_reset(void) {
    btstack_ring_buffer_init(&pcm_ring_buffer, pcm_ring_storage, sizeof(pcm_ring_storage));
    dropped_pcm_bytes = 0;
    pcm_written_frames = 0;
    pcm_read_frames = 0;
    pcm_underflow_count = 0;
    pcm_playback_callbacks = 0;
}

static void pcm_queue_write(const int16_t *samples, uint32_t frames) {
    uint32_t bytes = frames * PCM_BYTES_PER_FRAME;
    if (btstack_ring_buffer_bytes_free(&pcm_ring_buffer) < bytes) {
        dropped_pcm_bytes += bytes;
        return;
    }
    (void)btstack_ring_buffer_write(&pcm_ring_buffer, (uint8_t *)samples, bytes);
    pcm_written_frames += frames;
}

static void playback_handler(int16_t *buffer, uint16_t num_audio_frames,
                             const btstack_audio_context_t *context) {
    UNUSED(context);
    uint32_t requested = (uint32_t)num_audio_frames * PCM_BYTES_PER_FRAME;
    uint32_t read = 0;
    btstack_ring_buffer_read(&pcm_ring_buffer, (uint8_t *)buffer, requested, &read);
    pcm_read_frames += read / PCM_BYTES_PER_FRAME;
    pcm_playback_callbacks++;
    if (read < requested) {
        pcm_underflow_count++;
        memset(((uint8_t *)buffer) + read, 0, requested - read);
    }
    if ((pcm_playback_callbacks % 200) == 0) {
        printf("[PCM] cb=%u written=%llu read=%llu queued=%u underflows=%u dropped=%u\n",
               pcm_playback_callbacks, (unsigned long long)pcm_written_frames,
               (unsigned long long)pcm_read_frames,
               btstack_ring_buffer_bytes_available(&pcm_ring_buffer),
               pcm_underflow_count, dropped_pcm_bytes);
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
     * On onyx/Android LHDC V5, two bytes precede the raw frame stream.
     * Captured 96 kHz packets are exactly:
     *   [2-byte LHDC packet prefix] + 4 * ([u16 LE frame header] + 200-byte frame)
     * so the first decoder frame begins at offset 2, not offset 1.
     */
    size_t offset = 2;
    while (offset + 2 <= payload_size) {
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
            if ((decode_errors++ % 100) == 0) {
                printf("LHDC V5 decode error %d at %zu/%zu\n", (int)ret, offset, payload_size);
            }
            break;
        }

        lhdcv5_emit_pcm(pcm, generated, info.bit_depth);
        offset += consumed;
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
    if (lhdcv5_media_dump != NULL) {
        uint8_t len_le[2] = { (uint8_t)(size & 0xff), (uint8_t)(size >> 8) };
        fwrite(len_le, 1, sizeof(len_le), lhdcv5_media_dump);
        fwrite(packet, 1, size, lhdcv5_media_dump);
        fflush(lhdcv5_media_dump);
    }
    if (lhdcv5_diag_packets < 12) {
        printf("[RX] #%u size=%u first=", lhdcv5_diag_packets, size);
        uint16_t n = size < 32 ? size : 32;
        for (uint16_t i = 0; i < n; i++) printf("%02x", packet[i]);
        printf("\n");
    }
    lhdcv5_diag_packets++;
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

static void handle_media_data(uint8_t local_seid, uint8_t *packet, uint16_t size) {
    if (local_seid == lhdcv5_local_seid) {
        handle_lhdcv5_media(packet, size);
    } else if (local_seid == sbc_local_seid && sbc_decoder != NULL) {
        handle_sbc_media(packet, size);
    }
}

static uint8_t media_config_validator(const avdtp_stream_endpoint_t *stream_endpoint,
                                      const uint8_t *event, uint16_t size) {
    UNUSED(size);
    if (avdtp_local_seid(stream_endpoint) != lhdcv5_local_seid) return ERROR_CODE_SUCCESS;

    const uint8_t *info =
        a2dp_subevent_signaling_media_codec_other_configuration_get_media_codec_information(event);
    uint16_t len =
        a2dp_subevent_signaling_media_codec_other_configuration_get_media_codec_information_len(event);

    if (!is_lhdcv5_codec_info(info, len)) return AVDTP_ERROR_CODE_UNSUPPORTED_CONFIGURATION;
    if (lhdcv5_rate_from_codec_info(info) == 0 || lhdcv5_depth_from_codec_info(info) == 0) {
        return AVDTP_ERROR_CODE_UNSUPPORTED_CONFIGURATION;
    }
    if ((info[8] & LHDCV5_VERSION_1) == 0 || (info[8] & LHDCV5_FRAME_5MS) == 0) {
        return AVDTP_ERROR_CODE_UNSUPPORTED_CONFIGURATION;
    }
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
            if (!is_lhdcv5_codec_info(info, len)) break;
            negotiated_sample_rate = lhdcv5_rate_from_codec_info(info);
            negotiated_bit_depth = lhdcv5_depth_from_codec_info(info);
            printf("A2DP: LHDC V5 selected, %" PRIu32 " Hz, %u-bit, LL=%s, features=0x%02x\n",
                   negotiated_sample_rate, negotiated_bit_depth,
                   (len > 9 && (info[9] & LHDCV5_FEATURE_LL)) ? "ON" : "OFF",
                   len > 9 ? info[9] : 0);
            printf("A2DP LHDC V5 config (%u):", len);
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
                if (lhdcv5_media_dump != NULL) fclose(lhdcv5_media_dump);
                lhdcv5_media_dump = NULL;
                fopen_s(&lhdcv5_media_dump, "live_media_packets.bin", "wb");
                lhdcv5_diag_packets = 0;
                if (!lhdcv5_decoder_open(negotiated_sample_rate, negotiated_bit_depth)) break;
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
            printf("A2DP stream suspended\n");
            break;
        }

        case A2DP_SUBEVENT_STREAM_STOPPED:
            audio_close();
            lhdcv5_decoder_close();
            if (lhdcv5_media_dump != NULL) fclose(lhdcv5_media_dump);
            lhdcv5_media_dump = NULL;
            printf("A2DP stream stopped\n");
            break;

        case A2DP_SUBEVENT_STREAM_RELEASED:
            audio_close();
            lhdcv5_decoder_close();
            if (lhdcv5_media_dump != NULL) fclose(lhdcv5_media_dump);
            lhdcv5_media_dump = NULL;
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
                                "BTstack LHDC V5 Receiver", "BTstack");
    sdp_register_service(sdp_a2dp_sink_service_buffer);

    memset(device_id_sdp_service_buffer, 0, sizeof(device_id_sdp_service_buffer));
    device_id_create_sdp_record(device_id_sdp_service_buffer,
                                sdp_create_service_record_handle(),
                                DEVICE_ID_VENDOR_ID_SOURCE_BLUETOOTH,
                                BLUETOOTH_COMPANY_ID_BLUEKITCHEN_GMBH, 1, 1);
    sdp_register_service(device_id_sdp_service_buffer);

    gap_set_local_name("BTstack LHDC V5 Receiver 00:00:00:00:00:00");
    gap_set_class_of_device(0x240400);
    gap_discoverable_control(1);
    gap_set_default_link_policy_settings(
        LM_LINK_POLICY_ENABLE_ROLE_SWITCH | LM_LINK_POLICY_ENABLE_SNIFF_MODE);
    gap_set_allow_role_switch(true);

    hci_event_callback_registration.callback = &hci_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    pcm_queue_reset();

    printf("A2DP endpoints: SBC SEID %u, LHDC V5 SEID %u\n",
           sbc_local_seid, lhdcv5_local_seid);
    printf("LHDC V5 capabilities: 44.1/48/96/192 kHz, 16/24-bit, lossy\n");
}

int btstack_main(int argc, const char *argv[]) {
    UNUSED(argc);
    UNUSED(argv);
    setup_receiver();
    printf("Starting BTstack LHDC V5 receiver...\n");
    hci_power_control(HCI_POWER_ON);
    return 0;
}
