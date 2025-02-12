#ifndef DEFINE_MACROS_H_   /* Include guard */
#define DEFINE_MACROS_H_


#include <stdbool.h>

// define global variables
extern bool debug_settings;

// Macro to handle errors and log them
#define HANDLE_ERROR(msg) do { log_message(__func__, msg); return 0; } while (0)  

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