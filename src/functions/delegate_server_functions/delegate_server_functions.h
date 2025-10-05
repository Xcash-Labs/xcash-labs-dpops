#ifndef DELEGATE_SERVER_FUNCTIONS_H_
#define DELEGATE_SERVER_FUNCTIONS_H_

#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "VRF_functions.h"
#include "config.h"
#include "globals.h"
#include "macro_functions.h"
#include "db_functions.h"
#include "xcash_delegates.h"
#include "net_server.h"
#include "string_functions.h"
#include "xcash_round.h"
#include "network_daemon_functions.h"
#include "network_wallet_functions.h"

int check_for_valid_delegate_name(const char* DELEGATE_NAME);
int check_for_valid_ip_or_hostname(const char *host);
void server_receive_data_socket_nodes_to_block_verifiers_register_delegates(server_client_t* client, const char* MESSAGE);
void server_receive_data_socket_nodes_to_block_verifiers_update_delegates(server_client_t* client, const char* MESSAGE);
void server_receive_data_socket_nodes_to_block_verifiers_validate_block(server_client_t *client, const char *MESSAGE);
void server_receive_data_socket_node_to_block_verifiers_add_reserve_proof(server_client_t *client, const char *MESSAGE);
void server_receive_data_socket_node_to_block_verifiers_check_vote_status(server_client_t *client, const char *MESSAGE);

#endif