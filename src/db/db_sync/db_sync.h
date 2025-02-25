#ifndef DB_SYNC_H
#define DB_SYNC_H

#include <stdbool.h>
#include <stdlib.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"

typedef struct {
    size_t block_height; // the block height
    bool db_reserve_bytes_synced;
    char public_address[XCASH_WALLET_LENGTH+1];
    char db_hashes[DATABASE_TOTAL][DATA_HASH_LENGTH+1];
} xcash_node_sync_info_t;

#endif