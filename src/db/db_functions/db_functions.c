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

    if (strlen(data_hash) != DATA_HASH_LENGTH) {
      ERROR_PRINT("Invalid data hash length.");
      return XCASH_ERROR;
    }

    // Add the _id field
    bson_append_utf8(document, "_id", -1, data_hash, -1);

  }

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
  (void)result;
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
    result = message;
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
  bson_t update_doc;
  bson_init(&update_doc);
  BSON_APPEND_DOCUMENT(&update_doc, "$set", update_fields);

  if (!mongoc_collection_update_one(collection, filter, &update_doc, NULL, NULL, &error)) {
    bson_destroy(&update_doc);
    return handle_error("Failed to update document", NULL, NULL, collection, database_client_thread);
  }

  bson_destroy(&update_doc);
  mongoc_collection_destroy(collection);
  mongoc_client_pool_push(database_client_thread_pool, database_client_thread);
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

int get_statistics_totals_by_public_key(
    const char* public_key,
    uint64_t* block_verifier_total_rounds,
    uint64_t* block_verifier_online_total_rounds,
    uint64_t* block_producer_total_rounds)
{
  // Defensive default
  *block_verifier_total_rounds = 0;
  *block_verifier_online_total_rounds = 0;
  *block_producer_total_rounds = 0;

  // Get MongoDB client
  mongoc_client_t* database_client_thread = get_temporary_connection();
  if (!database_client_thread) return XCASH_ERROR;

  // Get collection
  mongoc_collection_t* collection = mongoc_client_get_collection(
      database_client_thread, DATABASE_NAME, DB_COLLECTION_STATISTICS);

  // Build query using safe stack-based BSON
  bson_t query;
  bson_init(&query);
  BSON_APPEND_UTF8(&query, "public_key", public_key);

  mongoc_cursor_t* cursor = mongoc_collection_find_with_opts(collection, &query, NULL, NULL);

  const bson_t* doc;
  bson_iter_t iter;
  bool success = false;

  // Search the first matched document
  if (mongoc_cursor_next(cursor, &doc)) {
    if (bson_iter_init(&iter, doc)) {
      while (bson_iter_next(&iter)) {
        const char* key = bson_iter_key(&iter);

        if ((strcmp(key, "block_verifier_total_rounds") == 0) &&
            BSON_ITER_HOLDS_NUMBER(&iter)) {
          *block_verifier_total_rounds = bson_iter_as_int64(&iter);
        } else if ((strcmp(key, "block_verifier_online_total_rounds") == 0) &&
                   BSON_ITER_HOLDS_NUMBER(&iter)) {
          *block_verifier_online_total_rounds = bson_iter_as_int64(&iter);
        } else if ((strcmp(key, "block_producer_total_rounds") == 0) &&
                   BSON_ITER_HOLDS_NUMBER(&iter)) {
          *block_producer_total_rounds = bson_iter_as_int64(&iter);
        }
      }
      success = true;
    }
  }

  // Clean up
  bson_destroy(&query);
  mongoc_cursor_destroy(cursor);
  mongoc_collection_destroy(collection);

  if (database_client_thread)
    mongoc_client_pool_push(database_client_thread_pool, database_client_thread);

  return success ? XCASH_OK : XCASH_ERROR;
}

bool is_replica_set_ready(void) {
  bson_t reply;
  bson_error_t error;
  bool is_ready = false;

  mongoc_client_t *client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) return false;

  bson_t *cmd = BCON_NEW("replSetGetStatus", BCON_INT32(1));
  if (mongoc_client_command_simple(client, "admin", cmd, NULL, &reply, &error)) {
    bson_iter_t iter;
    if (bson_iter_init_find(&iter, &reply, "myState")) {
      int32_t state = bson_iter_int32(&iter);
      // MongoDB states: 1 = PRIMARY, 2 = SECONDARY
      if (state == 1 || state == 2) {
        is_ready = true;
      }
    }
  } else {
    WARNING_PRINT("Could not run replSetGetStatus: %s", error.message);
  }

  bson_destroy(&reply);
  bson_destroy(cmd);
  mongoc_client_pool_push(database_client_thread_pool, client);
  return is_ready;
}

bool add_indexes(void) {
  bson_error_t err;
  bool ok = true;

  mongoc_client_t *client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) return false;

  /* =========================
     STATISTICS COLLECTION
     ========================= */
  {
    mongoc_collection_t *coll =
        mongoc_client_get_collection(client, DATABASE_NAME, DB_COLLECTION_STATISTICS);

    // models
    bson_t keys1, opts1; bson_init(&keys1); bson_init(&opts1);
    BSON_APPEND_INT32(&keys1, "public_key", 1);
    BSON_APPEND_UTF8(&opts1, "name", "uniq_public_key");
    BSON_APPEND_BOOL(&opts1, "unique", true);
    mongoc_index_model_t *m1 = mongoc_index_model_new(&keys1, &opts1);

    bson_t keys2, opts2; bson_init(&keys2); bson_init(&opts2);
    BSON_APPEND_INT32(&keys2, "public_key", 1);
    BSON_APPEND_INT32(&keys2, "last_counted_block", 1);
    BSON_APPEND_UTF8(&opts2, "name", "idx_public_key_last_counted_block");
    mongoc_index_model_t *m2 = mongoc_index_model_new(&keys2, &opts2);

    mongoc_index_model_t *models[2] = { m1, m2 };

    // createIndexes opts
    bson_t create_opts; bson_init(&create_opts);
    BSON_APPEND_UTF8(&create_opts, "commitQuorum", "majority");
    BSON_APPEND_INT32(&create_opts, "maxTimeMS", 15000);

    bson_t reply; bson_init(&reply);
    if (!mongoc_collection_create_indexes_with_opts(coll, models, 2, &create_opts, &reply, &err)) {
      ok = false;
      char *json = bson_as_canonical_extended_json(&reply, NULL);
      fprintf(stderr, "[indexes] statistics failed: %s\nDetails: %s\n",
              err.message, json ? json : "(no reply)");
      if (json) bson_free(json);
    }

    // cleanup
    bson_destroy(&reply);
    bson_destroy(&create_opts);
    mongoc_index_model_destroy(m2);
    mongoc_index_model_destroy(m1);
    bson_destroy(&opts2); bson_destroy(&keys2);
    bson_destroy(&opts1); bson_destroy(&keys1);
    mongoc_collection_destroy(coll);
  }

  /* =========================
     DELEGATES COLLECTION
     ========================= */
  {
    mongoc_collection_t *coll =
        mongoc_client_get_collection(client, DATABASE_NAME, DB_COLLECTION_DELEGATES);

    // 1) unique public_address
    bson_t k1, o1; bson_init(&k1); bson_init(&o1);
    BSON_APPEND_INT32(&k1, "public_address", 1);
    BSON_APPEND_UTF8(&o1, "name", "uniq_public_address");
    BSON_APPEND_BOOL(&o1, "unique", true);
    mongoc_index_model_t *m1 = mongoc_index_model_new(&k1, &o1);

    // 2) unique public_key
    bson_t k2, o2; bson_init(&k2); bson_init(&o2);
    BSON_APPEND_INT32(&k2, "public_key", 1);
    BSON_APPEND_UTF8(&o2, "name", "uniq_public_key");
    BSON_APPEND_BOOL(&o2, "unique", true);
    mongoc_index_model_t *m2 = mongoc_index_model_new(&k2, &o2);

    // 3) unique delegate_name (case-insensitive via collation)
    bson_t k3, o3, coll3; bson_init(&k3); bson_init(&o3); bson_init(&coll3);
    BSON_APPEND_INT32(&k3, "delegate_name", 1);
    BSON_APPEND_UTF8(&o3, "name", "uniq_delegate_name_ci");
    BSON_APPEND_BOOL(&o3, "unique", true);
    BSON_APPEND_UTF8(&coll3, "locale", "en");
    BSON_APPEND_INT32(&coll3, "strength", 2); // case-insensitive, diacritics-insensitive
    BSON_APPEND_DOCUMENT(&o3, "collation", &coll3);
    mongoc_index_model_t *m3 = mongoc_index_model_new(&k3, &o3);

    // 4) unique IP_address (only if you truly want one delegate per IP/host)
    bson_t k4, o4; bson_init(&k4); bson_init(&o4);
    BSON_APPEND_INT32(&k4, "IP_address", 1);
    BSON_APPEND_UTF8(&o4, "name", "uniq_IP_address");
    BSON_APPEND_BOOL(&o4, "unique", true);
    mongoc_index_model_t *m4 = mongoc_index_model_new(&k4, &o4);

    mongoc_index_model_t *models[] = { m1, m2, m3, m4 };

    bson_t create_opts; bson_init(&create_opts);
    BSON_APPEND_UTF8(&create_opts, "commitQuorum", "majority");
    BSON_APPEND_INT32(&create_opts, "maxTimeMS", 15000);

    bson_t reply; bson_init(&reply);
    if (!mongoc_collection_create_indexes_with_opts(coll, models, 4, &create_opts, &reply, &err)) {
      ok = false;
      char *json = bson_as_canonical_extended_json(&reply, NULL);
      fprintf(stderr, "[indexes] delegates failed: %s\nDetails: %s\n",
              err.message, json ? json : "(no reply)");
      if (json) bson_free(json);
    }

    // cleanup
    bson_destroy(&reply);
    bson_destroy(&create_opts);
    mongoc_index_model_destroy(m4); mongoc_index_model_destroy(m3);
    mongoc_index_model_destroy(m2); mongoc_index_model_destroy(m1);
    bson_destroy(&o4); bson_destroy(&k4);
    bson_destroy(&coll3); bson_destroy(&o3); bson_destroy(&k3);
    bson_destroy(&o2); bson_destroy(&k2);
    bson_destroy(&o1); bson_destroy(&k1);
    mongoc_collection_destroy(coll);
  }

  /* =========================
     CONSENSUS_ROUNDS COLLECTION
     ========================= */
  {
    const int32_t TTL_SEC = 60 * 60 * 24 * 365;  // 365 days

    mongoc_database_t* db = mongoc_client_get_database(client, DATABASE_NAME);

    // Ensure collection exists (ignore NamespaceExists = 48)
    bson_t c_opts;
    bson_init(&c_opts);
    if (!mongoc_database_create_collection(db, DB_COLLECTION_ROUNDS, &c_opts, &err)) {
      if (err.code != 48) {
        ok = false;
        WARNING_PRINT("[indexes] create_collection %s failed (domain=%d code=%d): %s",
                      DB_COLLECTION_ROUNDS, err.domain, err.code, err.message);
      }
    }
    bson_destroy(&c_opts);

    mongoc_collection_t* coll =
        mongoc_client_get_collection(client, DATABASE_NAME, DB_COLLECTION_ROUNDS);
    if (!coll) {
      ok = false;
      WARNING_PRINT("[indexes] get_collection %s failed", DB_COLLECTION_ROUNDS);
      mongoc_database_destroy(db);
      // return or goto as fits your flow
    } else {
      // 1) unique block_height
      bson_t rk1, ro1;
      bson_init(&rk1);
      bson_init(&ro1);
      BSON_APPEND_INT32(&rk1, "block_height", 1);
      BSON_APPEND_UTF8(&ro1, "name", "ux_block_height");
      BSON_APPEND_BOOL(&ro1, "unique", true);
      mongoc_index_model_t* rm1 = mongoc_index_model_new(&rk1, &ro1);

      // 2) TTL on ts_decided
      bson_t rk2, ro2;
      bson_init(&rk2);
      bson_init(&ro2);
      BSON_APPEND_INT32(&rk2, "ts_decided", 1);  // single-field TTL index
      BSON_APPEND_UTF8(&ro2, "name", "ttl_ts_decided_365d");
      BSON_APPEND_INT32(&ro2, "expireAfterSeconds", TTL_SEC);
      mongoc_index_model_t* rm2 = mongoc_index_model_new(&rk2, &ro2);

      mongoc_index_model_t* rmodels[] = {rm1, rm2};

      bson_t create_opts;
      bson_init(&create_opts);
      BSON_APPEND_UTF8(&create_opts, "commitQuorum", "majority");
      BSON_APPEND_INT32(&create_opts, "maxTimeMS", 15000);

      bson_t reply;
      bson_init(&reply);
      if (!mongoc_collection_create_indexes_with_opts(
              coll, rmodels, 2, &create_opts, &reply, &err)) {
        ok = false;
        char* json = bson_as_canonical_extended_json(&reply, NULL);
        fprintf(stderr, "[indexes] consensus_rounds create_indexes failed: %s\nDetails: %s\n",
                err.message, json ? json : "(no reply)");
        if (json) bson_free(json);
      }

      // cleanup
      bson_destroy(&reply);
      bson_destroy(&create_opts);
      mongoc_index_model_destroy(rm2);
      mongoc_index_model_destroy(rm1);
      bson_destroy(&ro2);
      bson_destroy(&rk2);
      bson_destroy(&ro1);
      bson_destroy(&rk1);
      mongoc_collection_destroy(coll);
      mongoc_database_destroy(db);
    }
  }

  mongoc_client_pool_push(database_client_thread_pool, client);
  return ok;
}





// db helpers

bson_t *assign_ids(bson_t *docs, xcash_dbs_t collection_id) {
    const char *key_name = NULL;
    const char *key_name_fmt = NULL;
    char id_value[ID_MAX_SIZE];
    bson_iter_t iter;
    int index = 0;
    char str_index[16];  // for converting integer to string just placeholder for 16 digits index

    switch (collection_id) {
        case XCASH_DB_DELEGATES:
            key_name = "public_key";
            key_name_fmt = "0000000000000000000000000000000000000000000000000000000000000000%s";
            break;

        case XCASH_DB_STATISTICS:
            key_name = "__placeholder__";
            key_name_fmt =
                "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
                "00000000000000000000000000";
            break;

        case XCASH_DB_RESERVE_PROOFS:
            key_name = "public_address_created_reserve_proof";
            key_name_fmt = "000000000000000000000000000000%s";
            break;

        case XCASH_DB_RESERVE_BYTES:
            key_name = "reserve_bytes_data_hash";
            key_name_fmt = "%s";
            break;

        default:
            break;
    };

    bson_t *new_docs = bson_new();

    if (bson_iter_init(&iter, docs)) {
        while (bson_iter_next(&iter)) {
            const uint8_t *data;
            uint32_t len;

            bson_iter_document(&iter, &len, &data);
            bson_t *sub_doc = bson_new_from_data(data, len);

            bson_iter_t sub_iter;
            if (bson_iter_init_find(&sub_iter, sub_doc, key_name)) {
                const char *key_value = bson_iter_utf8(&sub_iter, NULL);
                snprintf(id_value, sizeof(id_value), key_name_fmt, key_value);

                bson_append_utf8(sub_doc, "_id", -1, id_value, -1);
            } else {
                if (collection_id == XCASH_DB_STATISTICS) {
                    bson_append_utf8(sub_doc, "_id", -1, key_name_fmt, -1);
                }
            }
            snprintf(str_index, sizeof(str_index), "%d", index);
            bson_append_document(new_docs, str_index, -1, sub_doc);
            bson_destroy(sub_doc);
        }
    }

    return new_docs;
}


int remove_reserve_byte_duplicates(const char *db_name, const char *collection_name, bson_t *docs) {
    mongoc_client_t *client;
    mongoc_collection_t *collection;
    bson_iter_t iter;
    bson_error_t error;
    int removed_count = 0;

    // Pop a client from the pool
    client = mongoc_client_pool_pop(database_client_thread_pool);
    if (!client) {
        DEBUG_PRINT("Failed to pop client from pool");
        return -1;
    }

    // Get the collection
    collection = mongoc_client_get_collection(client, db_name, collection_name);
    if (!collection) {
        DEBUG_PRINT("Failed to get collection: %s", collection_name);
        mongoc_client_pool_push(database_client_thread_pool, client);
        return -1;
    }

    // Iterate through the docs array
    bson_iter_t child;
    if (bson_iter_init(&iter, docs)) {
        while (bson_iter_next(&iter)) {
            bson_t doc;
            bson_t query = BSON_INITIALIZER;
            const uint8_t *data = NULL;
            uint32_t len = 0;

            // Get the current document
            bson_iter_document(&iter, &len, &data);
            bson_init_static(&doc, data, len);

            // Extract block_height and _id from the current document
            const char *block_height = NULL;
            const char *doc_id = NULL;
            if (bson_iter_init_find(&child, &doc, "block_height") && BSON_ITER_HOLDS_UTF8(&child)) {
                block_height = bson_iter_utf8(&child, NULL);
            }
            if (bson_iter_init_find(&child, &doc, "_id") && BSON_ITER_HOLDS_UTF8(&child)) {
                doc_id = bson_iter_utf8(&child, NULL);
            }

            if (block_height && doc_id) {
                BSON_APPEND_UTF8(&query, "block_height", block_height);
            }

            // Find documents with the same block_height and _id
            mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(collection, &query, NULL, NULL);
            const bson_t *result;
            while (mongoc_cursor_next(cursor, &result)) {
                const char *result_id = NULL;
                if (bson_iter_init_find(&child, result, "_id") && BSON_ITER_HOLDS_UTF8(&child)) {
                    result_id = bson_iter_utf8(&child, NULL);
                }
                if (result_id && strcmp(result_id, doc_id) != 0) {
                    // Delete the duplicate document from the collection
                    if (!mongoc_collection_delete_one(collection, result, NULL, NULL, &error)) {
                        DEBUG_PRINT("Failed to delete duplicate from collection: %s. Error: %s", collection_name, error.message);

                        mongoc_cursor_destroy(cursor);
                        bson_destroy(&query);

                        mongoc_collection_destroy(collection);
                        mongoc_client_pool_push(database_client_thread_pool, client);

                        return -1;
                    }
                    removed_count++;
                }
            }
            mongoc_cursor_destroy(cursor);
            bson_destroy(&query);
            // bson_destroy(&doc);
        }
    }

    // Cleanup
    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(database_client_thread_pool, client);

    return removed_count;
}

int count_db_delegates(void) {
    bson_error_t error;
    bool result = false;
    int64_t count;

    result = db_count_doc(DATABASE_NAME, collection_names[XCASH_DB_DELEGATES], &count, &error);
    if (!result) {
        count = -1;
    }
    return count;
}

int count_db_statistics(void) {
    bson_error_t error;
    bool result = false;
    int64_t count;

    result = db_count_doc(DATABASE_NAME, collection_names[XCASH_DB_STATISTICS], &count, &error);
    if (!result) {
        count = -1;
    }
    return count;
}

int count_recs(const bson_t *recs) {
    bson_iter_t iter;
    int count = 0;

    if (bson_iter_init(&iter, recs)) {
        while (bson_iter_next(&iter)) {
            count++;
        }
    }
    return count;
}

int32_t
bson_lookup_int32 (const bson_t *b, const char *key)
{
   bson_iter_t iter;
   bson_iter_t descendent;

   bson_iter_init (&iter, b);
   BSON_ASSERT (bson_iter_find_descendant (&iter, key, &descendent));
   BSON_ASSERT (BSON_ITER_HOLDS_INT32 (&descendent));

   return bson_iter_int32 (&descendent);
}


const char *
bson_lookup_utf8 (const bson_t *b, const char *key)
{
   bson_iter_t iter;
   bson_iter_t descendent;

   bson_iter_init (&iter, b);
   BSON_ASSERT (bson_iter_find_descendant (&iter, key, &descendent));
   BSON_ASSERT (BSON_ITER_HOLDS_UTF8 (&descendent));

   return bson_iter_utf8 (&descendent, NULL);
}


int get_db_max_block_height(const char *dbname, size_t* max_block_heigh, size_t* max_reserve_bytes) {
    mongoc_client_t *client;
    mongoc_database_t *database;
    mongoc_collection_t *collection;
    mongoc_cursor_t *cursor;
    const bson_t *doc;
    // bson_t query;
    char *str;
    int maxCollectionNumber = 0;
    #define RESERVE_BYTES_PREFIX "reserve_bytes_"
    const int RESERVE_BYTES_PREFIX_SIZE = sizeof(RESERVE_BYTES_PREFIX)-1;


    *max_block_heigh = 0;
    *max_reserve_bytes = 0;


    // Pop a client from the pool
    client = mongoc_client_pool_pop(database_client_thread_pool);
    if (!client) {
        DEBUG_PRINT("Failed to pop client from pool");
        return -1;
    }

    database = mongoc_client_get_database(client, dbname);

    if (!database) {
        DEBUG_PRINT("Failed to get database");
        mongoc_client_pool_push(database_client_thread_pool, client);
        return -1;
    }

    // List all collections and find the one with the maximum number
    cursor = mongoc_database_find_collections_with_opts(database, NULL);
    while (mongoc_cursor_next(cursor, &doc)) {
        str = bson_as_legacy_extended_json(doc, NULL);
        const char *name = bson_lookup_utf8(doc, "name");
        if (strncmp(name, RESERVE_BYTES_PREFIX, RESERVE_BYTES_PREFIX_SIZE) == 0) {
            int collectionNumber = atoi(name + RESERVE_BYTES_PREFIX_SIZE);
            if (collectionNumber > maxCollectionNumber) {
                maxCollectionNumber = collectionNumber;
            }
        }
        bson_free(str);
    }
    mongoc_cursor_destroy(cursor);


    char maxCollectionName[256];
    sprintf(maxCollectionName, RESERVE_BYTES_PREFIX"%d", maxCollectionNumber);
    collection = mongoc_client_get_collection(client, dbname, maxCollectionName);

    if (!collection) {
        DEBUG_PRINT("Failed to get collection %s", maxCollectionName);
        mongoc_database_destroy(database);
        mongoc_client_pool_push(database_client_thread_pool, client);
        return -1;
    }


    // Query the collection to find the record with the maximum 'block_height'
    // bson_init(&query);
    bson_t *query = bson_new ();
    bson_t *opts = BCON_NEW("projection", "{", "_id", BCON_BOOL(false), "}","sort", "{", "block_height", BCON_INT32 (-1), "}");


    cursor = mongoc_collection_find_with_opts(collection, query, opts, NULL);
    int maxBlockHeight = 0;
    while (mongoc_cursor_next(cursor, &doc)) {
        const char* block_height_str = bson_lookup_utf8(doc, "block_height");
        maxBlockHeight = atoi(block_height_str);;
        break;
    }


    *max_block_heigh = maxBlockHeight;
    *max_reserve_bytes = maxCollectionNumber;

    bson_destroy(query);
    bson_destroy(opts);
    mongoc_cursor_destroy(cursor);

    mongoc_collection_destroy(collection);
    mongoc_database_destroy(database);
    mongoc_client_pool_push(database_client_thread_pool, client);
    return maxBlockHeight;
}



// from db_operations

bool db_export_collection_to_bson(const char* db_name, const char* collection_name, bson_t* out, bson_error_t* error) {
  if (!out) {
    ERROR_PRINT("Output BSON pointer is NULL");
    return false;
  }

  bson_init(out);

  bson_t filter = BSON_INITIALIZER;
  bool success = db_find_doc(db_name, collection_name, &filter, out, error, false);
  bson_destroy(&filter);
  return success;
}

bool db_find_all_doc(const char *db_name, const char *collection_name, bson_t *reply, bson_error_t *error) {
  bson_t filter = BSON_INITIALIZER;
  bool result = db_find_doc(db_name, collection_name, &filter, reply, error, true);
  bson_destroy(&filter);
  return result;
}

bool db_find_doc(const char *db_name, const char *collection_name, const bson_t *query, bson_t *reply,
                 bson_error_t *error, bool exclude_id) {

  if (!reply) {
    ERROR_PRINT("db_find_doc: 'reply' is NULL");
    return false;
  }

  mongoc_client_t *client;
  mongoc_collection_t *collection;
  mongoc_cursor_t *cursor;
  const bson_t *doc = NULL;
  bson_t *opts = NULL;

  // Pop a client from the pool
  client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) {
    DEBUG_PRINT("Failed to pop client from pool");
    return false;
  }

  // Get the collection
  collection = mongoc_client_get_collection(client, db_name, collection_name);
  if (!collection) {
    DEBUG_PRINT("Failed to get collection: %s", collection_name);
    mongoc_client_pool_push(database_client_thread_pool, client);
    return false;
  }

  if (exclude_id) {
    opts = BCON_NEW("projection", "{", "_id", BCON_BOOL(false), "}");
  }

  // Find documents
  cursor = mongoc_collection_find_with_opts(collection, query, opts, NULL);
  if (opts) bson_destroy(opts);

  if (!cursor) {
    DEBUG_PRINT("Failed to initiate find operation");
    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(database_client_thread_pool, client);
    return false;
  }

  int index = 0;
  char str_index[16];  // for converting integer to string
  while (mongoc_cursor_next(cursor, &doc)) {
    snprintf(str_index, sizeof(str_index), "%d", index);
    bson_append_document(reply, str_index, -1, doc);
    index++;
  }

  if (index == 0) {
    DEBUG_PRINT("Query returned no documents");
  }

  if (mongoc_cursor_error(cursor, error)) {
    DEBUG_PRINT("Cursor error: %s", error->message);
    mongoc_cursor_destroy(cursor);
    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(database_client_thread_pool, client);

    return false;
  }

  // Cleanup
  mongoc_cursor_destroy(cursor);
  mongoc_collection_destroy(collection);
  mongoc_client_pool_push(database_client_thread_pool, client);

  return true;
}

bool db_upsert_doc(const char *db_name, const char *collection_name, const bson_t *doc, bson_error_t *error) {
  mongoc_client_t *client;
  mongoc_collection_t *collection;
  bson_iter_t iter;
  bool result = true;

  // Pop a client from the pool
  client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) {
    DEBUG_PRINT("Failed to pop client from pool");
    return false;
  }

  // Get the collection
  collection = mongoc_client_get_collection(client, db_name, collection_name);
  if (!collection) {
    DEBUG_PRINT("Failed to get collection: %s", collection_name);
    mongoc_client_pool_push(database_client_thread_pool, client);
    return false;
  }

  bson_t *opts = BCON_NEW("upsert", BCON_BOOL(true));
  bson_t query = BSON_INITIALIZER;

  // Check if the document is single record
  if (bson_iter_init_find(&iter, doc, "_id")) {
    bson_append_value(&query, "_id", -1, bson_iter_value(&iter));

    if (!mongoc_collection_replace_one(collection, &query, doc, opts, NULL, error)) {
      DEBUG_PRINT("Failed to upsert document: %s", error->message);
      result = false;
    }
  } else {
    char *str = bson_as_legacy_extended_json(doc, NULL);
    DEBUG_PRINT("Failed to find '_id' in upsert document: %s", str);
    free(str);

    result = false;
  }

  // Cleanup
  bson_destroy(&query);
  bson_destroy(opts);
  mongoc_collection_destroy(collection);
  mongoc_client_pool_push(database_client_thread_pool, client);

  return result;
}

bool db_upsert_multi_docs(const char *db_name, const char *collection_name, const bson_t *docs, bson_error_t *error) {
  mongoc_client_t *client;
  mongoc_collection_t *collection;
  bson_iter_t iter;
  bool result = true;

  // Pop a client from the pool
  client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) {
    DEBUG_PRINT("Failed to pop client from pool");
    return false;
  }

  // Get the collection
  collection = mongoc_client_get_collection(client, db_name, collection_name);
  if (!collection) {
    DEBUG_PRINT("Failed to get collection: %s", collection_name);
    mongoc_client_pool_push(database_client_thread_pool, client);
    return false;
  }

  bson_t *opts = BCON_NEW("upsert", BCON_BOOL(true));

  if (bson_iter_init(&iter, docs)) {
    bson_iter_t child;
    while (bson_iter_next(&iter)) {
      bson_t query = BSON_INITIALIZER;
      const uint8_t *data;
      uint32_t len;
      bson_t sub_doc;

      bson_iter_document(&iter, &len, &data);
      bson_init_static(&sub_doc, data, len);

      if (bson_iter_init_find(&child, &sub_doc, "_id")) {
        bson_append_value(&query, "_id", -1, bson_iter_value(&child));
      } else {
        char *str = bson_as_legacy_extended_json(&sub_doc, NULL);
        DEBUG_PRINT("Failed to find '_id' in upsert document: %s", str);
        free(str);

        result = false;
        bson_destroy(&query);
        break;
      }

      if (!mongoc_collection_replace_one(collection, &query, &sub_doc, opts, NULL, error)) {
        DEBUG_PRINT("Failed to upsert document: %s", error->message);
        result = false;
        bson_destroy(&query);
        break;
      }
      bson_destroy(&query);
    }
  }

  // Cleanup
  bson_destroy(opts);
  mongoc_collection_destroy(collection);
  mongoc_client_pool_push(database_client_thread_pool, client);

  return result;
}

bool db_delete_doc(const char *db_name, const char *collection_name, const bson_t *query, bson_error_t *error) {
  mongoc_client_t *client;
  mongoc_collection_t *collection;
  bson_t opts = BSON_INITIALIZER;
  bool result;

  // Pop a client from the pool
  client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) {
    DEBUG_PRINT("Failed to pop client from pool");
    return false;
  }

  // Get the collection
  collection = mongoc_client_get_collection(client, db_name, collection_name);
  if (!collection) {
    DEBUG_PRINT("Failed to get collection: %s", collection_name);
    mongoc_client_pool_push(database_client_thread_pool, client);
    return false;
  }

  // Delete documents
  result = mongoc_collection_delete_many(collection, query, &opts, NULL, error);
  if (!result) {
    DEBUG_PRINT("Failed to delete documents: %s", error->message);
    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(database_client_thread_pool, client);
    return false;
  }

  // Cleanup
  mongoc_collection_destroy(collection);
  mongoc_client_pool_push(database_client_thread_pool, client);

  return true;
}

bool db_drop(const char *db_name, const char *collection_name, bson_error_t *error) {
  mongoc_client_t *client;
  mongoc_collection_t *collection;
  bool result;

  // Pop a client from the pool
  client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) {
    DEBUG_PRINT("Failed to pop client from pool");
    return false;
  }

  // Get the collection
  collection = mongoc_client_get_collection(client, db_name, collection_name);
  if (!collection) {
    DEBUG_PRINT("Failed to get collection: %s", collection_name);
    mongoc_client_pool_push(database_client_thread_pool, client);
    return false;
  }

  result = mongoc_collection_drop(collection, error);
  if (!result) {
    DEBUG_PRINT("Can't drop %s, error: %s", collection_name, error->message);
  }

  mongoc_collection_destroy(collection);
  mongoc_client_pool_push(database_client_thread_pool, client);

  return result;
}

bool db_count_doc(const char *db_name, const char *collection_name, int64_t *result_count, bson_error_t *error) {
  mongoc_client_t *client;
  mongoc_collection_t *collection;
  int64_t count;

  // Pop a client from the pool
  client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) {
    DEBUG_PRINT("Failed to pop client from pool");
    return false;
  }

  // Get the collection
  collection = mongoc_client_get_collection(client, db_name, collection_name);
  if (!collection) {
    DEBUG_PRINT("Failed to get collection: %s", collection_name);
    mongoc_client_pool_push(database_client_thread_pool, client);
    return false;
  }

  bson_t *filter = bson_new();  // empty filter

  count = mongoc_collection_count_documents(collection, filter, NULL, NULL, NULL, error);
  if (count < 0) {
    DEBUG_PRINT("Failed to count documents: %s", error->message);
    bson_destroy(filter);
    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(database_client_thread_pool, client);
    return false;
  }

  // Cleanup
  bson_destroy(filter);
  mongoc_collection_destroy(collection);
  mongoc_client_pool_push(database_client_thread_pool, client);

  *result_count = count;
  return true;
}

/// @brief Get multi data db hash
/// @param collection collection name prefix. in case if reserve_proofs and reserve_bytes calculates hash for all dbs
/// @param db_hash_result pointer to buffer to receive result hash
/// @return true or false in case of error
bool db_copy_collection(const char *db_name, const char *src_collection, const char *dst_collection, bson_error_t *error) {
  bson_t filter = BSON_INITIALIZER;
  bson_t reply = BSON_INITIALIZER;

  if (!db_find_doc(db_name, src_collection, &filter, &reply, error, true)) {
    bson_destroy(&filter);
    return false;
  }

  bson_iter_t iter;
  if (!bson_iter_init(&iter, &reply)) {
    bson_destroy(&reply);
    bson_destroy(&filter);
    return false;
  }

  bool success = true;
  while (bson_iter_next(&iter)) {
    const uint8_t *doc_data;
    uint32_t doc_len;
    bson_t doc;

    bson_iter_document(&iter, &doc_len, &doc_data);
    bson_init_static(&doc, doc_data, doc_len);

    if (!db_upsert_doc(db_name, dst_collection, &doc, error)) {
      success = false;
      break;
    }
  }

  bson_destroy(&reply);
  bson_destroy(&filter);
  return success;
}