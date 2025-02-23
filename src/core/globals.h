#ifndef GLOBALS_H_   /* Include guard */
#define GLOBALS_H_

#include "config.h"
#include "structures.h"
#include <mongoc/mongoc.h>

/*--------------------------------------------------------------------------------------------------
Global Variables
--------------------------------------------------------------------------------------------------*/
extern mongoc_client_pool_t* database_client_thread_pool;  // database

extern char XCASH_daemon_IP_address[IP_LENGTH + 1];

extern bool is_seed_node;   // True if node is a seed node
extern bool debug_enabled;  // True if debug enabled
extern int sig_requests; // for shutdown signal requests


extern const NetworkNode network_nodes[]; // Network nodes array (variable size, terminated with NULL)

extern char current_block_height[BUFFER_SIZE_NETWORK_BLOCK_DATA]; // The current block height
extern char xcash_wallet_public_address[XCASH_PUBLIC_ADDR_LENGTH + 1]; // xCash wallet public address

#endif