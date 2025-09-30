#ifndef XCASH_TIMER_THREAD_H
#define XCASH_TIMER_THREAD_H

#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <errno.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"
#include "structures.h"
#include "db_functions.h"
#include "network_wallet_functions.h"

void* timer_thread(void* arg);

#endif