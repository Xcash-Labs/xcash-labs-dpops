#include "logger.h"

/******************************************************************************
 * Log messages
 ******************************************************************************/
void logger(int level, const char *function, const char *format, ...)
{
    char buffer[LOG_BUFFER_LEN];            // Base message buffer
    char color_buffer[LOG_BUFFER_LEN + 16]; // Buffer for colored message
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    // Apply color formatting before logging
    if (level == LOG_DEBUG)
    {
        snprintf(color_buffer, sizeof(color_buffer), "\033[1;33m%s\033[0m", buffer); // Yellow
    }
    else
    {
        snprintf(color_buffer, sizeof(color_buffer), "\033[1;31m%s\033[0m", buffer); // Red
    }
    if (level == LOG_ERR || debug_enabled)
    {
        fprintf(stderr, "\n%s: %s\n", function, color_buffer);
    }
    if (level == LOG_ERR)
    {
        fprintf(stderr, "\n");
        exit(EXIT_FAILURE);
    }
}