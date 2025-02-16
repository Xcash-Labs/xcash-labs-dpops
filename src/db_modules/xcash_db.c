#include "xcash_db.h"
#include "variables.h"


bool initialize_database(const char* mongo_uri) {
    const char* mongo_connection_url = mongo_uri? mongo_uri: "mongodb://127.0.0.1:27017";
    // FIXME: remove later. it's just for compatibility with old info print
    strncpy(MongoDB_uri, mongo_connection_url, sizeof(MongoDB_uri));
    return initialize_mongo_database(mongo_connection_url, &database_client_thread_pool);
}


void shutdown_database(void){
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
        fprintf(stderr, "Failed to parse URI: %s\nError message: %s\n", mongo_uri, error.message);
        return false;
    }

    // Create a new client pool with the parsed URI object
    *db_client_thread_pool = mongoc_client_pool_new(uri_thread_pool);
    if (!*db_client_thread_pool) {
        fprintf(stderr, "Failed to create a new client pool.\n");
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

