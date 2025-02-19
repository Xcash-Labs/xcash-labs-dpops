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

extern int server_socket;
extern int epoll_fd;






extern bool is_seed_node;   // True if node is a seed node
extern char XCASH_DPOPS_delegates_IP_address[IP_LENGTH+1]; // The  block verifiers IP address to run the server on
extern char XCASH_daemon_IP_address[IP_LENGTH+1]; // The XCASH daemon IP
extern char XCASH_wallet_IP_address[IP_LENGTH+1]; // The  wallet IP address

extern const NetworkNode network_nodes[]; // Network nodes array (variable size, terminated with NULL)

#endif