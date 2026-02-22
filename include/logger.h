#ifndef APP_LOGGER_H
#define APP_LOGGER_H

typedef enum {
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
} LogLevel;

/** printf-style format string. */
void app_log(LogLevel level, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

#define log_info(fmt, ...)    app_log(LOG_LEVEL_INFO,    fmt, ##__VA_ARGS__)
#define log_warning(fmt, ...) app_log(LOG_LEVEL_WARNING, fmt, ##__VA_ARGS__)
#define log_error(fmt, ...)   app_log(LOG_LEVEL_ERROR,   fmt, ##__VA_ARGS__)

#endif
