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
    char buffer[LOG_BUFFER_LEN];        // Base message buffer
    char color_buffer[LOG_BUFFER_LEN + 16]; // Extra space for ANSI color codes

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // Store formatted message with color in a separate buffer
    snprintf(color_buffer, sizeof(color_buffer), "\033[1;33m%s\033[0m", buffer);

    logger(LOG_DEBUG, function, "%s", color_buffer);
}


void logger(int level, const char *function, const char *format, ...) {
    char buffer[LOG_BUFFER_LEN];  // Base message buffer
    char color_buffer[LOG_BUFFER_LEN + 16];  // Buffer for colored message

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // Apply color formatting before logging
    if (level == LOG_DEBUG) {
        snprintf(color_buffer, sizeof(color_buffer), "\033[1;33m%s\033[0m", buffer);  // Yellow
    } else {
        snprintf(color_buffer, sizeof(color_buffer), "\033[1;31m%s\033[0m", buffer);  // Red
    }

    fprintf(stderr, "\n%s: %s\n", function, color_buffer);

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