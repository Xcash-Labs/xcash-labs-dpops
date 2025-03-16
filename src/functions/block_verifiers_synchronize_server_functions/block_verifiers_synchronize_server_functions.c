#include "block_verifiers_synchronize_server_functions.h"

void server_received_msg_get_block_hash(server_client_t* client, const char* MESSAGE)
{
    ("Received %s, %s", __func__, "XCASH_GET_BLOCK_HASH");

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
    json_object_set_new(reply_json, "message_settings", json_string("XCASH_GET_BLOCK_HASH"));
    json_object_set_new(reply_json, "public_address", json_string(xcash_wallet_public_address));
    json_object_set_new(reply_json, "block_hash", json_string(block_hash));

    char *message_result_data = json_dumps(reply_json, JSON_COMPACT);
    json_decref(reply_json);

    if (message_result_data == NULL) {
        ERROR_PRINT("Failed to serialize reply JSON");
        return;
    }

    size_t message_result_size = strlen(message_result_data);
    size_t total_size = message_result_size + strlen(SOCKET_END_STRING) + 1; // +1 for null terminator

    char *final_message = malloc(total_size);
    if (final_message == NULL) {
        log_error("Memory allocation failed");
        free(message_result_data);
        return;
    }

    strcpy(final_message, message_result_data);

    send_data_uv(client, message);

    free(message_result_data);
    free(final_message);
}