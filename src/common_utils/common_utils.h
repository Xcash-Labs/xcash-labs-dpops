#ifndef COMMON_UTILS_H
#define COMMON_UTILS_H

#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include "dpops_config.h"

extern bool debug_settings;

void log_message(int level, const char *function, const char *format, ...);

#endif