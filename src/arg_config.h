#ifndef ARG_CONFIG_H
#define ARG_CONFIG_H

#include <stdbool.h>


typedef struct {
    char *block_verifiers_secret_key;
    bool debug_mode;
} arg_config_t;

// Define an enum for option IDs
typedef enum {
    OPTION_DEBUG,
} option_ids;

#endif // ARG_CONFIG_H