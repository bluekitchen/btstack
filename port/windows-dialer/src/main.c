#define BTSTACK_FILE__ "main.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "diag_logger.h"
#include "bt/bt_controller.h"
#include "bt/hfp_hf.h"
#include "bt/sco_audio.h"
#include "audio/audio_render.h"
#include "audio/audio_capture.h"
#include "audio/call_recorder.h"
#include "btstack.h"

static bool s_use_mic = true;
static bool s_ipc_mode = false;
static hfp_hf_status_t s_current_hfp_status;

static bd_addr_t s_last_device_addr;
static char s_last_device_str[18] = "";
static bool s_has_last_device = false;

static void on_sco_rx_pcm(const int16_t *samples, int num_samples) {
    UNUSED(samples);
    UNUSED(num_samples);
}

static void on_audio_capture_pcm(const int16_t *samples, int count) {
    if (s_use_mic && sco_audio_is_connected()) {
        sco_audio_push_tx_samples(samples, count);
    }
}

// ---------------------------------------------------------------------------
// IPC JSON Event Dispatcher
// ---------------------------------------------------------------------------
static void emit_ipc_json(const char *json) {
    if (s_ipc_mode) {
        printf("%s\n", json);
        fflush(stdout);
    }
}

static void emit_ipc_state(void) {
    if (!s_ipc_mode) return;

    const char *conn_state = "Disconnected";
    if (s_current_hfp_status.is_slc_connected) {
        conn_state = "SlcEstablished";
    } else if (s_current_hfp_status.call_state == HFP_STATE_CONNECTING_SLC) {
        conn_state = "Connecting";
    }

    const char *call_status = "Idle";
    bool is_outgoing = false;
    switch ((int)s_current_hfp_status.call_state) {
        case (int)HFP_STATE_IDLE: call_status = "Idle"; break;
        case (int)HFP_STATE_CONNECTING_SLC:
        case (int)HFP_STATE_SLC_CONNECTED: call_status = "Idle"; break;
        case (int)HFP_STATE_INCOMING_CALL: call_status = "Incoming"; break;
        case (int)HFP_STATE_OUTGOING_CALL: call_status = "Dialing"; is_outgoing = true; break;
        case (int)HFP_STATE_ACTIVE_CALL: call_status = "Active"; break;
    }

    char json[1024];
    bool has_device = s_current_hfp_status.is_slc_connected && strlen(s_current_hfp_status.peer_addr_str) > 0;
    const char *caller = strlen(s_current_hfp_status.caller_id) > 0 ? s_current_hfp_status.caller_id : "";
    const char *caller_name = strlen(s_current_hfp_status.caller_name) > 0 ? s_current_hfp_status.caller_name : "";
    const char *dev_name = strlen(s_current_hfp_status.device_name) > 0 ? s_current_hfp_status.device_name : "Connected Phone";

    if (strcmp(call_status, "Idle") != 0) {
        snprintf(json, sizeof(json),
            "{\"type\":\"state\",\"connState\":\"%s\",\"deviceName\":\"%s\",\"connDeviceId\":%s%s%s,"
            "\"calls\":[{\"index\":1,\"status\":\"%s\",\"number\":\"%s\",\"name\":\"%s\",\"isOutgoing\":%s,\"isMultiparty\":false}],"
            "\"signalBars\":%u,\"batteryLevel\":%u,\"scoActive\":%s}",
            conn_state,
            dev_name,
            has_device ? "\"" : "", has_device ? s_current_hfp_status.peer_addr_str : "null", has_device ? "\"" : "",
            call_status, caller, caller_name, is_outgoing ? "true" : "false",
            s_current_hfp_status.signal_strength > 5 ? 5 : s_current_hfp_status.signal_strength,
            s_current_hfp_status.battery_level > 5 ? 5 : s_current_hfp_status.battery_level,
            s_current_hfp_status.is_audio_connected ? "true" : "false"
        );
    } else {
        snprintf(json, sizeof(json),
            "{\"type\":\"state\",\"connState\":\"%s\",\"deviceName\":\"%s\",\"connDeviceId\":%s%s%s,"
            "\"calls\":[],"
            "\"signalBars\":%u,\"batteryLevel\":%u,\"scoActive\":%s}",
            conn_state,
            dev_name,
            has_device ? "\"" : "", has_device ? s_current_hfp_status.peer_addr_str : "null", has_device ? "\"" : "",
            s_current_hfp_status.signal_strength > 5 ? 5 : s_current_hfp_status.signal_strength,
            s_current_hfp_status.battery_level > 5 ? 5 : s_current_hfp_status.battery_level,
            s_current_hfp_status.is_audio_connected ? "true" : "false"
        );
    }

    emit_ipc_json(json);
}

static void emit_ipc_devices(void) {
    if (!s_ipc_mode) return;
    char json[512];
    if (s_current_hfp_status.is_slc_connected && strlen(s_current_hfp_status.peer_addr_str) > 0) {
        const char *dev_name = strlen(s_current_hfp_status.device_name) > 0 ? s_current_hfp_status.device_name : "Connected Phone";
        snprintf(json, sizeof(json),
            "{\"type\":\"devices\",\"message\":\"[{\\\"Id\\\":\\\"%s\\\",\\\"Name\\\":\\\"%s\\\",\\\"Paired\\\":true,\\\"Connected\\\":true,\\\"AudioReady\\\":true}]\"}",
            s_current_hfp_status.peer_addr_str,
            dev_name
        );
    } else if (s_has_last_device && strlen(s_last_device_str) > 0) {
        snprintf(json, sizeof(json),
            "{\"type\":\"devices\",\"message\":\"[{\\\"Id\\\":\\\"%s\\\",\\\"Name\\\":\\\"Paired Phone\\\",\\\"Paired\\\":true,\\\"Connected\\\":false,\\\"AudioReady\\\":true}]\"}",
            s_last_device_str
        );
    } else {
        snprintf(json, sizeof(json), "{\"type\":\"devices\",\"message\":\"[]\"}");
    }
    emit_ipc_json(json);
}

static void json_sanitize_path(const char *in, char *out, size_t out_len) {
    if (!in || !out || out_len == 0) return;
    strncpy(out, in, out_len - 1);
    out[out_len - 1] = '\0';
    for (char *p = out; *p; p++) {
        if (*p == '\\') *p = '/';
    }
}

static void on_call_recording_started(const char *session_dir, const char *stereo_wav, const char *rx_wav, const char *tx_wav) {
    char clean_dir[512], clean_stereo[512], clean_rx[512], clean_tx[512];
    json_sanitize_path(session_dir, clean_dir, sizeof(clean_dir));
    json_sanitize_path(stereo_wav, clean_stereo, sizeof(clean_stereo));
    json_sanitize_path(rx_wav, clean_rx, sizeof(clean_rx));
    json_sanitize_path(tx_wav, clean_tx, sizeof(clean_tx));

    char json[2048];
    const char *num = strlen(s_current_hfp_status.caller_id) > 0 ? s_current_hfp_status.caller_id : "Active Call";
    snprintf(json, sizeof(json),
        "{\"type\":\"call-started\",\"number\":\"%s\",\"sessionDir\":\"%s\",\"fullPath\":\"%s\",\"inPath\":\"%s\",\"outPath\":\"%s\"}",
        num, clean_dir, clean_stereo, clean_rx, clean_tx
    );
    emit_ipc_json(json);
}

static void on_call_recording_stopped(const char *session_dir, const char *stereo_wav, const char *rx_wav, const char *tx_wav, double duration_sec) {
    char clean_dir[512], clean_stereo[512], clean_rx[512], clean_tx[512];
    json_sanitize_path(session_dir, clean_dir, sizeof(clean_dir));
    json_sanitize_path(stereo_wav, clean_stereo, sizeof(clean_stereo));
    json_sanitize_path(rx_wav, clean_rx, sizeof(clean_rx));
    json_sanitize_path(tx_wav, clean_tx, sizeof(clean_tx));

    char json[2048];
    const char *num = strlen(s_current_hfp_status.caller_id) > 0 ? s_current_hfp_status.caller_id : "Active Call";
    snprintf(json, sizeof(json),
        "{\"type\":\"call-ended\",\"number\":\"%s\",\"sessionDir\":\"%s\",\"fullPath\":\"%s\",\"inPath\":\"%s\",\"outPath\":\"%s\",\"durationSec\":%.2f}",
        num, clean_dir, clean_stereo, clean_rx, clean_tx, duration_sec
    );
    emit_ipc_json(json);
}

static void on_call_segment_ready(const char *wav_path, const char *channel, double start_sec, double end_sec) {
    if (!s_ipc_mode) return;
    char clean_path[512];
    json_sanitize_path(wav_path, clean_path, sizeof(clean_path));

    char json[1024];
    snprintf(json, sizeof(json),
        "{\"type\":\"segment-ready\",\"channel\":\"%s\",\"path\":\"%s\",\"startSec\":%.3f,\"endSec\":%.3f}",
        channel, clean_path, start_sec, end_sec
    );
    emit_ipc_json(json);
}

static void on_discovery_result(const char *addr_str, const char *name, uint32_t cod, int8_t rssi) {
    UNUSED(cod);
    UNUSED(rssi);
    if (!s_ipc_mode) {
        printf("[DISCOVERY] Found: %s (%s)\n> ", addr_str, name);
        fflush(stdout);
        return;
    }
    char json[512];
    snprintf(json, sizeof(json),
        "{\"type\":\"nearby-device\",\"device\":{\"Id\":\"%s\",\"Name\":\"%s\",\"Paired\":false,\"Connected\":false,\"AudioReady\":true}}",
        addr_str, name
    );
    emit_ipc_json(json);
}

static void on_discovery_complete(void) {
    if (!s_ipc_mode) {
        printf("[DISCOVERY] Discovery scan completed.\n> ");
        fflush(stdout);
        return;
    }
    emit_ipc_json("{\"type\":\"discover-done\"}");
}

static void print_status_banner(void) {
    sco_audio_stats_t sco_stats;
    sco_audio_get_stats(&sco_stats);

    diag_log("------------------- [DIALER-BTSTACK STATUS] -------------------");
    diag_log(" Local Adapter   : %s (TP-Link UB500 / RTL8761BU)", bt_controller_get_bd_addr_string());
    diag_log(" Phone Link      : %s", s_current_hfp_status.is_slc_connected ? "CONNECTED" : "DISCONNECTED / PAIRING");
    if (s_current_hfp_status.is_slc_connected) {
        diag_log(" Phone Address   : %s", s_current_hfp_status.peer_addr_str);
        diag_log(" Operator        : %s | Signal: %u/5 | Battery: %u/5",
                 strlen(s_current_hfp_status.network_operator) > 0 ? s_current_hfp_status.network_operator : "N/A",
                 s_current_hfp_status.signal_strength, s_current_hfp_status.battery_level);
        const char *state_str = "IDLE";
        switch ((int)s_current_hfp_status.call_state) {
            case (int)HFP_STATE_IDLE: state_str = "IDLE"; break;
            case (int)HFP_STATE_CONNECTING_SLC: state_str = "CONNECTING SLC"; break;
            case (int)HFP_STATE_SLC_CONNECTED: state_str = "READY"; break;
            case (int)HFP_STATE_INCOMING_CALL: state_str = "INCOMING CALL"; break;
            case (int)HFP_STATE_OUTGOING_CALL: state_str = "OUTGOING CALL"; break;
            case (int)HFP_STATE_ACTIVE_CALL: state_str = "ACTIVE CALL (Talking)"; break;
        }
        diag_log(" Call State      : %s (%s)", state_str, s_current_hfp_status.caller_id);
    }
#ifdef _WIN32
    diag_log(" Audio Channel   : %s", sco_stats.is_connected ? "ACTIVE (Full Duplex WASAPI)" : "STANDBY");
#elif defined(__APPLE__)
    diag_log(" Audio Channel   : %s", sco_stats.is_connected ? "ACTIVE (Full Duplex CoreAudio)" : "STANDBY");
#else
    diag_log(" Audio Channel   : %s", sco_stats.is_connected ? "ACTIVE (Full Duplex Audio)" : "STANDBY");
#endif
    if (sco_stats.is_connected) {
        diag_log(" Audio Codec     : %s", s_current_hfp_status.negotiated_codec == HFP_CODEC_MSBC ? "mSBC (16kHz Wideband)" : "CVSD (8kHz Standard)");
        diag_log(" Audio Flow      : RX: %u pkts (%u B) | TX: %u pkts (%u B)",
                 sco_stats.rx_packets, sco_stats.rx_bytes, sco_stats.tx_packets, sco_stats.tx_bytes);
    }
#ifdef _WIN32
    diag_log(" Mic Source      : %s", s_use_mic ? "PC Microphone (WASAPI)" : "1kHz Sine Test Tone");
#elif defined(__APPLE__)
    diag_log(" Mic Source      : %s", s_use_mic ? "Mac Microphone (CoreAudio)" : "1kHz Sine Test Tone");
#else
    diag_log(" Mic Source      : %s", s_use_mic ? "Microphone" : "1kHz Sine Test Tone");
#endif
    diag_log(" Volume / Gain   : Speaker %u/15 | Mic Gain %u/15",
             s_current_hfp_status.speaker_volume, s_current_hfp_status.mic_gain);
    diag_log("---------------------------------------------------------------");
}

static void on_hfp_status_changed(const hfp_hf_status_t *status) {
    memcpy(&s_current_hfp_status, status, sizeof(hfp_hf_status_t));

    if (status->is_slc_connected) {
        diag_log("[HFP] SLC ESTABLISHED with %s (Ready to call 121)", status->peer_addr_str);
        if (strlen(status->device_name) == 0 || strcmp(status->device_name, "Connected Phone") == 0) {
            bt_controller_request_remote_name(status->peer_addr);
        }
    }

    if ((int)status->call_state == (int)HFP_STATE_INCOMING_CALL) {
        diag_log("[HFP] INCOMING CALL from %s! Type 'answer' or 'a' to pick up.", status->caller_id);
    } else if ((int)status->call_state == (int)HFP_STATE_ACTIVE_CALL) {
        diag_log("[HFP] CALL ACTIVE with %s", status->peer_addr_str);
    }

    // When audio link opens, configure rates and start audio engine
    if (status->is_audio_connected || sco_audio_is_connected()) {
        if (status->negotiated_codec == HFP_CODEC_MSBC) {
            audio_render_set_source_sample_rate(16000);
            audio_capture_set_target_sample_rate(16000);
        } else {
            audio_render_set_source_sample_rate(8000);
            audio_capture_set_target_sample_rate(8000);
        }
        audio_render_start();
        audio_capture_start();
    }

    if (status->speaker_volume > 0) {
        audio_render_set_volume((float)status->speaker_volume / 15.0f * 1.5f);
    }
    if (status->mic_gain > 0) {
        audio_capture_set_gain((float)status->mic_gain / 15.0f * 2.0f);
    }

    emit_ipc_state();
    emit_ipc_devices();
}

static void on_controller_ready(const bd_addr_t local_addr) {
    UNUSED(local_addr);
    diag_log("[DIALER] Adapter ready! Discoverable as 'PC Dialer'");
    
    // Automatically find and connect to last paired device
    s_has_last_device = bt_hfp_get_last_device(s_last_device_addr, s_last_device_str, sizeof(s_last_device_str));
    if (s_has_last_device) {
        diag_log("[AUTO_CONNECT] Last paired phone found: %s. Initiating automatic connection...", s_last_device_str);
        bt_hfp_connect(s_last_device_addr);
    }

    if (!s_ipc_mode) {
        print_status_banner();
        printf("\nType 'help' for commands list or 'dial 121' to place a test call.\n> ");
        fflush(stdout);
    } else {
        emit_ipc_state();
        emit_ipc_devices();
    }
}

static const char* json_get_string_field(const char *json, const char *key, char *out_val, size_t out_len) {
    if (!json || !key || !out_val || out_len == 0) return NULL;
    char p1[64], p2[64];
    snprintf(p1, sizeof(p1), "\"%s\"", key);
    // Also try capitalized key
    snprintf(p2, sizeof(p2), "\"%c%s\"", (key[0] >= 'a' && key[0] <= 'z') ? (key[0] - 'a' + 'A') : key[0], key + 1);

    const char *pos = strstr(json, p1);
    size_t key_len = strlen(p1);
    if (!pos) {
        pos = strstr(json, p2);
        key_len = strlen(p2);
    }
    if (!pos) return NULL;

    pos += key_len;
    while (*pos && (*pos == ' ' || *pos == ':' || *pos == '\t')) pos++;
    if (*pos != '\"') return NULL;
    pos++;
    size_t i = 0;
    while (*pos && *pos != '\"' && i < out_len - 1) {
        out_val[i++] = *pos++;
    }
    out_val[i] = '\0';
    return out_val;
}

static void process_command_line(char *line) {
    char *p = line;
    while (*p) {
        if (*p == '\r' || *p == '\n') { *p = '\0'; break; }
        p++;
    }

    if (strlen(line) == 0) return;

    diag_log("[CMD] Processing: %s", line);

    // Handle JSON command format e.g. {"cmd":"dial","number":"121"}
    if (line[0] == '{') {
        char cmd_buf[64] = "";
        if (json_get_string_field(line, "cmd", cmd_buf, sizeof(cmd_buf))) {
            if (_stricmp(cmd_buf, "dial") == 0) {
                char num[64] = "";
                if (json_get_string_field(line, "number", num, sizeof(num))) {
                    diag_log("[CMD] JSON Dial number: %s", num);
                    bt_hfp_dial(num);
                }
            } else if (_stricmp(cmd_buf, "answer") == 0) {
                diag_log("[CMD] JSON Answer call");
                bt_hfp_answer();
            } else if (_stricmp(cmd_buf, "hangup") == 0) {
                diag_log("[CMD] JSON Hangup call");
                bt_hfp_hangup();
            } else if (_stricmp(cmd_buf, "connect") == 0) {
                char addr[32] = "";
                if (json_get_string_field(line, "address", addr, sizeof(addr)) ||
                    json_get_string_field(line, "deviceId", addr, sizeof(addr))) {
                    diag_log("[CMD] JSON Connect address: %s", addr);
                    bt_hfp_connect_addr_string(addr);
                } else if (s_has_last_device) {
                    diag_log("[CMD] JSON Connect last device: %s", s_last_device_str);
                    bt_hfp_connect(s_last_device_addr);
                }
            } else if (_stricmp(cmd_buf, "disconnect") == 0) {
                diag_log("[CMD] JSON Disconnect");
                bt_hfp_disconnect();
            } else if (_stricmp(cmd_buf, "dtmf") == 0) {
                char digit[8] = "";
                if (json_get_string_field(line, "digit", digit, sizeof(digit)) && strlen(digit) > 0) {
                    diag_log("[CMD] JSON DTMF: %c", digit[0]);
                    bt_hfp_send_dtmf(digit[0]);
                }
            } else if (_stricmp(cmd_buf, "discover") == 0 || _stricmp(cmd_buf, "discover-nearby") == 0) {
                diag_log("[CMD] JSON Start discovery");
                bt_controller_start_discovery(&on_discovery_result, &on_discovery_complete);
            } else if (_stricmp(cmd_buf, "stop-discover") == 0) {
                diag_log("[CMD] JSON Stop discovery");
                bt_controller_stop_discovery();
            } else if (_stricmp(cmd_buf, "status") == 0) {
                emit_ipc_state();
            } else if (_stricmp(cmd_buf, "quit") == 0 || _stricmp(cmd_buf, "exit") == 0) {
                diag_log("[CMD] JSON Quit");
                audio_render_shutdown();
                audio_capture_shutdown();
                bt_controller_stop();
                diag_logger_close();
                exit(0);
            }
            return;
        }
    }

    // CLI format commands
    if (strncmp(line, "dial ", 5) == 0) {
        const char *num = line + 5;
        while (*num == ' ') num++;
        diag_log("[DIALER] Dialing: %s", num);
        bt_hfp_dial(num);
    } else if (strcmp(line, "dial") == 0 || strcmp(line, "121") == 0 || strcmp(line, "1") == 0) {
        diag_log("[DIALER] Dialing 121...");
        bt_hfp_dial("121");
    } else if (strcmp(line, "answer") == 0 || strcmp(line, "a") == 0) {
        diag_log("[DIALER] Answering call...");
        bt_hfp_answer();
    } else if (strcmp(line, "hangup") == 0 || strcmp(line, "h") == 0) {
        diag_log("[DIALER] Ending call...");
        bt_hfp_hangup();
    } else if (strcmp(line, "status") == 0 || strcmp(line, "s") == 0) {
        print_status_banner();
    } else if (strcmp(line, "connect") == 0 || strcmp(line, "c") == 0) {
        if (s_has_last_device) {
            diag_log("[DIALER] Connecting to last paired device: %s...", s_last_device_str);
            bt_hfp_connect(s_last_device_addr);
        } else {
            diag_log("[DIALER] No last paired device saved. Use: connect <AA:BB:CC:DD:EE:FF>");
        }
    } else if (strncmp(line, "connect ", 8) == 0) {
        const char *addr_str = line + 8;
        bd_addr_t target_addr;
        if (sscanf_bd_addr(addr_str, target_addr)) {
            diag_log("[DIALER] Connecting to %s...", addr_str);
            bt_hfp_connect(target_addr);
        } else {
            diag_log("[ERROR] Invalid Bluetooth address format. Example: connect A8:AB:B5:0C:87:F3");
        }
    } else if (strcmp(line, "disconnect") == 0) {
        diag_log("[DIALER] Disconnecting HFP SLC...");
        bt_hfp_disconnect();
    } else if (strcmp(line, "discover") == 0 || strcmp(line, "scan") == 0) {
        bt_controller_start_discovery(&on_discovery_result, &on_discovery_complete);
    } else if (strcmp(line, "stop-discover") == 0 || strcmp(line, "stop") == 0) {
        bt_controller_stop_discovery();
    } else if (strcmp(line, "audio on") == 0) {
        diag_log("[DIALER] Requesting audio link...");
        bt_hfp_establish_audio();
    } else if (strcmp(line, "audio off") == 0) {
        diag_log("[DIALER] Releasing audio link...");
        bt_hfp_release_audio();
    } else if (strcmp(line, "tone on") == 0) {
        s_use_mic = false;
        sco_audio_set_tx_mode(SCO_TX_MODE_TEST_TONE);
        diag_log("[DIALER] Switched SCO TX to 1kHz Test Tone generator");
    } else if (strcmp(line, "tone off") == 0) {
        s_use_mic = true;
        sco_audio_set_tx_mode(SCO_TX_MODE_LIVE_MIC);
        diag_log("[DIALER] Switched SCO TX to Live Microphone");
    } else if (strncmp(line, "vol ", 4) == 0) {
        int v = atoi(line + 4);
        bt_hfp_set_speaker_volume((uint8_t)v);
        audio_render_set_volume((float)v / 15.0f * 1.5f);
        diag_log("[DIALER] Set Speaker volume to %d/15", v);
    } else if (strncmp(line, "mic ", 4) == 0) {
        int m = atoi(line + 4);
        bt_hfp_set_mic_gain((uint8_t)m);
        audio_capture_set_gain((float)m / 15.0f * 4.0f);
        diag_log("[DIALER] Set Mic gain to %d/15", m);
    } else if (strncmp(line, "dtmf ", 5) == 0) {
        char code = line[5];
        diag_log("[DIALER] Sending DTMF: %c", code);
        bt_hfp_send_dtmf(code);
    } else if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0 || strcmp(line, "q") == 0) {
        diag_log("[DIALER] Shutting down...");
        audio_render_shutdown();
        audio_capture_shutdown();
        bt_controller_stop();
        diag_logger_close();
        exit(0);
    } else {
        printf("\nAvailable Commands:\n");
        printf("  dial 121 (or 1)   - Place call to 121\n");
        printf("  dial <number>     - Place an outgoing call (e.g. dial 9876543210)\n");
        printf("  answer (a)        - Answer incoming call\n");
        printf("  hangup (h)        - Terminate or reject call\n");
        printf("  status (s)        - Show adapter, phone, call, and audio status\n");
        printf("  dtmf <char>       - Send DTMF key tone (0-9, *, #)\n");
        printf("  vol <0-15>        - Set speaker volume\n");
        printf("  mic <0-15>        - Set microphone gain\n");
        printf("  discover          - Scan for nearby Bluetooth devices\n");
        printf("  stop-discover     - Stop scan\n");
        printf("  tone on|off       - Toggle 1kHz test tone vs PC microphone\n");
        printf("  audio on|off      - Transfer audio connection to/from PC\n");
        printf("  connect <address> - Connect to paired phone (e.g. connect A8:AB:B5:0C:87:F3)\n");
        printf("  disconnect        - Disconnect phone\n");
        printf("  exit (q)          - Quit dialer\n");
    }

    if (!s_ipc_mode) {
        printf("> ");
        fflush(stdout);
    }
}

static char s_stdin_buffer[256];
static int s_stdin_pos = 0;

static void stdin_process(char c) {
    if (c == '\r' || c == '\n') {
        s_stdin_buffer[s_stdin_pos] = '\0';
        if (!s_ipc_mode) putchar('\n');
        process_command_line(s_stdin_buffer);
        s_stdin_pos = 0;
    } else if (c == '\b' || c == 127) {
        if (s_stdin_pos > 0) {
            s_stdin_pos--;
            if (!s_ipc_mode) {
                printf("\b \b");
                fflush(stdout);
            }
        }
    } else if (s_stdin_pos < (int)(sizeof(s_stdin_buffer) - 1)) {
        s_stdin_buffer[s_stdin_pos++] = c;
        if (!s_ipc_mode) {
            putchar(c);
            fflush(stdout);
        }
    }
}

int main(int argc, const char * argv[]) {
    // Check for arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--ipc") == 0 || strcmp(argv[i], "-ipc") == 0 || strcmp(argv[i], "--json") == 0) {
            s_ipc_mode = true;
        } else if ((strcmp(argv[i], "--recordings-dir") == 0 || strcmp(argv[i], "-recordings-dir") == 0) && i + 1 < argc) {
            call_recorder_set_recordings_root_dir(argv[++i]);
        }
    }

    // Disable stdout buffering for immediate log visibility
    setvbuf(stdout, NULL, _IONBF, 0);

    // Initialize unified diagnostic session logger & PacketLogger binary dump
    diag_logger_init("dialer_production");

    if (!s_ipc_mode) {
        diag_log("======================================================================");
#ifdef _WIN32
        diag_log("   DIALER-BTSTACK: WINDOWS BLUETOOTH HFP-HF & FULL-DUPLEX AUDIO      ");
        diag_log("   TP-Link UB500 (Realtek RTL8761BU) + WinUSB + WASAPI Audio         ");
#elif defined(__APPLE__)
        diag_log("   DIALER-BTSTACK: MACOS BLUETOOTH HFP-HF & FULL-DUPLEX AUDIO        ");
        diag_log("   TP-Link UB500 (Realtek RTL8761BU) + libusb + CoreAudio            ");
#else
        diag_log("   DIALER-BTSTACK: BLUETOOTH HFP-HF & FULL-DUPLEX AUDIO              ");
        diag_log("   TP-Link UB500 (Realtek RTL8761BU) + libusb                        ");
#endif
        diag_log("======================================================================");
    }

    // 1. Initialize Audio Subsystem
    audio_render_init();
    audio_capture_init(&on_audio_capture_pcm);

    // 2. Set Call Recorder lifecycle and VAD chunk callbacks
    call_recorder_set_callbacks(&on_call_recording_started, &on_call_recording_stopped, &on_call_segment_ready);

    // 3. Initialize Controller, USB HCI Transport & Core Protocols (L2CAP, RFCOMM, SDP)
    const char *adapter_name = "PC Dialer";
    if (bt_controller_init(adapter_name, &on_controller_ready) != 0) {
        diag_log("[ERROR] Failed to initialize BT controller");
        diag_logger_close();
        return 1;
    }

    // 4. Initialize HFP HF Profile
    if (bt_hfp_init(&on_hfp_status_changed) != 0) {
        diag_log("[ERROR] Failed to initialize HFP-HF profile");
        diag_logger_close();
        return 1;
    }

    // 5. Initialize SCO Audio Subsystem & wire to Audio engine
    sco_audio_init();
    sco_audio_set_rx_callback(&on_sco_rx_pcm);
    sco_audio_set_tx_mode(SCO_TX_MODE_LIVE_MIC);

    // 6. Setup interactive command input
    btstack_stdin_setup(&stdin_process);

    // 7. Power on Bluetooth Controller & Start Event Loop
    bt_controller_start();

    // 8. Run BTstack main event loop
    bt_controller_run();

    diag_logger_close();
    return 0;
}

