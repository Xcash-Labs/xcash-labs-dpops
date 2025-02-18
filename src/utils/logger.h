#ifndef logger_H
#define logger_H

#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include "globals.h" 

void logger(int level, const char *function, const char *format, ...);

#endif