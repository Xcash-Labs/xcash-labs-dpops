#ifndef XCASH_DPOPS_H_   /* Include guard */
#define XCASH_DPOPS_H_

#include <argp.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "config.h"
#include "macro_functions.h"
#include "globals.h"
#include "db_init.h"
#include "structures.h"
#include "VRF_functions.h"
#include "xcash_round.h"
#include "net_server.h"
#include "node_functions.h"
#include "init_processing.h"
#include "xcash_timer_thread.h"

// Define an enum for option IDs
typedef enum {
    OPTION_GENERATE_KEY,
    OPTION_DELEGATES_WEBSITE,
    OPTION_SHARED_DELEGATES_WEBSITE,
    OPTION_FEE,
    OPTION_MINIMUM_AMOUNT,
    OPTION_LOG_LEVEL
} option_ids;

#endif