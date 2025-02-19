#ifndef XCASH_DPOPS_TEST_H_   /* Include guard */
#define XCASH_DPOPS_TEST_H_

#include <stdbool.h>

typedef struct {
    char *block_verifiers_secret_key; // Holds your wallets public address
    bool is_seed_node;
} arg_config_t;

// Define an enum for option IDs
typedef enum {
    OPTION_GENERATE_KEY,
    OPTION_DEBUG,
    OPTION_TOTAL_THREADS,
} option_ids;

bool init_processing(const arg_config_t* arg_config);

#endif