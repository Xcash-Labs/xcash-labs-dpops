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

    // Copy everything except fields below
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
bool create_delegate_online_ip_list(char* out_data, size_t out_data_size)
{
    char ip_list[(IP_LENGTH * BLOCK_VERIFIERS_TOTAL_AMOUNT) + 256];

    mongoc_client_t*       db_client   = NULL;
    mongoc_collection_t*   collection  = NULL;
    mongoc_cursor_t*       cursor      = NULL;
    bson_t*                query       = NULL;
    bson_t*                opts        = NULL;
    const bson_t*          doc         = NULL;
    bson_error_t           cursor_err;
    bool                   first       = true;
    bool                   success     = false;

    db_client = mongoc_client_pool_pop(database_client_thread_pool);
    if (!db_client) {
        ERROR_PRINT("%s: Failed to pop Mongo client from pool", __func__);
        return false;
    }

    collection = mongoc_client_get_collection(
        db_client,
        DATABASE_NAME,
        DB_COLLECTION_DELEGATES);
    if (!collection) {
      ERROR_PRINT("%s: Failed to get collection", __func__);
      mongoc_client_pool_push(database_client_thread_pool, db_client);
      return false;
    }

    // Special case on first pos block when nothing marked online yet
    uint64_t cur_height = strtoull(current_block_height, NULL, 10);
    if (cur_height == XCASH_PROOF_OF_STAKE_BLOCK_HEIGHT) {
      query = bson_new();
    } else {
      query = BCON_NEW("online_status", BCON_UTF8("true"));
    }

    opts = BCON_NEW("sort", "{",
                    "delegate_type", BCON_INT32(1),
                    "_id", BCON_INT32(1),
                    "}");

    if (!query || !opts) {
      ERROR_PRINT("%s: Failed to build query or opts", __func__);
      if (query) bson_destroy(query);
      if (opts) bson_destroy(opts);
      mongoc_collection_destroy(collection);
      mongoc_client_pool_push(database_client_thread_pool, db_client);
      return false;
    }

    cursor = mongoc_collection_find_with_opts(collection, query, opts, NULL);
    if (!cursor) {
        ERROR_PRINT("%s: Failed to create cursor", __func__);
        bson_destroy(query);
        bson_destroy(opts);
        mongoc_collection_destroy(collection);
        mongoc_client_pool_push(database_client_thread_pool, db_client);
        return false;
    }

    ip_list[0] = '\0';

    while (mongoc_cursor_next(cursor, &doc)) {
        const char* ip_val = NULL;
        bson_iter_t iter;

        if (bson_iter_init_find(&iter, doc, "IP_address") &&
            BSON_ITER_HOLDS_UTF8(&iter)) {
            ip_val = bson_iter_utf8(&iter, NULL);
        }

        if (ip_val) {
            if (!first) {
                strncat(ip_list, "|", sizeof(ip_list) - strlen(ip_list) - 1);
            }
            strncat(ip_list, ip_val, sizeof(ip_list) - strlen(ip_list) - 1);
            first = false;
        }
    }

    if (mongoc_cursor_error(cursor, &cursor_err)) {
        ERROR_PRINT("%s: Cursor error: %s", __func__, cursor_err.message);
        goto cleanup;
    }

    if (ip_list[0] != '\0') {
        strncat(ip_list, "|", sizeof(ip_list) - strlen(ip_list) - 1);
    }

    if (snprintf(out_data, out_data_size,
                 "{\r\n"
                 "  \"message_settings\": \"NETWORK_DATA_NODE_TO_NODE_SEND_CURRENT_BLOCK_VERIFIERS_IP_LIST\",\r\n"
                 "  \"block_verifiers_IP_address_list\": \"%s\"\r\n"
                 "}", ip_list) >= (int)out_data_size) {
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

  delegates_t* delegates = (delegates_t*)calloc(BLOCK_VERIFIERS_TOTAL_AMOUNT, sizeof(delegates_t));
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

// @brief Selects a random valid index from the online list, avoiding self-selection.
// Output: selected delegate index (or -1 if none)
int select_random_online_delegate(void) {
    int eligible_indices[BLOCK_VERIFIERS_TOTAL_AMOUNT];
    int eligible_count = 0;

    for (size_t i = 0; i < BLOCK_VERIFIERS_TOTAL_AMOUNT; ++i) {
        if (delegates_all[i].public_address[0] == '\0') {
            continue;
        }
        if (is_seed_node) {
          if (is_seed_address(delegates_all[i].public_address)) {
            continue;
          }
        }

        // Must be online and not self
        if ((strcmp(delegates_all[i].online_status, "partial") == 0) &&
        (strcmp(delegates_all[i].public_address, xcash_wallet_public_address) != 0)) {
            eligible_indices[eligible_count++] = i;
        }
        
    }

    if (eligible_count == 0) {
        return -1;
    }

    // Seed RNG once
    static int seeded = 0;
    if (!seeded) {
        srand(time(NULL));
        seeded = 1;
    }

    int random_index = rand() % eligible_count;
    return eligible_indices[random_index];
}