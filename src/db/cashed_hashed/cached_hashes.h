#ifndef CACHED_HASHES_H_
#define CACHED_HASHES_H_

#include <bson/bson.h>
#include <mongoc/mongoc.h>
#include <stdio.h>
#include "md5.h"
#include "config.h"
#include "globals.h"
#include "macro_functions.h"
#include <time.h>

int del_hash(mongoc_client_t *client, const char *db_name);

#endif
