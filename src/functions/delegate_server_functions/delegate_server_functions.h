#ifndef DELEGATE_SERVER_FUNCTIONS_H_
#define DELEGATE_SERVER_FUNCTIONS_H_

#include "config.h"
#include "globals.h"
#include "macro_functions.h"
#include "db_functions.h"
#include "xcash_delegates.h"
#include "net_server.h"

//int block_verifiers_add_reserve_proof_check_if_data_is_valid(const char* MESSAGE, struct reserve_proof* reserve_proof);
//int add_reserve_proof_remove_previous_vote(const char* PUBLIC_ADDRESS_CREATE_RESERVE_PROOF_DATA);
//void server_receive_data_socket_node_to_block_verifiers_add_reserve_proof(const int CLIENT_SOCKET, const char* MESSAGE);
int check_for_valid_delegate_name(const char* DELEGATE_NAME);
//int check_for_valid_ip_address(const char* HOST);
void server_receive_data_socket_nodes_to_block_verifiers_register_delegates(server_client_t* clientT, const char* MESSAGE);
//int check_for_valid_delegate_fee(const char* MESSAGE);
//void server_receive_data_socket_nodes_to_block_verifiers_update_delegates(const int CLIENT_SOCKET, const char* MESSAGE);
//void server_receive_data_socket_nodes_to_block_verifiers_recover_delegates(const int CLIENT_SOCKET, const char* MESSAGE);
//void server_receive_data_socket_nodes_to_network_data_nodes_check_vote_status(const int CLIENT_SOCKET, const char* MESSAGE);

#endif