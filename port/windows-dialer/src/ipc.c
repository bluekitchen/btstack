#define BTSTACK_FILE__ "ipc.c"

#include "ipc.h"
#include "bt/hfp_hf.h"
#include "bt/bt_controller.h"
#include "diag_logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static bool s_ipc_enabled = false;
static char s_recordings_dir[512] = "";
static bool s_next_call_outgoing = false;

bool ipc_parse_args(int argc, const char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--ipc") == 0) {
            s_ipc_enabled = true;
        } else if (strcmp(argv[i], "--recordings-dir") == 0 && i + 1 < argc) {
            snprintf(s_recordings_dir, sizeof(s_recordings_dir), "%s", argv[++i]);
        }
    }
    return s_ipc_enabled;
}

bool ipc_is_enabled(void) { return s_ipc_enabled; }
const char *ipc_recordings_dir(void) { return s_recordings_dir[0] ? s_recordings_dir : NULL; }
void ipc_set_call_outgoing(bool outgoing) { s_next_call_outgoing = outgoing; }

// ---------------------------------------------------------------------------
// Minimal JSON helpers
//
// The host sends flat objects like {"cmd":"dial","number":"123"}. We only need
// to pull a string value for a given key, and emit strings safely — a full JSON
// parser is unnecessary and would add a dependency. This scanner is tolerant of
// whitespace and ignores anything it does not recognize.
// ---------------------------------------------------------------------------

// Copy the string value for "key" from a JSON object into out. Returns true if
// found. Handles simple escapes (\" \\ \/ \n \t). Not a general parser.
static bool json_get_string(const char *json, const char *key, char *out, size_t out_len) {
    if (!json || !key || !out || out_len == 0) return false;
    out[0] = '\0';

    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) return false;
    p += strlen(pattern);

    // skip spaces + colon
    while (*p == ' ' || *p == '\t') p++;
    if (*p != ':') return false;
    p++;
    while (*p == ' ' || *p == '\t') p++;

    if (*p != '"') return false; // only string values supported
    p++;

    size_t o = 0;
    while (*p && *p != '"' && o + 1 < out_len) {
        if (*p == '\\' && p[1]) {
            p++;
            switch (*p) {
                case 'n': out[o++] = '\n'; break;
                case 't': out[o++] = '\t'; break;
                case 'r': out[o++] = '\r'; break;
                default:  out[o++] = *p;   break; // \" \\ \/ and others literal
            }
            p++;
        } else {
            out[o++] = *p++;
        }
    }
    out[o] = '\0';
    return true;
}

// Escape a string into a JSON-safe buffer (for emission).
static void json_escape(const char *in, char *out, size_t out_len) {
    size_t o = 0;
    if (out_len == 0) return;
    for (const char *p = in ? in : ""; *p && o + 2 < out_len; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') {
            out[o++] = '\\'; out[o++] = c;
        } else if (c == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else if (c == '\r') { out[o++] = '\\'; out[o++] = 'r'; }
        else if (c == '\t') { out[o++] = '\\'; out[o++] = 't'; }
        else if (c < 0x20) { /* skip other control chars */ }
        else out[o++] = c;
    }
    out[o] = '\0';
}

// Normalize a filesystem path into a JSON-friendly forward-slash form. Windows
// backslashes would otherwise need escaping and confuse the host's path logic.
static void json_sanitize_path(const char *in, char *out, size_t out_len) {
    if (!in || !out || out_len == 0) return;
    strncpy(out, in, out_len - 1);
    out[out_len - 1] = '\0';
    for (char *p = out; *p; p++) {
        if (*p == '\\') *p = '/';
    }
}

// Write one JSON line to stdout atomically-ish (single fputs + flush).
static void emit_line(const char *json_line) {
    fputs(json_line, stdout);
    fputc('\n', stdout);
    fflush(stdout);
}

// ---------------------------------------------------------------------------
// Log sink → {"type":"log","level":"dev","message":"..."}
// ---------------------------------------------------------------------------
static void ipc_log_sink(const char *line) {
    char esc[2200];
    json_escape(line, esc, sizeof(esc));
    char buf[2400];
    snprintf(buf, sizeof(buf), "{\"type\":\"log\",\"level\":\"dev\",\"message\":\"%s\"}", esc);
    emit_line(buf);
}

void ipc_install_log_sink(void) {
    // Suppress raw stdout so the stream carries only JSON; logs still hit the
    // session file and are forwarded as JSON log events.
    diag_logger_set_sink(&ipc_log_sink, true);
}

// ---------------------------------------------------------------------------
// Event emission
// ---------------------------------------------------------------------------

// Map the engine's HFP connection to the host's HfpConnectionState enum name.
static const char *conn_state_name(const hfp_hf_status_t *s) {
    if (s->is_slc_connected) return "SlcEstablished";
    if ((int)s->call_state == (int)HFP_STATE_CONNECTING_SLC) return "Connecting";
    return "Disconnected";
}

// Map the engine call state to the host CallStatus enum name (or NULL for none).
static const char *call_status_name(const hfp_hf_status_t *s) {
    switch ((int)s->call_state) {
        case (int)HFP_STATE_INCOMING_CALL: return "Incoming";
        case (int)HFP_STATE_OUTGOING_CALL: return "Dialing";
        case (int)HFP_STATE_ACTIVE_CALL:   return "Active";
        default: return NULL; // IDLE / CONNECTING / SLC_CONNECTED => no live call
    }
}

// Remember the last emitted state so we don't flood the host with identical
// "state" events (the engine calls notify_status_change() for volume/operator
// updates too, which would otherwise duplicate rows/logs on the app side).
static int s_last_call_state = -1;
static bool s_last_slc = false;
static bool s_last_audio = false;
static uint8_t s_last_signal = 0xFF;
static uint8_t s_last_battery = 0xFF;
static char s_last_caller[32] = "";
static char s_last_device_name[64] = "";

void ipc_emit_state(const hfp_hf_status_t *status) {
    if (!s_ipc_enabled) return;

    // Dedupe: only emit when something the host cares about actually changed.
    bool changed =
        (int)status->call_state != s_last_call_state ||
        status->is_slc_connected != s_last_slc ||
        status->is_audio_connected != s_last_audio ||
        status->signal_strength != s_last_signal ||
        status->battery_level != s_last_battery ||
        strncmp(status->caller_id, s_last_caller, sizeof(s_last_caller)) != 0 ||
        strncmp(status->device_name, s_last_device_name, sizeof(s_last_device_name)) != 0;
    if (!changed) return;
    s_last_call_state = (int)status->call_state;
    s_last_slc = status->is_slc_connected;
    s_last_audio = status->is_audio_connected;
    s_last_signal = status->signal_strength;
    s_last_battery = status->battery_level;
    snprintf(s_last_caller, sizeof(s_last_caller), "%s", status->caller_id);
    snprintf(s_last_device_name, sizeof(s_last_device_name), "%s", status->device_name);

    char addr[40];  json_escape(status->peer_addr_str, addr, sizeof(addr));
    char dname[80]; json_escape(status->device_name[0] ? status->device_name : "Phone", dname, sizeof(dname));
    char caller[64]; json_escape(status->caller_id, caller, sizeof(caller));
    char cname[80];  json_escape(status->caller_name, cname, sizeof(cname));

    const char *conn = conn_state_name(status);
    const char *callStatus = call_status_name(status);

    // Build the optional calls[] array.
    char calls[320] = "[]";
    if (callStatus) {
        bool outgoing = ((int)status->call_state == (int)HFP_STATE_OUTGOING_CALL);
        snprintf(calls, sizeof(calls),
                 "[{\"index\":0,\"status\":\"%s\",\"number\":\"%s\",\"name\":\"%s\",\"isOutgoing\":%s,\"isMultiparty\":false}]",
                 callStatus,
                 caller,          // number (caller id / dialed number if known)
                 cname,           // name (caller id name if the network provided one)
                 outgoing ? "true" : "false");
    }

    char buf[1000];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"state\","
        "\"connState\":\"%s\","
        "\"connDeviceId\":\"%s\","
        "\"deviceName\":\"%s\","
        "\"signalBars\":%u,"
        "\"batteryLevel\":%u,"
        "\"scoActive\":%s,"
        "\"calls\":%s}",
        conn,
        addr,
        dname,
        (unsigned)status->signal_strength,
        (unsigned)status->battery_level,
        status->is_audio_connected ? "true" : "false",
        calls);
    emit_line(buf);
}

void ipc_emit_connected_device(const char *device_id, const char *name) {
    if (!s_ipc_enabled) return;
    char id[64]; json_escape(device_id, id, sizeof(id));
    char nm[64]; json_escape(name && name[0] ? name : "Phone", nm, sizeof(nm));
    // Message carries a JSON-serialized DeviceDto[] (matches the host's parser).
    char buf[400];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"devices\",\"message\":\"[{\\\"Id\\\":\\\"%s\\\",\\\"Name\\\":\\\"%s\\\",\\\"Paired\\\":true,\\\"Connected\\\":true,\\\"AudioReady\\\":true}]\"}",
        id, nm);
    emit_line(buf);
}

// ---------------------------------------------------------------------------
// Recorder → IPC event adapters
//
// Wired to call_recorder_set_callbacks() in main.c. The VAD recorder produces
// per-channel chunk files (in-<start>-<end>.wav / out-<start>-<end>.wav) and a
// merged stereo file; these adapters emit the JSON events the host consumes.
// ---------------------------------------------------------------------------
void ipc_on_recording_started(const char *session_dir, const char *stereo_wav,
                              const char *rx_wav, const char *tx_wav) {
    if (!s_ipc_enabled) return;
    char sd[700], sw[700], rx[700], tx[700];
    json_sanitize_path(session_dir, sd, sizeof(sd));
    json_sanitize_path(stereo_wav, sw, sizeof(sw));
    json_sanitize_path(rx_wav, rx, sizeof(rx));
    json_sanitize_path(tx_wav, tx, sizeof(tx));
    char esd[720], esw[720], erx[720], etx[720];
    json_escape(sd, esd, sizeof(esd));
    json_escape(sw, esw, sizeof(esw));
    json_escape(rx, erx, sizeof(erx));
    json_escape(tx, etx, sizeof(etx));

    char buf[3200];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"call-started\",\"sessionDir\":\"%s\",\"fullPath\":\"%s\","
        "\"inPath\":\"%s\",\"outPath\":\"%s\",\"isOutgoing\":%s}",
        esd, esw, erx, etx, s_next_call_outgoing ? "true" : "false");
    emit_line(buf);
}

void ipc_on_recording_stopped(const char *session_dir, const char *stereo_wav,
                              const char *rx_wav, const char *tx_wav, double duration_sec) {
    if (!s_ipc_enabled) return;
    char sd[700], sw[700], rx[700], tx[700];
    json_sanitize_path(session_dir, sd, sizeof(sd));
    json_sanitize_path(stereo_wav, sw, sizeof(sw));
    json_sanitize_path(rx_wav, rx, sizeof(rx));
    json_sanitize_path(tx_wav, tx, sizeof(tx));
    char esd[720], esw[720], erx[720], etx[720];
    json_escape(sd, esd, sizeof(esd));
    json_escape(sw, esw, sizeof(esw));
    json_escape(rx, erx, sizeof(erx));
    json_escape(tx, etx, sizeof(etx));

    char buf[3200];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"call-ended\",\"sessionDir\":\"%s\",\"fullPath\":\"%s\","
        "\"inPath\":\"%s\",\"outPath\":\"%s\",\"durationSec\":%.2f}",
        esd, esw, erx, etx, duration_sec);
    emit_line(buf);
}

void ipc_on_recording_segment_ready(const char *wav_path, const char *channel,
                                    double start_sec, double end_sec) {
    if (!s_ipc_enabled) return;
    char p[700]; json_sanitize_path(wav_path, p, sizeof(p));
    char ep[720]; json_escape(p, ep, sizeof(ep));
    char ch[16]; json_escape(channel, ch, sizeof(ch));
    char buf[1100];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"segment-ready\",\"channel\":\"%s\",\"path\":\"%s\",\"startSec\":%.3f,\"endSec\":%.3f}",
        ch, ep, start_sec, end_sec);
    emit_line(buf);
}

// ---------------------------------------------------------------------------
// Discovery → IPC event adapters (wired to bt_controller discovery callbacks).
// ---------------------------------------------------------------------------
void ipc_on_discovery_result(const char *addr_str, const char *name, uint32_t cod, int8_t rssi) {
    (void)cod; (void)rssi;
    if (!s_ipc_enabled) return;
    char id[40]; json_escape(addr_str, id, sizeof(id));
    char nm[64]; json_escape(name && name[0] ? name : "", nm, sizeof(nm));
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"nearby-device\",\"device\":{\"Id\":\"%s\",\"Name\":\"%s\",\"Paired\":false,\"Connected\":false,\"AudioReady\":true}}",
        id, nm);
    emit_line(buf);
}

void ipc_on_discovery_complete(void) {
    if (!s_ipc_enabled) return;
    emit_line("{\"type\":\"discover-done\"}");
}

// ---------------------------------------------------------------------------
// Command dispatch
// ---------------------------------------------------------------------------
void ipc_handle_stdin_line(const char *line) {
    if (!line) return;
    // Ignore anything that is not a JSON object.
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '{') return;

    char cmd[32];
    if (!json_get_string(p, "cmd", cmd, sizeof(cmd))) return;

    if (strcmp(cmd, "dial") == 0) {
        char number[64];
        if (json_get_string(p, "number", number, sizeof(number)) && number[0]) {
            diag_log("[IPC] dial %s", number);
            bt_hfp_dial(number);
        }
    } else if (strcmp(cmd, "answer") == 0) {
        diag_log("[IPC] answer");
        bt_hfp_answer();
    } else if (strcmp(cmd, "hangup") == 0) {
        diag_log("[IPC] hangup");
        bt_hfp_hangup();
    } else if (strcmp(cmd, "connect") == 0) {
        char address[40];
        // The host may send the target as "address" or "deviceId".
        if ((json_get_string(p, "address", address, sizeof(address)) && address[0]) ||
            (json_get_string(p, "deviceId", address, sizeof(address)) && address[0])) {
            diag_log("[IPC] connect %s", address);
            bt_hfp_connect_addr_string(address);
        }
    } else if (strcmp(cmd, "disconnect") == 0) {
        diag_log("[IPC] disconnect");
        bt_hfp_disconnect();
    } else if (strcmp(cmd, "dtmf") == 0) {
        char digit[8];
        if (json_get_string(p, "digit", digit, sizeof(digit)) && digit[0]) {
            diag_log("[IPC] dtmf %c", digit[0]);
            bt_hfp_send_dtmf(digit[0]);
        }
    } else if (strcmp(cmd, "discover") == 0 || strcmp(cmd, "discover-nearby") == 0) {
        diag_log("[IPC] discover");
        bt_controller_start_discovery(&ipc_on_discovery_result, &ipc_on_discovery_complete);
    } else if (strcmp(cmd, "stop-discover") == 0) {
        diag_log("[IPC] stop-discover");
        bt_controller_stop_discovery();
    } else if (strcmp(cmd, "status") == 0) {
        hfp_hf_status_t st;
        bt_hfp_get_status(&st);
        // Force a re-emit even if unchanged by resetting the dedupe cache.
        s_last_call_state = -1;
        ipc_emit_state(&st);
    } else if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) {
        diag_log("[IPC] quit");
        exit(0);
    }
}
