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

int get_block_template(char* result, size_t result_size, size_t* reserved_offset_out);
bool submit_block_template(const char* DATA);
int get_current_block_height(char *result);
int get_current_block_hash(char *result_hash);
int get_previous_block_hash(char *result);
bool is_blockchain_synced(char *target_height, char *height);
int get_block_info_by_height(uint64_t height, char *out_hash, size_t out_hash_len, uint64_t *out_reward, uint64_t *out_timestamp, bool *out_orphan);

#endif