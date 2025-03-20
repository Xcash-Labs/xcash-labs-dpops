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
#include "block_verifiers_update_functions.h"

typedef struct {
    char* public_address;
    char* IP_address;
} producer_ref_t;

extern producer_ref_t producer_refs[];

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

bool select_block_producers(size_t round_number);
void show_block_producer(size_t round_number);
xcash_round_result_t process_round(size_t round_number);
void start_block_production(void);

#endif