#ifndef GLOBALS_H_   /* Include guard */
#define GLOBALS_H_


#include <mongoc/mongoc.h>
#include "crypto_vrf.h"
#include <pthread.h>
#include <stdatomic.h>
#include "config.h"
#include "macro_functions.h"
#include "structures.h"

/*--------------------------------------------------------------------------------------------------
Global Variables
--------------------------------------------------------------------------------------------------*/
extern int log_level;  // Log level for display log messages
extern int delegate_db_hash_mismatch; 
extern double delegate_fee_percent;
extern uint64_t minimum_payout;

extern bool startup_complete;

extern bool is_seed_node;   // True if node is a seed node - network_data_node_settings is same as seed node, removed
extern int network_data_nodes_amount; // Number of network data nodes

extern delegates_t delegates_all[BLOCK_VERIFIERS_TOTAL_AMOUNT];

extern delegates_timer_t delegates_timer_all[BLOCK_VERIFIERS_TOTAL_AMOUNT];

extern char xcash_wallet_public_address[XCASH_WALLET_LENGTH + 1]; // xCash wallet public address
extern char current_block_height[BLOCK_HEIGHT_LENGTH + 1]; // The current block height
extern char previous_block_hash[BLOCK_HASH_LENGTH + 1]; // The previous block hash

extern unsigned char secret_key_data[crypto_vrf_SECRETKEYBYTES]; // Holds the secret key for signing block verifier messages
extern char secret_key[VRF_SECRET_KEY_LENGTH +1]; // Holds the secret key text for signing block verifier messages

extern char vrf_public_key[VRF_PUBLIC_KEY_LENGTH + 1]; 

extern char current_round_part[3]; // The current round part
extern char delegates_hash[SHA256_HASH_SIZE + 1];

extern char sync_token[SYNC_TOKEN_LEN + 1];

//extern struct main_nodes_list main_nodes_list; // The list of main nodes public address and IP address
extern block_verifiers_list_t current_block_verifiers_list; // The list of block verifiers name, public address and IP address for the current round
//extern const char* collection_names[XCASH_DB_COUNT];
//extern bool cleanup_db_before_upsert;

//extern struct blockchain_data blockchain_data; // The data for a new block to be added to the network.
//extern char delegates_error_list[(MAXIMUM_BUFFER_SIZE_DELEGATES_NAME * 100) + 5000]; // Holds the list of delegates that did not complete a part of the round

extern mongoc_client_pool_t* database_client_thread_pool;  // database


extern pthread_mutex_t delegates_all_lock;
extern pthread_mutex_t current_block_verifiers_lock;
extern pthread_mutex_t producer_refs_lock;
extern pthread_mutex_t database_data_IP_address_lock;

extern atomic_bool server_running; 
extern atomic_bool wait_for_vrf_init;
extern atomic_bool wait_for_block_height_init;
extern atomic_bool shutdown_requested;
extern atomic_bool payment_inprocess;

extern pthread_t server_thread;

extern NetworkNode network_nodes[];

extern char* server_limit_IP_address_list; // holds all of the IP addresses that are currently running on the server.
extern char* server_limit_public_address_list; // holds all of the public addresses that are currently running on the server.

extern const char* xcash_net_messages[];
//extern const xcash_msg_t xcash_db_sync_messages[];
//extern const xcash_msg_t xcash_db_download_messages[];

void init_globals(void);

#endif