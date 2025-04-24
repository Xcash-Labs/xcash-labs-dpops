#ifndef BLOCK_VERIFIERS_FUNCTIONS_H_   /* Include guard */
#define BLOCK_VERIFIERS_FUNCTIONS_H_

#include <stdio.h>
#include <stdlib.h>
//#include <string.h>
#include <sodium/randombytes.h>
#include <pthread.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"
#include "VRF_functions.h"
#include "xcash_round.h"
#include "blockchain_functions.h"


//int sync_all_delegates(void);
int start_blocks_create_vrf_data(void);
int start_blocks_create_data(char* message, char* network_block_string);
int start_current_round_start_blocks(void);
bool generate_and_request_vrf_data_msg(char** message);
int block_verifiers_create_VRF_data(void);
int block_verifiers_create_block_signature(char* message);
void block_verifiers_create_vote_majority_results(char *result, const int SETTINGS);
//int block_verifiers_calculate_vote_majority_results(const int SETTINGS);
int block_verifiers_create_vote_results(char* message);
int block_verifiers_create_block_and_update_database(void);
//void print_block_producer(void);
int block_verifiers_create_block(void);
int sync_block_verifiers_minutes_and_seconds(const int MINUTES, const int SECONDS);
//int get_network_data_nodes_online_status(void);
//int block_verifiers_send_data_socket(const char* MESSAGE);
#endif