#include "common_utils.h"

/**
 * Logs messages to syslog only if debug_settings is enabled.
 */
void log_message(const char *function, const char *format, ...) {
    if (!debug_settings) {
        return;  // Skip logging if debug mode is off
    }

    va_list args;
    va_start(args, format);

    char message[512];
    vsnprintf(message, sizeof(message), format, args);
    syslog(LOG_INFO, "%s: %s", function, message);

    va_end(args);
}