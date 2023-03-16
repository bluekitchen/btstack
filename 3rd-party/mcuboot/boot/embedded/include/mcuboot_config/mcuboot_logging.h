#define MCUBOOT_LOG_LEVEL_OFF      0
#define MCUBOOT_LOG_LEVEL_ERROR    1
#define MCUBOOT_LOG_LEVEL_WARNING  2
#define MCUBOOT_LOG_LEVEL_INFO     3
#define MCUBOOT_LOG_LEVEL_DEBUG    4
#ifndef MCUBOOT_LOG_LEVEL
#define MCUBOOT_LOG_LEVEL MCUBOOT_LOG_LEVEL_DEBUG
#endif
extern int printf( const char *pcFormat, ...);
#if MCUBOOT_LOG_LEVEL >= MCUBOOT_LOG_LEVEL_ERROR
#define MCUBOOT_LOG_ERR(_fmt, ...)                                      \
    do {                                                                \
            printf("[ERR] [%s] line:%d " _fmt "\n", __func__, __LINE__, ##__VA_ARGS__);         \
    } while (0)
#else
#define MCUBOOT_LOG_ERR(_fmt, ...)
#endif

#if MCUBOOT_LOG_LEVEL >= MCUBOOT_LOG_LEVEL_WARNING
#define MCUBOOT_LOG_WRN(_fmt, ...)                                      \
    do {                                                                \
            printf("[WRN] [%s] line:%d " _fmt "\n", __func__, __LINE__, ##__VA_ARGS__);         \
    } while (0)
#else
#define MCUBOOT_LOG_WRN(_fmt, ...)
#endif

#if MCUBOOT_LOG_LEVEL >= MCUBOOT_LOG_LEVEL_INFO
#define MCUBOOT_LOG_INF(_fmt, ...)                                      \
    do {                                                                \
            printf("[INF] [%s] line:%d " _fmt "\n", __func__, __LINE__, ##__VA_ARGS__);         \
    } while (0)
#else
#define MCUBOOT_LOG_INF(_fmt, ...)
#endif

#if MCUBOOT_LOG_LEVEL >= MCUBOOT_LOG_LEVEL_DEBUG
#define MCUBOOT_LOG_DBG(_fmt, ...)                                      \
    do {                                                                \
            printf("[DBG] [%s] line:%d " _fmt "\n", __func__, __LINE__, ##__VA_ARGS__);         \
    } while (0)
#else
#define MCUBOOT_LOG_DBG(_fmt, ...)
#endif

#if MCUBOOT_LOG_LEVEL >= MCUBOOT_LOG_LEVEL_DEBUG
#define MCUBOOT_LOG_BUF(name, buf, len)                                                 \
    do {                                                                                \
        printf("[DBG] [%s] line:%d, %s[", __func__, __LINE__, (name));                  \
        for (uint32_t i = 0;i < (len);i++) {                                            \
            printf("%02X ", (buf)[i]);                                                  \
        }                                                                               \
        printf("]\n");                                                                  \
    } while(0)
#else
#define MCUBOOT_LOG_BUF(name, buf, len)
#endif

#define MCUBOOT_LOG_MODULE_DECLARE(...)
#define MCUBOOT_LOG_MODULE_REGISTER(...)

