#ifndef BLOCK_VERIFIERS_SYNCHRONIZE_SERVER_FUNCTIONS_H_   /* Include guard */
#define BLOCK_VERIFIERS_SYNCHRONIZE_SERVER_FUNCTIONS_H_
#include "uv_net_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>
#include "config.h"
#include "globals.h"

#include "macro_functions.h"
#include "db_operations.h"

void server_received_msg_get_block_hash(server_client_t* client, const char* MESSAGE);

void server_received_msg_get_block_producers(server_client_t* client, const char* MESSAGE);

void server_received_msg_get_sync_info(server_client_t* client, const char* MESSAGE);

void server_receive_data_socket_get_current_block_height(const char* CLIENT_IP_ADDRESS);   //  ???????????
void server_receive_data_socket_send_current_block_height(const char* MESSAGE);
void server_receive_data_socket_node_to_network_data_nodes_get_previous_current_next_block_verifiers_list(server_client_t* client);
void server_receive_data_socket_node_to_network_data_nodes_get_current_block_verifiers_list(server_client_t* client);
void server_receive_data_socket_network_data_nodes_to_network_data_nodes_database_sync_check(const char* MESSAGE);
void server_receive_data_socket_node_to_block_verifiers_get_reserve_bytes_database_hash(server_client_t* client, const char* MESSAGE);
void server_receive_data_socket_node_to_block_verifiers_check_if_current_block_verifier(server_client_t* client);
void server_receive_data_socket_block_verifiers_to_block_verifiers_reserve_proofs_database_sync_check_all_update(server_client_t* client, const char* MESSAGE);
void server_receive_data_socket_block_verifiers_to_block_verifiers_reserve_proofs_database_download_file_update(server_client_t* client, const char* MESSAGE);
void server_receive_data_socket_block_verifiers_to_block_verifiers_reserve_bytes_database_sync_check_all_update(server_client_t* client, const char* MESSAGE);
void server_receive_data_socket_block_verifiers_to_block_verifiers_reserve_bytes_database_download_file_update(server_client_t* client, const char* MESSAGE);
void server_receive_data_socket_block_verifiers_to_block_verifiers_delegates_database_sync_check_update(server_client_t* client, const char* MESSAGE);
void server_receive_data_socket_block_verifiers_to_block_verifiers_delegates_database_download_file_update(server_client_t* client);
void server_receive_data_socket_block_verifiers_to_block_verifiers_statistics_database_sync_check_update(server_client_t* client, const char* MESSAGE);
void server_receive_data_socket_block_verifiers_to_block_verifiers_statistics_database_download_file_update(server_client_t* client);
#endif