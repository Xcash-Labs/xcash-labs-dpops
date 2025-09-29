#ifndef XCASH_TIMER_THREAD_H
#define XCASH_TIMER_THREAD_H

#include <pthread.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"
#include "structures.h"

void* timer_thread(void* arg);

#endif