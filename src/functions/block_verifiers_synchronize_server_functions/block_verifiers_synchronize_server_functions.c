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
                break;
            }
    
            // All checks passed — mark online
            strncpy(delegates_all[i].online_status, "true", sizeof(delegates_all[i].online_status));
            delegates_all[i].online_status[sizeof(delegates_all[i].online_status) - 1] = '\0';
            DEBUG_PRINT("Marked delegate %s as online (ck)", parsed_address);
            break;
        }
    }
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
    bool ok = create_delegate_online_list(out_data, sizeof(out_data));

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