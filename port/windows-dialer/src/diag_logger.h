#ifndef DIAG_LOGGER_H
#define DIAG_LOGGER_H

#include <stdarg.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize detailed diagnostic logging to console, session log file, and PacketLogger binary trace.
 * @param app_name Prefix name for log files (e.g., "test_run", "dialer_run").
 * @return 0 on success, negative error code on failure.
 */
int diag_logger_init(const char *app_name);

/**
 * @brief Write formatted log message with microsecond-level timestamp to both console and file.
 */
void diag_log(const char *fmt, ...);

/**
 * @brief Optional sink invoked with each fully-formatted log line (without the
 * trailing newline). Used by IPC mode to forward logs to the host as JSON
 * events. When a sink is set with suppress_stdout=true, diag_log no longer
 * prints the human line to stdout (so stdout can carry clean JSON only); the
 * file log is unaffected. Passing NULL clears the sink (default behavior).
 */
typedef void (*diag_log_sink_t)(const char *line);
void diag_logger_set_sink(diag_log_sink_t sink, bool suppress_stdout);

/**
 * @brief Get the path to current session text log file.
 */
const char* diag_logger_get_text_log_path(void);

/**
 * @brief Get the path to current PacketLogger (.pklg) binary trace file.
 */
const char* diag_logger_get_pklg_path(void);

/**
 * @brief Flush and close diagnostic log files.
 */
void diag_logger_close(void);

#ifdef __cplusplus
}
#endif

#endif // DIAG_LOGGER_H
