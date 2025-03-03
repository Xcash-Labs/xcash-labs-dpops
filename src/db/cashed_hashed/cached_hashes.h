#ifndef CACHED_HASHES_H_
#define CACHED_HASHES_H_

#include <bson/bson.h>
#include <mongoc/mongoc.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"

int del_hash(mongoc_client_t *client, const char *db_name);

#endif
