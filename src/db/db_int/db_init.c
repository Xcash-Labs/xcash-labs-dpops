#include "db_init.h"

bool initialize_database(void) {
  char mongo_uri[512];  // Increased buffer size for full URI
  configure_mongo_error_only_logging();

#ifdef SEED_NODE_ON
  is_seed_node = true;

  const char *username = getenv("MONGODB_USERNAME");
  const char *password = getenv("MONGODB_PASSWORD");

  if (!username || !password) {
    ERROR_PRINT("Missing MongoDB credentials: MONGODB_USERNAME or MONGODB_PASSWORD not set");
    return false;
  }

  // TLS-enabled replica set URI (no tlsCAFile in the URI)
  snprintf(mongo_uri, sizeof(mongo_uri),
           "mongodb://%s:%s@46.202.89.18:27017,82.180.154.21:27017,91.108.104.25:27017,212.85.13.137:27017/"
           "?authSource=admin&replicaSet=xcashRS&tls=true&retryWrites=true&w=majority&journal=true",
           username, password);

#else
  // Fallback for non-seed nodes (from config)
  strncpy(mongo_uri, DATABASE_CONNECTION, sizeof(mongo_uri) - 1);
  mongo_uri[sizeof(mongo_uri) - 1] = '\0';  // Always null-terminate
#endif

  if (is_seed_node) {
    if (!initialize_mongo_database_seed(mongo_uri, &database_client_thread_pool)) {
      return false;
    }
  } else {
      if (!initialize_mongo_database(mongo_uri, &database_client_thread_pool)) {
        return false;
      }
  }

  return true;
}

static void error_only_log_handler(mongoc_log_level_t log_level_mongo,
                                   const char *log_domain,
                                   const char *message,
                                   void *user_data) {
  (void)user_data;
  // The can be changed base on MongoDB Log Levels, Only displaying errors for now
  if (log_level_mongo == MONGOC_LOG_LEVEL_ERROR) {
    ERROR_PRINT("[MONGODB ERROR] %s: %s", log_domain, message);
  }
}

void configure_mongo_error_only_logging(void) {
    mongoc_log_set_handler(error_only_log_handler, NULL);
}

void shutdown_db(void){
    shutdown_mongo_database(&database_client_thread_pool);
}

bool initialize_mongo_database_seed(const char *mongo_uri, mongoc_client_pool_t **db_client_thread_pool) {
    bson_error_t error;
    mongoc_uri_t *uri_thread_pool = NULL;
    mongoc_client_pool_t *pool = NULL;

    // Initialize the MongoDB driver
    mongoc_init();

    // Parse the MongoDB URI
    uri_thread_pool = mongoc_uri_new_with_error(mongo_uri, &error);
    if (!uri_thread_pool) {
        ERROR_PRINT("Failed to parse MongoDB URI: %s\nError: %s", mongo_uri, error.message);
        return XCASH_ERROR;
    }

    // Create the client pool
    pool = mongoc_client_pool_new(uri_thread_pool);
    if (!pool) {
        ERROR_PRINT("Failed to create MongoDB client pool");
        mongoc_uri_destroy(uri_thread_pool);
        return XCASH_ERROR;
    }

    // If URI uses TLS, apply mutual TLS certificate options
    if (strstr(mongo_uri, "tls=true") != NULL) {
        const mongoc_ssl_opt_t *default_opts = mongoc_ssl_opt_get_default();
        mongoc_ssl_opt_t ssl_opts = *default_opts;  // Copy default options

        ssl_opts.pem_file = "/etc/ssl/mongodb/mongodb.pem";  // Client cert + key
        ssl_opts.ca_file  = "/etc/ssl/mongodb/mongodb.crt";  // CA cert to verify server

        mongoc_client_pool_set_ssl_opts(pool, &ssl_opts);
    }

    // Success — store pool reference
    *db_client_thread_pool = pool;
    mongoc_uri_destroy(uri_thread_pool);

    return XCASH_OK;
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