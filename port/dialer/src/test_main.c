#define BTSTACK_FILE__ "test_main.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diag_logger.h"
#include "bt/bt_controller.h"
#include "bt/hfp_hf.h"
#include "bt/sco_audio.h"
#include "audio/audio_render.h"
#include "audio/audio_capture.h"
#include "btstack.h"

static btstack_timer_source_t s_telemetry_timer;
static bool s_tone_enabled = false; // Default: Live Microphone

static void on_sco_rx_pcm(const int16_t *samples, int num_samples) {
    UNUSED(samples);
    UNUSED(num_samples);
}

static void on_audio_capture_pcm(const int16_t *samples, int count) {
    if (!s_tone_enabled && sco_audio_is_connected()) {
        sco_audio_push_tx_samples(samples, count);
    }
}

static void print_telemetry_report(void) {
    sco_audio_stats_t sco_stats;
    sco_audio_get_stats(&sco_stats);

    hfp_hf_status_t hfp_stats;
    bt_hfp_get_status(&hfp_stats);

    diag_log("---------------- [DIALER-BTSTACK TELEMETRY] ----------------");
    diag_log(" Local BD_ADDR  : %s (%s)", bt_controller_get_bd_addr_string(),
             bt_controller_is_ready() ? "READY" : "INITIALIZING");
    diag_log(" HFP Profile    : %s", hfp_stats.is_slc_connected ? "SLC CONNECTED" : "LISTENING / ADVERTISING");
    if (hfp_stats.is_slc_connected) {
        diag_log(" Connected Peer : %s", hfp_stats.peer_addr_str);
        diag_log(" Network / Op   : %s (Signal: %u/5, Battery: %u/5)",
                 strlen(hfp_stats.network_operator) > 0 ? hfp_stats.network_operator : "Unknown",
                 hfp_stats.signal_strength, hfp_stats.battery_level);
        const char *state_str = "IDLE";
        switch ((int)hfp_stats.call_state) {
            case (int)HFP_STATE_IDLE: state_str = "IDLE"; break;
            case (int)HFP_STATE_CONNECTING_SLC: state_str = "CONNECTING SLC"; break;
            case (int)HFP_STATE_SLC_CONNECTED: state_str = "SLC READY (No active call)"; break;
            case (int)HFP_STATE_INCOMING_CALL: state_str = "INCOMING CALL (Ringing)"; break;
            case (int)HFP_STATE_OUTGOING_CALL: state_str = "OUTGOING CALL (Dialing/Alerting)"; break;
            case (int)HFP_STATE_ACTIVE_CALL: state_str = "ACTIVE CALL"; break;
        }
        diag_log(" Call State     : %s", state_str);
    }
    diag_log(" SCO Audio Link : %s", sco_stats.is_connected ? "CONNECTED (Speaker Audio Active)" : "DISCONNECTED / IDLE");
    if (sco_stats.is_connected) {
        diag_log(" SCO Parameters : Handle: 0x%04x | Air Mode: %s | Pkt Svg RX: %u B, TX: %u B",
                 sco_stats.sco_handle,
                 sco_stats.air_mode == 0x02 ? "Transparent" : "CVSD (8 kHz PCM)",
                 sco_stats.rx_packet_length, sco_stats.tx_packet_length);
    }
#ifdef _WIN32
    diag_log(" TX Audio Mode  : %s", s_tone_enabled ? "1 kHz PCM SINE WAVE TEST TONE" : "LIVE PC MICROPHONE (WASAPI)");
#elif defined(__APPLE__)
    diag_log(" TX Audio Mode  : %s", s_tone_enabled ? "1 kHz PCM SINE WAVE TEST TONE" : "LIVE MAC MICROPHONE (CoreAudio)");
#else
    diag_log(" TX Audio Mode  : %s", s_tone_enabled ? "1 kHz PCM SINE WAVE TEST TONE" : "LIVE MICROPHONE");
#endif
    diag_log(" SCO Statistics : RX: %u pkts (%u B) | TX: %u pkts (%u B) | Peak RX Amp: %u/32767 | Peak TX Amp: %u/32767 | Errors: %u",
             sco_stats.rx_packets, sco_stats.rx_bytes, sco_stats.tx_packets, sco_stats.tx_bytes,
             sco_stats.peak_rx_amplitude, sco_stats.peak_tx_amplitude, sco_stats.errors);
    diag_log("------------------------------------------------------------");
}

static bd_addr_t s_last_device_addr;
static char s_last_device_str[18] = "";
static bool s_has_last_device = false;
static int s_reconnect_ticks = 0;

static void telemetry_timer_handler(btstack_timer_source_t *ts) {
    print_telemetry_report();

    hfp_hf_status_t hfp_stats;
    bt_hfp_get_status(&hfp_stats);

    s_reconnect_ticks++;
    // Attempt auto-reconnection every 6 seconds when disconnected
    if ((s_reconnect_ticks % 2) == 0 && !hfp_stats.is_slc_connected && s_has_last_device) {
        diag_log("[AUTO_CONNECT] Attempting auto-connection to %s...", s_last_device_str);
        bt_hfp_connect(s_last_device_addr);
    }

    btstack_run_loop_set_timer(ts, 3000);
    btstack_run_loop_add_timer(ts);
}

static void on_controller_ready(const bd_addr_t local_addr) {
    UNUSED(local_addr);
    diag_log("[DIAGNOSTIC] Controller ready! Discoverable and Connectable as 'Dialer Test (UB500)'");
    
    // Automatically find and connect to last paired device
    s_has_last_device = bt_hfp_get_last_device(s_last_device_addr, s_last_device_str, sizeof(s_last_device_str));
    if (s_has_last_device) {
        diag_log("[AUTO_CONNECT] Last paired phone found: %s. Initiating automatic connection...", s_last_device_str);
        bt_hfp_connect(s_last_device_addr);
    } else {
        diag_log("[DIAGNOSTIC] No previous phone recorded. Pair your smartphone from Bluetooth Settings.");
    }
}

static void on_hfp_status_changed(const hfp_hf_status_t *status) {
    if (status->is_slc_connected) {
        s_has_last_device = true;
        memcpy(s_last_device_addr, status->peer_addr, sizeof(bd_addr_t));
        snprintf(s_last_device_str, sizeof(s_last_device_str), "%s", status->peer_addr_str);
        diag_log("[HFP] >>> SLC CONNECTED with %s! Ready for calls (Press '1' or 'd' to dial 121) <<<", status->peer_addr_str);
    }
    if ((int)status->call_state == (int)HFP_STATE_INCOMING_CALL) {
        diag_log("[HFP] >>> INCOMING CALL from '%s'! (Press 'a' to answer) <<<", status->caller_id);
    } else if ((int)status->call_state == (int)HFP_STATE_ACTIVE_CALL) {
        diag_log("[HFP] >>> CALL ACTIVE with %s <<<", status->peer_addr_str);
    }
    if (status->is_audio_connected) {
        diag_log("[HFP] >>> SCO AUDIO LINK ACTIVE (Codec: %s) <<<",
                 status->negotiated_codec == HFP_CODEC_MSBC ? "mSBC (16kHz)" : "CVSD (8kHz)");
        if (status->negotiated_codec == HFP_CODEC_MSBC) {
            audio_render_set_source_sample_rate(16000);
            audio_capture_set_target_sample_rate(16000);
        } else {
            audio_render_set_source_sample_rate(8000);
            audio_capture_set_target_sample_rate(8000);
        }
        diag_log("[AUDIO] Starting Speakers & Microphone Audio Engines...");
        audio_render_start();
        audio_capture_start();
    }
}

static void stdin_process(char c) {
    switch (c) {
        case '1':
        case 'd':
        case 'D':
            diag_log("[DIAGNOSTIC] Command received: Dialing 121 via HFP...");
            bt_hfp_dial("121");
            break;

        case 't':
        case 'T':
            s_tone_enabled = !s_tone_enabled;
            sco_audio_set_tx_mode(s_tone_enabled ? SCO_TX_MODE_TEST_TONE : SCO_TX_MODE_LIVE_MIC);
            diag_log("[DIAGNOSTIC] Toggled TX Mode: %s", s_tone_enabled ? "1 kHz Sine Test Tone" : "LIVE MICROPHONE");
            break;

        case 's':
        case 'S':
            print_telemetry_report();
            break;

        case 'b':
        case 'B':
            diag_log("[DIAGNOSTIC] Requesting Audio Connection (SCO)...");
            bt_hfp_establish_audio();
            break;

        case 'a':
        case 'A':
            diag_log("[DIAGNOSTIC] Answering Call...");
            bt_hfp_answer();
            break;

        case 'h':
        case 'H':
            diag_log("[DIAGNOSTIC] Hanging Up Call / Audio...");
            bt_hfp_hangup();
            break;

        case 'q':
        case 'Q':
            diag_log("[DIAGNOSTIC] Exiting test...");
            audio_render_shutdown();
            audio_capture_shutdown();
            bt_controller_stop();
            diag_logger_close();
            exit(0);
            break;

        case '?':
            diag_log("--- Interactive Test Commands ---");
            diag_log("  1 or d - Dial 121 immediately");
            diag_log("  a      - Answer incoming call");
            diag_log("  h      - Hangup / terminate call");
            diag_log("  t      - Toggle Microphone vs 1 kHz PCM test tone");
            diag_log("  s      - Print instant telemetry stats");
            diag_log("  b      - Request SCO audio connection");
            diag_log("  q      - Quit");
            break;

        default:
            break;
    }
}

int main(int argc, const char * argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    // Disable stdout buffering for immediate log visibility
    setvbuf(stdout, NULL, _IONBF, 0);

    // Initialize unified diagnostic session logger & PacketLogger binary dump
    diag_logger_init("test_suite");

    diag_log("======================================================================");
#ifdef _WIN32
    diag_log("   DIALER-BTSTACK: PHASE 1 DIAGNOSTIC & BLUETOOTH SCO TEST SUITE     ");
    diag_log("   TP-Link UB500 (Realtek RTL8761BU) + WinUSB + WASAPI Laptop Audio   ");
#elif defined(__APPLE__)
    diag_log("   DIALER-BTSTACK: PHASE 1 DIAGNOSTIC & BLUETOOTH SCO TEST SUITE     ");
    diag_log("   TP-Link UB500 (Realtek RTL8761BU) + libusb + CoreAudio Audio      ");
#else
    diag_log("   DIALER-BTSTACK: PHASE 1 DIAGNOSTIC & BLUETOOTH SCO TEST SUITE     ");
    diag_log("   TP-Link UB500 (Realtek RTL8761BU) + libusb Audio                  ");
#endif
    diag_log("======================================================================");

    // 1. Initialize Audio Subsystem (Speakers + Microphone)
    audio_render_init();
    audio_render_start();
    audio_capture_init(&on_audio_capture_pcm);
    audio_capture_start();

    // 2. Initialize Controller, USB HCI Transport & Core Protocols (L2CAP, RFCOMM, SDP)
    if (bt_controller_init("Dialer Test (UB500)", &on_controller_ready) != 0) {
        diag_log("[ERROR] Failed to initialize BT controller");
        diag_logger_close();
        return 1;
    }

    // 3. Initialize HFP HF Profile
    if (bt_hfp_init(&on_hfp_status_changed) != 0) {
        diag_log("[ERROR] Failed to initialize HFP-HF profile");
        diag_logger_close();
        return 1;
    }

    // 4. Initialize SCO Audio Subsystem & wire to Audio engine
    sco_audio_init();
    sco_audio_set_rx_callback(&on_sco_rx_pcm);
    sco_audio_set_tx_mode(SCO_TX_MODE_LIVE_MIC);

    // 5. Setup periodic telemetry timer (every 3 seconds)
    btstack_run_loop_set_timer_handler(&s_telemetry_timer, &telemetry_timer_handler);
    btstack_run_loop_set_timer(&s_telemetry_timer, 3000);
    btstack_run_loop_add_timer(&s_telemetry_timer);

    // 6. Setup STDIN handler for interactive keypresses
    btstack_stdin_setup(&stdin_process);

    // 7. Power on Bluetooth Controller & Start Event Loop
    bt_controller_start();

    diag_log("[DIAGNOSTIC] Starting BTstack Event Loop. Press '?' for commands or '1' to dial 121.");
    bt_controller_run();

    audio_render_shutdown();
    audio_capture_shutdown();
    diag_logger_close();
    return 0;
}
