#include "db_sync.h"

bool hash_delegates_collection(char *out_hash_hex) {
  if (!out_hash_hex || !database_client_thread_pool) return XCASH_ERROR;

  mongoc_client_t *client = mongoc_client_pool_pop(database_client_thread_pool);
  if (!client) return XCASH_ERROR;

  mongoc_collection_t *collection = NULL;
  mongoc_cursor_t *cursor = NULL;
  bson_t *query = NULL;
  bson_t *opts = NULL;
  EVP_MD_CTX *ctx = NULL;
  const bson_t *doc = NULL;
  bool result = XCASH_ERROR;

  // Step 1: Access collection
  collection = mongoc_client_get_collection(client, DATABASE_NAME, DB_COLLECTION_DELEGATES);
  if (!collection) goto cleanup;

  // Step 2: Build query and sort options
  query = bson_new();
  opts = BCON_NEW("sort", "{", "_id", BCON_INT32(1), "}");
  if (!query || !opts) goto cleanup;

  // Step 3: Create cursor
  cursor = mongoc_collection_find_with_opts(collection, query, opts, NULL);
  if (!cursor) goto cleanup;

  // Step 4: Initialize hash
  ctx = EVP_MD_CTX_new();
  if (!ctx || EVP_DigestInit_ex(ctx, EVP_md5(), NULL) != 1) goto cleanup;

  // Step 5: Feed documents into hash
  while (mongoc_cursor_next(cursor, &doc)) {
    bson_t filtered;
    bson_init(&filtered);

    // Copy everything except "registration_time"
    bson_iter_t iter;
    if (bson_iter_init(&iter, doc)) {
      while (bson_iter_next(&iter)) {
        const char *key = bson_iter_key(&iter);
        if (strcmp(key, "registration_timestamp") != 0 &&
          strcmp(key, "online_status") != 0) {
            bson_append_value(&filtered, key, -1, bson_iter_value(&iter));
        }
      }
    }

    char *json = bson_as_canonical_extended_json(&filtered, NULL);
    if (json) {
      EVP_DigestUpdate(ctx, json, strlen(json));
      bson_free(json);
    }
    bson_destroy(&filtered);
  }

  if (mongoc_cursor_error(cursor, NULL)) {
    ERROR_PRINT("Cursor error occurred during delegate hashing.");
    goto cleanup;
  }

  // Step 6: Finalize
  unsigned char hash_bin[MD5_DIGEST_LENGTH];
  if (EVP_DigestFinal_ex(ctx, hash_bin, NULL) != 1) goto cleanup;

  bin_to_hex(hash_bin, MD5_DIGEST_LENGTH, out_hash_hex);
  result = XCASH_OK;

cleanup:
  if (ctx) EVP_MD_CTX_free(ctx);
  if (cursor) mongoc_cursor_destroy(cursor);
  if (opts) bson_destroy(opts);
  if (query) bson_destroy(query);
  if (collection) mongoc_collection_destroy(collection);
  if (client) mongoc_client_pool_push(database_client_thread_pool, client);

  return result;
}

// Caller provides `out_data` with capacity `out_data_size`.
// On success, this function writes the complete JSON message into `out_data`
// and returns true. On failure, it returns false.
bool create_delegate_online_list(char* out_data, size_t out_data_size)
{
    char addr_list[((XCASH_WALLET_LENGTH * MAXIMUM_AMOUNT_OF_DELEGATES) + 256)];
    char key_list [((VRF_PUBLIC_KEY_LENGTH * MAXIMUM_AMOUNT_OF_DELEGATES) +256)];
    char ip_list  [((IP_LENGTH * MAXIMUM_AMOUNT_OF_DELEGATES) + 256)];

    mongoc_client_t*       db_client   = NULL;
    mongoc_collection_t*   collection  = NULL;
    mongoc_cursor_t*       cursor      = NULL;
    bson_t*                query       = NULL;
    bson_t*                opts        = NULL;
    const bson_t*          doc         = NULL;
    bson_error_t           cursor_err;
    bool                   first       = true;
    bool                   success     = false;

    // 1) Pop a client from the pool
    db_client = mongoc_client_pool_pop(database_client_thread_pool);
    if (!db_client) {
        ERROR_PRINT("%s: Failed to pop Mongo client from pool", __func__);
        return false;
    }

    // 2) Get the delegates collection
    collection = mongoc_client_get_collection(
                     db_client,
                     DATABASE_NAME,
                     DB_COLLECTION_DELEGATES);
    if (!collection) {
        ERROR_PRINT("%s: mongoc_client_get_collection returned NULL", __func__);
        mongoc_client_pool_push(database_client_thread_pool, db_client);
        return false;
    }

    // 3) Build a filter: { "online_status": "true" }
    query = BCON_NEW("online_status", BCON_UTF8("true"));
    if (!query) {
        ERROR_PRINT("%s: BCON_NEW failed for filter", __func__);
        mongoc_collection_destroy(collection);
        mongoc_client_pool_push(database_client_thread_pool, db_client);
        return false;
    }

    // 4) Sort by _id ascending for deterministic order
    opts = BCON_NEW("sort", "{", "_id", BCON_INT32(1), "}");
    if (!opts) {
        ERROR_PRINT("%s: BCON_NEW failed for sort options", __func__);
        bson_destroy(query);
        mongoc_collection_destroy(collection);
        mongoc_client_pool_push(database_client_thread_pool, db_client);
        return false;
    }

    // 5) Create the cursor
    cursor = mongoc_collection_find_with_opts(collection, query, opts, NULL);
    if (!cursor) {
        ERROR_PRINT("%s: mongoc_collection_find_with_opts returned NULL", __func__);
        bson_destroy(opts);
        bson_destroy(query);
        mongoc_collection_destroy(collection);
        mongoc_client_pool_push(database_client_thread_pool, db_client);
        return false;
    }

    // 6) Initialize each list buffer as an empty string
    addr_list[0] = '\0';
    key_list [0] = '\0';
    ip_list  [0] = '\0';

    // 7) Iterate over all matching documents
    while (mongoc_cursor_next(cursor, &doc)) {
        const char* addr_val = NULL;
        const char* key_val  = NULL;
        const char* ip_val   = NULL;
        bson_iter_t iter;

        // 7a) Extract "public_address"
        if (bson_iter_init_find(&iter, doc, "public_address") &&
            BSON_ITER_HOLDS_UTF8(&iter)) {
            addr_val = bson_iter_utf8(&iter, NULL);
        }

        // 7b) Extract "public_key"
        if (bson_iter_init_find(&iter, doc, "public_key") &&
            BSON_ITER_HOLDS_UTF8(&iter)) {
            key_val = bson_iter_utf8(&iter, NULL);
        }

        // 7c) Extract "IP_address"
        if (bson_iter_init_find(&iter, doc, "IP_address") &&
            BSON_ITER_HOLDS_UTF8(&iter)) {
            ip_val = bson_iter_utf8(&iter, NULL);
        }

        // 7d) Only append if all three are non‐NULL
        if (addr_val && key_val && ip_val) {
            if (!first) {
                strncat(addr_list, "|", sizeof(addr_list) - strlen(addr_list) - 1);
                strncat(key_list,  "|", sizeof(key_list) - strlen(key_list)  - 1);
                strncat(ip_list,   "|", sizeof(ip_list)- strlen(ip_list)   - 1);
            }
            first = false;

            strncat(addr_list, addr_val, sizeof(addr_list) - strlen(addr_list) - 1);
            strncat(key_list, key_val, sizeof(key_list) - strlen(key_list)  - 1);
            strncat(ip_list, ip_val, sizeof(ip_list) - strlen(ip_list)   - 1);
        }
    }

    // 8) Check for cursor errors
    if (mongoc_cursor_error(cursor, &cursor_err)) {
        ERROR_PRINT("%s: Cursor error: %s", __func__, cursor_err.message);
        goto cleanup;
    }

    // 8.5) Append a trailing "|" to each non‐empty list
    if (addr_list[0] != '\0') {
        // addr_list already contains at least one entry, so tack on a "|" at the end
        strncat(addr_list, "|", sizeof(addr_list) - strlen(addr_list) - 1);
    }
    if (key_list[0] != '\0') {
        strncat(key_list, "|", sizeof(key_list) - strlen(key_list) - 1);
    }
    if (ip_list[0] != '\0') {
        strncat(ip_list, "|", sizeof(ip_list) - strlen(ip_list) - 1);
    }
    

    // 9) Build final JSON into 'out_data'
    if (snprintf(out_data, out_data_size,
                 "{\r\n"
                 "  \"message_settings\": \"NETWORK_DATA_NODE_TO_NODE_SEND_CURRENT_BLOCK_VERIFIERS_LIST\",\r\n"
                 "  \"block_verifiers_public_address_list\": \"%s\",\r\n"
                 "  \"block_verifiers_public_key_list\": \"%s\",\r\n"
                 "  \"block_verifiers_IP_address_list\": \"%s\"\r\n"
                 "}",
                 addr_list,
                 key_list,
                 ip_list) >= (int)out_data_size) {
        ERROR_PRINT("%s: Output buffer too small (%zu bytes)", __func__, out_data_size);
        goto cleanup;
    }

    success = true;

cleanup:
    if (cursor)     mongoc_cursor_destroy(cursor);
    if (opts)       bson_destroy(opts);
    if (query)      bson_destroy(query);
    if (collection) mongoc_collection_destroy(collection);
    if (db_client)  mongoc_client_pool_push(database_client_thread_pool, db_client);

    return success;
}

bool fill_delegates_from_db(void) {
  delegates_t *delegates = (delegates_t *)calloc(MAXIMUM_AMOUNT_OF_DELEGATES, sizeof(delegates_t));
  size_t total_delegates = 0;

  if (read_organize_delegates(delegates, &total_delegates) != XCASH_OK) {
    ERROR_PRINT("Could not organize the delegates");

    free(delegates);
    return false;
  }

  total_delegates = total_delegates > BLOCK_VERIFIERS_TOTAL_AMOUNT ? BLOCK_VERIFIERS_TOTAL_AMOUNT : total_delegates;

  // fill actual list of all delegates from db
  for (size_t i = 0; i < BLOCK_VERIFIERS_TOTAL_AMOUNT; i++) {
    if (i < total_delegates) {
      delegates_all[i] = delegates[i];
    } else {
      memset(&delegates_all[i], 0, sizeof(delegates_t));
    }
  }

  // cleanup the allocated memory
  free(delegates);

  return true;
}

/**
 * @brief Selects a random valid index from the majority list, avoiding self-selection.
 *
 * @param majority_list Pointer to the array of majority nodes.
 * @param majority_count The number of items in the majority list.
 * @return int The index of a randomly selected node, avoiding self-selection. Returns -1 on error.
 */
int get_random_majority(xcash_node_sync_info_t *majority_list, size_t majority_count) {
  if (!majority_list || majority_count == 0) {
    ERROR_PRINT("Invalid majority list or zero count.");
    return -1;
  }

  int random_index = -1;

  // Randomly select an index, avoiding self-selection
  for (size_t attempt = 0; attempt < majority_count; ++attempt) {
    random_index = rand() % (int)majority_count;

    // Prevent syncing from myself
    if (strcmp(xcash_wallet_public_address, majority_list[random_index].public_address) != 0) {
      return random_index;
    }
  }

  // Fallback to the first valid non-self index if all attempts failed
  for (size_t i = 0; i < majority_count; ++i) {
    if (strcmp(xcash_wallet_public_address, majority_list[i].public_address) != 0) {
      return (int)i;
    }
  }

  // If no valid node is found (should not happen), return an error
  ERROR_PRINT("No valid majority node found that is not self.");
  return -1;
}