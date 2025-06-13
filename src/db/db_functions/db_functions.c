#include "db_functions.h"

// Unified resource cleanup function
static inline void free_resources(bson_t* document, bson_t* document2, mongoc_collection_t* collection, mongoc_client_t* database_client_thread) {
  if (document) bson_destroy(document);
  if (document2) bson_destroy(document2);
  if (collection) mongoc_collection_destroy(collection);
  if (database_client_thread) mongoc_client_pool_push(database_client_thread_pool, database_client_thread);
}

// Unified error handling function
static inline int handle_error(const char* message, bson_t* document, bson_t* document2, mongoc_collection_t* collection, mongoc_client_t* database_client_thread) {
  if (message != NULL) {
    ERROR_PRINT("%s", message);
  }
  if (document) bson_destroy(document);
  if (document2) bson_destroy(document2);
  if (collection) mongoc_collection_destroy(collection);
  if (database_client_thread) mongoc_client_pool_push(database_client_thread_pool, database_client_thread);
  return XCASH_ERROR;
}

// Helper function to get a temporary connection
static inline mongoc_client_t* get_temporary_connection(void) {
  if (!database_client_thread_pool) {
    ERROR_PRINT("Database client pool is not initialized!");
    return NULL;
  }
  return mongoc_client_pool_pop(database_client_thread_pool);
}

// Helper function to create a BSON document from JSON
static inline bson_t* create_bson_document(const char* DATA, bson_error_t* error) {
  return bson_new_from_json((const uint8_t*)DATA, -1, error);
}

// Function to count documents in a collection based on a filter
int count_documents_in_collection(const char* DATABASE, const char* COLLECTION, const char* DATA) {
  mongoc_client_t* database_client_thread = get_temporary_connection();
  if (!database_client_thread) return -1;

  mongoc_collection_t* collection = mongoc_client_get_collection(database_client_thread, DATABASE, COLLECTION);
  if (!check_if_database_collection_exist(DATABASE, COLLECTION)) {
    DEBUG_PRINT("Collection does not exist: %s", COLLECTION);
    free_resources(NULL, NULL, collection, database_client_thread);
    return 0;
  }

  bson_error_t error;
  bson_t* document = create_bson_document(DATA, &error);
  if (!document) return handle_error("Invalid JSON format", NULL, NULL, collection, database_client_thread);

  int count = (int)mongoc_collection_count_documents(collection, document, NULL, NULL, NULL, &error);
  if (count < 0) {
    ERROR_PRINT("Error counting documents in %s: %s", COLLECTION, error.message);
    count = -1;
  }

  free_resources(document, NULL, collection, database_client_thread);
  return count;
}

// Function to count all documents in a collection
int count_all_documents_in_collection(const char* DATABASE, const char* COLLECTION) {
  mongoc_client_t* database_client_thread = get_temporary_connection();
  if (!database_client_thread) return -1;

  mongoc_collection_t* collection = mongoc_client_get_collection(database_client_thread, DATABASE, COLLECTION);
  if (!check_if_database_collection_exist(DATABASE, COLLECTION)) {
    DEBUG_PRINT("Collection does not exist: %s", COLLECTION);
    free_resources(NULL, NULL, collection, database_client_thread);
    return 0;
  }

  bson_t* filter = bson_new();
  bson_error_t error;
  int count = (int)mongoc_collection_count_documents(collection, filter, NULL, NULL, NULL, &error);
  if (count < 0) ERROR_PRINT("Error counting documents in %s: %s", COLLECTION, error.message);

  bson_destroy(filter);
  free_resources(NULL, NULL, collection, database_client_thread);
  return count;
}

// Function to insert a document into a collection
int insert_document_into_collection_bson(const char* DATABASE, const char* COLLECTION, bson_t* document) {
  if (document == NULL) {
    ERROR_PRINT("BSON document is NULL.");
    return XCASH_ERROR;
  }

  // Extract or create a hash-based _id if needed
  char data_hash[DATA_HASH_LENGTH + 1] = {0};

  // Optionally extract a key from the BSON document for consistent _id (like public_key)
  bson_iter_t iter;
  if (strstr(COLLECTION, "delegates") && bson_iter_init_find(&iter, document, "public_key") && BSON_ITER_HOLDS_UTF8(&iter)) {
    const char* public_key = bson_iter_utf8(&iter, NULL);
    snprintf(data_hash, sizeof(data_hash), "0000000000000000000000000000000000000000000000000000000000000000%.*s", DATA_HASH_LENGTH - 64, public_key);
  } else if (strstr(COLLECTION, "statistics")) {
    memset(data_hash, '0', DATA_HASH_LENGTH);
  } else {
    random_string(data_hash, DATA_HASH_LENGTH);
  }

  if (strlen(data_hash) != DATA_HASH_LENGTH) {
    ERROR_PRINT("Invalid data hash length.");
    return XCASH_ERROR;
  }

  // Add the _id field
  bson_append_utf8(document, "_id", -1, data_hash, -1);

  // Setup MongoDB connection
  mongoc_client_t* database_client_thread = get_temporary_connection();
  if (!database_client_thread) return XCASH_ERROR;

  mongoc_collection_t* collection = mongoc_client_get_collection(database_client_thread, DATABASE, COLLECTION);
  if (!collection) return handle_error("Failed to get collection", NULL, NULL, NULL, database_client_thread);

  bson_error_t error;
  if (!mongoc_collection_insert_one(collection, document, NULL, NULL, &error)) {
    ERROR_PRINT("Could not insert BSON document: %s", error.message);
    free_resources(NULL, NULL, collection, database_client_thread);
    return XCASH_ERROR;
  }

  free_resources(NULL, NULL, collection, database_client_thread);
  return XCASH_OK;
}

/*-----------------------------------------------------------------------------------------------------------
Name: check_if_database_collection_exist
Description: Checks if a database collection exists.
Parameters:
  DATABASE - The database name.
  COLLECTION - The collection name.
Return: 0 if an error has occurred or collection does not exist, 1 if successful.
-----------------------------------------------------------------------------------------------------------*/
int check_if_database_collection_exist(const char* DATABASE, const char* COLLECTION) {
  mongoc_client_t* database_client_thread = get_temporary_connection();
  if (!database_client_thread) return XCASH_ERROR;

  mongoc_database_t* database = mongoc_client_get_database(database_client_thread, DATABASE);
  if (!database) return handle_error("Failed to get database", NULL, NULL, NULL, database_client_thread);

  bson_error_t error;
  bool collection_exists = mongoc_database_has_collection(database, COLLECTION, &error);

  mongoc_database_destroy(database);
  mongoc_client_pool_push(database_client_thread_pool, database_client_thread);

  if (!collection_exists) {
    if (error.message[0] != '\0') {
      ERROR_PRINT("MongoDB error: %s", error.message);
    } else {
      DEBUG_PRINT("Collection does not exist: %s", COLLECTION);
    }
    return XCASH_ERROR;
  }
  return XCASH_OK;
}

// Function to read a document from collection
int read_document_from_collection(const char* DATABASE, const char* COLLECTION, const char* DATA, char* result) {
  const bson_t* current_document;
  mongoc_client_t* database_client_thread = get_temporary_connection();
  if (!database_client_thread) return XCASH_ERROR;

  mongoc_collection_t* collection = mongoc_client_get_collection(database_client_thread, DATABASE, COLLECTION);
  if (!check_if_database_collection_exist(DATABASE, COLLECTION)) {
    free_resources(NULL, NULL, collection, database_client_thread);
    return XCASH_ERROR;
  }

  bson_error_t error;
  bson_t* document = create_bson_document(DATA, &error);
  if (!document) return handle_error("Invalid JSON format", NULL, NULL, collection, database_client_thread);

  mongoc_cursor_t* document_settings = mongoc_collection_find_with_opts(collection, document, NULL, NULL);
  char* message = NULL;
  int count = 0;

  while (mongoc_cursor_next(document_settings, &current_document)) {
    message = bson_as_canonical_extended_json(current_document, NULL);
    strncpy(result, message, strlen(message));
    bson_free(message);
    count = 1;
  }

  mongoc_cursor_destroy(document_settings);
  if (count != 1) return handle_error("Document not found", document, NULL, collection, database_client_thread);

  free_resources(document, NULL, collection, database_client_thread);
  return XCASH_OK;
}

// Function to read a specific field from a document
int read_document_field_from_collection(const char* DATABASE, const char* COLLECTION, const char* DATA, const char* FIELD_NAME, char* result, size_t result_size) {
  if (!DATABASE || !COLLECTION || !DATA || !FIELD_NAME || !result || result_size == 0) {
    fprintf(stderr, "Invalid input parameters.\n");
    return XCASH_ERROR;
  }

  const bson_t* current_document;
  mongoc_client_t* database_client_thread = get_temporary_connection();
  if (!database_client_thread) return XCASH_ERROR;

  mongoc_collection_t* collection = mongoc_client_get_collection(database_client_thread, DATABASE, COLLECTION);
  if (!check_if_database_collection_exist(DATABASE, COLLECTION)) {
    free_resources(NULL, NULL, collection, database_client_thread);
    return XCASH_ERROR;
  }

  bson_error_t error;
  bson_t* document = create_bson_document(DATA, &error);
  if (!document) {
    handle_error("Invalid JSON format", NULL, NULL, collection, database_client_thread);
    return XCASH_ERROR;
  }

  mongoc_cursor_t* document_settings = mongoc_collection_find_with_opts(collection, document, NULL, NULL);
  int found = 0;

  while (mongoc_cursor_next(document_settings, &current_document)) {
    bson_iter_t iter;
    if (bson_iter_init_find(&iter, current_document, FIELD_NAME)) {
      const char* value = bson_iter_utf8(&iter, NULL);
      if (value) {
        strncpy(result, value, result_size - 1);
        result[result_size - 1] = '\0';  // Ensure null-termination
        found = 1;
        break;
      }
    }
  }

  mongoc_cursor_destroy(document_settings);
  bson_destroy(document);
  free_resources(NULL, NULL, collection, database_client_thread);

  if (!found) {
    ERROR_PRINT("Field '%s' not found in document.", FIELD_NAME);
    return XCASH_ERROR;
  }

  return XCASH_OK;
}











// Function to parse JSON data
int database_document_parse_json_data(const char* DATA, struct database_document_fields* result) {
  if (!strstr(DATA, ",")) {
    ERROR_PRINT("Invalid JSON data");
    return XCASH_ERROR;
  }

  char* data2 = strstr(DATA, ",") + 3;
  char* data3 = strstr(data2, "\"");
  if (!data3) {
    ERROR_PRINT("Invalid JSON format");
    return XCASH_ERROR;
  }

  strncpy(result->item[0], data2, data3 - data2);
  for (size_t count = 0; count < result->count; count++) {
    data2 = data3 + 5;
    data3 = strstr(data2, "\"");
    if (!data3) {
      ERROR_PRINT("Invalid JSON format");
      return XCASH_ERROR;
    }
    strncpy(result->value[count], data2, data3 - data2);

    if (count + 1 != result->count) {
      data2 = data3 + 4;
      data3 = strstr(data2, "\"");
      if (!data3) {
        ERROR_PRINT("Invalid JSON format");
        return XCASH_ERROR;
      }
      strncpy(result->item[count + 1], data2, data3 - data2);
    }
  }
  return XCASH_OK;
}

// Function to parse multiple documents from JSON
int database_multiple_documents_parse_json_data(const char* data, struct database_multiple_documents_fields* result, const int document_count) {
  char* data2 = strstr(data, ",") + 3;
  char* data3 = strstr(data2, "\"");
  if (!data2 || !data3) {
    ERROR_PRINT("Invalid JSON format");
    return XCASH_ERROR;
  }

  strncpy(result->item[document_count][0], data2, data3 - data2);
  for (size_t count = 0; count < result->database_fields_count; count++) {
    data2 = data3 + 5;
    data3 = strstr(data2, "\"");
    if (!data3) {
      ERROR_PRINT("Invalid JSON format");
      return XCASH_ERROR;
    }
    strncpy(result->value[document_count][count], data2, data3 - data2);

    if (count + 1 != result->database_fields_count) {
      data2 = data3 + 4;
      data3 = strstr(data2, "\"");
      if (!data3) {
        ERROR_PRINT("Invalid JSON format");
        return XCASH_ERROR;
      }
      strncpy(result->item[document_count][count + 1], data2, data3 - data2);
    }
  }
  return XCASH_OK;
}

// Function to read all fields from a document
int read_document_all_fields_from_collection(const char* DATABASE, const char* COLLECTION, const char* DATA, struct database_document_fields* result) {
  mongoc_client_t* database_client_thread = get_temporary_connection();
  if (!database_client_thread) return XCASH_ERROR;

  mongoc_collection_t* collection = mongoc_client_get_collection(database_client_thread, DATABASE, COLLECTION);
  if (!check_if_database_collection_exist(DATABASE, COLLECTION)) {
    free_resources(NULL, NULL, collection, database_client_thread);
    return XCASH_ERROR;
  }

  bson_error_t error;
  bson_t* document = create_bson_document(DATA, &error);
  if (!document) return handle_error("Invalid JSON format", NULL, NULL, collection, database_client_thread);

  mongoc_cursor_t* document_settings = mongoc_collection_find_with_opts(collection, document, NULL, NULL);
  const bson_t* current_document;
  char* message = NULL;
  int count = 0;

  while (mongoc_cursor_next(document_settings, &current_document)) {
    message = bson_as_canonical_extended_json(current_document, NULL);
    if (!message) {
      handle_error("Failed to convert BSON to JSON", document, NULL, collection, database_client_thread);
    }
    if (database_document_parse_json_data(message, result) == XCASH_ERROR) {
      bson_free(message);
      return handle_error("JSON parsing failed", document, NULL, collection, database_client_thread);
    }
    bson_free(message);
    count = 1;
  }

  mongoc_cursor_destroy(document_settings);
  if (count != 1) return handle_error("Document not found", document, NULL, collection, database_client_thread);

  free_resources(document, NULL, collection, database_client_thread);
  return XCASH_OK;
}

// Function to read multiple documents from a collection
int read_multiple_documents_all_fields_from_collection(const char* DATABASE, const char* COLLECTION, const char* DATA, struct database_multiple_documents_fields* result, const size_t DOCUMENT_COUNT_START, const size_t DOCUMENT_COUNT_TOTAL, const int DOCUMENT_OPTIONS, const char* DOCUMENT_OPTIONS_DATA) {
  mongoc_client_t* database_client_thread = get_temporary_connection();
  if (!database_client_thread) return XCASH_ERROR;

  mongoc_collection_t* collection = mongoc_client_get_collection(database_client_thread, DATABASE, COLLECTION);
  if (!check_if_database_collection_exist(DATABASE, COLLECTION)) {
    free_resources(NULL, NULL, collection, database_client_thread);
    return XCASH_ERROR;
  }

  bson_error_t error;
  bson_t* document = create_bson_document(DATA, &error);
  if (!document) return handle_error("Invalid JSON format", NULL, NULL, collection, database_client_thread);

  bson_t* document_options = NULL;
  if (DOCUMENT_OPTIONS == 1) {
    document_options = BCON_NEW("sort", "{", DOCUMENT_OPTIONS_DATA, BCON_INT32(-1), "}");
  }

  mongoc_cursor_t* document_settings = mongoc_collection_find_with_opts(collection, document, document_options, NULL);
  const bson_t* current_document;
  char* message = NULL;
  size_t count = 1;
  size_t counter = 0;

  while (mongoc_cursor_next(document_settings, &current_document)) {
    if (count >= DOCUMENT_COUNT_START) {
      message = bson_as_canonical_extended_json(current_document, NULL);
      if (database_multiple_documents_parse_json_data(message, result, (int)counter) == XCASH_ERROR) {
        bson_free(message);
        return handle_error("JSON parsing failed", document, NULL, collection, database_client_thread);
      }
      bson_free(message);
      counter++;
      result->document_count++;
      if (counter == DOCUMENT_COUNT_TOTAL) break;
    }
    count++;
  }

  bson_destroy(document_options);
  mongoc_cursor_destroy(document_settings);
  if (counter == 0) return handle_error("No documents found", document, NULL, collection, database_client_thread);

  free_resources(document, NULL, collection, database_client_thread);
  return XCASH_OK;
}

// Function to update a single document in a collection
int update_document_from_collection_bson(const char* DATABASE, const char* COLLECTION, const bson_t* filter, const bson_t* update_fields) {

  mongoc_client_t* database_client_thread = get_temporary_connection();
  if (!database_client_thread) return XCASH_ERROR;

  mongoc_collection_t* collection = mongoc_client_get_collection(database_client_thread, DATABASE, COLLECTION);
  if (!check_if_database_collection_exist(DATABASE, COLLECTION)) {
    return handle_error("Collection does not exist", NULL, NULL, collection, database_client_thread);
  }

  bson_error_t error;

  // Wrap update_fields in a $set
  bson_t update_doc;
  bson_init(&update_doc);
  BSON_APPEND_DOCUMENT(&update_doc, "$set", update_fields);

  if (!mongoc_collection_update_one(collection, filter, &update_doc, NULL, NULL, &error)) {
    bson_destroy(&update_doc);
    return handle_error("Failed to update document", NULL, NULL, collection, database_client_thread);
  }

  bson_destroy(&update_doc);
  mongoc_collection_destroy(collection);
  mongoc_client_destroy(database_client_thread);
  return XCASH_OK;
}





// Function to update multiple documents in a collection
int update_multiple_documents_from_collection(const char* DATABASE, const char* COLLECTION, const char* DATA, const char* FIELD_NAME_AND_DATA) {
  if (strlen(FIELD_NAME_AND_DATA) > MAXIMUM_DATABASE_WRITE_SIZE) {
    ERROR_PRINT("Data exceeds maximum write size.");
    return XCASH_ERROR;
  }

  mongoc_client_t* database_client_thread = get_temporary_connection();
  if (!database_client_thread) return XCASH_ERROR;

  mongoc_collection_t* collection = mongoc_client_get_collection(database_client_thread, DATABASE, COLLECTION);
  if (!check_if_database_collection_exist(DATABASE, COLLECTION)) {
    return handle_error("Collection does not exist", NULL, NULL, collection, database_client_thread);
  }

  bson_error_t error;
  bson_t* update = create_bson_document(DATA, &error);
  if (!update) return handle_error("Invalid JSON format", NULL, NULL, collection, database_client_thread);

  char data2[BUFFER_SIZE];
  snprintf(data2, sizeof(data2), "{\"$set\":%s}", FIELD_NAME_AND_DATA);

  bson_t* update_settings = create_bson_document(data2, &error);
  if (!update_settings) return handle_error("Invalid update settings format", update, NULL, collection, database_client_thread);

  if (!mongoc_collection_update_many(collection, update, update_settings, NULL, NULL, &error)) {
    return handle_error("Failed to update documents", update, update_settings, collection, database_client_thread);
  }

  free_resources(update, update_settings, collection, database_client_thread);
  return XCASH_OK;
}

// Function to update all documents in a collection
int update_all_documents_from_collection(const char* DATABASE, const char* COLLECTION, const char* DATA) {
  if (strlen(DATA) > MAXIMUM_DATABASE_WRITE_SIZE) {
    ERROR_PRINT("Data exceeds maximum write size.");
    return XCASH_ERROR;
  }

  mongoc_client_t* database_client_thread = get_temporary_connection();
  if (!database_client_thread) return XCASH_ERROR;

  mongoc_collection_t* collection = mongoc_client_get_collection(database_client_thread, DATABASE, COLLECTION);
  if (!check_if_database_collection_exist(DATABASE, COLLECTION)) {
    return handle_error("Collection does not exist", NULL, NULL, collection, database_client_thread);
  }

  bson_error_t error;
  bson_t* update = bson_new();  // Empty BSON to match all documents
  if (!update) return handle_error("Failed to create empty BSON", NULL, NULL, collection, database_client_thread);

  char data2[BUFFER_SIZE];
  snprintf(data2, sizeof(data2), "{\"$set\":%s}", DATA);

  bson_t* update_settings = create_bson_document(data2, &error);
  if (!update_settings) return handle_error("Invalid update settings format", update, NULL, collection, database_client_thread);

  if (!mongoc_collection_update_many(collection, update, update_settings, NULL, NULL, &error)) {
    return handle_error("Failed to update all documents", update, update_settings, collection, database_client_thread);
  }

  free_resources(update, update_settings, collection, database_client_thread);
  return XCASH_OK;
}

// Function to delete a document from a collection
int delete_document_from_collection(const char* DATABASE, const char* COLLECTION, const char* DATA) {
  mongoc_client_t* database_client_thread = get_temporary_connection();
  if (!database_client_thread) return XCASH_ERROR;

  mongoc_collection_t* collection = mongoc_client_get_collection(database_client_thread, DATABASE, COLLECTION);
  if (!check_if_database_collection_exist(DATABASE, COLLECTION)) {
    return handle_error("Collection does not exist", NULL, NULL, collection, database_client_thread);
  }

  bson_error_t error;
  bson_t* document = create_bson_document(DATA, &error);
  if (!document) return handle_error("Invalid JSON format", NULL, NULL, collection, database_client_thread);

  if (!mongoc_collection_delete_one(collection, document, NULL, NULL, &error)) {
    return handle_error("Failed to delete document", document, NULL, collection, database_client_thread);
  }

  free_resources(document, NULL, collection, database_client_thread);
  return XCASH_OK;
}

// Function to delete a collection from a database
int delete_collection_from_database(const char* DATABASE, const char* COLLECTION) {
  mongoc_client_t* database_client_thread = get_temporary_connection();
  if (!database_client_thread) return XCASH_ERROR;

  mongoc_collection_t* collection = mongoc_client_get_collection(database_client_thread, DATABASE, COLLECTION);
  if (!check_if_database_collection_exist(DATABASE, COLLECTION)) {
    return handle_error("Collection does not exist", NULL, NULL, collection, database_client_thread);
  }

  bson_error_t error;
  if (!mongoc_collection_drop(collection, &error)) {
    return handle_error("Failed to delete collection", NULL, NULL, collection, database_client_thread);
  }

  free_resources(NULL, NULL, collection, database_client_thread);
  return XCASH_OK;
}

// Function to delete a database
int delete_database(const char* DATABASE) {
  mongoc_client_t* database_client_thread = get_temporary_connection();
  if (!database_client_thread) return XCASH_ERROR;

  mongoc_database_t* database = mongoc_client_get_database(database_client_thread, DATABASE);
  if (!database) return handle_error("Failed to get database", NULL, NULL, NULL, database_client_thread);

  bson_error_t error;
  if (!mongoc_database_drop(database, &error)) {
    mongoc_database_destroy(database);
    return handle_error("Failed to delete database", NULL, NULL, NULL, database_client_thread);
  }

  drop_all_hashes(database_client_thread);
  mongoc_database_destroy(database);
  mongoc_client_pool_push(database_client_thread_pool, database_client_thread);

  return XCASH_OK;
}

// Function to get database collection size
size_t get_database_collection_size(const char* DATABASE, const char* COLLECTION) {
    if (!check_if_database_collection_exist(DATABASE, COLLECTION)) return 0;

    mongoc_client_t* database_client_thread = get_temporary_connection();
    if (!database_client_thread) return 0;

    mongoc_collection_t* collection = mongoc_client_get_collection(database_client_thread, DATABASE, COLLECTION);
    bson_t* command = BCON_NEW("collStats", BCON_UTF8(COLLECTION));
    bson_t document;
    bson_error_t error;
    size_t size = 0;

    if (!mongoc_collection_command_simple(collection, command, NULL, &document, &error)) {
        handle_error("Failed to get collection stats", command, NULL, collection, database_client_thread);
        return 0;
    }

    bson_iter_t iter;
    if (bson_iter_init_find(&iter, &document, "size") && BSON_ITER_HOLDS_DOUBLE(&iter)) {
        size = (size_t)bson_iter_double(&iter);
    } else if (bson_iter_init_find(&iter, &document, "size") && BSON_ITER_HOLDS_INT64(&iter)) {
        size = (size_t)bson_iter_int64(&iter);
    } else if (bson_iter_init_find(&iter, &document, "size") && BSON_ITER_HOLDS_INT32(&iter)) {
        size = (size_t)bson_iter_int32(&iter);
    }

    free_resources(command, NULL, collection, database_client_thread);
    return size;
}

/**
 * @brief Retrieves the MD5 hash of the specified MongoDB collection.
 * 
 * @param data_hash Buffer to store the resulting MD5 hash.
 * @param DATABASE Name of the database.
 * @param COLLECTION Name of the collection.
 * @return int Returns 1 if successful, 0 if an error occurs.
 */
int get_database_data_hash(char *data_hash, const char *DATABASE, const char *COLLECTION)
{
    if (!data_hash || !DATABASE || !COLLECTION) {
        return handle_error("Invalid arguments passed to get_database_data_hash.", NULL, NULL, NULL, NULL);
    }

    char data[BUFFER_SIZE] = {0};
    char data2[SMALL_BUFFER_SIZE] = {0};
    char *message = NULL;
    char *message2 = NULL;
    size_t count = 0;
    size_t count2 = 0;
    mongoc_client_t *database_client_thread = NULL;
    mongoc_collection_t *collection = NULL;
    bson_error_t error;
    bson_t *command = NULL;
    bson_t document;

    // Get a temporary MongoDB connection
    database_client_thread = get_temporary_connection();
    if (!database_client_thread) {
        return XCASH_ERROR;
    }

    // Try to get a cached result first
    int cache_request_result = get_multi_hash(database_client_thread, COLLECTION, data_hash);
    mongoc_client_pool_push(database_client_thread_pool, database_client_thread);
    if (cache_request_result >= 0) {
        return XCASH_OK;  // Cache hit
    }

    // Set the collection
    collection = mongoc_client_get_collection(database_client_thread, DATABASE, COLLECTION);
    if (!collection) {
        return handle_error("Failed to get collection.", NULL, NULL, collection, database_client_thread);
    }

    // Build query to fetch all documents
    strncat(data, "{\"dbHash\":1,\"collections\":[\"", sizeof(data) - strlen(data) - 1);

    if (strncmp(COLLECTION, "reserve_bytes", BUFFER_SIZE) == 0) {
      if (get_reserve_bytes_database(&count2)) {
        for (count = 1; count <= count2; ++count) {
          snprintf(data + strlen(data), sizeof(data) - strlen(data), "reserve_bytes_%zu", count);
          if (count != count2) strncat(data, "\",\"", sizeof(data) - strlen(data) - 1);
        }
      } else {
        return XCASH_ERROR;
      }
    } else if (strncmp(COLLECTION, "reserve_proofs", BUFFER_SIZE) == 0) {
        for (count = 1; count <= TOTAL_RESERVE_PROOFS_DATABASES; ++count) {
            snprintf(data + strlen(data), sizeof(data) - strlen(data), "reserve_proofs_%zu", count);
            snprintf(data2, sizeof(data2), "reserve_proofs_%zu", count + 1);
            if (check_if_database_collection_exist(DATABASE_NAME, data2) == 1) {
                strncat(data, "\",\"", sizeof(data) - strlen(data) - 1);
            } else {
                break;
            }
        }
    } else {
        strncat(data, COLLECTION, sizeof(data) - strlen(data) - 1);
    }

    strncat(data, "\"]}", sizeof(data) - strlen(data) - 1);

    command = strncmp(COLLECTION, "ALL", 3) == 0 ? BCON_NEW("dbHash", BCON_INT32(1)) : create_bson_document(data, &error);
    memset(data, 0, sizeof(data));

    if (!command) {
        return handle_error("Failed to create BSON command.", NULL, command, collection, database_client_thread);
    }

    if (!mongoc_collection_command_simple(collection, command, NULL, &document, &error)) {
        return handle_error("Command execution failed.", &document, command, collection, database_client_thread);
    }

    if (!(message = bson_as_legacy_extended_json(&document, NULL))) {
        return handle_error("Failed to convert BSON to JSON.", &document, command, collection, database_client_thread);
    }

    strncpy(data, message, sizeof(data) - 1);
    bson_free(message);

    if (!(message2 = strstr(data, "\"md5\""))) {
        return handle_error("MD5 field not found in response.", &document, command, collection, database_client_thread);
    }

    memset(data_hash, '0', 96);
    data_hash[128] = '\0';  // Ensure null-termination
    strncpy(data_hash + 96, message2 + 9, 32);

    free_resources(&document, command, collection, database_client_thread);
    return XCASH_OK;
}

// Function to get database data
int get_database_data(char* database_data, const char* DATABASE, const char* COLLECTION) {
  mongoc_client_t* database_client_thread = get_temporary_connection();
  if (!database_client_thread) return XCASH_ERROR;

  mongoc_collection_t* collection = mongoc_client_get_collection(database_client_thread, DATABASE, COLLECTION);
  if (!check_if_database_collection_exist(DATABASE, COLLECTION)) {
    return handle_error("Collection does not exist", NULL, NULL, collection, database_client_thread);
  }

  bson_t* document = bson_new();
  bson_t* document_options = BCON_NEW("sort", "{", "_id", BCON_INT32(1), "}");
  mongoc_cursor_t* cursor = mongoc_collection_find_with_opts(collection, document, document_options, NULL);
  const bson_t* current_document;
  char* message;
  int count = 0;

  memset(database_data, 0, MAXIMUM_BUFFER_SIZE);
  while (mongoc_cursor_next(cursor, &current_document)) {
    message = bson_as_canonical_extended_json(current_document, NULL);
    if (count == 0)
      strcat(database_data, "{");
    else
      strcat(database_data, ",{");
    strncat(database_data, message + 142, strlen(message) - 142);
    bson_free(message);
    count++;
  }

  if (count == 0) strcpy(database_data, DATABASE_EMPTY_STRING);
  free_resources(document, document_options, collection, database_client_thread);
  if (cursor) mongoc_cursor_destroy(cursor);
  return XCASH_OK;
}

/**
 * @brief Retrieves a specific field's data from the "hashes" collection.
 * 
 * @param client MongoDB client connection.
 * @param db_name Name of the database.
 * @param field_name Name of the field to retrieve.
 * @param data Buffer to store the retrieved data.
 * @return int Returns XCASH_OK (1) if successful, XCASH_ERROR (0) if an error occurs.
 */
int get_data(mongoc_client_t *client, const char *db_name, const char *field_name, char *data)
{
    if (!client || !db_name || !field_name || !data) {
        return handle_error("Invalid arguments passed to get_data.", NULL, NULL, NULL, NULL);
    }

    bson_t *query = NULL;
    bson_t *opts = NULL;
    mongoc_collection_t *collection = NULL;
    mongoc_cursor_t *cursor = NULL;
    const bson_t *doc = NULL;
    bson_iter_t iter;
    bson_iter_t field;
    int result = XCASH_ERROR;
    uint32_t len = 0;

    // Get collection
    collection = mongoc_client_get_collection(client, DATABASE_NAME, "hashes");
    if (!collection) {
        return handle_error("Failed to get collection: hashes.", NULL, NULL, collection, NULL);
    }

    // Create query to find documents by db_name
    query = BCON_NEW("db_name", BCON_UTF8(db_name));
    if (!query) {
        return handle_error("Failed to create query.", query, NULL, collection, NULL);
    }

    // Create options for projection (only the specified field, exclude _id)
    opts = BCON_NEW("projection", "{",
                    field_name, BCON_BOOL(true),
                    "_id", BCON_BOOL(false),
                    "}");
    if (!opts) {
        return handle_error("Failed to create options.", query, opts, collection, NULL);
    }

    // Execute query and manage the cursor locally
    cursor = mongoc_collection_find_with_opts(collection, query, opts, NULL);
    if (!cursor) {
        return handle_error("Failed to execute query.", query, opts, collection, NULL);
    }

    // Process query results
    while (mongoc_cursor_next(cursor, &doc)) {
        if (bson_iter_init(&iter, doc) &&
            bson_iter_find_descendant(&iter, field_name, &field) &&
            BSON_ITER_HOLDS_UTF8(&field))
        {
            const char *retrieved_data = bson_iter_utf8(&field, &len);
            if (len < BUFFER_SIZE) {  // Prevent buffer overflow
                strncpy(data, retrieved_data, len);
                data[len] = '\0';  // Ensure null termination
                result = XCASH_OK;
            } else {
                ERROR_PRINT("Retrieved data exceeds buffer size.");
                result = XCASH_ERROR;
                break;
            }
        }
    }

    // Check for cursor errors
    if (mongoc_cursor_error(cursor, NULL)) {
      mongoc_cursor_destroy(cursor);
      return handle_error("Cursor error occurred while fetching data.", query, opts, collection, NULL);
    }

    // Cleanup cursor explicitly before using free_resources
    mongoc_cursor_destroy(cursor);

    // Cleanup other resources
    free_resources(query, opts, collection, NULL);

    return result;
}