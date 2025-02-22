#ifndef CACHED_HASHES_H_
#define CACHED_HASHES_H_

#include <bson/bson.h>
#include <mongoc/mongoc.h>
#include <stdio.h>
#include "md5.h"
//#include "define_macro_functions.h"
#include <time.h>

int get_multi_hash(mongoc_client_t *client, const char *db_prefix, char *hash);
int del_hash(mongoc_client_t *client, const char *db_name);
int drop_all_hashes(mongoc_client_t *client);

#endif
