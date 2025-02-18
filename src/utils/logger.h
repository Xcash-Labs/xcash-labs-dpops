#ifndef logger_H
#define logger_H

#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include "config.h"
#include "globals.h" 

#define RED_TEXT(text) "\033[31m"text"\033[0m"
#define YELLOW_TEXT(text) "\033[1;33m"text"\033[0m"
#define GREEN_TEXT(text) "\x1b[32m"text"\x1b[0m"
#define BRIGHT_WHITE_TEXT(text) "\033[1;97m"text"\033[0m"
#define YELLOW_WRAP(buffer) ({ \
    static char color_msg[1024]; \
    snprintf(color_msg, sizeof(color_msg), "\033[1;33m%s\033[0m", buffer); \
    color_msg; \
})
#define LOG_ERR      3   /* error conditions */
#define LOG_DEBUG    7   /* debug-level messages */
// Macros to handle errors and log them

#define HANDLE_ERROR(msg) do { \
    log_message(LOG_ERR, __func__, "%s", RED_TEXT(msg)); \
    exit(EXIT_FAILURE); \
} while (0)
//#define HANDLE_DEBUG(msg) do { \
//    if (debug_enabled) log_message(LOG_DEBUG, __func__, "%s", YELLOW_TEXT(msg)); \
//} while (0)

#define HANDLE_DEBUG(msg) do { \
    if (debug_enabled) log_message(LOG_DEBUG, __func__, "%s", \
        _Generic((msg), char*: msg, default: YELLOW_WRAP(msg))); \
} while (0)

void log_message(int level, const char *function, const char *format, ...);

#endif