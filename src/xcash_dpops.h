#ifndef XCASH_DPOPS_H_   /* Include guard */
#define XCASH_DPOPS_H_

#include <argp.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"
#include "db_init.h"
#include "structures.h"
#include "uv_net_server.h"
#include "xcash_initialize.h"
#include "xcash_round.h"

// Define an enum for option IDs
typedef enum {
    OPTION_GENERATE_KEY,
    OPTION_TOTAL_THREADS,
    OPTION_INIT_DB_FROM_SEEDS,
    OPTION_INIT_DB_FROM_TOP,
} option_ids;

#endif