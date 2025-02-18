#include "logger.h"

/******************************************************************************
* Logs messages
******************************************************************************/
//void log_debug_buffer(const char *format, ...) {
//    char wsbuf[1024];
//    va_list args;
//    va_start(args, format);
//    vsnprintf(wsbuf, sizeof(wsbuf), format, args);
//    va_end(args);
//    HANDLE_DEBUG_BUFFER(wsbuf);
//}

void log_debug_buffer(const char *function, const char *format, ...) {
    char wsbuf[1024];
    char color_wsbuf[1100];
    va_list args;
    va_start(args, format);
    vsnprintf(wsbuf, sizeof(wsbuf), format, args);
    va_end(args);
    snprintf(color_wsbuf, sizeof(color_wsbuf), "\033[1;33m%s\033[0m", wsbuf);
    log_message(LOG_DEBUG, function, "%s", wsbuf);
}

void log_message(int level, const char *function, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    if (level == LOG_ERR || level == LOG_DEBUG)
    {
        fprintf(stderr, "\n");
        fprintf(stderr, "%s: ", function);
        vfprintf(stderr, format, args);
        fprintf(stderr, "\n");
    }
    va_end(args);
}