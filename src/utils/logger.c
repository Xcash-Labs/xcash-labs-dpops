#include "logger.h"

/******************************************************************************
* Logs messages
******************************************************************************/
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

void log_debug_buffer(const char *format, ...) {
    char wsbuf[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(wsbuf, sizeof(wsbuf), format, args);
    va_end(args);
    HANDLE_DEBUG(wsbuf);
}