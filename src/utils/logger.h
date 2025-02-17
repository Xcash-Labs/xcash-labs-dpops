#ifndef logger_H
#define logger_H

#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include "config.h"

#define LOG_ERR      3   /* error conditions */
#define LOG_DEBUG    7   /* debug-level messages */
// Macros to handle errors and log them
#define HANDLE_ERROR(msg) do { \
    log_message(LOG_ERR, __func__, "%s", RED_TEXT(msg)); \
    exit(EXIT_FAILURE); \
} while (0)
#define HANDLE_DEBUG(msg) do { \
    if (debug_enabled) log_message(LOG_DEBUG, __func__, "%s", YELLOW_TEXT(msg)); \
} while (0)

/*---------------------------------------------------------------------------------------------------------
Global Variables
-----------------------------------------------------------------------------------------------------------
*/
extern bool debug_enabled;  // True if debug enabled

void log_message(int level, const char *function, const char *format, ...);

#endif