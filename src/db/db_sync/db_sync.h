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

bool hash_delegates_collection(char *out_hash_hex);
bool fill_delegates_from_db(void);;
int select_random_online_delegate(void);
bool create_delegate_online_ip_list(char* out_data, size_t out_data_size);

#endif