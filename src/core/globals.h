#ifndef GLOBALS_H_   /* Include guard */
#define GLOBALS_H_

#include "config.h"
#include "structures.h"
#include <mongoc/mongoc.h>
#include "crypto_vrf.h"
#include <pthread.h>

/*--------------------------------------------------------------------------------------------------
Global Variables
--------------------------------------------------------------------------------------------------*/
extern int log_level;  // Log level for display log messages
extern bool is_seed_node;   // True if node is a seed node - network_data_node_settings is same as seed node, removed
extern int network_data_nodes_amount; // Number of network data nodes
extern delegates_t delegates_all[BLOCK_VERIFIERS_TOTAL_AMOUNT];
extern char xcash_wallet_public_address[XCASH_PUBLIC_ADDR_LENGTH + 1]; // xCash wallet public address
extern char current_block_height[BUFFER_SIZE_NETWORK_BLOCK_DATA]; // The current block height
extern char previous_block_hash[BLOCK_HASH_LENGTH+1]; // The previous block hash
extern unsigned char secret_key_data[crypto_vrf_SECRETKEYBYTES+1]; // Holds the secret key for signing block verifier messages
extern char secret_key[VRF_SECRET_KEY_LENGTH+1]; // Holds the secret key text for signing block verifier messages
extern char current_round_part[2]; // The current round part (1-4)
extern char current_round_part_backup_node[2]; // The current main node in the current round part (0-5)
extern struct main_nodes_list main_nodes_list; // The list of main nodes public address and IP address
extern block_verifiers_list_t previous_block_verifiers_list; // The list of block verifiers name, public address and IP address for the previous round
extern block_verifiers_list_t current_block_verifiers_list; // The list of block verifiers name, public address and IP address for the current round
extern block_verifiers_list_t next_block_verifiers_list; // The list of block verifiers name, public address and IP address for the next round
extern const char* collection_names[XCASH_DB_COUNT];
extern bool cleanup_db_before_upsert;
extern int main_network_data_node_create_block; // 1 if the main network data node can create a block, 0 if not
extern mongoc_client_pool_t* database_client_thread_pool;  // database

extern pthread_rwlock_t rwlock;
extern pthread_rwlock_t rwlock_reserve_proofs;
extern pthread_mutex_t lock;
extern pthread_mutex_t database_lock;
extern pthread_mutex_t verify_network_block_lock;
extern pthread_mutex_t vote_lock;
extern pthread_mutex_t add_reserve_proof_lock;
extern pthread_mutex_t invalid_reserve_proof_lock;
extern pthread_mutex_t database_data_IP_address_lock;
extern pthread_mutex_t update_current_block_height_lock;
extern pthread_mutex_t hash_mutex;

extern const NetworkNode network_nodes[];

extern char* server_limit_IP_address_list; // holds all of the IP addresses that are currently running on the server.
extern char* server_limit_public_address_list; // holds all of the public addresses that are currently running on the server.

extern const char* xcash_net_messages[];
extern const xcash_msg_t xcash_db_sync_messages[];
extern const xcash_msg_t xcash_db_download_messages[];

void init_globals(void);

#endif