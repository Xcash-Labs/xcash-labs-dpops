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

    pthread_mutex_lock(&delegates_mutex);

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
                delegate_db_hash_mismatch = delegate_db_hash_mismatch + 1;
                strncpy(delegates_all[i].online_status, "partial", sizeof(delegates_all[i].online_status));
                delegates_all[i].online_status[sizeof(delegates_all[i].online_status) - 1] = '\0';
                break;
            }

            // All checks passed — mark online
            strncpy(delegates_all[i].online_status, "true", sizeof(delegates_all[i].online_status));
            delegates_all[i].online_status[sizeof(delegates_all[i].online_status) - 1] = '\0';
            DEBUG_PRINT("Marked delegate %s as online (ck)", parsed_address);
            break;
        }
    }

    pthread_mutex_unlock(&delegates_mutex);

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

  // Export the collection to BSON (with _id included)
  if (!db_export_collection_to_bson(DATABASE_NAME, DB_COLLECTION_DELEGATES, &reply, &error)) {
    ERROR_PRINT("Failed to export collection: %s", error.message);
    return;
  }

  // Convert BSON to canonical extended JSON
  char* json_string = bson_as_canonical_extended_json(&reply, NULL);
  bson_destroy(&reply);

  if (!json_string) {
    ERROR_PRINT("Failed to convert BSON to JSON");
    return;
  }

  // Wrap the message using your key-value format
  const char* params[] = {
    "message_settings", "XMSG_NODES_TO_NODES_DATABASE_SYNC_DATA",
    "public_address", xcash_wallet_public_address,
    "json", json_string,
    NULL
  };

  char* message = create_message_param_list(XMSG_NODES_TO_NODES_DATABASE_SYNC_DATA, params);

  if (!message) {
    ERROR_PRINT("Failed to create sync message for %s", client->client_ip);
    bson_free(json_string);
    return;
  }

  // Send the complete message
  if (send_data(client, (const unsigned char*)message, strlen(message)) <= 0) {
    ERROR_PRINT("Failed to send the DB sync message to %s", client->client_ip);
  } else {
    INFO_PRINT("Sent delegate sync message to %s", client->client_ip);
  }

  bson_free(json_string);
  free(message);
}

void server_receive_data_socket_node_to_node_db_sync_data(const char *MESSAGE) {
  if (!MESSAGE) {
    ERROR_PRINT("Received null MESSAGE in sync data handler");
    return;
  }

  INFO_PRINT("SYNCING DATA....................................");

  // Extract the "json" field from the message
  char json_data[BUFFER_SIZE] = {0};
  if (parse_json_data(MESSAGE, "json", json_data, sizeof(json_data)) == 0 || strlen(json_data) == 0) {
    ERROR_PRINT("Failed to parse 'json' from message");
    return;
  }


  // Convert the JSON string to BSON
  bson_error_t error;
  bson_t *doc = bson_new_from_json((const uint8_t*)json_data, -1, &error);
  if (!doc) {
    ERROR_PRINT("Failed to parse BSON from JSON: %s", error.message);
    return;
  }

  if (!db_drop(DATABASE_NAME, DB_COLLECTION_DELEGATES, &error)) {
    ERROR_PRINT("Failed to clear old delegates table before sync: %s", error.message);
    bson_destroy(doc);
    return;
  }

  // Upsert the documents into the delegates collection
  if (!db_upsert_multi_docs(DATABASE_NAME, DB_COLLECTION_DELEGATES, doc, &error)) {
    ERROR_PRINT("Failed to upsert delegates sync data: %s", error.message);
    bson_destroy(doc);
    return;
  }

  INFO_PRINT("Successfully updated delegates database from sync message");
  bson_destroy(doc);
}