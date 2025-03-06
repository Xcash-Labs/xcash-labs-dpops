# include "cached_hashes.h"

/*---------------------------------------------------------------------------------------------------------
Name: del_hash
Description: Deletes a hash entry from the "hashes" collection in the specified database.
             If the collection does not exist, the function returns 0 without attempting deletion.
Parameters:
  client   - A pointer to the MongoDB client connection.
  db_name  - The name of the database where the "hashes" collection is located.
Return: 
  -1 if an error occurs (e.g., database/collection access failure, deletion error).
   0 if the collection does not exist or no matching document was found.
   1 if a hash was successfully deleted.
---------------------------------------------------------------------------------------------------------*/
int del_hash(mongoc_client_t *client, const char *db_name) {
    if (!client || !db_name) {
        ERROR_PRINT("del_hash() received NULL parameters.");
        return -1;
    }

    mongoc_collection_t *collection = mongoc_client_get_collection(client, db_name, "hashes");
    if (!collection) {
        ERROR_PRINT("Failed to get collection 'hashes' for database: %s", db_name);
        return -1;
    }

    bson_t *filter = BCON_NEW("db_name", BCON_UTF8(db_name));
    if (!filter) {
        ERROR_PRINT("Failed to create BSON filter for db_name: %s", db_name);
        mongoc_collection_destroy(collection);
        return -1;
    }

    bson_error_t error;
    int result = 0;

    if (mongoc_collection_delete_one(collection, filter, NULL, NULL, &error)) {
        DEBUG_PRINT("Hash successfully deleted for %s", db_name);
        result = XCASH_OK;
    } else {
        ERROR_PRINT("Delete hashes %s failed: %s", db_name, error.message);
        result = -1;
    }

    bson_destroy(filter);
    mongoc_collection_destroy(collection);
    
    return result;
}





















int calc_multi_hash(mongoc_client_t *client, const char *db_prefix, int max_index, char *hash)
{
    // Validate input arguments
    if (!client || !db_prefix || !hash || max_index <= 0) {
        ERROR_PRINT("Invalid arguments passed to calc_multi_hash");
        return -1;
    }

    MD5_CTX md5;
    struct timeval start_time, last_time, current_time, tmp_time;
    char l_db_hash[33] = {0};
    unsigned char md5_bin[16] = {0};
    char db_name[64] = {0};
    int result = 0;

    // Check for cached multi-hash first
    if (get_data(client, db_prefix, "hash", hash) == 0) {
        return 0;  // Return immediately if cache hit
    }

    // Allocate memory for sorting array and validate allocation
    char (*names_array)[MAXIMUM_NUMBER_SIZE] = calloc(max_index, MAXIMUM_NUMBER_SIZE);
    if (!names_array) {
        ERROR_PRINT("Memory allocation failed for names_array");
        return -1;
    }

    // Prepare and sort index names
    for (int i = 0; i < max_index; i++) {
        snprintf(names_array[i], MAXIMUM_NUMBER_SIZE, "%d", i + 1);
    }
    qsort(names_array, max_index, MAXIMUM_NUMBER_SIZE, cmpfunc);

    // Initialize MD5 context
    MD5_Init(&md5);

    // Start measuring time
    gettimeofday(&start_time, NULL);
    last_time = start_time;

    // Process each sorted database name
    for (int i = 0; i < max_index; i++) {
        snprintf(db_name, sizeof(db_name), "%s_%s", db_prefix, names_array[i]);

        if (get_dbhash(client, db_name, l_db_hash) != 0) {
            ERROR_PRINT("Error getting hash for %s", db_name);
            result = -1;
            break;
        }

        MD5_Update(&md5, l_db_hash, strlen(l_db_hash));

        // Log if processing is slow
        gettimeofday(&current_time, NULL);
        timersub(&current_time, &last_time, &tmp_time);
        if (tmp_time.tv_sec > 2) {
            INFO_PRINT("Hash calculation taking longer than expected [%d/%d]", i + 1, max_index);
            last_time = current_time;
        }
    }

    // Finalize MD5 hash
    MD5_Final(md5_bin, &md5);

    // Convert binary hash to hex and store in the result buffer
    memset(hash, '0', 96);                   // Ensure the first 96 chars are '0'
    bin_to_hex(md5_bin, sizeof(md5_bin), hash + 96);  // Store MD5 hex result starting at hash + 96

    // Free allocated memory
    free(names_array);

    // Update multi-hash in the database
    result = update_hashes(client, db_prefix, hash, hash + 96);
    if (result != 0) {
        ERROR_PRINT("Failed to update multi-hash for %s", db_prefix);
        return -2;  // Return specific error if update fails
    }

    return 0;  // Return 0 if successful
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

    pthread_mutex_lock(&hash_mutex);

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

    pthread_mutex_unlock(&hash_mutex);

    return result;
}

int get_multi_hash(mongoc_client_t *client, const char *db_prefix, char *hash)
{
    // Validate input pointers early
    if (!client || !db_prefix || !hash) {
        ERROR_PRINT("Invalid arguments passed to get_multi_hash");
        return -1;  // Return error if any input is NULL
    }

    size_t reserve_bytes_index = 0;  // Initialize to avoid undefined behavior

    if (strcmp(db_prefix, "reserve_bytes") == 0) {
        get_reserve_bytes_database(reserve_bytes_index, 0);
        return calc_multi_hash(client, db_prefix, reserve_bytes_index, hash);
    } 

    if (strcmp(db_prefix, "reserve_proofs") == 0) {
        return calc_multi_hash(client, db_prefix, TOTAL_RESERVE_PROOFS_DATABASES, hash);
    }

    // Default case for other databases
    return get_hash(client, db_prefix, hash);
}