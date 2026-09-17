#define BTSTACK_FILE__ "main.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diag_logger.h"
#include "bt/bt_controller.h"
#include "bt/hfp_hf.h"
#include "bt/sco_audio.h"
#include "audio/audio_render.h"
#include "audio/audio_capture.h"
#include "audio/call_recorder.h"
#include "ipc.h"
#include "btstack.h"

static bool s_use_mic = true;
static hfp_hf_status_t s_current_hfp_status;

// The SCO decoder path (sco_audio.c) already pushes decoded caller audio to the
// render engine and to the VAD call_recorder. This callback therefore does not
// re-push; it exists so the engine has a place to hook additional RX handling
// without duplicating the render/record writes.
static void on_sco_rx_pcm(const int16_t *samples, int num_samples) {
    (void)samples;
    (void)num_samples;
}

static void on_audio_capture_pcm(const int16_t *samples, int count) {
    // Feed the live mic into the SCO uplink. sco_audio_push_tx_samples() also
    // feeds the VAD call_recorder "out" channel, so recording follows the same
    // single path on every OS.
    if (s_use_mic && sco_audio_is_connected()) {
        sco_audio_push_tx_samples(samples, count);
    }
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

static bool s_ipc_slc_was_connected = false;
static bool s_call_was_incoming = false;   // sticky: set on RING, so we know an
                                           // answered call's direction after it
                                           // transitions to ACTIVE.

static void on_hfp_status_changed(const hfp_hf_status_t *status) {
    memcpy(&s_current_hfp_status, status, sizeof(hfp_hf_status_t));

    // Track direction: an incoming call is one that rang before it went active.
    // Reset when the call clears (back to SLC-connected/idle with no live call).
    if ((int)status->call_state == (int)HFP_STATE_INCOMING_CALL) {
        s_call_was_incoming = true;
    } else if ((int)status->call_state == (int)HFP_STATE_OUTGOING_CALL) {
        s_call_was_incoming = false;
    } else if ((int)status->call_state == (int)HFP_STATE_SLC_CONNECTED ||
               (int)status->call_state == (int)HFP_STATE_IDLE) {
        s_call_was_incoming = false;
    }
    // Tell the IPC layer the direction of the next call-started event (the VAD
    // recorder starts on SCO up and its callback emits call-started).
    ipc_set_call_outgoing(!s_call_was_incoming);

    if (status->is_slc_connected && !s_ipc_slc_was_connected) {
        diag_log("[HFP] SLC ESTABLISHED with %s (Ready to call 121)", status->peer_addr_str);

        // Make the connected phone's name reliable and consistent:
        //  1. If we already resolved this address during discovery, seed the
        //     name immediately so the UI never shows the "Connected Phone"
        //     placeholder for a device we already know.
        //  2. Always issue a fresh remote-name request so the real friendly name
        //     resolves (or refreshes) even if it was never seen during a scan.
        const char *cached = bt_controller_get_cached_name(status->peer_addr);
        if (cached && cached[0] &&
            (status->device_name[0] == '\0' ||
             strcmp(status->device_name, "Connected Phone") == 0)) {
            bt_hfp_set_device_name(cached);
        }
        bt_controller_request_remote_name(status->peer_addr);
    } else if (status->is_slc_connected) {
        diag_log("[HFP] SLC connected with %s", status->peer_addr_str);
    }

    // In IPC mode, announce the device once when the SLC first comes up and
    // forward every status change to the host as a JSON "state" event.
    // Recording is owned by the VAD call_recorder, driven from sco_audio on the
    // SCO link edge — no recording calls are made from here.
    if (ipc_is_enabled()) {
        if (status->is_slc_connected && !s_ipc_slc_was_connected) {
            // Prefer a real name (current or cached); only fall back to a neutral
            // label when nothing is known yet. The follow-up remote-name request
            // will emit an updated state once the true name resolves.
            const char *nm = status->device_name[0] &&
                             strcmp(status->device_name, "Connected Phone") != 0
                                 ? status->device_name
                                 : bt_controller_get_cached_name(status->peer_addr);
            ipc_emit_connected_device(status->peer_addr_str, (nm && nm[0]) ? nm : "Phone");
        }
        s_ipc_slc_was_connected = status->is_slc_connected;
        ipc_emit_state(status);
    }

    if ((int)status->call_state == (int)HFP_STATE_INCOMING_CALL) {
        diag_log("[HFP] INCOMING CALL from %s! Type 'answer' or 'a' to pick up.", status->caller_id);
    } else if ((int)status->call_state == (int)HFP_STATE_ACTIVE_CALL) {
        diag_log("[HFP] CALL ACTIVE with %s", status->peer_addr_str);
    }

    // Only act on an actual change of the audio-link state. Previously this
    // ran on EVERY status change, so an incidental notify (e.g. dial setting
    // OUTGOING_CALL, or a volume/operator update) with is_audio_connected still
    // false would call audio_render_stop()/audio_capture_stop() and kill live
    // call audio. Edge-detect so start runs once when SCO opens and stop runs
    // once when it releases. This is the single owner of the audio-engine
    // lifecycle (sco_audio.c intentionally does not start/stop the engines).
    static bool s_audio_engines_running = false;
    if (status->is_audio_connected && !s_audio_engines_running) {
        s_audio_engines_running = true;
        if (status->negotiated_codec == HFP_CODEC_MSBC) {
            audio_render_set_source_sample_rate(16000);
            audio_capture_set_target_sample_rate(16000);
        } else {
            audio_render_set_source_sample_rate(8000);
            audio_capture_set_target_sample_rate(8000);
        }
        if (status->speaker_volume > 0) {
            audio_render_set_volume((float)status->speaker_volume / 15.0f * 1.5f);
        } else {
            audio_render_set_volume(1.2f);
        }
        if (status->mic_gain > 0) {
            audio_capture_set_gain((float)status->mic_gain / 15.0f * 3.0f);
        } else {
            audio_capture_set_gain(2.5f);
        }
        diag_log("[AUDIO] Starting audio rendering & capture engines (Codec: %s, Vol: %.1fx, Gain: %.1fx)...",
                 status->negotiated_codec == HFP_CODEC_MSBC ? "mSBC (16kHz)" : "CVSD (8kHz)",
                 status->speaker_volume > 0 ? (float)status->speaker_volume / 15.0f * 1.5f : 1.2f,
                 status->mic_gain > 0 ? (float)status->mic_gain / 15.0f * 3.0f : 2.5f);
        audio_render_start();
        audio_capture_start();
    } else if (!status->is_audio_connected && s_audio_engines_running) {
        s_audio_engines_running = false;
        audio_render_stop();
        audio_capture_stop();
    }
}

static bd_addr_t s_last_device_addr;
static char s_last_device_str[18] = "";
static bool s_has_last_device = false;

// Adapter-status notifications from the controller (e.g. the USB dongle being
// held by the OS on macOS, and automatic recovery attempts). Forwarded to the
// host as an "adapter-status" IPC event and logged for the console.
static void on_adapter_status(const char *message, bool retrying) {
    diag_log("[ADAPTER] %s", message ? message : "");
    if (ipc_is_enabled()) {
        ipc_emit_adapter_status(message, retrying);
    }
}

static void on_controller_ready(const bd_addr_t local_addr) {
    UNUSED(local_addr);
#ifdef _WIN32
    diag_log("[DIALER] Adapter ready! Discoverable as 'Windows Dialer (UB500)'");
#elif defined(__APPLE__)
    diag_log("[DIALER] Adapter ready! Discoverable as 'Mac Dialer (UB500)'");
#else
    diag_log("[DIALER] Adapter ready! Discoverable as 'Dialer (UB500)'");
#endif

    // Automatically find and connect to last paired device
    s_has_last_device = bt_hfp_get_last_device(s_last_device_addr, s_last_device_str, sizeof(s_last_device_str));
    if (s_has_last_device) {
        diag_log("[AUTO_CONNECT] Last paired phone found: %s. Initiating automatic connection...", s_last_device_str);
        bt_hfp_connect(s_last_device_addr);
    }

    if (ipc_is_enabled()) {
        // Host-driven mode: no interactive banner/prompt. Emit an initial state
        // so the app reflects the current adapter/connection immediately.
        ipc_emit_state(&s_current_hfp_status);
        return;
    }

    print_status_banner();
    printf("\nType 'help' for commands list or 'dial 121' to place a test call.\n> ");
    fflush(stdout);
}

// Interactive-console discovery result printer (non-IPC mode only).
static void on_cli_discovery_result(const char *addr_str, const char *name, uint32_t cod, int8_t rssi) {
    UNUSED(cod);
    UNUSED(rssi);
    printf("[DISCOVERY] Found: %s (%s)\n> ", addr_str, name ? name : "");
    fflush(stdout);
}

static void on_cli_discovery_complete(void) {
    printf("[DISCOVERY] Scan complete.\n> ");
    fflush(stdout);
}

static void process_command_line(char *line) {
    char *p = line;
    while (*p) {
        if (*p == '\r' || *p == '\n') { *p = '\0'; break; }
        p++;
    }

    if (strlen(line) == 0) return;

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
        diag_log("[DIALER] Starting device discovery...");
        bt_controller_start_discovery(&on_cli_discovery_result, &on_cli_discovery_complete);
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
        printf("  tone on|off       - Toggle 1kHz test tone vs PC microphone\n");
        printf("  audio on|off      - Transfer audio connection to/from PC\n");
        printf("  connect <address> - Connect to paired phone (e.g. connect A8:AB:B5:0C:87:F3)\n");
        printf("  discover | scan   - Scan for nearby phones\n");
        printf("  disconnect        - Disconnect phone\n");
        printf("  exit (q)          - Quit dialer\n");
    }

    printf("> ");
    fflush(stdout);
}

static char s_stdin_buffer[512];
static int s_stdin_pos = 0;

static void stdin_process(char c) {
    // IPC mode: accumulate a full line and hand the JSON to the IPC dispatcher.
    // No echo, no prompt, no backspace handling (the host sends clean lines).
    if (ipc_is_enabled()) {
        if (c == '\n' || c == '\r') {
            s_stdin_buffer[s_stdin_pos] = '\0';
            if (s_stdin_pos > 0) ipc_handle_stdin_line(s_stdin_buffer);
            s_stdin_pos = 0;
        } else if (s_stdin_pos < (int)(sizeof(s_stdin_buffer) - 1)) {
            s_stdin_buffer[s_stdin_pos++] = c;
        } else {
            s_stdin_pos = 0; // overflow guard: drop the oversized line
        }
        return;
    }

    if (c == '\r' || c == '\n') {
        s_stdin_buffer[s_stdin_pos] = '\0';
        putchar('\n');
        process_command_line(s_stdin_buffer);
        s_stdin_pos = 0;
    } else if (c == '\b' || c == 127) {
        if (s_stdin_pos > 0) {
            s_stdin_pos--;
            printf("\b \b");
            fflush(stdout);
        }
    } else if (s_stdin_pos < (int)(sizeof(s_stdin_buffer) - 1)) {
        s_stdin_buffer[s_stdin_pos++] = c;
        putchar(c);
        fflush(stdout);
    }
}

int main(int argc, const char * argv[]) {
    // Parse --ipc / --recordings-dir. In IPC mode the host (Dialer.Service)
    // drives us with JSON over stdin and consumes JSON events on stdout.
    ipc_parse_args(argc, argv);

    // Disable stdout buffering for immediate log visibility
    setvbuf(stdout, NULL, _IONBF, 0);

    // Initialize unified diagnostic session logger & PacketLogger binary dump
    diag_logger_init("dialer_production");

    // In IPC mode, route logs to the host as JSON events and keep stdout clean
    // (JSON only). Must be set right after the logger init so the banner lines
    // below are forwarded rather than printed as raw text onto the JSON stream.
    if (ipc_is_enabled()) {
        ipc_install_log_sink();
    }

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

    // 1. Initialize Audio Subsystem
    audio_render_init();
    audio_capture_init(&on_audio_capture_pcm);

    // 2. Wire the VAD call recorder's lifecycle to the host IPC events, and set
    //    the recordings root dir the host passed via --recordings-dir.
    call_recorder_set_callbacks(&ipc_on_recording_started,
                                &ipc_on_recording_stopped,
                                &ipc_on_recording_segment_ready);
    if (ipc_recordings_dir()) {
        call_recorder_set_recordings_root_dir(ipc_recordings_dir());
    }

    // 3. Initialize Controller, USB HCI Transport & Core Protocols (L2CAP, RFCOMM, SDP)
#ifdef _WIN32
    const char *adapter_name = "Windows Dialer (UB500)";
#elif defined(__APPLE__)
    const char *adapter_name = "Mac Dialer (UB500)";
#else
    const char *adapter_name = "Dialer (UB500)";
#endif
    bt_controller_set_status_callback(&on_adapter_status);
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
