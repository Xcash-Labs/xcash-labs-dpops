#ifndef ARG_CONFIG_H
#define ARG_CONFIG_H

#include <stdbool.h>

typedef struct {
    .block_verifiers_secret_key = NULL,
    .generate_key = false,
    .debug_mode = false,
    .total_threads = 0
} arg_config_t;

// Define an enum for option IDs
typedef enum {
    OPTION_GENERATE_KEY,
    OPTION_DEBUG,
    OPTION_TOTAL_THREADS,
} option_ids;

#endif // ARG_CONFIG_H