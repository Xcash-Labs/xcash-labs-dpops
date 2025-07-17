#include "db_init.h"

bool initialize_database(void) {
  char mongo_uri[256];

#ifdef SEED_NODE_ON
  snprintf(mongo_uri, sizeof(mongo_uri),
           "mongodb://%s:%s@127.0.0.1:27017/?authSource=admin",
           getenv("MONGODB_USERNAME"),
           getenv("MONGODB_PASSWORD"));
#else
  strncpy(mongo_uri, DATABASE_CONNECTION, sizeof(mongo_uri) - 1);
  mongo_uri[sizeof(mongo_uri) - 1] = '\0';  // Always null-terminate
#endif

  if (!initialize_mongo_database(mongo_uri, &database_client_thread_pool)) {
    return false;
  }
}

void shutdown_db(void){
    shutdown_mongo_database(&database_client_thread_pool);
}

bool initialize_mongo_database(const char *mongo_uri, mongoc_client_pool_t **db_client_thread_pool) {
    mongoc_uri_t *uri_thread_pool;
    bson_error_t error;
    // Initialize the MongoDB client library
    mongoc_init();
    // Create a new URI object from the provided URI string
    uri_thread_pool = mongoc_uri_new_with_error(mongo_uri, &error);
    if (!uri_thread_pool) {
        ERROR_PRINT("Failed to parse URI: %s\nError message: %s", mongo_uri, error.message);
        return XCASH_ERROR;
    }
    // Create a new client pool with the parsed URI object
    *db_client_thread_pool = mongoc_client_pool_new(uri_thread_pool);
    if (!*db_client_thread_pool) {
        ERROR_PRINT("Failed to create a new client pool.");
        mongoc_uri_destroy(uri_thread_pool);
        return XCASH_ERROR;
    }
    mongoc_uri_destroy(uri_thread_pool);
    return XCASH_OK;
}

void shutdown_mongo_database(mongoc_client_pool_t **db_client_thread_pool) {
    if (*db_client_thread_pool) {
        mongoc_client_pool_destroy(*db_client_thread_pool);
        *db_client_thread_pool = NULL;
    }
    mongoc_cleanup();
}