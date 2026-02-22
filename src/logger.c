#include <stdarg.h>
#include <stdio.h>
#include "logger.h"

void app_log(LogLevel level, const char *fmt, ...)
{
    FILE *stream = (level == LOG_LEVEL_ERROR) ? stderr : stdout;
    const char *prefix;

    switch (level) {
    case LOG_LEVEL_INFO:    prefix = "[INFO]   "; break;
    case LOG_LEVEL_WARNING: prefix = "[WARNING]"; break;
    case LOG_LEVEL_ERROR:   prefix = "[ERROR]  "; break;
    default:                prefix = "[LOG]    "; break;
    }

    fprintf(stream, "%s ", prefix);

    va_list args;
    va_start(args, fmt);
    vfprintf(stream, fmt, args);
    va_end(args);

    fprintf(stream, "\n");
}
