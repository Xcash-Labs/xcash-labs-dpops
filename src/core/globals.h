#ifndef GLOBALS_H_   /* Include guard */
#define GLOBALS_H_

#include "config.h"
#include "structures.h"
#include <mongoc/mongoc.h>
#include "crypto_vrf.h"

/*--------------------------------------------------------------------------------------------------
Global Variables
--------------------------------------------------------------------------------------------------*/
extern const NetworkNode network_nodes[];
extern bool debug_enabled;  // True if debug enabled
extern bool is_seed_node;   // True if node is a seed node - network_data_node_settings is same as seed node, removed
extern char xcash_wallet_public_address[XCASH_PUBLIC_ADDR_LENGTH + 1]; // xCash wallet public address
extern char current_block_height[BUFFER_SIZE_NETWORK_BLOCK_DATA]; // The current block height
extern char previous_block_hash[BLOCK_HASH_LENGTH+1]; // The previous block hash
extern unsigned char secret_key_data[crypto_vrf_SECRETKEYBYTES+1]; // Holds the secret key for signing block verifier messages
extern char secret_key[VRF_SECRET_KEY_LENGTH+1]; // Holds the secret key text for signing block verifier messages

extern mongoc_client_pool_t* database_client_thread_pool;  // database

#endif