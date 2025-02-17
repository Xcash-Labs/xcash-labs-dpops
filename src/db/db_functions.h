#ifndef DB_FUNCTIONS_H
#define DB_FUNCTIONS_H

#include <mongoc/mongoc.h>
#include <bson/bson.h>
#include "globals.h"

bool initialize_mongo_database(const char* mongo_uri, mongoc_client_pool_t** database_client_thread_pool);

void shutdown_mongo_database(mongoc_client_pool_t** database_client_thread_pool);

#endif