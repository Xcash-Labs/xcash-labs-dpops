#ifndef VARIABLES_H_   /* Include guard */
#define VARIABLES_H_

/*
-----------------------------------------------------------------------------------------------------------
Global Variables
-----------------------------------------------------------------------------------------------------------
*/
extern bool debug_enabled;  // True if debug enabled
extern bool is_seed_node;   // True if node is a seed node
extern char xcash_wallet_public_address[XCASH_WALLET_LENGTH+1]; // Holds your wallets public address
extern char XCASH_DPOPS_delegates_IP_address[BLOCK_VERIFIERS_IP_ADDRESS_TOTAL_LENGTH]; // The  block verifiers IP address to run the server on
extern char XCASH_daemon_IP_address[BLOCK_VERIFIERS_IP_ADDRESS_TOTAL_LENGTH]; // The XCASH daemon IP
extern char XCASH_wallet_IP_address[BLOCK_VERIFIERS_IP_ADDRESS_TOTAL_LENGTH]; // The  wallet IP address
extern char MongoDB_uri[256];

#endif