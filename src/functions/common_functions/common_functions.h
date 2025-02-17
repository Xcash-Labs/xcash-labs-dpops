#ifndef COMMON_FUNCTIONS_H
#define COMMON_FUNCTIONS_H

#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include "config.h"

extern bool debug_settings;

void log_message(int level, const char *function, const char *format, ...);

#endif