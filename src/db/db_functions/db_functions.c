#include "db_functions.h"

/*---------------------------------------------------------------------------------------------------------
Name: count_documents_in_collection
Description: Counts the documents in the collection that match a specific field name and field
Parameters:
  DATABASE - The database name
  COLLECTION - The collection name
  DATA - The json data to use to search the collection for
Return: -1 if an error has occured, otherwise the amount of documents that match a specific field name and field in the collection
---------------------------------------------------------------------------------------------------------*/
int count_documents_in_collection(const char* DATABASE, const char* COLLECTION, const char* DATA) {

  if (!database_client_thread_pool) {
      ERROR_PRINT("Database client pool is not initialized! Cannot count documents.");
      return -1;
  }

  mongoc_client_t* database_client_thread = mongoc_client_pool_pop(database_client_thread_pool);
  if (!database_client_thread) {
      ERROR_PRINT("Failed to get a database connection from the pool.");
      return -1;
  }

  mongoc_collection_t* collection = mongoc_client_get_collection(database_client_thread, DATABASE, COLLECTION);
  if (!collection) {
      ERROR_PRINT("Failed to get collection: %s", COLLECTION);
      mongoc_client_pool_push(database_client_thread_pool, database_client_thread);
      return -1;
  }

  if (check_if_database_collection_exist(DATABASE, COLLECTION) == 0) {
      DEBUG_PRINT("Collection does not exist: %s", COLLECTION);
      mongoc_collection_destroy(collection);
      mongoc_client_pool_push(database_client_thread_pool, database_client_thread);
      return 0;  // Return 0 instead of an error when the collection does not exist
  }

  bson_error_t error;
  bson_t* document = bson_new_from_json((const uint8_t*)DATA, -1, &error);
  if (!document) {
      ERROR_PRINT("Could not convert JSON to BSON: %s", error.message);
      mongoc_collection_destroy(collection);
      mongoc_client_pool_push(database_client_thread_pool, database_client_thread);
      return -1;
  }

  int count = (int)mongoc_collection_count_documents(collection, document, NULL, NULL, NULL, &error);
  if (count < 0) {
      ERROR_PRINT("Error counting documents in %s: %s", COLLECTION, error.message);
      count = -1;
  }

  // Cleanup
  bson_destroy(document);
  mongoc_collection_destroy(collection);
  mongoc_client_pool_push(database_client_thread_pool, database_client_thread);

  return count;
}

/*---------------------------------------------------------------------------------------------------------
Name: count_all_documents_in_collection
Description: Counts all the documents in the collection
Parameters:
  DATABASE - The database name
  COLLECTION - The collection name
Return: -1 if an error has occured, otherwise the amount of documents in the collection
---------------------------------------------------------------------------------------------------------*/
int count_all_documents_in_collection(const char* DATABASE, const char* COLLECTION) {
  // Sanity check to ensure database is initialized
  if (!database_client_thread_pool) {
      ERROR_PRINT("Database client pool is not initialized! Cannot count documents.");
      return -1;
  }

  mongoc_client_t* database_client_thread = mongoc_client_pool_pop(database_client_thread_pool);
  if (!database_client_thread) {
      ERROR_PRINT("Failed to get a database connection from the pool.");
      return -1;
  }

  mongoc_collection_t* collection = mongoc_client_get_collection(database_client_thread, DATABASE, COLLECTION);
  if (!collection) {
      ERROR_PRINT("Failed to get collection: %s", COLLECTION);
      mongoc_client_pool_push(database_client_thread_pool, database_client_thread);
      return -1;
  }

  if (check_if_database_collection_exist(DATABASE, COLLECTION) == 0) {
      DEBUG_PRINT("Collection does not exist: %s", COLLECTION);
      mongoc_collection_destroy(collection);
      mongoc_client_pool_push(database_client_thread_pool, database_client_thread);
      return 0; // Collection does not exist → Return 0 instead of an error
  }

  // Count the documents
  bson_error_t error;
  int count = (int)mongoc_collection_count_documents(collection, NULL, NULL, NULL, NULL, &error);
  if (count < 0) {
      ERROR_PRINT("Error counting documents in %s: %s", COLLECTION, error.message);
  }

  mongoc_collection_destroy(collection);
  mongoc_client_pool_push(database_client_thread_pool, database_client_thread);
  
  return count;
}

/*---------------------------------------------------------------------------------------------------------
Name: insert_document_into_collection_json
Description: Inserts a document into the collection in the database from json data
Parameters:
  DATABASE - The database name
  COLLECTION - The collection name
  DATA - The json data to insert into the collection
Return: 0 if an error has occured, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int insert_document_into_collection_json(const char* DATABASE, const char* COLLECTION, const char* DATA) {
    if (strlen(DATA) > MAXIMUM_DATABASE_WRITE_SIZE) {
        ERROR_PRINT("Data exceeds maximum write size.");
        return XCASH_ERROR;
    }

    char data_hash[DATA_HASH_LENGTH + 1] = {0};
    char data_buffer[BUFFER_SIZE] = {0};
    char formatted_json[BUFFER_SIZE] = {0};
    mongoc_client_t* database_client_thread = NULL;
    mongoc_collection_t* collection = NULL;
    bson_error_t error;
    bson_t* document = NULL;
    strncpy(data_buffer, DATA, sizeof(data_buffer) - 1);
    string_replace(data_buffer, sizeof(data_buffer), "\r\n", "");
    string_replace(data_buffer, sizeof(data_buffer), "\n", "");
    string_replace(data_buffer, sizeof(data_buffer), "\" : \"", "\":\"");
    string_replace(data_buffer, sizeof(data_buffer), "\", \"", "\",\"");
    string_replace(data_buffer, sizeof(data_buffer), "\" }", "\"}");

    const char* message = NULL;
    if (strstr(COLLECTION, "reserve_proofs") && (message = strstr(data_buffer, "\"public_address_created_reserve_proof\":\""))) {
        message += 40;
        snprintf(data_hash, sizeof(data_hash), "000000000000000000000000000000%.*s", DATA_HASH_LENGTH - 32, message);
    } else if (strstr(COLLECTION, "reserve_bytes") && (message = strstr(data_buffer, "\"reserve_bytes_data_hash\":\""))) {
        message += 27;
        strncpy(data_hash, message, DATA_HASH_LENGTH);
        data_hash[DATA_HASH_LENGTH] = '\0';
    } else if (strstr(COLLECTION, "delegates") && (message = strstr(data_buffer, "\"public_key\":\""))) {
        message += 14;
        snprintf(data_hash, sizeof(data_hash), "0000000000000000000000000000000000000000000000000000000000000000%.*s", DATA_HASH_LENGTH - 64, message);
    } else if (strstr(COLLECTION, "statistics")) {
        memset(data_hash, '0', DATA_HASH_LENGTH);
    } else {
        random_string(data_hash, DATA_HASH_LENGTH);
    }

    if (strlen(data_hash) != DATA_HASH_LENGTH) {
        ERROR_PRINT("Invalid data hash length.");
        return XCASH_ERROR;
    }

    // Ensure formatted JSON does not exceed buffer size
    int json_size = snprintf(formatted_json, sizeof(formatted_json), "{\"_id\":\"%s\",%s}", data_hash, data_buffer);
    if (json_size < 0 || json_size >= (int)sizeof(formatted_json)) {
        ERROR_PRINT("Formatted JSON size exceeds buffer limit.");
        return XCASH_ERROR;
    }

    database_client_thread = mongoc_client_pool_pop(database_client_thread_pool);
    if (!database_client_thread) {
        ERROR_PRINT("Failed to get a database connection from the pool.");
        return XCASH_ERROR;
    }

    del_hash(database_client_thread, COLLECTION);

    collection = mongoc_client_get_collection(database_client_thread, DATABASE, COLLECTION);
    if (!collection) {
        ERROR_PRINT("Failed to get collection: %s", COLLECTION);
        mongoc_client_pool_push(database_client_thread_pool, database_client_thread);
        return XCASH_ERROR;
    }

    document = bson_new_from_json((const uint8_t*)formatted_json, -1, &error);
    if (!document) {
        ERROR_PRINT("Could not convert JSON to BSON: %s", error.message);
        mongoc_collection_destroy(collection);
        mongoc_client_pool_push(database_client_thread_pool, database_client_thread);
        return XCASH_ERROR;
    }

    if (!mongoc_collection_insert_one(collection, document, NULL, NULL, &error)) {
        ERROR_PRINT("Could not insert document into collection: %s", error.message);
        bson_destroy(document);
        mongoc_collection_destroy(collection);
        mongoc_client_pool_push(database_client_thread_pool, database_client_thread);
        return XCASH_ERROR;
    }

    bson_destroy(document);
    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(database_client_thread_pool, database_client_thread);

    return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: check_if_database_collection_exist
Description: Checks if a database collection exist
Parameters:
  DATABASE - The database name
  COLLECTION - The collection name
Return: 0 if an error has occured, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int check_if_database_collection_exist(const char* DATABASE, const char* COLLECTION) {
    if (!database_client_thread_pool) {
        ERROR_PRINT("Database client pool is not initialized! Cannot check collections.");
        return XCASH_ERROR;
    }

    mongoc_client_t* database_client_thread = mongoc_client_pool_pop(database_client_thread_pool);
    if (!database_client_thread) {
        ERROR_PRINT("Failed to get a database connection from the pool.");
        return XCASH_ERROR;
    }

    mongoc_database_t* database = mongoc_client_get_database(database_client_thread, DATABASE);
    if (!database) {
        ERROR_PRINT("Failed to get database: %s", DATABASE);
        mongoc_client_pool_push(database_client_thread_pool, database_client_thread);
        return XCASH_ERROR;
    }

    bson_error_t error;
    bool collection_exists = mongoc_database_has_collection(database, COLLECTION, &error);
    
    if (!collection_exists) {
        if (error.message[0] != '\0') {
            ERROR_PRINT("MongoDB error while checking for collection '%s' in database '%s': %s", COLLECTION, DATABASE, error.message);
        } else {
            DEBUG_PRINT("Collection does not exist: %s", COLLECTION);
        }
        mongoc_database_destroy(database);
        mongoc_client_pool_push(database_client_thread_pool, database_client_thread);
        return XCASH_ERROR;
    }

    mongoc_database_destroy(database);
    mongoc_client_pool_push(database_client_thread_pool, database_client_thread);

    return XCASH_OK;
}
