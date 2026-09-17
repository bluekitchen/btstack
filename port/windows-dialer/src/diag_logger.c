#define BTSTACK_FILE__ "diag_logger.c"

#include "diag_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#include "hci_dump_windows_fs.h"
static CRITICAL_SECTION s_log_cs;
#else
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include "hci_dump_posix_fs.h"
static pthread_mutex_t s_log_cs = PTHREAD_MUTEX_INITIALIZER;
#endif

#include "hci_dump.h"

static FILE *s_log_file = NULL;
static char s_text_log_path[256] = "";
static char s_pklg_path[256] = "";
static bool s_cs_initialized = false;

static void ensure_logs_dir(void) {
#ifdef _WIN32
    CreateDirectoryA("logs", NULL);
#else
    mkdir("logs", 0755);
#endif
}

static void get_current_timestamp(char *time_str, size_t max_len,
                                  unsigned int *out_year, unsigned int *out_month, unsigned int *out_day,
                                  unsigned int *out_hour, unsigned int *out_min, unsigned int *out_sec,
                                  unsigned int *out_msec) {
#ifdef _WIN32
    SYSTEMTIME st;
    GetLocalTime(&st);
    if (out_year)  *out_year  = st.wYear;
    if (out_month) *out_month = st.wMonth;
    if (out_day)   *out_day   = st.wDay;
    if (out_hour)  *out_hour  = st.wHour;
    if (out_min)   *out_min   = st.wMinute;
    if (out_sec)   *out_sec   = st.wSecond;
    if (out_msec)  *out_msec  = st.wMilliseconds;
    if (time_str) {
        snprintf(time_str, max_len, "[%02u:%02u:%02u.%03u] ",
                 st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    }
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm tm_info;
    localtime_r(&tv.tv_sec, &tm_info);
    unsigned int msec = (unsigned int)(tv.tv_usec / 1000);
    if (out_year)  *out_year  = tm_info.tm_year + 1900;
    if (out_month) *out_month = tm_info.tm_mon + 1;
    if (out_day)   *out_day   = tm_info.tm_mday;
    if (out_hour)  *out_hour  = tm_info.tm_hour;
    if (out_min)   *out_min   = tm_info.tm_min;
    if (out_sec)   *out_sec   = tm_info.tm_sec;
    if (out_msec)  *out_msec  = msec;
    if (time_str) {
        snprintf(time_str, max_len, "[%02u:%02u:%02u.%03u] ",
                 tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec, msec);
    }
#endif
}

int diag_logger_init(const char *app_name) {
#ifdef _WIN32
    if (!s_cs_initialized) {
        InitializeCriticalSection(&s_log_cs);
        s_cs_initialized = true;
    }
#else
    s_cs_initialized = true;
#endif

    ensure_logs_dir();

    unsigned int y, m, d, h, min, s, ms;
    get_current_timestamp(NULL, 0, &y, &m, &d, &h, &min, &s, &ms);

    snprintf(s_text_log_path, sizeof(s_text_log_path),
             "logs/%s_%04u%02u%02u_%02u%02u%02u.log",
             app_name ? app_name : "session",
             y, m, d, h, min, s);

    snprintf(s_pklg_path, sizeof(s_pklg_path),
             "logs/hci_dump_%04u%02u%02u_%02u%02u%02u.pklg",
             y, m, d, h, min, s);

    s_log_file = fopen(s_text_log_path, "a+");
    if (!s_log_file) {
        printf("[LOGGER] Warning: Could not open log file '%s'\n", s_text_log_path);
    }

    // Initialize BTstack PacketLogger trace file
#ifdef _WIN32
    hci_dump_windows_fs_open(s_pklg_path, HCI_DUMP_PACKETLOGGER);
    const hci_dump_t * dump_impl = hci_dump_windows_fs_get_instance();
#else
    hci_dump_posix_fs_open(s_pklg_path, HCI_DUMP_PACKETLOGGER);
    const hci_dump_t * dump_impl = hci_dump_posix_fs_get_instance();
#endif
    if (dump_impl) {
        hci_dump_init(dump_impl);
    }

    diag_log("================================================================================");
    diag_log(" SESSION LOG STARTED: %s", app_name ? app_name : "dialer-btstack");
    diag_log(" Session Text Log   : %s", s_text_log_path);
    diag_log(" PacketLogger Trace : %s", s_pklg_path);
    diag_log(" System Local Time  : %04u-%02u-%02u %02u:%02u:%02u.%03u",
             y, m, d, h, min, s, ms);
    diag_log("================================================================================");

    return 0;
}

void diag_log(const char *fmt, ...) {
    if (!fmt) return;

    char time_buf[32];
    get_current_timestamp(time_buf, sizeof(time_buf), NULL, NULL, NULL, NULL, NULL, NULL, NULL);

    char msg_buf[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    va_end(args);

    if (s_cs_initialized) {
#ifdef _WIN32
        EnterCriticalSection(&s_log_cs);
#else
        pthread_mutex_lock(&s_log_cs);
#endif
    }

    // Print to stdout
    printf("%s%s\n", time_buf, msg_buf);
    fflush(stdout);

    // Write to session log file
    if (s_log_file) {
        fprintf(s_log_file, "%s%s\n", time_buf, msg_buf);
        fflush(s_log_file);
    }

    if (s_cs_initialized) {
#ifdef _WIN32
        LeaveCriticalSection(&s_log_cs);
#else
        pthread_mutex_unlock(&s_log_cs);
#endif
    }
}

const char* diag_logger_get_text_log_path(void) {
    return s_text_log_path;
}

const char* diag_logger_get_pklg_path(void) {
    return s_pklg_path;
}

void diag_logger_close(void) {
    diag_log("================================================================================");
    diag_log(" SESSION LOG CLOSED");
    diag_log("================================================================================");

    if (s_log_file) {
        fclose(s_log_file);
        s_log_file = NULL;
    }

#ifdef _WIN32
    hci_dump_windows_fs_close();
    if (s_cs_initialized) {
        DeleteCriticalSection(&s_log_cs);
        s_cs_initialized = false;
    }
#else
    hci_dump_posix_fs_close();
#endif
}
