#ifndef DB_SYNC_H
#define DB_SYNC_H

#include <stdbool.h>
#include <stdlib.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"
#include "db_functions.h"
#include "node_functions.h"
#include "net_multi.h"
#include "xcash_net.h"
#include "xcash_delegates.h"

typedef struct {
  size_t block_height;  // the block height
  bool db_reserve_bytes_synced;
  char public_address[XCASH_WALLET_LENGTH + 1];
  char db_hashes[DATABASE_TOTAL][DATA_HASH_LENGTH + 1];
} xcash_node_sync_info_t;

typedef struct
{
  xcash_node_sync_info_t* sync_info;
  char overall_md5_hash[SHA256_HASH_SIZE + 1];

} xcash_db_sync_prehash_t;

bool hash_delegates_collection(char *out_hash_hex);
bool fill_delegates_from_db(void);;
int select_random_online_delegate(void);
bool create_delegate_online_ip_list(char* out_data, size_t out_data_size);

#endif