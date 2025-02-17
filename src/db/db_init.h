#ifndef DB_INIT_H
#define DB_INIT_H

#include <mongoc/mongoc.h>
#include <bson/bson.h>
#include <stdbool.h>

#include "globals.h"

bool initialize_database(void);
void shutdown_database(void);
bool initialize_mongo_database(const char* mongo_uri, mongoc_client_pool_t** database_client_thread_pool);
void shutdown_mongo_database(mongoc_client_pool_t** database_client_thread_pool);

#endif