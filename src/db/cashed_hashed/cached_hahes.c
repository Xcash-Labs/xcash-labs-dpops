# include "cached_hashes.h"

/*--------------------------------------------------------------------------------------------------------
 * @brief Compares two strings for use with qsort or bsearch.
 * 
 * @param a Pointer to the first string (const void* for qsort compatibility).
 * @param b Pointer to the second string (const void* for qsort compatibility).
 * @return int Returns negative if a < b, 0 if a == b, positive if a > b.
------------------------------------------------------------------------------------------------------==*/
// compare strings function
int cmpfunc(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}
/*---------------------------------------------------------------------------------------------------------
 * @brief Update the hash and db_hash for a given collection in the database.
 * 
 * @param client MongoDB client connection.
 * @param db_name Name of the database collection.
 * @param hash 128-byte zero-padded MD5 hash.
 * @param db_hash 32-byte short MD5 hash.
 * @return int Returns 0 if successful, <0 for error codes.
-------------------------------------------------------------------------------------------------------*/
int update_hashes(mongoc_client_t *client, const char *db_name, const char *hash, const char *db_hash)
{
    if (!client || !db_name || !hash || !db_hash) {
        ERROR_PRINT("Invalid arguments passed to update_hashes.");
        return -1;
    }

    // Check if db_hash is for an empty collection (MD5 of empty string)
    if (strcmp(db_hash, "d41d8cd98f00b204e9800998ecf8427e") == 0) {
        INFO_PRINT("Skipping update for empty collection: %s", db_name);
        return 0;  // Do not store hashes for empty collections
    }

    mongoc_collection_t *collection = NULL;
    bson_t *filter = NULL;
    bson_t *update = NULL;
    bson_t *opts = NULL;
    bson_error_t error;
    int result = 0;

    // Get collection
    collection = mongoc_client_get_collection(client, DATABASE_NAME, "hashes");
    if (!collection) {
        ERROR_PRINT("Failed to get collection: hashes");
        return -2;
    }

    // Create BSON documents for filter, update, and options
    filter = BCON_NEW("db_name", BCON_UTF8(db_name));
    if (!filter) {
        ERROR_PRINT("Failed to create BSON filter.");
        result = -3;
        goto cleanup;
    }

    update = BCON_NEW("$set",
                      "{",
                      "db_hash", BCON_UTF8(db_hash),
                      "hash", BCON_UTF8(hash),
                      "}");
    if (!update) {
        ERROR_PRINT("Failed to create BSON update.");
        result = -4;
        goto cleanup;
    }

    opts = BCON_NEW("upsert", BCON_BOOL(true));
    if (!opts) {
        ERROR_PRINT("Failed to create BSON options.");
        result = -5;
        goto cleanup;
    }

    // Perform the update operation
    if (!mongoc_collection_update_one(collection, filter, update, opts, NULL, &error)) {
        ERROR_PRINT("Failed to update hashes for %s: %s", db_name, error.message);
        result = -6;
        goto cleanup;
    }

    INFO_PRINT("Successfully updated hashes for collection: %s", db_name);

cleanup:
    // Cleanup allocated resources
    if (filter) bson_destroy(filter);
    if (update) bson_destroy(update);
    if (opts) bson_destroy(opts);
    if (collection) mongoc_collection_destroy(collection);

    return result;
}

/*---------------------------------------------------------------------------------------------------------
 * @brief Calculate the MD5 hash of the given database.
 * 
 * @param client MongoDB client connection.
 * @param db_name Name of the database collection.
 * @param hash Pointer to a 128-byte zero-padded MD5 hash buffer.
 * @param db_hash Pointer to a 32-byte short MD5 hash buffer.
 * @return int Returns 0 if successful, <0 for error codes.
-------------------------------------------------------------------------------------------------------*/
int calc_db_hashes(mongoc_client_t *client, const char *db_name, char *hash, char *db_hash)
{
    if (!client || !db_name || !hash || !db_hash) {
        ERROR_PRINT("Invalid arguments passed to calc_db_hashes.");
        return -1;
    }

    mongoc_collection_t *collection = NULL;
    mongoc_cursor_t *cursor = NULL;
    bson_t *query = NULL;
    bson_t *opts = NULL;
    const bson_t *doc = NULL;

    EVP_MD_CTX *mdctx = NULL;
    unsigned char md5_bin[MD5_DIGEST_LENGTH] = {0};
    int result = 0;

    // Get collection
    collection = mongoc_client_get_collection(client, DATABASE_NAME, db_name);
    if (!collection) {
        ERROR_PRINT("Failed to get collection: %s", db_name);
        return -2;
    }

    // Create query and options
    query = bson_new();
    opts = BCON_NEW("projection", "{", "_id", BCON_BOOL(false), "}", "sort", "{", "_id", BCON_INT32(1), "}");
    if (!query || !opts) {
        ERROR_PRINT("Failed to create BSON objects for query or options.");
        result = -3;
        goto cleanup;
    }

    // Find documents
    cursor = mongoc_collection_find_with_opts(collection, query, opts, NULL);
    if (!cursor) {
        ERROR_PRINT("Failed to create cursor for collection: %s", db_name);
        result = -4;
        goto cleanup;
    }

    // Create MD5 EVP digest context
    mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        ERROR_PRINT("Failed to allocate EVP_MD_CTX");
        result = -5;
        goto cleanup;
    }

    if (EVP_DigestInit_ex(mdctx, EVP_md5(), NULL) != 1) {
        ERROR_PRINT("EVP_DigestInit_ex failed for MD5");
        result = -6;
        goto cleanup;
    }

    // Process each document
    while (mongoc_cursor_next(cursor, &doc)) {
        char *str = bson_as_canonical_extended_json(doc, NULL);
        if (str) {
            EVP_DigestUpdate(mdctx, str, strlen(str));
            bson_free(str);
        } else {
            ERROR_PRINT("Failed to convert BSON document to JSON.");
            result = -7;
            goto cleanup;
        }
    }

    // Check for cursor errors
    if (mongoc_cursor_error(cursor, NULL)) {
        ERROR_PRINT("Cursor error while iterating documents.");
        result = -8;
        goto cleanup;
    }

    // Finalize the MD5 hash
    if (EVP_DigestFinal_ex(mdctx, md5_bin, NULL) != 1) {
        ERROR_PRINT("EVP_DigestFinal_ex failed.");
        result = -9;
        goto cleanup;
    }

    // Convert MD5 binary to hex
    bin_to_hex(md5_bin, MD5_DIGEST_LENGTH, db_hash);

    // Zero-pad the hash and copy the MD5 hex string
    memset(hash, '0', 96);
    strncpy(hash + 96, db_hash, 32);
    hash[128] = '\0';  // Ensure null-termination

cleanup:
    // Cleanup allocated resources
    if (mdctx) EVP_MD_CTX_free(mdctx);
    if (opts) bson_destroy(opts);
    if (query) bson_destroy(query);
    if (cursor) mongoc_cursor_destroy(cursor);
    if (collection) mongoc_collection_destroy(collection);

    return result;
}

/*---------------------------------------------------------------------------------------------------------
 * @brief Get the MD5 hash of the specified database.
 * 
 * @param client MongoDB client connection.
 * @param db_name Name of the database collection.
 * @param db_hash Pointer to a 32-byte short MD5 hash buffer.
 * @return int Returns 0 if successful, <0 for error codes.
-------------------------------------------------------------------------------------------------------*/
int get_dbhash(mongoc_client_t *client, const char *db_name, char *db_hash)
{
    if (!client || !db_name || !db_hash) {
        ERROR_PRINT("Invalid arguments passed to get_dbhash.");
        return -1;
    }

    char l_hash[129] = {0};      // For full hash
    char l_db_hash[33] = {0};    // For short hash
    int result = 0;
    struct timeval start_time, current_time, result_time;

    // Start measuring time
    gettimeofday(&start_time, NULL);

    // Try to get cached db_hash first
    result = get_data(client, db_name, "db_hash", db_hash);
    if (result == 0) {
        return result;  // Cache hit, return immediately
    }

    // Lock mutex to handle concurrency
//    pthread_mutex_lock(&hash_mutex);

    // Recheck cache to handle concurrent access
    result = get_data(client, db_name, "db_hash", db_hash);
    if (result == 0) {  // Cache hit on recheck
//        pthread_mutex_unlock(&hash_mutex);
        return result;
    }

    // Recalculate hashes if cache miss
    if ((result = calc_db_hashes(client, db_name, l_hash, l_db_hash)) != 0) {
        ERROR_PRINT("Failed to calculate hashes for %s", db_name);
//        pthread_mutex_unlock(&hash_mutex);
        return -1;
    }

    // Update the hash in the database
    if ((result = update_hashes(client, db_name, l_hash, l_db_hash)) != 0) {
        ERROR_PRINT("Failed to update hashes for %s", db_name);
//        pthread_mutex_unlock(&hash_mutex);
        return -2;
    }

    // Copy the calculated db_hash
    strncpy(db_hash, l_db_hash, 32);
    db_hash[32] = '\0';  // Ensure null-termination

    // Log timing information
    gettimeofday(&current_time, NULL);
    timersub(&current_time, &start_time, &result_time);
    INFO_PRINT("Recalculated hash for %s in %ld.%06ld sec", db_name, 
               (long int)result_time.tv_sec, (long int)result_time.tv_usec);

    // Unlock mutex
//    pthread_mutex_unlock(&hash_mutex);

    return result;
}

int calc_multi_hash(mongoc_client_t *client, const char *db_prefix, int max_index, char *hash)
{
    if (!client || !db_prefix || !hash || max_index <= 0) {
        ERROR_PRINT("Invalid arguments passed to calc_multi_hash");
        return -1;
    }

    EVP_MD_CTX *mdctx = NULL;
    struct timeval start_time, last_time, current_time, tmp_time;
    char l_db_hash[33] = {0};
    unsigned char md5_bin[16] = {0};
    char db_name[64] = {0};
    int result = 0;

    // Check for cached multi-hash first
    if (get_data(client, db_prefix, "hash", hash) == 0) {
        return 0;  // Return immediately if cache hit
    }

    char (*names_array)[MAXIMUM_NUMBER_SIZE] = calloc(max_index, MAXIMUM_NUMBER_SIZE);
    if (!names_array) {
        ERROR_PRINT("Memory allocation failed for names_array");
        return -1;
    }

    for (int i = 0; i < max_index; i++) {
        snprintf(names_array[i], MAXIMUM_NUMBER_SIZE, "%d", i + 1);
    }
    qsort(names_array, max_index, MAXIMUM_NUMBER_SIZE, cmpfunc);

    // Initialize MD5 context
    mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        ERROR_PRINT("EVP_MD_CTX allocation failed");
        free(names_array);
        return -1;
    }

    if (EVP_DigestInit_ex(mdctx, EVP_md5(), NULL) != 1) {
        ERROR_PRINT("EVP_DigestInit_ex failed");
        EVP_MD_CTX_free(mdctx);
        free(names_array);
        return -1;
    }

    gettimeofday(&start_time, NULL);
    last_time = start_time;

    for (int i = 0; i < max_index; i++) {
        snprintf(db_name, sizeof(db_name), "%s_%s", db_prefix, names_array[i]);

        if (get_dbhash(client, db_name, l_db_hash) != 0) {
            ERROR_PRINT("Error getting hash for %s", db_name);
            result = -1;
            break;
        }

        if (EVP_DigestUpdate(mdctx, l_db_hash, strlen(l_db_hash)) != 1) {
            ERROR_PRINT("EVP_DigestUpdate failed for %s", db_name);
            result = -1;
            break;
        }

        gettimeofday(&current_time, NULL);
        timersub(&current_time, &last_time, &tmp_time);
        if (tmp_time.tv_sec > 2) {
            INFO_PRINT("Hash calculation taking longer than expected [%d/%d]", i + 1, max_index);
            last_time = current_time;
        }
    }

    if (result == 0) {
        if (EVP_DigestFinal_ex(mdctx, md5_bin, NULL) != 1) {
            ERROR_PRINT("EVP_DigestFinal_ex failed");
            result = -1;
        } else {
            memset(hash, '0', 96);
            bin_to_hex(md5_bin, sizeof(md5_bin), hash + 96);
            result = update_hashes(client, db_prefix, hash, hash + 96);
            if (result != 0) {
                ERROR_PRINT("Failed to update multi-hash for %s", db_prefix);
                result = -2;
            }
        }
    }

    // Cleanup
    EVP_MD_CTX_free(mdctx);
    free(names_array);

    return result;
}

int get_hash(mongoc_client_t *client, const char *db_name, char *hash)
{
    // Validate input arguments
    if (!client || !db_name || !hash) {
        ERROR_PRINT("Invalid arguments passed to get_hash");
        return -1;
    }

    char l_hash[129] = {0};      // For full hash
    char l_db_hash[33] = {0};    // For short hash
    int result = 0;

    struct timeval start_time, current_time, result_time;

    // Start measuring time
    gettimeofday(&start_time, NULL);

 //   pthread_mutex_lock(&hash_mutex);

    // Try to get cached hash first
    result = get_data(client, db_name, "hash", hash);
    if (result != 0) {  // Cache miss
        // Recalculate hashes if cache miss
        if ((result = calc_db_hashes(client, db_name, l_hash, l_db_hash)) != 0) {
            ERROR_PRINT("Failed to calculate hashes for %s", db_name);
        } else if ((result = update_hashes(client, db_name, l_hash, l_db_hash)) != 0) {
            ERROR_PRINT("Failed to update hashes for %s", db_name);
        } else {
            // Success: copy new hash
            strncpy(hash, l_hash, 128);
            hash[128] = '\0';  // Ensure null-termination

            // Log timing information
            gettimeofday(&current_time, NULL);
            timersub(&current_time, &start_time, &result_time);
            INFO_PRINT("Recalculated hash for %s in %ld.%06ld sec", db_name, (long int)result_time.tv_sec, (long int)result_time.tv_usec);
        }
    }

//    pthread_mutex_unlock(&hash_mutex);

    return result;
}

int get_multi_hash(mongoc_client_t *client, const char *db_prefix, char *hash) {
  // Validate input pointers early
  if (!client || !db_prefix || !hash) {
    ERROR_PRINT("Invalid arguments passed to get_multi_hash");
    return -1;
  }

  size_t reserve_bytes_index = 0;  // Initialize to avoid undefined behavior

  if (strcmp(db_prefix, "reserve_bytes") == 0) {
    if (get_reserve_bytes_database(&reserve_bytes_index)) {
      return calc_multi_hash(client, db_prefix, reserve_bytes_index, hash);
    } else {
        ERROR_PRINT("Failed to get reserver bytes database.");
        return -1; 
    }
  }

  if (strcmp(db_prefix, "reserve_proofs") == 0) {
    return calc_multi_hash(client, db_prefix, TOTAL_RESERVE_PROOFS_DATABASES, hash);
  }

  // Default case for other databases
  return get_hash(client, db_prefix, hash);
}

/*---------------------------------------------------------------------------------------------------------
 * @brief Drops all documents in the "hashes2" collection of the specified database.
 * 
 * @param client MongoDB client connection.
 * @return int Returns XCASH_OK (1) if successful, XCASH_ERROR (0) if an error occurs.
---------------------------------------------------------------------------------------------------------*/
int drop_all_hashes(mongoc_client_t *client)
{
    if (!client) {
        ERROR_PRINT("Invalid MongoDB client.");
        return XCASH_ERROR;
    }

    mongoc_collection_t *collection = NULL;
    bson_error_t error;
    int result = XCASH_OK;

    // Get collection
    collection = mongoc_client_get_collection(client, DATABASE_NAME, "hashes");
    if (!collection) {
        ERROR_PRINT("Failed to get collection: hashes2");
        return XCASH_ERROR;
    }

    // Drop collection
    if (!mongoc_collection_drop_with_opts(collection, NULL, &error)) {
        ERROR_PRINT("Failed to drop collection 'hashes2': %s", error.message);
        result = XCASH_ERROR;
    } else {
        INFO_PRINT("Successfully dropped all documents in collection 'hashes2'.");
    }

    // Cleanup
    mongoc_collection_destroy(collection);

    return result;
}