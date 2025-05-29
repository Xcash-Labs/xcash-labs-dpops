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

bool init_db_from_seeds(void) {
  bool result = false;

  while (!result) {
    size_t nodes_majority_count = 0;
    xcash_node_sync_info_t *nodes_majority_list = NULL;

    if (!get_daemon_data()) {
      WARNING_PRINT("Can't get daemon data. You need to start xcash daemon service before");
      return false;
    }

    if (!get_node_data()) {
      WARNING_PRINT("Can't get wallet data. You need to start xcash rpc wallet service before");
      return false;
    }

    INFO_STAGE_PRINT("Checking the network data majority");
    if (!get_sync_seeds_majority_list(&nodes_majority_list, &nodes_majority_count)) {
      WARNING_PRINT("Could not get data majority nodes sync list");
    } else {
      show_majority_statistics(nodes_majority_list, nodes_majority_count);

      if ((nodes_majority_count < NETWORK_DATA_NODES_VALID_AMOUNT)) {
        INFO_PRINT_STATUS_FAIL("Not enough data majority. Nodes: [%ld/%d]", nodes_majority_count, NETWORK_DATA_NODES_VALID_AMOUNT);
        result = false;
      } else {
        INFO_PRINT_STATUS_OK("Data majority reached. Nodes: [%ld/%d]", nodes_majority_count, NETWORK_DATA_NODES_VALID_AMOUNT);
        int sync_source_index = get_random_majority(nodes_majority_list, nodes_majority_count);
        result = initial_sync_node(&nodes_majority_list[sync_source_index]);
      }
    }
    free(nodes_majority_list);

    if (!result) {
      INFO_STAGE_PRINT("Waiting for network recovery...");
      sleep(10);
    };
  }
  if (result) {
    INFO_PRINT_STATUS_OK("Database successfully synced");
  }

  return result;
}

bool init_db_from_top(void) {
  bool result = false;

  while (!result) {
    size_t nodes_majority_count = 0;
    xcash_node_sync_info_t *nodes_majority_list = NULL;

    INFO_STAGE_PRINT("Checking the network data majority");

    if (!get_daemon_data()) {
      WARNING_PRINT("Can't get daemon data. You need to start xcash daemon service before");
      return false;
    }

    if (!get_node_data()) {
      WARNING_PRINT("Can't get wallet data. You need to start xcash rpc wallet service before");
      return false;
    }

    if (!get_sync_nodes_majority_list_top(&nodes_majority_list, &nodes_majority_count)) {
      WARNING_PRINT("Could not get data majority nodes sync list");
    } else {
      show_majority_statistics(nodes_majority_list, nodes_majority_count);

      if ((nodes_majority_count < BLOCK_VERIFIERS_VALID_AMOUNT - 2)) {
        INFO_PRINT_STATUS_FAIL("Not enough data majority. Nodes: [%ld/%d]", nodes_majority_count, BLOCK_VERIFIERS_VALID_AMOUNT);
        result = false;
      } else {
        INFO_PRINT_STATUS_OK("Data majority reached. Nodes: [%ld/%d]", nodes_majority_count, BLOCK_VERIFIERS_VALID_AMOUNT);
        int sync_source_index = get_random_majority(nodes_majority_list, nodes_majority_count);
        result = initial_sync_node(&nodes_majority_list[sync_source_index]);
      }
    }
    free(nodes_majority_list);

    if (!result) {
      INFO_STAGE_PRINT("Waiting for network recovery...");
      sleep(10);
    };
  }
  if (result) {
    INFO_PRINT_STATUS_OK("Database successfully synced");
  }

  return result;
}