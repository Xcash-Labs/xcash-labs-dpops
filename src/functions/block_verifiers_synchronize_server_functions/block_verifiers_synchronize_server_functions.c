#include "block_verifiers_synchronize_server_functions.h"

bool get_block_hash(unsigned long block_height, char* block_hash, size_t block_hash_size) {
  char db_collection_name[DB_COLLECTION_NAME_SIZE];
  char block_height_str[32]; // block height is a number, doesn't need large buffer
  bool result = false;

  // Calculate reserve bytes DB index
  unsigned long reserve_bytes_db_index = ((block_height - XCASH_PROOF_OF_STAKE_BLOCK_HEIGHT) / BLOCKS_PER_DAY_FIVE_MINUTE_BLOCK_TIME) + 1;

  // Safer string formatting
  snprintf(db_collection_name, sizeof(db_collection_name), "reserve_bytes_%lu", reserve_bytes_db_index);
  snprintf(block_height_str, sizeof(block_height_str), "%lu", block_height);

  // Create BSON filter
  bson_error_t error;
  bson_t *filter = BCON_NEW("block_height", BCON_UTF8(block_height_str));
  bson_t *doc = bson_new();

  if (!filter || !doc) {
    ERROR_PRINT("Failed to allocate BSON filter or document");
    goto cleanup;
  }

  // Query MongoDB
  if (!db_find_doc(DATABASE_NAME, db_collection_name, filter, doc, &error)) {
    ERROR_PRINT("Failed to find document: %s", error.message);
    goto cleanup;
  }

  // Extract block hash
  bson_iter_t iter;
  if (bson_iter_init(&iter, doc)) {
    bson_iter_t sub_iter;
    if (bson_iter_find_descendant(&iter, "0.reserve_bytes_data_hash", &sub_iter) && BSON_ITER_HOLDS_UTF8(&sub_iter)) {
      const char *hash = bson_iter_utf8(&sub_iter, NULL);
      strncpy(block_hash, hash, block_hash_size - 1);
      block_hash[block_hash_size - 1] = '\0';
      result = true;
      goto cleanup;
    }
  }

  ERROR_PRINT("block_hash not found in document");

cleanup:
  if (filter) bson_destroy(filter);
  if (doc) bson_destroy(doc);
  return result;
}

void server_received_msg_get_block_hash(server_client_t *client, const char *MESSAGE) {
  INFO_PRINT("Received %s, %s", __func__, "XCASH_GET_BLOCK_HASH");

  // Parse JSON
  json_error_t error;
  json_t *json_message = json_loads(MESSAGE, 0, &error);
  if (!json_message) {
    ERROR_PRINT("Error parsing JSON: %s", error.text);
    return;
  }

  json_t *block_height_json = json_object_get(json_message, "block_height");
  if (!json_is_integer(block_height_json)) {
    ERROR_PRINT("block_height is not an integer");
    json_decref(json_message);
    return;
  }

  unsigned long block_height = (unsigned long)json_integer_value(block_height_json);
  json_decref(json_message);

  // Find block hash
  char block_hash[DATA_HASH_LENGTH + 1] = {0};  // Initialize properly
  if (!get_block_hash(block_height, block_hash, sizeof(block_hash))) {
    ERROR_PRINT("Failed to get block hash for block height %lu", block_height);
    return;
  }

  // Build reply JSON
  json_t *reply_json = json_object();
  if (!reply_json) {
    ERROR_PRINT("Failed to create reply JSON object");
    return;
  }

  json_object_set_new(reply_json, "message_settings", json_string("XCASH_GET_BLOCK_HASH"));
  json_object_set_new(reply_json, "public_address", json_string(xcash_wallet_public_address));
  json_object_set_new(reply_json, "block_hash", json_string(block_hash));
  char *message_result_data = json_dumps(reply_json, JSON_COMPACT);
  json_decref(reply_json);

  if (message_result_data == NULL) {
    ERROR_PRINT("Failed to serialize reply JSON");
    return;
  }
  send_data_uv(client, message_result_data);
  free(message_result_data);
}