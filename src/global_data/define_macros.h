#ifndef DEFINE_MACROS_H_   /* Include guard */
#define DEFINE_MACROS_H_


#include <stdbool.h>

// define global variables
extern bool debug_enabled;

// Macro to handle errors and log them
#define LOG_ERR      3   /* error conditions */
#define LOG_DEBUG    7   /* debug-level messages */
#define HANDLE_ERROR(msg) do { log_message(LOG_ERR, __func__, "%s", msg); return 0; } while (0)
#define HANDLE_DEBUG(msg) do { if (debug_enabled) log_message(LOG_DEBUG, __func__, "%s", msg); } while (0)

#define SMALL_BUFFER_SIZE 256
#define BUFFER_SIZE 1024
#define VRF_SECRET_KEY_LENGTH 64

#define INVALID_PARAMETERS_ERROR_MESSAGE \
"Parameters\n" \
"All parameters are optional, except for --block-verifiers-secret-key\n\n" \
"--parameters - List of all valid parameters\n" \
"--block-verifiers-secret-key <block_verifiers_secret_key> - The block verifiers secret key. Must be the first parameter.\n" \
"--debug - Show all incoming and outgoing messages from the server.\n"

#endif