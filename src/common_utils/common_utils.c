#include "common_utils.h"

/**
 * Logs messages
 */
void log_message(int level, const char *function, const char *format, ...)
{
    va_list args;
    char buffer[1024];

    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args); // Format message
    va_end(args);
    if (level == LOG_ERR || level == LOG_DEBUG)
    {
//        fprintf(stderr, "%s: ", function);
//        vfprintf(stderr, format, args);
//        fprintf(stderr, "\n");
        fprintf(stderr, "%s: %s", function, buffer);
    }
//    va_end(args);
}