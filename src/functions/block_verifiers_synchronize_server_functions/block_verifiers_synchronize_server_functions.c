#include "block_verifiers_synchronize_server_functions.h"

/*---------------------------------------------------------------------------------------------------------
Name: server_receive_data_socket_node_to_network_data_nodes_get_current_block_verifiers_list
Description: Runs the code when the server receives the NODE_TO_NETWORK_DATA_NODES_GET_CURRENT_BLOCK_VERIFIERS_LIST message
Parameters:
  CLIENT_SOCKET - The socket to send data to
---------------------------------------------------------------------------------------------------------*/
void server_receive_data_socket_node_to_network_data_nodes_get_current_block_verifiers_list(server_client_t *client) {
  char out_data[DELEGATES_ONLINE_BUFFER];
  bool ok = create_delegate_online_ip_list(out_data, sizeof(out_data));

  if (ok) {
    DEBUG_PRINT("Generated JSON (%zu bytes):\n%s\n", strlen(out_data), out_data);
    send_data(client, (unsigned char *)out_data, strlen(out_data));
  } else {
    ERROR_PRINT("Failed to build delegate online list (buffer too small or DB error)");
    send_data(client, (unsigned char *)"0|Could not get a list of the current online delegates",
      strlen("0|Could not get a list of the current online delegates"));
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
void server_receive_data_socket_node_to_node_db_sync_req(server_client_t *client, const char* MESSAGE) {
  bson_t reply;
  bson_error_t error;
  char incoming_token[SYNC_TOKEN_LEN + 1] = {0};

  INFO_PRINT("Req Message: %s", MESSAGE);

  // Extract sync_token from the incoming MESSAGE
  cJSON *root = cJSON_Parse(MESSAGE);
  if (root) {
    cJSON *json_field = root;
    if (json_field && cJSON_IsObject(json_field)) {
      cJSON *token_item = cJSON_GetObjectItemCaseSensitive(json_field, "sync_token");
      if (token_item && cJSON_IsString(token_item) && token_item->valuestring != NULL) {
        strncpy(incoming_token, token_item->valuestring, SYNC_TOKEN_LEN);
        incoming_token[SYNC_TOKEN_LEN] = '\0';
      }
    }
    cJSON_Delete(root);
  } else {
    ERROR_PRINT("cJSON parse failed");  // <-- added missing semicolon
    return;
  }

  if (incoming_token[0] == '\0') {
    ERROR_PRINT("Failed to read sync_token");
    return;
  }

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

  cJSON* message = cJSON_CreateObject();
  cJSON_AddStringToObject(message, "message_settings", "NODES_TO_NODES_DATABASE_SYNC_DATA");
  cJSON_AddStringToObject(message, "public_address", xcash_wallet_public_address);

  // Parse the JSON string to object
  cJSON* json_data = cJSON_Parse(json_string);
  bson_free(json_string);
  if (!json_data) {
    ERROR_PRINT("Failed to parse inner JSON data");
    cJSON_Delete(message);
    return;
  }

  // Add the extracted sync_token to the outgoing json if it was present
  cJSON_AddStringToObject(json_data, "sync_token", incoming_token);
  
  cJSON_AddItemToObject(message, "json", json_data);  // now added as actual nested object

  char* message_str = cJSON_PrintUnformatted(message);
  cJSON_Delete(message);

  // Send message
  if (send_message_to_ip_or_hostname(client->client_ip, XCASH_DPOPS_PORT, message_str) != XCASH_OK) {
    ERROR_PRINT("Failed to send the DB sync message to %s", client->client_ip);
  } else {
    DEBUG_PRINT("Sent delegate sync message to %s", client->client_ip);
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

  INFO_PRINT("Data Message: %s", MESSAGE);

  char tmp_token[SYNC_TOKEN_LEN + 1] = {0};

  // 1) Parse root JSON
  cJSON *root = cJSON_Parse(MESSAGE);
  if (!root) {
    ERROR_PRINT("Failed to parse root JSON message");
    return;
  }

  // 2) Get "json" container (must be an object of numbered docs)
  cJSON *json_field = cJSON_GetObjectItemCaseSensitive(root, "json");
  if (!json_field || !cJSON_IsObject(json_field)) {
    ERROR_PRINT("Field 'json' not found or not a JSON object");
    cJSON_Delete(root);
    return;
  }

  // 3) Extract and verify sync_token (inside "json")
  cJSON *token_item = cJSON_GetObjectItemCaseSensitive(json_field, "sync_token");
  if (!token_item || !cJSON_IsString(token_item) || !token_item->valuestring) {
    ERROR_PRINT("Field 'sync_token' not found or not a valid string");
    cJSON_Delete(root);
    return;
  }
  size_t toklen = strnlen(token_item->valuestring, SYNC_TOKEN_LEN + 1);
  if (toklen != SYNC_TOKEN_LEN) {
    ERROR_PRINT("sync_token wrong length: %zu (expected %d)", toklen, SYNC_TOKEN_LEN);
    cJSON_Delete(root);
    return;
  }
  memcpy(tmp_token, token_item->valuestring, SYNC_TOKEN_LEN);
  tmp_token[SYNC_TOKEN_LEN] = '\0';

  if (strcmp(tmp_token, sync_token) != 0) {
    ERROR_PRINT("Skipping db sync, invalid sync_token received: %s", tmp_token);
    cJSON_Delete(root);
    return;
  }

  // 4) Remove sync_token so the payload shape matches the old one (object of numbered docs)
  cJSON_DeleteItemFromObjectCaseSensitive(json_field, "sync_token");

  // (Optional) quick sanity: ensure there is at least one numbered object
  int doc_count = 0;
  for (cJSON *it = json_field->child; it; it = it->next) {
    if (it->string && cJSON_IsObject(it)) ++doc_count;
  }
  if (doc_count == 0) {
    ERROR_PRINT("DB sync payload empty: no documents under 'json'");
    cJSON_Delete(root);
    return;
  }
  INFO_PRINT("DB sync payload object contains %d documents", doc_count);

  // 5) Serialize the OBJECT (not an array) exactly like before
  char *json_compact = cJSON_PrintUnformatted(json_field);
  cJSON_Delete(root);
  if (!json_compact) {
    ERROR_PRINT("Failed to serialize 'json' object");
    return;
  }
  if (json_compact[0] != '{') {
    ERROR_PRINT("Upsert expects an object of documents; got non-object JSON");
    free(json_compact);
    return;
  }

  // 6) Convert JSON -> BSON
  bson_error_t error;
  bson_t *doc = bson_new_from_json((const uint8_t *)json_compact, -1, &error);
  free(json_compact);

  if (!doc) {
    ERROR_PRINT("Failed to parse BSON from JSON: %s", error.message);
    return;
  }
  if (doc->len == 0) {
    ERROR_PRINT("Constructed BSON has zero length");
    bson_destroy(doc);
    return;
  }

  // 7) Replace collection contents
  pthread_mutex_lock(&delegates_all_lock);

  if (!db_drop(DATABASE_NAME, DB_COLLECTION_DELEGATES, &error)) {
    ERROR_PRINT("Failed to clear old delegates table before sync: %s", error.message);
    pthread_mutex_unlock(&delegates_all_lock);
    bson_destroy(doc);
    return;
  }

  if (!db_upsert_multi_docs(DATABASE_NAME, DB_COLLECTION_DELEGATES, doc, &error)) {
    ERROR_PRINT("Failed to upsert delegates sync data: %s", error.message);
    pthread_mutex_unlock(&delegates_all_lock);
    bson_destroy(doc);
    return;
  }

  pthread_mutex_unlock(&delegates_all_lock);
  bson_destroy(doc);

  if(add_indexes()) {
    ERROR_PRINT("Failed to create index on ")
    return;
  }

  INFO_PRINT("Successfully updated delegates database from sync message");
}