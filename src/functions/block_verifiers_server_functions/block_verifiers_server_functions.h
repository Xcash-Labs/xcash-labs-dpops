#ifndef BLOCK_VERIFIERS_SERVER_FUNCTIONS_H_   /* Include guard */
#define BLOCK_VERIFIERS_SERVER_FUNCTIONS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"
#include "string_functions.h"

void server_receive_data_socket_node_to_node_vote_majority(const char* MESSAGE);
void server_receive_data_socket_block_verifiers_to_block_verifiers_vrf_data(const char* MESSAGE);

#endif