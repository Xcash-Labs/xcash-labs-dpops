#ifndef GLOBALS_H_   /* Include guard */
#define GLOBALS_H_

#include <mongoc/mongoc.h>

/*--------------------------------------------------------------------------------------------------
Constants 
--------------------------------------------------------------------------------------------------*/
const int MAX_CONNECTIONS = 10;

/*--------------------------------------------------------------------------------------------------
Global Variables
--------------------------------------------------------------------------------------------------*/
extern mongoc_client_pool_t* database_client_thread_pool;  // database

extern bool debug_enabled;  // True if debug enabled

extern bool is_seed_node;   // True if node is a seed node
extern char XCASH_DPOPS_delegates_IP_address[IP_LENGTH+1]; // The  block verifiers IP address to run the server on
extern char XCASH_daemon_IP_address[IP_LENGTH+1]; // The XCASH daemon IP
extern char XCASH_wallet_IP_address[IP_LENGTH+1]; // The  wallet IP address

#endif