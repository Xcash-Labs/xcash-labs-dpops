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

bool hash_delegates_collection(char *out_hash_hex);
bool fill_delegates_from_db(void);;
int get_random_majority(xcash_node_sync_info_t* majority_list, size_t majority_count);

#endif