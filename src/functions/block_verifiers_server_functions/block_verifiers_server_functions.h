#ifndef BLOCK_VERIFIERS_SERVER_FUNCTIONS_H_   /* Include guard */
#define BLOCK_VERIFIERS_SERVER_FUNCTIONS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"
#include "string_functions.h"
#include "sha256EL.h"
#include "network_daemon_functions.h"

void server_receive_data_socket_node_to_node_vote_majority(const char* MESSAGE);
void server_receive_data_socket_block_verifiers_to_block_verifiers_vrf_data(const char* MESSAGE);
bool verify_vrf_vote_signature(const char *block_height, const char *vrf_beta_hex, const char *vrf_pubkey_hex, const char *public_wallet_address,
  const char *vote_signature);

#endif