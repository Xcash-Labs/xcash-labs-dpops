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
  if (!db_find_doc(DATABASE_NAME, db_collection_name, filter, doc, &error, true)) {
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

void server_received_msg_get_sync_info(server_client_t *client, const char *MESSAGE)
{
    char parse_block_height[BLOCK_HEIGHT_LENGTH + 1] = {0};
    char parsed_address[XCASH_WALLET_LENGTH + 1] = {0};
    char parsed_delegates_hash[MD5_HASH_SIZE + 1] = {0};

    DEBUG_PRINT("Received %s, %s", __func__, "XCASH_GET_SYNC_INFO");

    if (parse_json_data(MESSAGE, "public_address", parsed_address, sizeof(parsed_address)) == 0) {
        ERROR_PRINT("Can't parse 'public_address' from %s", client->client_ip);
        return;
    }

    if (parse_json_data(MESSAGE, "block_height", parse_block_height, sizeof(parse_block_height)) == 0) {
        ERROR_PRINT("Can't parse 'block_height' from %s", client->client_ip);
        return;
    }

    if (parse_json_data(MESSAGE, "delegates_hash", parsed_delegates_hash, sizeof(parsed_delegates_hash)) == 0) {
        ERROR_PRINT("Can't parse 'delegates_hash' from %s", client->client_ip);
        return;
    }

    DEBUG_PRINT("Parsed remote public_address: %s, block_height: %s, delegates_hash: %s", parsed_address, parse_block_height, 
        parsed_delegates_hash);

    if (strlen(parsed_address) < 5 || parsed_address[0] != 'X') {
        DEBUG_PRINT("Invalid or missing delegate address: '%s'", parsed_address);
      return;
    }

    int wait_seconds = 0;
    while (atomic_load(&wait_for_block_height_init) && wait_seconds < DELAY_EARLY_TRANSACTIONS_MAX) {
      sleep(1);
      wait_seconds++;
    }
    if (atomic_load(&wait_for_block_height_init)) {
      ERROR_PRINT("Timed out waiting for current_block_height in server_received_msg_get_sync_info");
    }

    pthread_mutex_lock(&delegates_all_lock);

    for (size_t i = 0; i < BLOCK_VERIFIERS_TOTAL_AMOUNT; i++) {
        if (strcmp(delegates_all[i].public_address, parsed_address) == 0) {

            if (strcmp(parse_block_height, current_block_height) != 0) {
                DEBUG_PRINT("Block height mismatch for %s: remote=%s, local=%s",
                            parsed_address, parse_block_height, current_block_height);
                break;
            }
    
            // Compare delegate list hash
            if (strcmp(parsed_delegates_hash, delegates_hash) != 0) {
                DEBUG_PRINT("Delegates hash mismatch for %s: remote=%s, local=%s",
                            parsed_address, parsed_delegates_hash, delegates_hash);
                delegate_db_hash_mismatch = delegate_db_hash_mismatch + 1;;
                break;
            }

            // All checks passed — mark online
            strncpy(delegates_all[i].online_status, "true", sizeof(delegates_all[i].online_status));
            delegates_all[i].online_status[sizeof(delegates_all[i].online_status) - 1] = '\0';
            DEBUG_PRINT("Marked delegate %s as online (ck)", parsed_address);
            break;
        }
    }

    pthread_mutex_unlock(&delegates_all_lock);

    return;
}

/*---------------------------------------------------------------------------------------------------------
Name: server_receive_data_socket_node_to_network_data_nodes_get_current_block_verifiers_list
Description: Runs the code when the server receives the NODE_TO_NETWORK_DATA_NODES_GET_CURRENT_BLOCK_VERIFIERS_LIST message
Parameters:
  CLIENT_SOCKET - The socket to send data to
---------------------------------------------------------------------------------------------------------*/
void server_receive_data_socket_node_to_network_data_nodes_get_current_block_verifiers_list(server_client_t* client)
{
    char out_data[DELEGATES_ONLINE_BUFFER];
    bool ok = create_delegate_online_ip_list(out_data, sizeof(out_data));

    if (ok) {
        INFO_PRINT("Generated JSON (%zu bytes):\n%s\n", strlen(out_data), out_data);
        send_data(client, (unsigned char*)out_data, strlen(out_data));
    } else {
        ERROR_PRINT("Failed to build delegate online list (buffer too small or DB error)");
        send_data(client,
                  (unsigned char*)"Could not get a list of the current online delegates",
                  strlen("Could not get a list of the current online delegates"));
    }
}

/*---------------------------------------------------------------------------------------------------------
Name: server_receive_data_socket_node_to_node_db_sync_req
Description:
  Handles an incoming database sync request from another node.
  When a peer node sends a XMSG_NODES_TO_NODES_DATABASE_SYNC_REQ message, this function is triggered.
  It responds by exporting the local delegates collection, converting it to canonical extended JSON,
  and sending it back in a structured message using the existing message format.

Parameters:
  client - Pointer to the server_client_t structure representing the requesting peer connection.

Behavior:
  - Exports the delegates collection from the local database (including "_id" fields).
  - Converts the data to a JSON string.
  - Packages it into a key-value parameter message.
  - Sends the message back to the requesting client over the socket.

Returns:
  None
---------------------------------------------------------------------------------------------------------*/
void server_receive_data_socket_node_to_node_db_sync_req(server_client_t *client) {
    bson_t reply;
    bson_error_t error;
    char incoming_token[SYNC_TOKEN_LEN + 1] = {0};

    // Parse incoming sync request from client->message
    if (!client || !client->message) {
        ERROR_PRINT("Invalid client or empty message");
        return;
    }

    cJSON *root = cJSON_Parse(client->message);
    if (!root) {
        ERROR_PRINT("Failed to parse incoming sync request JSON");
        return;
    }

    cJSON *json_field = cJSON_GetObjectItemCaseSensitive(root, "json");
    if (!json_field || !cJSON_IsObject(json_field)) {
        ERROR_PRINT("Missing or invalid 'json' field in incoming message");
        cJSON_Delete(root);
        return;
    }

    cJSON *token_item = cJSON_GetObjectItemCaseSensitive(json_field, "sync_token");
    if (!token_item || !cJSON_IsString(token_item) || token_item->valuestring == NULL) {
        ERROR_PRINT("Missing or invalid 'sync_token' in incoming message");
        cJSON_Delete(root);
        return;
    }

    strncpy(incoming_token, token_item->valuestring, SYNC_TOKEN_LEN);
    incoming_token[SYNC_TOKEN_LEN] = '\0'; // Ensure null termination

    cJSON_Delete(root); // Done with incoming message

    // Export the delegates DB to BSON
    if (!db_export_collection_to_bson(DATABASE_NAME, DB_COLLECTION_DELEGATES, &reply, &error)) {
        ERROR_PRINT("Failed to export collection: %s", error.message);
        return;
    }

    char* json_string = bson_as_canonical_extended_json(&reply, NULL);
    bson_destroy(&reply);
    if (!json_string) {
        ERROR_PRINT("Failed to convert BSON to JSON");
        return;
    }

    // Create outer message structure
    cJSON* message = cJSON_CreateObject();
    cJSON_AddStringToObject(message, "message_settings", "NODES_TO_NODES_DATABASE_SYNC_DATA");
    cJSON_AddStringToObject(message, "public_address", xcash_wallet_public_address);

    // Parse exported delegate data as JSON
    cJSON* json_data = cJSON_Parse(json_string);
    bson_free(json_string);
    if (!json_data) {
        ERROR_PRINT("Failed to parse exported delegate JSON data");
        cJSON_Delete(message);
        return;
    }

    // Echo the incoming sync_token in the response
    cJSON_AddStringToObject(json_data, "sync_token", incoming_token);
    cJSON_AddItemToObject(message, "json", json_data);

    char* message_str = cJSON_PrintUnformatted(message);
    cJSON_Delete(message);

    // Send the sync response
    if (send_message_to_ip_or_hostname(client->client_ip, XCASH_DPOPS_PORT, message_str) != XCASH_OK) {
        ERROR_PRINT("Failed to send the DB sync message to %s", client->client_ip);
    } else {
        INFO_PRINT("Sent delegate sync message to %s", client->client_ip);
    }

    free(message_str);
}

/*---------------------------------------------------------------------------------------------------------
 * @brief Handles incoming delegate database sync messages from other nodes.
 *
 * This function is called when a node receives a `NODES_TO_NODES_DATABASE_SYNC_DATA` message.
 * It parses the received JSON message, extracts the embedded "json" object containing delegate
 * records, converts it to BSON, clears the current delegates collection, and upserts the new data
 * into the MongoDB database.
 *
 * The "json" field in the incoming message should be a valid JSON object (not a stringified JSON)
 * representing multiple delegate documents keyed by index ("0", "1", etc).
 *
 * Example message format:
 * {
 *   "message_settings": "NODES_TO_NODES_DATABASE_SYNC_DATA",
 *   "public_address": "XCA1...",
 *   "json": {
 *     "0": { "_id": "...", "public_address": "...", ... },
 *     "1": { "_id": "...", "public_address": "...", ... },
 *     ...
 *   }
 * }
 *
 * @param MESSAGE The full raw JSON message string received from another node.
 ---------------------------------------------------------------------------------------------------------*/
void server_receive_data_socket_node_to_node_db_sync_data(const char *MESSAGE) {
  if (!MESSAGE) {
    ERROR_PRINT("Received null MESSAGE in sync data handler");
    return;
  }

  char tmp_token[SYNC_TOKEN_LEN + 1] = {0};

  // Parse the incoming message into cJSON
  cJSON *root = cJSON_Parse(MESSAGE);
  if (!root) {
    ERROR_PRINT("Failed to parse root JSON message");
    return;
  }

  // Extract the embedded JSON object from "json"
  cJSON *json_field = cJSON_GetObjectItemCaseSensitive(root, "json");
  if (!json_field || !cJSON_IsObject(json_field)) {
    ERROR_PRINT("Field 'json' not found or not a JSON object");
    cJSON_Delete(root);
    return;
  }

  // Extract "sync_token" from the JSON object
  cJSON *token_item = cJSON_GetObjectItemCaseSensitive(json_field, "sync_token");
  if (!token_item || !cJSON_IsString(token_item) || token_item->valuestring == NULL) {
    ERROR_PRINT("Field 'sync_token' not found or not a valid string");
    cJSON_Delete(root);
    return;
  }

  // Copy the token into tmp_token safely
  strncpy(tmp_token, token_item->valuestring, SYNC_TOKEN_LEN);
  tmp_token[SYNC_TOKEN_LEN] = '\0';  // Ensure null-termination

  // Optional: verify sync token now
  if (strcmp(tmp_token, sync_token) != 0) {
    ERROR_PRINT("Skipping db sync, invalid sync_token received: %s", tmp_token);
    cJSON_Delete(root);
    return;
  }

  // Serialize the "json" object into a compact string
  char *json_compact = cJSON_PrintUnformatted(json_field);
  if (!json_compact) {
    ERROR_PRINT("Failed to serialize 'json' field");
    cJSON_Delete(root);
    return;
  }

  // Convert the JSON string to BSON
  bson_error_t error;
  bson_t *doc = bson_new_from_json((const uint8_t *)json_compact, -1, &error);
  free(json_compact);
  cJSON_Delete(root);

  if (!doc) {
    ERROR_PRINT("Failed to parse BSON from JSON: %s", error.message);
    return;
  }

  bool is_primary = false;

#ifdef SEED_NODE_ON
  if (is_primary_node()) {
    is_primary = true;
  }
#endif

  if (!is_seed_node || is_primary) {
    pthread_mutex_lock(&delegates_all_lock);
    // Drop old delegates collection before sync
    if (!db_drop(DATABASE_NAME, DB_COLLECTION_DELEGATES, &error)) {
      ERROR_PRINT("Failed to clear old delegates table before sync: %s", error.message);
      bson_destroy(doc);
      pthread_mutex_unlock(&delegates_all_lock);
      return;
    }

    // Insert new delegate data
    if (!db_upsert_multi_docs(DATABASE_NAME, DB_COLLECTION_DELEGATES, doc, &error)) {
      ERROR_PRINT("Failed to upsert delegates sync data: %s", error.message);
      bson_destroy(doc);
      pthread_mutex_unlock(&delegates_all_lock);
      return;
    }

    pthread_mutex_unlock(&delegates_all_lock);
  }

  INFO_PRINT("Successfully updated delegates database from sync message");
  bson_destroy(doc);

  return;
}