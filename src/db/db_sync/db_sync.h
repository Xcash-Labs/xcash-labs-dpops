#ifndef DB_SYNC_H
#define DB_SYNC_H

#include <stdbool.h>
#include <stdlib.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"
#include "db_helpers.h"
#include "db_operations.h"
#include "node_functions.h"
#include "net_multi.h"
#include "xcash_net.h"
#include "xcash_delegates.h"

/*
typedef struct {
    size_t db_rec_index;
    bool db_rec_synced;
} xcash_dbs_check_status_t;

typedef struct {
    size_t db_node_index;  // the node db was checked from
    size_t records_count;
    bool db_synced;
    xcash_dbs_check_status_t* sync_records;
} xcash_db_sync_obj_t;

typedef struct {
  size_t block_height;  // the block height
  bool db_reserve_bytes_synced;
  char public_address[XCASH_WALLET_LENGTH + 1];
  char db_hashes[DATABASE_TOTAL][DATA_HASH_LENGTH + 1];
} xcash_node_sync_info_t;

typedef struct
{
  xcash_node_sync_info_t* sync_info;
  char overall_md5_hash[MD5_HASH_SIZE + 1];

} xcash_db_sync_prehash_t;
*/

bool hash_delegates_collection(char *out_hash_hex);
bool fill_delegates_from_db(void);;

#endif