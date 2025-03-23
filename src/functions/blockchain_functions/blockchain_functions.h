#ifndef BLOCKCHAIN_FUNCTIONS_H_   /* Include guard */
#define BLOCKCHAIN_FUNCTIONS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sha512EL.h"
#include "config.h"
#include "globals.h"
#include "macro_functions.h"
#include "string_functions.h"
#include "VRF_functions.h"

int varint_encode(long long int number, char *result, const size_t RESULT_TOTAL_LENGTH);
size_t varint_decode(size_t varint);
double get_generated_supply(const size_t BLOCK_HEIGHT);
int network_block_string_to_blockchain_data(const char* DATA, const char* BLOCK_HEIGHT, const int BLOCK_VERIFIERS_TOTAL);
int blockchain_data_to_network_block_string(char *result, const int BLOCK_VERIFIERS_TOTAL);
int add_data_hash_to_network_block_string(const char* NETWORK_BLOCK_STRING, char *network_block_string_data_hash);
int verify_network_block_data(const int BLOCK_VALIDATION_SIGNATURES_SETTINGS, const int PREVIOUS_BLOCK_HASH_SETTINGS, const char* BLOCK_HEIGHT, const int BLOCK_VERIFIERS_TOTAL);

#endif