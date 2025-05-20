#ifndef NETWORK_DAEMON_FUNCTIONS_H_   /* Include guard */
#define NETWORK_DAEMON_FUNCTIONS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"
#include "network_functions.h"
#include "string_functions.h"

//int check_if_blockchain_is_fully_synced(void);
//int get_current_block_height_network_data_nodes(void);
int get_block_template(char* result, size_t result_size);
bool submit_block_template(const char* DATA);
//int get_block_reserve_byte_data_hash(char *reserve_byte_data_hash, const char* BLOCK_HEIGHT);
int get_current_block_height(char *result);
int get_previous_block_hash(char *result);
bool is_blockchain_synced(void);
//int get_previous_block_information(char *block_hash, char *block_reward, char *block_date_and_time);
char* base58_encode(const uint8_t* input, size_t input_len);
#endif