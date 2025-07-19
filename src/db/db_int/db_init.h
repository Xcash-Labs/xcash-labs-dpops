#ifndef DB_INIT_H
#define DB_INIT_H

#include "globals.h"
#include <mongoc/mongoc.h>
#include <bson/bson.h>
#include <mongoc/mongoc-log.h>
#include <stdbool.h>
#include "string.h"
#include "config.h"
#include "globals.h"
#include "macro_functions.h"
#include "db_functions.h"

bool initialize_database(void);
void shutdown_db(void);
bool initialize_mongo_database(const char* mongo_uri, mongoc_client_pool_t** database_client_thread_pool);
bool initialize_mongo_database_seed(const char* mongo_uri, mongoc_client_pool_t** database_client_thread_pool);
void shutdown_mongo_database(mongoc_client_pool_t** database_client_thread_pool);
void configure_mongo_error_only_logging(void);

#endif