#define BTSTACK_FILE__ "hfp_hf.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hfp_hf.h"
#include "bt_controller.h"
#include "sco_audio.h"
#include "btstack.h"
#include "classic/hfp_hf.h"
#include "classic/sdp_server.h"
#include "btstack_event.h"

static hfp_hf_status_t s_status;
static hfp_status_changed_callback_t s_status_callback = NULL;

static void notify_status_change(void);

static uint8_t s_sdp_service_buffer[150];
static const uint8_t s_rfcomm_channel_nr = 1;
static const char s_service_name[] = "Handsfree Gateway";

static uint8_t s_codecs[] = {
    HFP_CODEC_CVSD,
#ifdef ENABLE_HFP_WIDE_BAND_SPEECH
    HFP_CODEC_MSBC,
#endif
};

static uint16_t s_indicators[2] = {0x01, 0x02}; // Enhanced Safety, Battery Level

static void notify_status_change(void) {
    if (s_status_callback) {
        s_status_callback(&s_status);
    }
}

static uint8_t s_call_indicator = 0;
static uint8_t s_callsetup_indicator = 0;

static void update_call_state_from_indicators(void) {
    if (s_call_indicator == 1) {
        s_status.call_state = HFP_STATE_ACTIVE_CALL;
    } else if (s_callsetup_indicator == 1) {
        s_status.call_state = HFP_STATE_INCOMING_CALL;
    } else if (s_callsetup_indicator == 2 || s_callsetup_indicator == 3) {
        s_status.call_state = HFP_STATE_OUTGOING_CALL;
    } else {
        // No active call and no call setup in progress
        s_status.call_state = s_status.is_slc_connected ? HFP_STATE_SLC_CONNECTED : HFP_STATE_IDLE;
        s_status.caller_id[0] = '\0';
        s_status.caller_name[0] = '\0';
    }
}

static void hfp_hf_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;

    uint8_t event = hci_event_packet_get_type(packet);
    if (event != HCI_EVENT_HFP_META) return;

    uint8_t subevent = hci_event_hfp_meta_get_subevent_code(packet);
    switch (subevent) {
        case HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_ESTABLISHED: {
            uint8_t status = hfp_subevent_service_level_connection_established_get_status(packet);
            if (status == ERROR_CODE_SUCCESS) {
                s_status.acl_handle = little_endian_read_16(packet, 3);
                hfp_subevent_service_level_connection_established_get_bd_addr(packet, s_status.peer_addr);
                snprintf(s_status.peer_addr_str, sizeof(s_status.peer_addr_str), "%s", bd_addr_to_str(s_status.peer_addr));
                s_status.is_slc_connected = true;
                s_call_indicator = 0;
                s_callsetup_indicator = 0;
                s_status.call_state = HFP_STATE_SLC_CONNECTED;
                if (strlen(s_status.device_name) == 0) {
                    strncpy(s_status.device_name, "Connected Phone", sizeof(s_status.device_name) - 1);
                }
                printf("\n[HFP_HF] >>> SLC ESTABLISHED with %s (Handle: 0x%04x) <<<\n",
                       s_status.peer_addr_str, s_status.acl_handle);
                
                // Save last connected device address persistently
                bt_hfp_save_last_device(s_status.peer_addr_str);

                // Query Network operator name
                hfp_hf_query_operator_selection(s_status.acl_handle);
                // Enable call waiting notifications
                hfp_hf_activate_call_waiting_notification(s_status.acl_handle);
                // Enable caller ID notifications
                hfp_hf_activate_calling_line_notification(s_status.acl_handle);
            } else {
                printf("[HFP_HF] SLC Establishment FAILED (Status: 0x%02x)\n", status);
                s_status.is_slc_connected = false;
                s_call_indicator = 0;
                s_callsetup_indicator = 0;
                s_status.call_state = HFP_STATE_IDLE;
            }
            notify_status_change();
            break;
        }

        case HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_RELEASED: {
            printf("\n[HFP_HF] >>> SLC RELEASED <<<\n");
            s_status.is_slc_connected = false;
            s_status.is_audio_connected = false;
            s_status.acl_handle = HCI_CON_HANDLE_INVALID;
            s_call_indicator = 0;
            s_callsetup_indicator = 0;
            s_status.call_state = HFP_STATE_IDLE;
            s_status.caller_id[0] = '\0';
            s_status.caller_name[0] = '\0';
            s_status.device_name[0] = '\0';
            s_status.network_operator[0] = '\0';
            notify_status_change();
            break;
        }

        case HFP_SUBEVENT_AUDIO_CONNECTION_ESTABLISHED: {
            uint8_t status = hfp_subevent_audio_connection_established_get_status(packet);
            if (status == ERROR_CODE_SUCCESS) {
                s_status.negotiated_codec = hfp_subevent_audio_connection_established_get_negotiated_codec(packet);
                s_status.is_audio_connected = true;
                sco_audio_set_codec(s_status.negotiated_codec);
                printf("\n[HFP_HF] >>> AUDIO CONNECTION ESTABLISHED (Codec: %s) <<<\n",
                       s_status.negotiated_codec == HFP_CODEC_MSBC ? "mSBC (Wideband 16kHz)" : "CVSD (8kHz)");
            } else {
                printf("[HFP_HF] Audio Connection FAILED (Status: 0x%02x)\n", status);
                s_status.is_audio_connected = false;
            }
            notify_status_change();
            break;
        }

        case HFP_SUBEVENT_AUDIO_CONNECTION_RELEASED: {
            printf("\n[HFP_HF] >>> AUDIO CONNECTION RELEASED <<<\n");
            s_status.is_audio_connected = false;
            if (s_call_indicator == 0 && s_callsetup_indicator == 0) {
                s_status.call_state = s_status.is_slc_connected ? HFP_STATE_SLC_CONNECTED : HFP_STATE_IDLE;
                s_status.caller_id[0] = '\0';
                s_status.caller_name[0] = '\0';
            }
            notify_status_change();
            break;
        }

        case HFP_SUBEVENT_START_RINGING:
        case HFP_SUBEVENT_RING: {
            printf("[HFP_HF] >>> RING <<< (Incoming Call)\n");
            s_callsetup_indicator = 1;
            s_status.call_state = HFP_STATE_INCOMING_CALL;
            notify_status_change();
            break;
        }

        case HFP_SUBEVENT_STOP_RINGING: {
            s_callsetup_indicator = 0;
            if (s_call_indicator == 0) {
                s_status.call_state = s_status.is_slc_connected ? HFP_STATE_SLC_CONNECTED : HFP_STATE_IDLE;
                s_status.caller_id[0] = '\0';
                s_status.caller_name[0] = '\0';
                notify_status_change();
            }
            break;
        }

        case HFP_SUBEVENT_CALLING_LINE_IDENTIFICATION_NOTIFICATION: {
            const char *number = (const char *)hfp_subevent_calling_line_identification_notification_get_number(packet);
            const char *alpha = (const char *)hfp_subevent_calling_line_identification_notification_get_alpha(packet);
            uint8_t num_len = hfp_subevent_calling_line_identification_notification_get_number_length(packet);
            uint8_t alpha_len = hfp_subevent_calling_line_identification_notification_get_alpha_length(packet);
            if (number && num_len > 0) {
                snprintf(s_status.caller_id, sizeof(s_status.caller_id), "%.*s", num_len, number);
            }
            if (alpha && alpha_len > 0) {
                snprintf(s_status.caller_name, sizeof(s_status.caller_name), "%.*s", alpha_len, alpha);
            }
            printf("[HFP_HF] Incoming Caller ID: %s (Name: %s)\n", s_status.caller_id, s_status.caller_name);
            notify_status_change();
            break;
        }

        case HFP_SUBEVENT_CALL_WAITING_NOTIFICATION: {
            const char *number = (const char *)hfp_subevent_call_waiting_notification_get_number(packet);
            uint8_t num_len = hfp_subevent_call_waiting_notification_get_number_length(packet);
            if (number && num_len > 0) {
                printf("[HFP_HF] Call Waiting from: %.*s\n", num_len, number);
            }
            break;
        }

        case HFP_SUBEVENT_CALL_ANSWERED: {
            printf("[HFP_HF] Call Status: ANSWERED (Active)\n");
            s_call_indicator = 1;
            s_callsetup_indicator = 0;
            s_status.call_state = HFP_STATE_ACTIVE_CALL;
            notify_status_change();
            break;
        }

        case HFP_SUBEVENT_CALL_TERMINATED: {
            printf("[HFP_HF] Call Status: TERMINATED\n");
            s_call_indicator = 0;
            s_callsetup_indicator = 0;
            s_status.call_state = s_status.is_slc_connected ? HFP_STATE_SLC_CONNECTED : HFP_STATE_IDLE;
            s_status.caller_id[0] = '\0';
            s_status.caller_name[0] = '\0';
            notify_status_change();
            break;
        }

        case HFP_SUBEVENT_NETWORK_OPERATOR_CHANGED: {
            const char *op = hfp_subevent_network_operator_changed_get_network_operator_name(packet);
            if (op && strlen(op) > 0) {
                snprintf(s_status.network_operator, sizeof(s_status.network_operator), "%s", op);
                printf("[HFP_HF] Network Operator: %s\n", s_status.network_operator);
                notify_status_change();
            }
            break;
        }

        case HFP_SUBEVENT_SPEAKER_VOLUME: {
            s_status.speaker_volume = hfp_subevent_speaker_volume_get_gain(packet);
            printf("[HFP_HF] Remote Speaker Volume: %u/15\n", s_status.speaker_volume);
            notify_status_change();
            break;
        }

        case HFP_SUBEVENT_MICROPHONE_VOLUME: {
            s_status.mic_gain = hfp_subevent_microphone_volume_get_gain(packet);
            printf("[HFP_HF] Remote Microphone Gain: %u/15\n", s_status.mic_gain);
            notify_status_change();
            break;
        }

        case HFP_SUBEVENT_AG_INDICATOR_STATUS_CHANGED: {
            const char *name = hfp_subevent_ag_indicator_status_changed_get_indicator_name(packet);
            uint8_t value = hfp_subevent_ag_indicator_status_changed_get_indicator_status(packet);
            if (name) {
                if (strcmp(name, "call") == 0) {
                    s_call_indicator = value;
                    update_call_state_from_indicators();
                    printf("[HFP_HF] AG Indicator 'call' = %u (New state: %d)\n", value, s_status.call_state);
                } else if (strcmp(name, "callsetup") == 0) {
                    s_callsetup_indicator = value;
                    update_call_state_from_indicators();
                    printf("[HFP_HF] AG Indicator 'callsetup' = %u (New state: %d)\n", value, s_status.call_state);
                } else if (strcmp(name, "service") == 0) {
                    s_status.service_status = value;
                } else if (strcmp(name, "signal") == 0) {
                    s_status.signal_strength = value;
                } else if (strcmp(name, "battchg") == 0) {
                    s_status.battery_level = value;
                }
            }
            notify_status_change();
            break;
        }

        default:
            break;
    }
}

int bt_hfp_init(hfp_status_changed_callback_t on_status_changed_cb) {
    memset(&s_status, 0, sizeof(s_status));
    s_status.acl_handle = HCI_CON_HANDLE_INVALID;
    s_status.call_state = HFP_STATE_IDLE;
    s_status.speaker_volume = 9;
    s_status.mic_gain = 9;
    s_status_callback = on_status_changed_cb;

    // Supported features bitmask
    uint32_t supported_features = 
        (1 << HFP_HFSF_EC_NR_FUNCTION) |
        (1 << HFP_HFSF_THREE_WAY_CALLING) |
        (1 << HFP_HFSF_CLI_PRESENTATION_CAPABILITY) |
        (1 << HFP_HFSF_REMOTE_VOLUME_CONTROL) |
        (1 << HFP_HFSF_ENHANCED_CALL_STATUS) |
        (1 << HFP_HFSF_ENHANCED_CALL_CONTROL) |
#ifdef ENABLE_HFP_WIDE_BAND_SPEECH
        (1 << HFP_HFSF_CODEC_NEGOTIATION) |
#endif
        (1 << HFP_HFSF_HF_INDICATORS) |
        (1 << HFP_HFSF_ESCO_S4);

    // 1. Initialize HFP HF core
    hfp_hf_init(s_rfcomm_channel_nr);
    hfp_hf_init_supported_features(supported_features);
    hfp_hf_init_codecs(sizeof(s_codecs), s_codecs);
    hfp_hf_init_hf_indicators(sizeof(s_indicators) / sizeof(uint16_t), s_indicators);
    hfp_hf_register_packet_handler(&hfp_hf_packet_handler);

    // 2. Register SDP Service Record for HFP HF (Channel 1)
    memset(s_sdp_service_buffer, 0, sizeof(s_sdp_service_buffer));
    hfp_hf_create_sdp_record_with_codecs(
        s_sdp_service_buffer,
        0x10001,
        s_rfcomm_channel_nr,
        s_service_name,
        (uint16_t)supported_features,
        sizeof(s_codecs),
        s_codecs
    );
    sdp_register_service(s_sdp_service_buffer);

    printf("[HFP_HF] Initialized HFP 1.8 Hands-Free profile on RFCOMM channel %u\n", s_rfcomm_channel_nr);
    return 0;
}

int bt_hfp_connect(const bd_addr_t remote_addr) {
    if (s_status.is_slc_connected) return 0;
    s_status.call_state = HFP_STATE_CONNECTING_SLC;
    bd_addr_t target_addr;
    memcpy(target_addr, remote_addr, sizeof(bd_addr_t));
    return hfp_hf_establish_service_level_connection(target_addr);
}

int bt_hfp_connect_addr_string(const char *addr_str) {
    if (!addr_str) return -1;
    bd_addr_t target_addr;
    if (sscanf_bd_addr(addr_str, target_addr)) {
        return bt_hfp_connect(target_addr);
    }
    return -1;
}

void bt_hfp_save_last_device(const char *addr_str) {
    if (!addr_str || strlen(addr_str) < 11) return;
    FILE *f = fopen("last_device.txt", "w");
    if (f) {
        fprintf(f, "%s\n", addr_str);
        fclose(f);
    }
}

bool bt_hfp_get_last_device(bd_addr_t out_addr, char *out_addr_str, size_t str_len) {
    FILE *f = fopen("last_device.txt", "r");
    if (!f) return false;
    char line[64];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return false;
    }
    fclose(f);

    // Trim whitespace / newlines
    char *p = line;
    while (*p && (*p == ' ' || *p == '\t')) p++;
    char *end = p + strlen(p) - 1;
    while (end > p && (*end == '\r' || *end == '\n' || *end == ' ' || *end == '\t')) {
        *end = '\0';
        end--;
    }

    if (strlen(p) < 11) return false;

    bd_addr_t parsed;
    if (sscanf_bd_addr(p, parsed)) {
        if (out_addr) memcpy(out_addr, parsed, sizeof(bd_addr_t));
        if (out_addr_str && str_len > 0) {
            strncpy(out_addr_str, p, str_len - 1);
            out_addr_str[str_len - 1] = '\0';
        }
        return true;
    }
    return false;
}

void bt_hfp_set_device_name(const char *name) {
    if (!name || strlen(name) == 0) return;
    strncpy(s_status.device_name, name, sizeof(s_status.device_name) - 1);
    s_status.device_name[sizeof(s_status.device_name) - 1] = '\0';
    notify_status_change();
}

int bt_hfp_disconnect(void) {
    if (s_status.acl_handle == HCI_CON_HANDLE_INVALID) return -1;
    return hfp_hf_release_service_level_connection(s_status.acl_handle);
}

int bt_hfp_dial(const char *number) {
    if (!s_status.is_slc_connected || !number) {
        printf("[HFP_HF] Cannot dial '%s': SLC is not connected\n", number ? number : "null");
        return -1;
    }
    char num_buf[64];
    strncpy(num_buf, number, sizeof(num_buf) - 1);
    num_buf[sizeof(num_buf) - 1] = '\0';
    snprintf(s_status.caller_id, sizeof(s_status.caller_id), "%s", num_buf);
    s_callsetup_indicator = 2; // outgoing call
    s_status.call_state = HFP_STATE_OUTGOING_CALL;
    notify_status_change();
    printf("[HFP_HF] Dialing number '%s' on ACL handle 0x%04x...\n", num_buf, s_status.acl_handle);

    if (s_status.acl_handle == HCI_CON_HANDLE_INVALID) {
        printf("[HFP_HF] Error: ACL handle is invalid\n");
        return -1;
    }
    int res = hfp_hf_dial_number(s_status.acl_handle, num_buf);
    printf("[HFP_HF] hfp_hf_dial_number returned: %d\n", res);
    return res;
}

int bt_hfp_answer(void) {
    if (!s_status.is_slc_connected) {
        printf("[HFP_HF] Cannot answer: SLC is not connected\n");
        return -1;
    }
    printf("[HFP_HF] Answering incoming call on ACL handle 0x%04x...\n", s_status.acl_handle);

    if (s_status.acl_handle == HCI_CON_HANDLE_INVALID) return -1;
    int res = hfp_hf_answer_incoming_call(s_status.acl_handle);
    return res;
}

int bt_hfp_hangup(void) {
    if (!s_status.is_slc_connected) return -1;
    printf("[HFP_HF] Terminating call...\n");

    if (s_status.acl_handle == HCI_CON_HANDLE_INVALID) return -1;
    int res = hfp_hf_terminate_call(s_status.acl_handle);
    if (s_status.is_audio_connected) {
        printf("[HFP_HF] Releasing SCO audio connection on hangup...\n");
        hfp_hf_release_audio_connection(s_status.acl_handle);
    }
    s_status.call_state = s_status.is_slc_connected ? HFP_STATE_SLC_CONNECTED : HFP_STATE_IDLE;
    s_status.caller_id[0] = '\0';
    s_status.caller_name[0] = '\0';
    notify_status_change();
    return res;
}

int bt_hfp_send_dtmf(char code) {
    if (!s_status.is_slc_connected || s_status.acl_handle == HCI_CON_HANDLE_INVALID) return -1;
    return hfp_hf_send_dtmf_code(s_status.acl_handle, code);
}

int bt_hfp_establish_audio(void) {
    if (!s_status.is_slc_connected || s_status.acl_handle == HCI_CON_HANDLE_INVALID) return -1;
    return hfp_hf_establish_audio_connection(s_status.acl_handle);
}

int bt_hfp_release_audio(void) {
    if (!s_status.is_slc_connected || s_status.acl_handle == HCI_CON_HANDLE_INVALID) return -1;
    return hfp_hf_release_audio_connection(s_status.acl_handle);
}

int bt_hfp_set_speaker_volume(uint8_t volume) {
    if (volume > 15) volume = 15;
    s_status.speaker_volume = volume;
    if (s_status.is_slc_connected && s_status.acl_handle != HCI_CON_HANDLE_INVALID) {
        return hfp_hf_set_speaker_gain(s_status.acl_handle, volume);
    }
    return 0;
}

int bt_hfp_set_mic_gain(uint8_t gain) {
    if (gain > 15) gain = 15;
    s_status.mic_gain = gain;
    if (s_status.is_slc_connected && s_status.acl_handle != HCI_CON_HANDLE_INVALID) {
        return hfp_hf_set_microphone_gain(s_status.acl_handle, gain);
    }
    return 0;
}

void bt_hfp_get_status(hfp_hf_status_t *out_status) {
    if (out_status) {
        memcpy(out_status, &s_status, sizeof(hfp_hf_status_t));
    }
}
