#include "db_operations.h"

bool db_export_collection_to_bson(const char* db_name, const char* collection_name, bson_t* out, bson_error_t* error) {
  bson_t filter = BSON_INITIALIZER;
        INFO_PRINT("BACK............");
  bool success = db_find_doc(db_name, collection_name, &filter, out, error);
            INFO_PRINT("BACK2............");
  bson_destroy(&filter);
  return success;
}

bool db_find_all_doc(const char *db_name, const char *collection_name, bson_t *reply, bson_error_t *error) {
    bson_t filter = BSON_INITIALIZER;
    bool result = db_find_doc(db_name, collection_name, &filter, reply, error);
    bson_destroy(&filter);
    return result;
}

bool db_find_doc(const char *db_name, const char *collection_name, const bson_t *query, bson_t *reply,
  bson_error_t *error) {
    mongoc_client_t *client;
    mongoc_collection_t *collection;
    mongoc_cursor_t *cursor;
    const bson_t *doc = NULL;

          INFO_PRINT("HERE1............");

    // Pop a client from the pool
    client = mongoc_client_pool_pop(database_client_thread_pool);
    if (!client) {
        DEBUG_PRINT("Failed to pop client from pool");
        return false;
    }
          INFO_PRINT("HERE2............"
    // Get the collection
    collection = mongoc_client_get_collection(client, db_name, collection_name);
    if (!collection) {
        DEBUG_PRINT("Failed to get collection: %s", collection_name);
        mongoc_client_pool_push(database_client_thread_pool, client);
        return false;
    }

    // suppress '_id' output to result data
    bson_t *opts = BCON_NEW("projection", "{", "_id", BCON_BOOL(false), "}");
          INFO_PRINT("HERE3............"
    // Find documents
    cursor = mongoc_collection_find_with_opts(collection, query, opts, NULL);
    // clean it immediately
    bson_destroy(opts);
          INFO_PRINT("HERE4............"
    if (!cursor) {
        DEBUG_PRINT("Failed to initiate find operation");
        mongoc_collection_destroy(collection);
        mongoc_client_pool_push(database_client_thread_pool, client);
        return false;
    }
          INFO_PRINT("HERE5............"
    int index = 0;
    char str_index[16];  // for converting integer to string
    while (mongoc_cursor_next(cursor, &doc)) {
        snprintf(str_index, sizeof(str_index), "%d", index);
        bson_append_document(reply, str_index, -1, doc);
        index++;
    }
          INFO_PRINT("HERE6............");
    if (mongoc_cursor_error(cursor, error)) {
        DEBUG_PRINT("Cursor error: %s", error->message);
        mongoc_cursor_destroy(cursor);
        mongoc_collection_destroy(collection);
        mongoc_client_pool_push(database_client_thread_pool, client);

        return false;
    }
          INFO_PRINT("HERE7............");
    // Cleanup
    mongoc_cursor_destroy(cursor);
    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(database_client_thread_pool, client);
          INFO_PRINT("HERE8............");
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

bool db_count_doc_by(const char *db_name, const char *collection_name, const bson_t *query, int64_t *result_count,
                     bson_error_t *error) {
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


    count = mongoc_collection_count_documents(collection, query, NULL, NULL, NULL, error);
    if (count < 0) {
        DEBUG_PRINT("Failed to count documents: %s", error->message);
        mongoc_collection_destroy(collection);
        mongoc_client_pool_push(database_client_thread_pool, client);
        return false;
    }

    // Cleanup
    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(database_client_thread_pool, client);

    *result_count = count;
    return true;
}

/// @brief Get multi data db hash
/// @param collection collection name prefix. in case if reserve_proofs and reserve_bytes calculates hash for all dbs
/// @param db_hash_result pointer to buffer to receive result hash
/// @return true or false in case of error
bool get_db_data_hash(const char *collection_prefix, char *db_hash_result) {
    mongoc_client_t *client;
    int cache_request_result;

    // Pop a client from the pool
    client = mongoc_client_pool_pop(database_client_thread_pool);
    if (!client) {
        DEBUG_PRINT("Failed to pop client from pool");
        return false;
    }

    cache_request_result = get_multi_hash(client, collection_prefix, db_hash_result);

    mongoc_client_pool_push(database_client_thread_pool, client);

    return cache_request_result < 0 ? false : true;
}

/// @brief Get multi data db hash
/// @param collection collection name prefix. in case if reserve_proofs and reserve_bytes calculates hash for all dbs
/// @param db_hash_result pointer to buffer to receive result hash
/// @return true or false in case of error
bool db_copy_collection(const char *db_name, const char *src_collection, const char *dst_collection, bson_error_t *error) {
    bson_t filter = BSON_INITIALIZER;
    bson_t reply = BSON_INITIALIZER;

    if (!db_find_doc(db_name, src_collection, &filter, &reply, error)) {
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
