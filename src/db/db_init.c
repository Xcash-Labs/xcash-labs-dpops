#include "db_init.h"
#include "globals.h"
#include "logger.h"
#include <mongoc/mongoc.h>
#include "config.h"


bool initialize_database(void){
    return initialize_mongo_database(DATABASE_CONNECTION, &database_client_thread_pool);
}

void shutdown_database(void){
    shutdown_mongo_database(&database_client_thread_pool);
}

bool initialize_mongo_database(const char *mongo_uri, mongoc_client_pool_t **db_client_thread_pool) {
    char wsbuf[1024];
    mongoc_uri_t *uri_thread_pool;
    bson_error_t error;
    // Initialize the MongoDB client library
    mongoc_init();
    // Create a new URI object from the provided URI string
    uri_thread_pool = mongoc_uri_new_with_error(mongo_uri, &error);
    if (!uri_thread_pool) {
        snprintf(buffer, sizeof(wsbuf), "Failed to parse URI: %s\nError message: %s", mongo_uri, error.message", mongo_uri, error.message);
        HANDLE_DEBUG(wsbuff);
        return false;
    }
    // Create a new client pool with the parsed URI object
    *db_client_thread_pool = mongoc_client_pool_new(uri_thread_pool);
    if (!*db_client_thread_pool) {
        HANDLE_DEBUG("Failed to create a new client pool.");
        mongoc_uri_destroy(uri_thread_pool);
        return false;
    }
    mongoc_uri_destroy(uri_thread_pool);
    return true;
}

void shutdown_mongo_database(mongoc_client_pool_t **db_client_thread_pool) {
    if (*db_client_thread_pool) {
        mongoc_client_pool_destroy(*db_client_thread_pool);
        *db_client_thread_pool = NULL;
    }
    mongoc_cleanup();
}