#ifndef XCASH_ROUND_H
#define XCASH_ROUND_H

#include <stdbool.h>
#include <time.h> 
#include "config.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <stdlib.h> 
#include "config.h"
#include "globals.h"
#include "macro_functions.h"
#include "network_daemon_functions.h"
#include "db_sync.h"
#include "block_verifiers_functions.h"
#include "string_functions.h"

typedef struct {
    char public_address[XCASH_WALLET_LENGTH + 1];
    char IP_address[BLOCK_VERIFIERS_IP_ADDRESS_TOTAL_LENGTH + 1];
    char vrf_public_key[VRF_PUBLIC_KEY_LENGTH + 1];
    char random_buf_hex[VRF_RANDOMBYTES_LENGTH * 2 + 1];
    char vrf_proof_hex[VRF_PROOF_LENGTH + 1];
    char vrf_beta_hex[VRF_BETA_LENGTH + 1];
} producer_ref_t;

extern producer_ref_t producer_refs[PRODUCER_REF_COUNT];

typedef struct {
    char public_address[XCASH_WALLET_LENGTH+1];
    char IP_address[BLOCK_VERIFIERS_IP_ADDRESS_TOTAL_LENGTH+1];
    bool is_online;
} producer_node_t;

typedef enum {
    ROUND_ERROR, // some system fault occurred. mostly communication errors or other non-fatal error.
    ROUND_OK, //all the procedures finished successfully
    ROUND_SKIP, // wait till next round
} xcash_round_result_t;

xcash_round_result_t process_round(void);
void start_block_production(void);

#endif