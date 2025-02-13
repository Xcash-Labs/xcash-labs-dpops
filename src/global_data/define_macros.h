#ifndef DEFINE_MACROS_H_   /* Include guard */
#define DEFINE_MACROS_H_

#include <stdbool.h>

#define XCASH_DAEMON_PORT 18281 // The X-CASH Daemon RPC port
#define XCASH_WALLET_PORT 18285 // The X-CASH Wallet RPC port
#define XCASH_DPOPS_PORT 18283 // The X-CASH Dpops service
#define XCASH_WALLET_LENGTH 98 // The length of a XCA addres
#define BLOCK_VERIFIERS_IP_ADDRESS_TOTAL_LENGTH 100 // The maximum length of the block verifiers IP address
#define BUFFER_SIZE 1024

#define XCASH_DPOPS_CURRENT_VERSION "xcash-labs-dpops - Version 2.0.0\n"

#define LOG_ERR      3   /* error conditions */
#define LOG_DEBUG    7   /* debug-level messages */
// Macros to handle errors and log them
#define HANDLE_ERROR(msg) do { log_message(LOG_ERR, __func__, "%s", msg); return 0; } while (0)
#define HANDLE_DEBUG(msg) do { if (debug_enabled) log_message(LOG_DEBUG, __func__, "%s", msg); } while (0)

#define INVALID_PARAMETERS_ERROR_MESSAGE \
"Parameters\n" \
"All parameters are optional, except for --block-verifiers-secret-key\n\n" \
"--help or --h - List of all valid parameters\n" \
"--block-verifiers-secret-key <block_verifiers_secret_key> - The block verifiers secret key. Must be the first parameter.\n" \
"--debug - Show information and debug messages.\n"

#endif