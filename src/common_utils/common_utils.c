#include "common_utils.h"

/**
 * Logs messages to syslog only if debug_settings is enabled.
 */
void log_message(const char *function, const char *format, ...) {
    if (!debug_settings) {
        return;
    }

    va_list args;
    va_start(args, format);

    fprintf(stderr, "%s: ", function);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");

    va_end(args);
}