#ifndef COMMON_UTILS_H
#define COMMON_UTILS_H

#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include "define_macros.h"


extern bool debug_settings;

// Log levels
typedef enum { LOG_INFO, LOG_ERROR } LogLevel;

void log_message(const char *function, const char *format, ...);

#endif