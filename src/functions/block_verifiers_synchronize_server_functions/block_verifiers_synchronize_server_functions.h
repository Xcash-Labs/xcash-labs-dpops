#ifndef BLOCK_VERIFIERS_SYNCHRONIZE_SERVER_FUNCTIONS_H_   /* Include guard */
#define BLOCK_VERIFIERS_SYNCHRONIZE_SERVER_FUNCTIONS_H_

#include "net_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"
#include "structures.h"
#include "db_functions.h"
#include "db_sync.h"
#include "xcash_message.h"
#include "db_sync.h"

void server_receive_data_socket_node_to_network_data_nodes_get_current_block_verifiers_list(server_client_t* client);
void server_receive_data_socket_node_to_node_db_sync_req(server_client_t *client, const char *MESSAGE);
void server_receive_data_socket_node_to_node_db_sync_data(const char *MESSAGE);
bool build_seed_to_nodes_vote_count_update(const char* public_address, uint64_t vote_count_atomic, char** upd_vote_message);
void server_receive_update_delegate_vote_count(const char* MESSAGE);

#endif