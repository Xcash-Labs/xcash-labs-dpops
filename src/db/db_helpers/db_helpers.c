#include "db_helpers.h"


const char *collection_names[XCASH_DB_COUNT] = {"delegates", "statistics", "reserve_proofs", "reserve_bytes"};

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

    if (removed_count>0) {
        // we need to rehash this db next time
        del_hash(client, collection_name);
    }

    // Cleanup
    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(database_client_thread_pool, client);

    return removed_count;
}


int upsert_json_to_db(const char *db_name, const xcash_dbs_t collection_id, int db_file_index,
                      const char *db_data_source, bool is_json_array) {
    bson_t *docs = NULL;
    bson_error_t error;
    char *db_data = NULL;
    bson_t *updated_docs = NULL;

    char collection_name[DB_COLLECTION_NAME_SIZE];

    switch (collection_id) {
        case XCASH_DB_DELEGATES:
        case XCASH_DB_STATISTICS:
            sprintf(collection_name, "%s", collection_names[collection_id]);
            break;
        case XCASH_DB_RESERVE_PROOFS:
        case XCASH_DB_RESERVE_BYTES:
            sprintf(collection_name, "%s_%d", collection_names[collection_id], db_file_index);
            break;
        default:
            break;
    }

    if (cleanup_db_before_upsert) {
        INFO_PRINT("Cleaning up %s database", collection_name);
        db_drop(database_name, collection_name, &error);
    }

    size_t db_data_size = strlen(db_data_source) + 3;  // '[' +db_data_size +']' + \0
    db_data = malloc(db_data_size);

    if (!db_data) {
        DEBUG_PRINT("Can't allocate memory for collection '%s'\n", collection_name);
        return XCASH_ERROR;
    }
    // make it standard json array
    const char *conversion_parameter;
    if (!is_json_array)
        conversion_parameter = "[%s]";
    else
        conversion_parameter = "%s";

    snprintf(db_data, db_data_size, conversion_parameter, db_data_source);

    docs = bson_new_from_json((const uint8_t *)db_data, -1, &error);
    if (!docs) {
        DEBUG_PRINT("Parsing json error for collection \"%s\": %s\n", collection_name, error.message);
        free(db_data);
        return XCASH_ERROR;
    }

    updated_docs = assign_ids(docs, collection_id);

    // reserve_bytes check for duplicates with the same block_height but different hash
    if (collection_id == XCASH_DB_RESERVE_BYTES && db_file_index > 0) {
        // FIXME better migrate to new _id based on block_height
        int dups = remove_reserve_byte_duplicates(db_name, collection_name, updated_docs);
        if ( dups > 0) {
            WARNING_PRINT("Found %d duplicates of the same block_height in '%s'. Cleaned", dups, collection_name);
        }
    }

    bool result = db_upsert_multi_docs(db_name, collection_name, updated_docs, &error);
    if (!result) {
        DEBUG_PRINT("Failed to upsert document array \"%s\": %s", collection_name, error.message);
        bson_destroy(docs);
        bson_destroy(updated_docs);
        free(db_data);
        return XCASH_ERROR;
    }

    // Cleanup
    bson_destroy(docs);
    bson_destroy(updated_docs);
    free(db_data);

    char hash_buffer[512];
    get_db_data_hash(collection_names[collection_id], hash_buffer);


    return XCASH_OK;
}

int count_db_delegates(void) {
    bson_error_t error;
    bool result = false;
    int64_t count;

    result = db_count_doc(DPOPS_DB, collection_names[XCASH_DB_DELEGATES], &count, &error);
    if (!result) {
        count = -1;
    }
    return count;
}

int count_db_statistics(void) {
    bson_error_t error;
    bool result = false;
    int64_t count;

    result = db_count_doc(DPOPS_DB, collection_names[XCASH_DB_STATISTICS], &count, &error);
    if (!result) {
        count = -1;
    }
    return count;
}

int count_db_reserve_proofs(void) {
    bson_error_t error;
    bool result = false;
    int64_t count;

    result = db_count_doc(DPOPS_DB, collection_names[XCASH_DB_RESERVE_PROOFS], &count, &error);
    if (!result) {
        count = -1;
    }
    return count;
}

int count_db_reserve_bytes(void) {
    bson_error_t error;
    bool result = false;
    int64_t count;

    result = db_count_doc(DPOPS_DB, collection_names[XCASH_DB_RESERVE_BYTES], &count, &error);
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
        str = bson_as_json(doc, NULL);
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