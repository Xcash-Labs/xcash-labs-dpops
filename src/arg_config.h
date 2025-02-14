#ifndef ARG_CONFIG_H
#define ARG_CONFIG_H

#include <stdbool.h>

typedef struct {
    char *block_verifiers_secret_key;
    bool generate_key;
    bool debug_mode;
    int total_threads;
} arg_config_t;

// Define an enum for option IDs
typedef enum {
    OPTION_GENERATE_KEY,
    OPTION_DEBUG,
    OPTION_TOTAL_THREADS
} option_ids;

#endif // ARG_CONFIG_H