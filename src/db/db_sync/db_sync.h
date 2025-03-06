#ifndef DB_SYNC_H
#define DB_SYNC_H

#include <stdbool.h>
#include <stdlib.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"
#include "uv_net_multi.h"
#include "xcash_net.h"


typedef struct {
    size_t block_height; // the block height
    bool db_reserve_bytes_synced;
    char public_address[XCASH_WALLET_LENGTH+1];
    char db_hashes[DATABASE_TOTAL][DATA_HASH_LENGTH+1];
} xcash_node_sync_info_t;

typedef struct 
{
    xcash_node_sync_info_t* sync_info;
    char overall_md5_hash[MD5_HASH_SIZE+1];

}xcash_db_sync_prehash_t;

void show_majority_statistics(const xcash_node_sync_info_t* majority_list, size_t items_count);
bool get_sync_nodes_majority_list_top(xcash_node_sync_info_t** majority_list_result, size_t* majority_count_result);
bool initial_db_sync_check(size_t* majority_count, xcash_node_sync_info_t** majority_list_result);

#endif