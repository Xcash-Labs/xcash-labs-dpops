#ifndef BLOCK_VERIFIERS_UPDATE_FUNCTIONS_H_   /* Include guard */
#define BLOCK_VERIFIERS_UPDATE_FUNCTIONS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"
#include "db_functions.h"

int get_block_verifiers_from_network_block(const int TOTAL_DELEGATES, const delegates_t* delegates, const size_t CURRENT_BLOCK_HEIGHT, const int SETTINGS);
int update_block_verifiers_list(void);
//int update_databases(void);
//int add_block_verifiers_round_statistics(const char* BLOCK_HEIGHT);
//int add_round_statistics(void);
//int calculate_main_nodes_roles(void);
//void check_for_updates(void);

#endif