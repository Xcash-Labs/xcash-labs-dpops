#ifndef DEFINE_CONFIG_H_   /* Include guard */
#define CONFIG_H_

#include <stdbool.h>

#define XCASH_DPOPS_CURRENT_VERSION "xCash Labs DPoPs V. 2.0.0"
#define XCASH_DAEMON_PORT 18281 // The X-CASH Daemon RPC port
#define XCASH_WALLET_PORT 18285 // The X-CASH Wallet RPC port
#define XCASH_DPOPS_PORT 18283 // The X-CASH Dpops service
#define DATABASE_CONNECTION "mongodb://127.0.0.1:27017" // The database connection string

#define BLOCK_VERIFIERS_IP_ADDRESS_TOTAL_LENGTH 100 // The maximum length of the block verifiers IP address
#define XCASH_WALLET_LENGTH 98 // The length of a XCA address
#define VRF_SECRET_KEY_LENGTH 128 // Length of VRF Secret Key
#define IP_LENGTH 39 // Length of ip address for IPv4 and IPv6

#define RED_TEXT(text) "\033[31m"text"\033[0m"
#define YELLOW_TEXT(text) "\033[1;33m"text"\033[0m"
#define GREEN_TEXT(text) "\x1b[32m"text"\x1b[0m"
#define BRIGHT_WHITE_TEXT(text) "\033[1;97m"text"\033[0m"

#define LOG_ERR      3   /* error conditions */
#define LOG_DEBUG    7   /* debug-level messages */
// Macros to handle errors and log them
#define HANDLE_ERROR(msg) do { \
    log_message(LOG_ERR, __func__, "%s", RED_TEXT(msg)); \
    exit(EXIT_FAILURE); \
} while (0)
#define HANDLE_DEBUG(msg) do { \
    if (debug_enabled) log_message(LOG_DEBUG, __func__, "%s", YELLOW_TEXT(msg)); \
} while (0)
#define INVALID_PARAMETERS_ERROR_MESSAGE \

/*---------------------------------------------------------------------------------------------------------
Global Variables
-----------------------------------------------------------------------------------------------------------
*/
extern bool debug_enabled;  // True if debug enabled

extern bool is_seed_node;   // True if node is a seed node
extern char XCASH_DPOPS_delegates_IP_address[IP_LENGTH+1]; // The  block verifiers IP address to run the server on
extern char XCASH_daemon_IP_address[IP_LENGTH+1]; // The XCASH daemon IP
extern char XCASH_wallet_IP_address[IP_LENGTH+1]; // The  wallet IP address

#endif