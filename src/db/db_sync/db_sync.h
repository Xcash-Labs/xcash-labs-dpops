#ifndef DB_SYNC_H
#define DB_SYNC_H

#include <stdbool.h>
#include <stdlib.h>

#include "config.h"
#include "db_helpers.h"
#include "db_operations.h"
#include "globals.h"
#include "macro_functions.h"
#include "node_functions.h"
#include "uv_net_multi.h"
#include "xcash_net.h"

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

void show_majority_statistics(const xcash_node_sync_info_t* majority_list, size_t items_count);
bool get_sync_nodes_majority_list_top(xcash_node_sync_info_t** majority_list_result, size_t* majority_count_result);
bool initial_db_sync_check(size_t* majority_count, xcash_node_sync_info_t** majority_list_result);
bool check_sync_nodes_majority_list(response_t** replies, xcash_node_sync_info_t** majority_list_result,
                                    size_t* majority_count_result, bool by_top_block_height);
//
bool get_node_sync_info(xcash_node_sync_info_t* sync_info);
bool download_db_from_node(const char* host, xcash_dbs_t db_type, int index, char* result_db_data_buf,
                           size_t result_db_data_buf_size);
void cleanup_db_sync_results(xcash_db_sync_obj_t** sync_objs_result);
size_t get_db_sub_count(xcash_dbs_t db_type);
bool send_db_sync_request_to_all_seeds(xcash_dbs_t db_type, size_t start_db_index, response_t*** reply);
bool parse_nodes_sync_reply(response_t** replies, xcash_dbs_t db_type, xcash_db_sync_obj_t*** sync_objs_result);
bool check_db_hashes_from_seeds(xcash_dbs_t db_type, xcash_db_sync_obj_t*** sync_objs_result);
bool update_db_from_node(const char* public_address, xcash_dbs_t db_type);
bool fill_delegates_from_db(void);
bool get_actual_nodes_list(bool is_seeds_offline);
xcash_node_sync_info_t** make_nodes_majority_list(xcash_node_sync_info_t* sync_states_list, size_t states_count,
                                                  bool by_top_block_height);
bool get_sync_seeds_majority_list(xcash_node_sync_info_t** majority_list_result, size_t* majority_count_result);
bool get_sync_nodes_majority_list(xcash_node_sync_info_t** majority_list_result, size_t* majority_count_result);
int get_random_majority(xcash_node_sync_info_t* majority_list, size_t majority_count);
bool check_time_sync_to_seeds(void);
bool init_db_from_seeds(void);
bool init_db_from_top(void);

#endif