#ifndef GLOBALS_H_   /* Include guard */
#define GLOBALS_H_

#include "config.h"
#include "structures.h"
#include <mongoc/mongoc.h>

/*--------------------------------------------------------------------------------------------------
Global Variables
--------------------------------------------------------------------------------------------------*/
extern mongoc_client_pool_t* database_client_thread_pool;  // database

extern bool debug_enabled;  // True if debug enabled
extern int sig_requests; // for shutdown signal requests

extern char XCASH_daemon_IP_address[IP_LENGTH + 1];

extern bool is_seed_node;   // True if node is a seed node
extern bool is_shutdown_state; // True if shutdown requested

 // The  wallet IP address




extern const NetworkNode network_nodes[]; // Network nodes array (variable size, terminated with NULL)

extern char current_block_height[BUFFER_SIZE_NETWORK_BLOCK_DATA]; // The current block height


#endif