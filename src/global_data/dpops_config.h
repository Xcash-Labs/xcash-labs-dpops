#ifndef DEFINE_DPOPS_CONFIG_H_   /* Include guard */
#define DPOPS_CONFIG_H_

#include <stdbool.h>

#define XCASH_DPOPS_CURRENT_VERSION "xcash-labs-dpops - Version 2.0.0\n"
#define XCASH_DAEMON_PORT 18281 // The X-CASH Daemon RPC port
#define XCASH_WALLET_PORT 18285 // The X-CASH Wallet RPC port
#define XCASH_DPOPS_PORT 18283 // The X-CASH Dpops service
#define DATABASE_CONNECTION "mongodb://localhost:27017" // The database connection string

#define BLOCK_VERIFIERS_IP_ADDRESS_TOTAL_LENGTH 100 // The maximum length of the block verifiers IP address
#define XCASH_WALLET_LENGTH 98 // The length of a XCA address
#define VRF_SECRET_KEY_LENGTH 128 // Length of VRF Secret Key
#define IP_LENGTH 39 // Length of ip address for IPv4 and IPv6

#define LOG_ERR      3   /* error conditions */
#define LOG_DEBUG    7   /* debug-level messages */
// Macros to handle errors and log them
#define HANDLE_ERROR(msg) do { \
    fprintf(stderr, "\033[1;31m"); /* Set text color to bold red */ \
    log_message(LOG_ERR, __func__, "%s", msg); \
    fprintf(stderr, "\033[0m"); /* Reset text color */ \
    exit(EXIT_FAILURE); \
} while (0)
#define HANDLE_DEBUG(msg) do { if (debug_enabled) log_message(LOG_DEBUG, __func__, "%s", msg); } while (0)

#define INVALID_PARAMETERS_ERROR_MESSAGE \
"Parameters\n" \
"All parameters are optional, except for --block-verifiers-secret-key\n\n" \
"--help or --h - List of all valid parameters\n" \
"--block-verifiers-secret-key <block_verifiers_secret_key> - The block verifiers secret key. Must be the first parameter.\n" \
"--debug - Show information and debug messages.\n"

/*
-----------------------------------------------------------------------------------------------------------
Global Variables
-----------------------------------------------------------------------------------------------------------
*/
extern bool debug_enabled;  // True if debug enabled
extern bool is_seed_node;   // True if node is a seed node
extern char xcash_wallet_public_address[XCASH_WALLET_LENGTH+1]; // Holds your wallets public address
extern char XCASH_DPOPS_delegates_IP_address[IP_LENGTH+1]; // The  block verifiers IP address to run the server on
extern char XCASH_daemon_IP_address[IP_LENGTH+1]; // The XCASH daemon IP
extern char XCASH_wallet_IP_address[IP_LENGTH+1]; // The  wallet IP address

#endif