#define BTSTACK_FILE__ "sco_audio.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "sco_audio.h"
#include "bt_controller.h"
#include "btstack.h"
#include "btstack_event.h"
#include "classic/btstack_sbc.h"
#include "classic/btstack_sbc_bluedroid.h"
#include "classic/hfp_codec.h"
#include "audio/audio_common.h"
#include "audio/audio_render.h"
#include "audio/audio_capture.h"
#include "audio/call_recorder.h"
#include "hfp_hf.h"
#include "diag_logger.h"


#define SINE_TABLE_SIZE 8
// 1000 Hz sine wave at 8000 Hz sample rate (8 samples per cycle)
static const int16_t s_sine_table[SINE_TABLE_SIZE] = {
    0, 11585, 16384, 11585, 0, -11585, -16384, -11585
};

static audio_ring_buffer_t s_tx_ring_buf;

static sco_audio_stats_t s_stats;
static sco_rx_pcm_callback_t s_rx_callback = NULL;
static int s_sine_index = 0;
static btstack_packet_callback_registration_t s_hci_sco_event_registration;
static uint8_t s_negotiated_codec = HFP_CODEC_CVSD;

// SBC / mSBC codec instances
static btstack_sbc_decoder_bluedroid_t s_sbc_decoder_context;
static const btstack_sbc_decoder_t * s_sbc_decoder_instance = NULL;

static btstack_sbc_encoder_bluedroid_t s_sbc_encoder_context;
static const btstack_sbc_encoder_t * s_sbc_encoder_instance = NULL;
static hfp_codec_t s_hfp_codec;

static int16_t s_last_peak_rx = 0;
static int16_t s_last_peak_tx = 0;

static float s_rx_energy_envelope = 0.0f;

static void update_rx_envelope(const int16_t *samples, int count) {
    if (count <= 0) return;
    double sum = 0;
    for (int i = 0; i < count; i++) {
        sum += (double)samples[i] * (double)samples[i];
    }
    float rms = (float)sqrt(sum / (double)count);
    if (rms > s_rx_energy_envelope) {
        s_rx_energy_envelope = 0.4f * s_rx_energy_envelope + 0.6f * rms;
    } else {
        s_rx_energy_envelope = 0.94f * s_rx_energy_envelope + 0.06f * rms;
    }
}

static void sbc_decoder_pcm_callback(int16_t * data, int num_samples, int num_channels, int sample_rate, void * context) {
    UNUSED(num_channels);
    UNUSED(sample_rate);
    UNUSED(context);

    for (int i = 0; i < num_samples; i++) {
        int16_t abs_val = data[i] < 0 ? -data[i] : data[i];
        if (abs_val > s_last_peak_rx) s_last_peak_rx = abs_val;
    }

    update_rx_envelope(data, num_samples);
    call_recorder_write_rx(data, num_samples);
    audio_render_push_samples(data, num_samples);

    if (s_rx_callback && num_samples > 0) {
        s_rx_callback(data, num_samples);
    }
}

void sco_audio_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);

    // 1. Handle internal BTstack SCO events (e.g. HCI_EVENT_SCO_CAN_SEND_NOW)
    if (packet_type == HCI_EVENT_PACKET) {
        if (hci_event_packet_get_type(packet) == HCI_EVENT_SCO_CAN_SEND_NOW) {
            hci_con_handle_t handle = hci_event_sco_can_send_now_get_handle(packet);
            if (handle == HCI_CON_HANDLE_INVALID) {
                handle = s_stats.sco_handle;
            }
            if (s_stats.is_connected && handle != HCI_CON_HANDLE_INVALID) {
                sco_audio_send_next(handle);
            }
        }
        return;
    }

    // 2. Handle incoming SCO data packets from peer device
    if (packet_type != HCI_SCO_DATA_PACKET || size < 3) return;

    s_stats.rx_packets++;
    s_stats.rx_bytes += (size - 3);

    // Air Mode 0x03 (Transparent) or negotiated mSBC codec: decode via mSBC
    if ((s_negotiated_codec == HFP_CODEC_MSBC || s_stats.air_mode == 0x03) && s_sbc_decoder_instance) {
        uint8_t packet_status_flag = (packet[1] >> 4) & 0x03;
        s_sbc_decoder_instance->decode_signed_16(&s_sbc_decoder_context, packet_status_flag, packet + 3, size - 3);
    } else {
        // Air Mode 0x02 (CVSD) or linear PCM (8 kHz)
        uint8_t *payload = &packet[3];
        uint16_t payload_len = size - 3;
        int num_samples = payload_len / sizeof(int16_t);
        const int16_t *raw_samples = (const int16_t *)payload;

        int16_t samples[256];
        if (num_samples > 256) num_samples = 256;
        for (int i = 0; i < num_samples; i++) {
            samples[i] = raw_samples[i];
        }

        for (int i = 0; i < num_samples; i++) {
            int16_t abs_val = samples[i] < 0 ? -samples[i] : samples[i];
            if (abs_val > s_last_peak_rx) s_last_peak_rx = abs_val;
        }

        update_rx_envelope(samples, num_samples);
        call_recorder_write_rx(samples, num_samples);
        audio_render_push_samples(samples, num_samples);

        if (s_rx_callback && num_samples > 0) {
            s_rx_callback(samples, num_samples);
        }
    }
}


void sco_audio_send_next(hci_con_handle_t sco_handle) {
    if (sco_handle == HCI_CON_HANDLE_INVALID || !s_stats.is_connected) return;

    int total_packet_len = hci_get_sco_packet_length_for_connection(sco_handle);
    if (total_packet_len <= 3) {
        if (s_stats.tx_packet_length > 0) {
            total_packet_len = s_stats.tx_packet_length + 3;
        } else {
            total_packet_len = (s_negotiated_codec == HFP_CODEC_MSBC || s_stats.air_mode == 0x03) ? 63 : 49;
        }
    }
    int payload_len = total_packet_len - 3;

    if (!hci_can_send_prepared_sco_packet_now()) {
        hci_request_sco_can_send_now_event_for_con_handle(sco_handle);
        return;
    }

    hci_reserve_packet_buffer();
    uint8_t *sco_packet = hci_get_outgoing_packet_buffer();
    uint8_t *audio_payload = &sco_packet[3];

    if ((s_negotiated_codec == HFP_CODEC_MSBC || s_stats.air_mode == 0x03) && s_sbc_encoder_instance) {
        // Encode via hfp_codec for mSBC
        uint16_t bytes_to_fill = (uint16_t)payload_len;
        uint8_t *out_ptr = audio_payload;
        while (bytes_to_fill > 0) {
            if (hfp_codec_can_encode_audio_frame_now(&s_hfp_codec)) {
                int num_samples = hfp_codec_num_audio_samples_per_frame(&s_hfp_codec);
                int16_t sample_buffer[128];
                if (num_samples > 128) num_samples = 128;

                switch (s_stats.tx_mode) {
                    case SCO_TX_MODE_TEST_TONE:
                        for (int i = 0; i < num_samples; i++) {
                            sample_buffer[i] = s_sine_table[s_sine_index];
                            s_sine_index = (s_sine_index + 1) % SINE_TABLE_SIZE;
                        }
                        break;
                    case SCO_TX_MODE_WASAPI_MIC: {
                        int popped = audio_ring_buffer_pop(&s_tx_ring_buf, sample_buffer, num_samples);
                        if (popped < num_samples) {
                            memset(&sample_buffer[popped], 0, (num_samples - popped) * sizeof(int16_t));
                        }
                        break;
                    }
                    case SCO_TX_MODE_SILENCE:
                    default:
                        memset(sample_buffer, 0, num_samples * sizeof(int16_t));
                        break;
                }
                hfp_codec_encode_audio_frame(&s_hfp_codec, sample_buffer);
            }
            uint16_t avail = hfp_codec_num_bytes_available(&s_hfp_codec);
            if (avail == 0) {
                memset(out_ptr, 0, bytes_to_fill);
                break;
            }
            uint16_t read_bytes = (avail < bytes_to_fill) ? avail : bytes_to_fill;
            hfp_codec_read_from_stream(&s_hfp_codec, out_ptr, read_bytes);
            bytes_to_fill -= read_bytes;
            out_ptr += read_bytes;
        }
    } else {
        // CVSD linear PCM (8 kHz)
        int samples_needed = payload_len / sizeof(int16_t);
        int16_t *pcm_payload = (int16_t *)audio_payload;

        switch (s_stats.tx_mode) {
            case SCO_TX_MODE_TEST_TONE:
                for (int i = 0; i < samples_needed; i++) {
                    pcm_payload[i] = s_sine_table[s_sine_index];
                    s_sine_index = (s_sine_index + 1) % SINE_TABLE_SIZE;
                }
                break;

            case SCO_TX_MODE_WASAPI_MIC: {
                int popped = audio_ring_buffer_pop(&s_tx_ring_buf, pcm_payload, samples_needed);
                if (popped < samples_needed) {
                    memset(&pcm_payload[popped], 0, (samples_needed - popped) * sizeof(int16_t));
                }
                break;
            }

            case SCO_TX_MODE_SILENCE:
            default:
                memset(pcm_payload, 0, payload_len);
                break;
        }
    }

    // Pack 3-byte HCI SCO Header: Connection Handle (12 bits) + Packet Status (4 bits) + Total Length (8 bits)
    little_endian_store_16(sco_packet, 0, sco_handle);
    sco_packet[2] = (uint8_t)payload_len;

    // Dispatch packet to HCI transport
    hci_send_sco_packet_buffer(total_packet_len);

    s_stats.tx_packets++;
    s_stats.tx_bytes += payload_len;

    // Request the next send slot immediately to sustain the audio stream
    hci_request_sco_can_send_now_event_for_con_handle(sco_handle);
}

void sco_audio_hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;

    uint8_t event = hci_event_packet_get_type(packet);
    switch (event) {
        case HCI_EVENT_SYNCHRONOUS_CONNECTION_COMPLETE: {
            uint8_t status = hci_event_synchronous_connection_complete_get_status(packet);
            if (status == ERROR_CODE_SUCCESS) {
                s_stats.sco_handle = hci_event_synchronous_connection_complete_get_handle(packet);
                s_stats.link_type = hci_event_synchronous_connection_complete_get_link_type(packet);
                s_stats.transmission_interval = hci_event_synchronous_connection_complete_get_transmission_interval(packet);
                s_stats.retransmission_window = hci_event_synchronous_connection_complete_get_retransmission_interval(packet);
                s_stats.rx_packet_length = hci_event_synchronous_connection_complete_get_rx_packet_length(packet);
                s_stats.tx_packet_length = hci_event_synchronous_connection_complete_get_tx_packet_length(packet);
                s_stats.air_mode = hci_event_synchronous_connection_complete_get_air_mode(packet);
                s_stats.is_connected = true;

                diag_log("\n[SCO_AUDIO] >>> SYNCHRONOUS CONNECTION COMPLETE <<<");
                diag_log("[SCO_AUDIO] Handle: 0x%04x | LinkType: 0x%02x (eSCO) | AirMode: 0x%02x (%s)",
                         s_stats.sco_handle, s_stats.link_type, s_stats.air_mode,
                         s_stats.air_mode == 0x03 ? "Transparent/mSBC (16kHz)" : "CVSD (8kHz)");
                diag_log("[SCO_AUDIO] Packet Lengths -> RX: %u bytes, TX: %u bytes, Interval: %u",
                         s_stats.rx_packet_length, s_stats.tx_packet_length, s_stats.transmission_interval);

                // Configure render and capture sample rates according to air mode
                if (s_stats.air_mode == 0x03) {
                    s_negotiated_codec = HFP_CODEC_MSBC;
                    audio_render_set_source_sample_rate(16000);
                    audio_capture_set_target_sample_rate(16000);
                } else {
                    s_negotiated_codec = HFP_CODEC_CVSD;
                    audio_render_set_source_sample_rate(8000);
                    audio_capture_set_target_sample_rate(8000);
                }

                // Reset TX ring buffer and tone index
                audio_ring_buffer_clear(&s_tx_ring_buf);
                s_sine_index = 0;

                // NOTE: audio engine start/stop is intentionally NOT done here.
                // main.c's on_hfp_status_changed owns the audio-engine lifecycle
                // with edge detection (start once when SCO opens, stop once when
                // it releases). Managing it in two places raced the notify path
                // and could kill live call audio.

                // Prime the SCO TX pipeline
                hci_request_sco_can_send_now_event_for_con_handle(s_stats.sco_handle);

                // Start automatic FLAC/WAV call recording
                uint32_t sample_rate = (s_stats.air_mode == 0x03) ? 16000 : 8000;
                hfp_hf_status_t hfp_stat;
                bt_hfp_get_status(&hfp_stat);
                const char *prefix = strlen(hfp_stat.caller_id) > 0 ? hfp_stat.caller_id : "call";
                call_recorder_start(sample_rate, prefix);
            } else {
                diag_log("[SCO_AUDIO] Connection FAILED (Status: 0x%02x)", status);
                s_stats.is_connected = false;
                s_stats.sco_handle = HCI_CON_HANDLE_INVALID;
                s_stats.errors++;
            }
            break;
        }

        case HCI_EVENT_DISCONNECTION_COMPLETE: {
            hci_con_handle_t handle = hci_event_disconnection_complete_get_connection_handle(packet);
            // Only react to the teardown of OUR SCO link. Previously this also
            // matched on `|| s_stats.is_connected`, so a disconnect on any other
            // handle (e.g. the ACL/SLC link) while SCO was up would prematurely
            // stop the recording and desync SCO state.
            if (s_stats.is_connected && handle == s_stats.sco_handle &&
                s_stats.sco_handle != HCI_CON_HANDLE_INVALID) {
                diag_log("[SCO_AUDIO] >>> SCO DISCONNECTED (Handle: 0x%04x) <<<", s_stats.sco_handle);
                s_stats.is_connected = false;
                s_stats.sco_handle = HCI_CON_HANDLE_INVALID;
                // Audio engine stop is owned by main.c (edge-detected). See note
                // in the SYNCHRONOUS_CONNECTION_COMPLETE handler above.
                call_recorder_stop();
            }
            break;
        }

        default:
            break;
    }
}

void sco_audio_init(void) {
    memset(&s_stats, 0, sizeof(s_stats));
    s_stats.sco_handle = HCI_CON_HANDLE_INVALID;
    s_stats.tx_mode = SCO_TX_MODE_WASAPI_MIC;
    s_negotiated_codec = HFP_CODEC_CVSD;

    audio_ring_buffer_init(&s_tx_ring_buf, 32000);
    call_recorder_init();

#ifdef ENABLE_HFP_WIDE_BAND_SPEECH
    // Initialize mSBC decoder
    s_sbc_decoder_instance = btstack_sbc_decoder_bluedroid_init_instance(&s_sbc_decoder_context);
    if (s_sbc_decoder_instance && s_sbc_decoder_instance->configure) {
        s_sbc_decoder_instance->configure(&s_sbc_decoder_context, SBC_MODE_mSBC, &sbc_decoder_pcm_callback, NULL);
    }

    // Initialize mSBC encoder via hfp_codec
    s_sbc_encoder_instance = btstack_sbc_encoder_bluedroid_init_instance(&s_sbc_encoder_context);
    if (s_sbc_encoder_instance) {
        hfp_codec_init_msbc_with_codec(&s_hfp_codec, s_sbc_encoder_instance, &s_sbc_encoder_context);
    }
#endif

    // Register SCO packet and event handlers
    hci_register_sco_packet_handler(&sco_audio_packet_handler);

    s_hci_sco_event_registration.callback = &sco_audio_hci_event_handler;
    hci_add_event_handler(&s_hci_sco_event_registration);

    // Set voice setting to 16-bit linear PCM CVSD
    hci_set_sco_voice_setting(0x0060);
}

void sco_audio_set_codec(uint8_t negotiated_codec) {
    s_negotiated_codec = negotiated_codec;
    if (negotiated_codec == HFP_CODEC_MSBC) {
        audio_render_set_source_sample_rate(16000);
        audio_capture_set_target_sample_rate(16000);
        diag_log("[SCO_AUDIO] Codec set to mSBC (16 kHz Wideband Speech)");
    } else {
        audio_render_set_source_sample_rate(8000);
        audio_capture_set_target_sample_rate(8000);
        diag_log("[SCO_AUDIO] Codec set to CVSD (8 kHz Linear PCM)");
    }
}

void sco_audio_set_rx_callback(sco_rx_pcm_callback_t rx_cb) {
    s_rx_callback = rx_cb;
}

void sco_audio_set_tx_mode(sco_tx_mode_t mode) {
    s_stats.tx_mode = mode;
}

int sco_audio_push_tx_samples(const int16_t *samples, int num_samples) {
    if (num_samples <= 0) return 0;

    int16_t clean_samples[2048];
    int count = num_samples > 2048 ? 2048 : num_samples;

    for (int i = 0; i < count; i++) {
        int16_t abs_val = samples[i] < 0 ? -samples[i] : samples[i];
        if (abs_val > s_last_peak_tx) s_last_peak_tx = abs_val;
    }

    memcpy(clean_samples, samples, count * sizeof(int16_t));
    call_recorder_write_tx(clean_samples, count);
    audio_ring_buffer_push(&s_tx_ring_buf, clean_samples, count);
    return count;
}


void sco_audio_get_stats(sco_audio_stats_t *out_stats) {
    if (out_stats) {
        memcpy(out_stats, &s_stats, sizeof(sco_audio_stats_t));
        out_stats->peak_rx_amplitude = (uint16_t)s_last_peak_rx;
        out_stats->peak_tx_amplitude = (uint16_t)s_last_peak_tx;
        s_last_peak_rx = 0; // reset for next sampling window
        s_last_peak_tx = 0;
    }
}

bool sco_audio_is_connected(void) {
    return s_stats.is_connected;
}

hci_con_handle_t sco_audio_get_handle(void) {
    return s_stats.sco_handle;
}
