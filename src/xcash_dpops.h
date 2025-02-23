#ifndef XCASH_DPOPS_TEST_H_   /* Include guard */
#define XCASH_DPOPS_TEST_H_

#include <argp.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/sysinfo.h>
#include <uv.h>
#include "config.h"
#include "macro_functions.h"
#include "globals.h"
#include "db_init.h"
#include "structures.h"
#include "VRF_functions.h"
#include "dpops_round.h"

typedef struct {
    char *block_verifiers_secret_key; // Holds your wallets public address
    bool is_seed_node;
} arg_config_t;

// Define an enum for option IDs
typedef enum {
    OPTION_GENERATE_KEY,
    OPTION_DEBUG,
    OPTION_TOTAL_THREADS,
} option_ids;

bool init_processing(const arg_config_t* arg_config);
bool configure_uv_threadpool(void);

#endif