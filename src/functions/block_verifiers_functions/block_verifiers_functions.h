#ifndef BLOCK_VERIFIERS_FUNCTIONS_H_   /* Include guard */
#define BLOCK_VERIFIERS_FUNCTIONS_H_

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sodium/randombytes.h>
#include <crypto_hash_sha256.h>
#include <pthread.h>
#include "config.h"
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include "globals.h"
#include "macro_functions.h"
#include "VRF_functions.h"
#include "xcash_round.h"
#include "blockchain_functions.h"

bool generate_and_request_vrf_data_msg(char** message);
int block_verifiers_create_block(void);
int sync_block_verifiers_minutes_and_seconds(const int MINUTES, const int SECONDS);
bool create_sync_msg(char** message);
bool block_verifiers_create_vote_majority_result(char **message, int producer_indx);
bool create_delegates_db_sync_request(int selected_index);
#endif