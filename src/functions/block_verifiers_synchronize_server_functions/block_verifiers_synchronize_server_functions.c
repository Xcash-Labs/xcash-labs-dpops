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

static inline void wait_for_atomic_bool(void) {
    const int max_wait_ms = 1000;  // 1 second
    const int sleep_step_us = 10000;  // 10 milliseconds
    int waited_ms = 0;

    while (!atomic_load(&delegates_loaded)) {
        if (waited_ms >= max_wait_ms) {
            break;
        }
        usleep(sleep_step_us);
        waited_ms += sleep_step_us / 1000;
    }
}

void server_received_msg_get_sync_info(server_client_t *client, const char *MESSAGE)
{
    char parse_block_height[BLOCK_HEIGHT_LENGTH + 1] = {0};
    char parsed_address[XCASH_WALLET_LENGTH + 1] = {0};

    DEBUG_PRINT("Received %s, %s", __func__, "XCASH_GET_SYNC_INFO");

    if (parse_json_data(MESSAGE, "public_address", parsed_address, sizeof(parsed_address)) == 0) {
        ERROR_PRINT("Can't parse 'public_address' from %s", client->client_ip);
        return;
    }

    if (parse_json_data(MESSAGE, "block_height", parse_block_height, sizeof(parse_block_height)) == 0) {
        ERROR_PRINT("Can't parse 'block_height' from %s", client->client_ip);
        return;
    }

    DEBUG_PRINT("Parsed public_address: %s, remote block_height: %s", parsed_address, parse_block_height);

    // Wait for delegates table to load before searching
    if (!atomic_load(&delegates_loaded)) {
        wait_for_atomic_bool();
    }

    bool found = false;

    for (size_t i = 0; i < BLOCK_VERIFIERS_TOTAL_AMOUNT; i++) {
        if (strcmp(delegates_all[i].public_address, parsed_address) == 0) {
            strncpy(delegates_all[i].online_status_ck, "true", sizeof(delegates_all[i].online_status_ck));
            delegates_all[i].online_status_ck[sizeof(delegates_all[i].online_status_ck) - 1] = '\0';
            found = true;
            DEBUG_PRINT("Marked delegate %s as online (ck)", parsed_address);
            break;
        }
    }

    if (!found) {
        DEBUG_PRINT("Delegate with address %s not found in delegates_all[]", parsed_address);
    }
}



static inline void wait_for_atomic_bool(void) {
    const int max_wait_ms = 1000;  // 1 second
    int waited_ms = 0;
    while (!atomic_load(&delegates_loaded)) {
      if (waited_ms >= max_wait_ms) {
        break;
      }
      usleep(10000);  // 10 milliseconds
      waited_ms += sleep_step_us / 1000;
    }
  }

void server_received_msg_get_sync_info(server_client_t *client, const char *MESSAGE)
{
 
    char parse_block_height[BLOCK_HEIGHT_LENGTH + 1] = {0};
    char parsed_address[XCASH_WALLET_LENGTH + 1] = {0};

    DEBUG_PRINT("Received %s, %s", __func__, "XCASH_GET_SYNC_INFO");

    // Parse the public address
    if (parse_json_data(MESSAGE, "public_address", parsed_address, sizeof(parsed_address)) == 0) {
        ERROR_PRINT("Can't parse 'public_address' from %s", client->client_ip);
        return;
    }

    // Parse the block height
    if (parse_json_data(MESSAGE, "block_height", parse_block_height, sizeof(parse_block_height)) == 0) {
        ERROR_PRINT("Can't parse 'block_height' from %s", client->client_ip);
        return;
    }

    DEBUG_PRINT("Parsed public_address: %s, remote block_height: %s", parsed_address, parse_block_height);

    // Wait for delegates to load
    bool found = false;
    for (size_t i = 0; i < BLOCK_VERIFIERS_TOTAL_AMOUNT; i++) {
        if (i = 0 ) {
            wait_for_atomic_bool();
        }
        if (strcmp(delegates_all[i].public_address, parsed_address) == 0) {
            strncpy(delegates_all[i].online_status_ck, "true", sizeof(delegates_all[i].online_status_ck));
            delegates_all[i].online_status_ck[sizeof(delegates_all[i].online_status_ck) - 1] = '\0';  // Ensure null-termination
            found = true;
            DEBUG_PRINT("Marked delegate %s as online (ck)", parsed_address);
            break;
        }
    }

    if (!found) {
        DEBUG_PRINT("Delegate with address %s not found in delegates_all[]", parsed_address);
    }
}

void server_received_msg_get_block_producers(server_client_t *client, const char *MESSAGE)
{
    (void)MESSAGE;
    DEBUG_PRINT("received %s, %s", __func__, "XCASH_GET_BLOCK_PRODUCERS");

    // Create root JSON object
    cJSON *reply_json = cJSON_CreateObject();
    if (!reply_json) {
        ERROR_PRINT("Failed to create JSON object");
        return;
    }

    cJSON_AddStringToObject(reply_json, "message_settings", "XCASH_GET_BLOCK_PRODUCERS");
    cJSON_AddStringToObject(reply_json, "public_address", xcash_wallet_public_address);

    // Arrays for producer addresses and IPs
    cJSON *producers_array = cJSON_CreateArray();
    cJSON *producers_ip_array = cJSON_CreateArray();

    if (!producers_array || !producers_ip_array) {
        ERROR_PRINT("Failed to create JSON arrays");
        cJSON_Delete(reply_json);
        return;
    }

    for (size_t i = 0; i < BLOCK_VERIFIERS_TOTAL_AMOUNT; i++) {
        if (strcmp(delegates_all[i].online_status, "true") == 0) {
            cJSON_AddItemToArray(producers_array, cJSON_CreateString(delegates_all[i].public_address));
            cJSON_AddItemToArray(producers_ip_array, cJSON_CreateString(delegates_all[i].IP_address));
        }
    }

    cJSON_AddItemToObject(reply_json, "producers", producers_array);
    cJSON_AddItemToObject(reply_json, "producers_ip", producers_ip_array);

    // Serialize and send
    char *message_data = cJSON_PrintUnformatted(reply_json);
    if (message_data) {
        send_data_uv(client, message_data);  // Sends + appends SOCKET_END_STRING internally
        free(message_data);
    } else {
        ERROR_PRINT("Failed to serialize producer JSON");
    }

    cJSON_Delete(reply_json);  // Cleanup
}