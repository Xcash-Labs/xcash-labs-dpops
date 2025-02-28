#ifndef DPOPS_ROUND_H
#define DPOPS_ROUND_H

#include <stdbool.h>
#include <time.h> 
#include "config.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"
#include "network_daemon_functions.h"
#include "db_sync.h"

typedef struct {
    char public_address[XCASH_WALLET_LENGTH+1];
    char IP_address[BLOCK_VERIFIERS_IP_ADDRESS_TOTAL_LENGTH+1];
    bool is_online;
} producer_node_t;

typedef enum {
    ROUND_ERROR, // some system fault occurred. mostly communication errors or other non-fatal error. In that case better wait till next round
    ROUND_OK, //all the procedures finished successfully
    ROUND_SKIP, // wait till next round
    ROUND_RETRY,
    ROUND_NEXT,
} xcash_round_result_t;

void start_block_production(void);

#endif