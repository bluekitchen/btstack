#ifndef DIALER_IPC_H
#define DIALER_IPC_H

#include <stdbool.h>
#include <stdint.h>
#include "bt/hfp_hf.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * JSON IPC bridge between the C# host (Dialer.Service BtstackEngineProcess) and
 * this native engine. When the process is launched with --ipc, the host speaks
 * one JSON object per line on stdin (commands) and expects one JSON ServiceEvent
 * object per line on stdout (state/log events). Without --ipc the engine keeps
 * its interactive text console.
 *
 * Recording itself is owned by the VAD call_recorder module. This IPC layer only
 * turns the recorder's lifecycle callbacks into the JSON events the host expects
 * (call-started / segment-ready / call-ended), so there is a single recorder.
 */

/** Parse argv; returns true if --ipc was passed. Also records --recordings-dir. */
bool ipc_parse_args(int argc, const char *argv[]);

/** True when running in JSON IPC mode. */
bool ipc_is_enabled(void);

/** The value passed to --recordings-dir, or NULL. */
const char *ipc_recordings_dir(void);

/** Install the diag_logger sink so engine logs are forwarded as JSON log events
 *  and raw stdout text is suppressed (stdout carries JSON only). */
void ipc_install_log_sink(void);

/** Handle one line of stdin input in IPC mode: parse the JSON command and
 *  dispatch to the bt_hfp_* / discovery API. Safe to call for non-JSON lines. */
void ipc_handle_stdin_line(const char *line);

/** Emit a ServiceEvent-shaped "state" event derived from the HFP status.
 *  De-duplicates: only emits when a host-relevant field actually changed. */
void ipc_emit_state(const hfp_hf_status_t *status);

/** Emit a "devices" event for a single connected device (used on SLC up). */
void ipc_emit_connected_device(const char *device_id, const char *name);

// ---------------------------------------------------------------------------
// Recorder -> IPC event adapters
//
// These are wired to call_recorder_set_callbacks() so the VAD recorder's
// lifecycle produces the JSON the host ingests: call-started on start,
// segment-ready per finalized VAD chunk (in-<start>-<end>.wav /
// out-<start>-<end>.wav, matching RecordingChunkName), and call-ended on stop.
// `outgoing` for the call-started event is provided by the caller (main.c),
// which tracks call direction.
// ---------------------------------------------------------------------------

/** Set the direction used for the next call-started event. */
void ipc_set_call_outgoing(bool outgoing);

/** call_recorder started callback -> {"type":"call-started", ...}. */
void ipc_on_recording_started(const char *session_dir, const char *stereo_wav,
                              const char *rx_wav, const char *tx_wav);

/** call_recorder stopped callback -> {"type":"call-ended", ...}. */
void ipc_on_recording_stopped(const char *session_dir, const char *stereo_wav,
                              const char *rx_wav, const char *tx_wav, double duration_sec);

/** call_recorder segment-ready callback -> {"type":"segment-ready", ...}. */
void ipc_on_recording_segment_ready(const char *wav_path, const char *channel,
                                    double start_sec, double end_sec);

// ---------------------------------------------------------------------------
// Discovery -> IPC event adapters (wired to bt_controller discovery callbacks).
// ---------------------------------------------------------------------------

/** bt_controller inquiry-result callback -> {"type":"nearby-device", ...}. */
void ipc_on_discovery_result(const char *addr_str, const char *name, uint32_t cod, int8_t rssi);

/** bt_controller inquiry-complete callback -> {"type":"discover-done"}. */
void ipc_on_discovery_complete(void);

#ifdef __cplusplus
}
#endif

#endif // DIALER_IPC_H
