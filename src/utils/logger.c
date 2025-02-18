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
    char buffer[LOG_BUFFER_LEN + 11];  // Single buffer with extra space for color codes
    va_list args;
    va_start(args, format);
    vsnprintf(buffer + 7, sizeof(buffer) - 11, format, args);  // Leave space for "\033[1;33m"
    va_end(args);

    snprintf(buffer, sizeof(buffer), "\033[1;33m%s\033[0m", buffer + 7);
    log_message(LOG_DEBUG, function, "%s", buffer);
}

void logger(int level, const char *function, const char *format, ...) {
    char buffer[LOG_BUFFER_LEN];  // No extra space needed for color
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    // Apply color based on log level
    if (level == LOG_DEBUG) {
        fprintf(stderr, "\n%s: %s\n", function, YELLOW_TEXT(buffer));
    } else {
        fprintf(stderr, "\n%s: %s\n", function, RED_TEXT(buffer));
    }
    if (level == LOG_ERR) {
        exit(EXIT_FAILURE);
    }
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