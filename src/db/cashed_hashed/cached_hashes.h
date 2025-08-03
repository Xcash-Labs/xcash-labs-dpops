#ifndef CACHED_HASHES_H_
#define CACHED_HASHES_H_

#include <string.h>
#include <bson/bson.h>
#include <mongoc/mongoc.h>
#include <openssl/evp.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"
#include "string_functions.h"
#include "node_functions.h"
#include "db_functions.h"

//int get_multi_hash(mongoc_client_t *client, const char *db_prefix, char *hash);
//int drop_all_hashes(mongoc_client_t *client);

#endif