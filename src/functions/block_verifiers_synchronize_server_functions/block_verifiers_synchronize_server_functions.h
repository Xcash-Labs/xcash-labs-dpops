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
#include "db_operations.h"
#include "db_sync.h"
#include "xcash_message.h"
#include "db_sync.h"

void server_received_msg_get_sync_info(server_client_t* client, const char* MESSAGE);
void server_receive_data_socket_node_to_network_data_nodes_get_current_block_verifiers_list(server_client_t* client);
void server_receive_data_socket_node_to_node_db_sync_req(server_client_t *client);
void server_receive_data_socket_node_to_node_db_sync_data(const char *MESSAGE);

#endif