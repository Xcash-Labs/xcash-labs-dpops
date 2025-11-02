#include "xcash_message.h"

const xcash_msg_t WALLET_SIGN_MESSAGES[] = {
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_VRF_DATA,
    XMSG_NODES_TO_NODES_VOTE_MAJORITY_RESULTS,
    XMSG_NODES_TO_NODES_DATABASE_SYNC_REQ,
    XMSG_SEED_TO_NODES_UPDATE_VOTE_COUNT,
    XMSG_SEED_TO_NODES_BANNED,
    XMSG_NONE};
const size_t WALLET_SIGN_MESSAGES_COUNT = ARRAY_SIZE(WALLET_SIGN_MESSAGES) - 1;

const xcash_msg_t WALLET_SIGN_ACTION_MESSAGES[] = {
    XMSG_NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE,
    XMSG_NODES_TO_BLOCK_VERIFIERS_UPDATE_DELEGATE,
    XMSG_NODES_TO_BLOCK_VERIFIERS_VOTE,
    XMSG_NODES_TO_BLOCK_VERIFIERS_REVOTE,
    XMSG_NONE};
const size_t WALLET_SIGN_ACTION_MESSAGES_COUNT = ARRAY_SIZE(WALLET_SIGN_ACTION_MESSAGES) - 1;

// Checks if a message requires wallet signature
bool is_walletsign_type(xcash_msg_t msg) {
  for (size_t i = 0; i < WALLET_SIGN_MESSAGES_COUNT; i++) {
    if (msg == WALLET_SIGN_MESSAGES[i]) {
      return true;
    }
  }
  return false;
}

// Checks if an action message (vote, register, ect) requires wallet signature
bool is_walletsign_action_type(xcash_msg_t msg) {
  for (size_t i = 0; i < WALLET_SIGN_ACTION_MESSAGES_COUNT; i++) {
    if (msg == WALLET_SIGN_ACTION_MESSAGES[i]) {
      return true;
    }
  }
  return false;
}

// Create a message with key-value parameters
char* create_message_param_list(xcash_msg_t msg, const char** pair_params) {
  char message_buf[BUFFER_SIZE];
  memset(message_buf, 0, sizeof(message_buf));

  snprintf(message_buf, sizeof(message_buf), "{\r\n \"message_settings\": \"%s\"", xcash_net_messages[msg]);
  int message_offset = (int)strlen(message_buf);

  size_t current_pair_index = 0;
  while (pair_params[current_pair_index]) {
    const char* param_key = pair_params[current_pair_index++];
    const char* param_value = pair_params[current_pair_index++];
    if (!param_value) {
      ERROR_PRINT("Mismatched param key/value for message %s", xcash_net_messages[msg]);
      break;
    }
    snprintf(message_buf + message_offset, sizeof(message_buf) - message_offset,
             ",\r\n \"%s\": \"%s\"", param_key, param_value);
    message_offset = (int)strlen(message_buf);
  }

  strncat(message_buf, "\r\n}", sizeof(message_buf) - strlen(message_buf) - 1);

  // Check if message is signed
  if (is_walletsign_type(msg)) {
    if (sign_data(message_buf) != XCASH_OK) {
      ERROR_PRINT("Failed to sign message: %s", xcash_net_messages[msg]);
      return NULL;
    }
  }

  return strdup(message_buf);
}

// Create a message from variadic arguments
char* create_message_args(xcash_msg_t msg, va_list args) {
  // Calculate how many key/value pairs
  va_list args_copy;
  va_copy(args_copy, args);

  size_t param_count = 0;
  while (va_arg(args_copy, char*)) {
    param_count++;
    // skip the next param in pair
    va_arg(args_copy, char*);
  }
  va_end(args_copy);

  // param_count x 2 but we store them in sequence. +1 for sentinel
  param_count = param_count * 2 + 1;
  const char** param_list = calloc(param_count, sizeof(char*));
  if (!param_list) {
    return NULL;
  }

  size_t index = 0;
  while (true) {
    char* key = va_arg(args, char*);
    if (!key) {
      break;
    }
    char* value = va_arg(args, char*);
    param_list[index++] = key;
    param_list[index++] = value;
  }
  param_list[index] = NULL;

  char* message = create_message_param_list(msg, param_list);
  if (!message) {
    free(param_list);
    return NULL;
  }

  free(param_list);
  return message;
}

// Create message with variadic parameters (exposed)
char* create_message_param(xcash_msg_t msg, ...) {
  va_list args;
  va_start(args, msg);
  char* message = create_message_args(msg, args);
  va_end(args);
  
  if (!message) {
    return NULL;
  }
  return message;
}

// Create a basic message with no parameters
char* create_message(xcash_msg_t msg) {
  return create_message_param(msg, NULL);
}

// Splits a string by delimiter, returning array of elements
int split(const char* str, char delimiter, char*** result_elements) {
  if (!str || !result_elements) return -1;

  int elemCount = 1;
  for (int i = 0; str[i]; i++) {
    if (str[i] == delimiter) {
      elemCount++;
    }
  }

  char** result = calloc(elemCount + 1, sizeof(char*));  // +1 for sentinel
  if (!result) return -1;

  int startIdx = 0, rIndex = 0;
  for (int i = 0;; i++) {
    if (str[i] == delimiter || str[i] == '\0') {
      int length = i - startIdx;
      result[rIndex] = malloc(length + 1);
      if (!result[rIndex]) {
        // free previously allocated
        for (int k = 0; k < rIndex; k++) {
          free(result[k]);
        }
        free(result);
        return -1;
      }
      strncpy(result[rIndex], &str[startIdx], length);
      result[rIndex][length] = '\0';
      rIndex++;
      startIdx = i + 1;

      if (str[i] == '\0') {
        break;
      }
    }
  }
  *result_elements = result;
  return rIndex;  // Number of elements
}

// Frees array created by split()
void cleanup_char_list(char** element_list) {
  if (!element_list) return;
  for (int i = 0; element_list[i]; i++) {
    free(element_list[i]);
  }
  free(element_list);
}

xcash_msg_t get_message_type(const char* data) {
  if (!data || *data == '\0') {
    return XMSG_NONE;  // Handle NULL or empty data safely
  }
  for (int i = 0; i < XMSG_MESSAGES_COUNT; i++) {
    if (strncmp(data, xcash_net_messages[i], strlen(xcash_net_messages[i])) == 0) {
      return (xcash_msg_t)i;
    }
  }
  return XMSG_NONE;  // Default case if no match is found
}

//
//  Handle Server Messages
//
void handle_srv_message(const char* data, size_t length, server_client_t* client) {
  if (data == NULL || length == 0) {
    ERROR_PRINT("Message received by server is null.");
    return;
  }

  if (!client) {
    ERROR_PRINT("handle_srv_message: Client data is NULL");
    return;
  }

  DEBUG_PRINT("Processing message from client IP: %s", client->client_ip);

  char trans_type[128] = {0};
  if (strstr(data, "{") && strstr(data, "}")) {
    cJSON* json_obj = cJSON_Parse(data);
    if (!json_obj) {
      ERROR_PRINT("Invalid message received, JSON parsing error in handle_srv_message: %s", cJSON_GetErrorPtr());
      return;
    }

    cJSON* settings_obj = cJSON_GetObjectItemCaseSensitive(json_obj, "message_settings");
    if (!cJSON_IsString(settings_obj) || (settings_obj->valuestring == NULL)) {
      ERROR_PRINT("Invalid message received, missing or invalid message_settings");
      cJSON_Delete(json_obj);
      return;
    }

    snprintf(trans_type, sizeof(trans_type), "%s", settings_obj->valuestring);
    cJSON_Delete(json_obj);

  } else {
    ERROR_PRINT("Message does not match expected JSON format");
    return;
  }
 
  xcash_msg_t msg_type = get_message_type(trans_type);

  // Must come from seed
  if ((msg_type == XMSG_SEED_TO_NODES_UPDATE_VOTE_COUNT || msg_type == XMSG_SEED_TO_NODES_PAYOUT || msg_type == XMSG_SEED_TO_NODES_BANNED)) {
    if (verify_the_ip(data, client->client_ip, true) != XCASH_OK) {
      ERROR_PRINT("IP seed check failed for msg_type=%s from %s", trans_type, client->client_ip);
      return;
    }
  // These messages can come from non-delegate wallets so the IP can not be verified
  } else if ((msg_type != XMSG_NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE) && 
    (msg_type != XMSG_NODE_TO_NETWORK_DATA_NODES_GET_CURRENT_BLOCK_VERIFIERS_LIST) &&
    (msg_type != XMSG_NODES_TO_BLOCK_VERIFIERS_VOTE) &&
    (msg_type != XMSG_NODES_TO_BLOCK_VERIFIERS_REVOTE) &&
    (msg_type != XMSG_NODES_TO_BLOCK_VERIFIERS_CHECK_VOTE_STATUS)) {
    if (verify_the_ip(data, client->client_ip, false) != XCASH_OK) {
      ERROR_PRINT("IP check failed for msg_type=%s from %s", trans_type, client->client_ip);
      return;
    }
  }

  if (is_walletsign_type(msg_type)) {
    if (verify_data(data, msg_type) == XCASH_ERROR) {
      if (startup_complete) {
        WARNING_PRINT("Failed to validate message sign data");
      }
      return;
    }
  }

  if (is_walletsign_action_type(msg_type)) {
    if (verify_action_data(data, client->client_ip, msg_type) == XCASH_ERROR) {
      ERROR_PRINT("Failed to validate action message sign data");
      return;
    }
  }

  switch (msg_type) {

    case XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_VRF_DATA:
      if (server_limit_IP_addresses(LIMIT_CHECK, client->client_ip) == 1) {
        server_receive_data_socket_block_verifiers_to_block_verifiers_vrf_data(data);
        server_limit_IP_addresses(LIMIT_REMOVE, client->client_ip);
      }
      break;

    case XMSG_NODES_TO_NODES_VOTE_MAJORITY_RESULTS:
      if (server_limit_IP_addresses(LIMIT_CHECK, client->client_ip) == 1) {        
        server_receive_data_socket_node_to_node_vote_majority(data);
        server_limit_IP_addresses(LIMIT_REMOVE, data);
      }
      break;

    case XMSG_NODE_TO_NETWORK_DATA_NODES_GET_CURRENT_BLOCK_VERIFIERS_LIST:
      if (server_limit_public_addresses(LIMIT_CHECK, data) == 1) {
        server_receive_data_socket_node_to_network_data_nodes_get_current_block_verifiers_list(client);
        server_limit_public_addresses(LIMIT_REMOVE, data);
      }
      break;

    case XMSG_NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE:
      if (server_limit_public_addresses(LIMIT_CHECK, data) == 1) {
        server_receive_data_socket_nodes_to_block_verifiers_register_delegates(client, data);
        server_limit_public_addresses(LIMIT_REMOVE, data);
      }
      break;

    case XMSG_NODES_TO_BLOCK_VERIFIERS_VOTE:
      if (server_limit_public_addresses(LIMIT_CHECK, data) == 1) {
        server_receive_data_socket_node_to_block_verifiers_add_reserve_proof(client, data);
        server_limit_public_addresses(LIMIT_REMOVE, data);
      }
      break;

    case XMSG_NODES_TO_BLOCK_VERIFIERS_REVOTE:
      if (server_limit_public_addresses(LIMIT_CHECK, data) == 1) {
        server_receive_data_socket_node_to_block_verifiers_add_reserve_proof(client, data);
        server_limit_public_addresses(LIMIT_REMOVE, data);
      }
      break;

    case XMSG_NODES_TO_BLOCK_VERIFIERS_CHECK_VOTE_STATUS:
      if (server_limit_public_addresses(LIMIT_CHECK, data) == 1) {
        server_receive_data_socket_node_to_block_verifiers_check_vote_status(client, data);
        server_limit_public_addresses(LIMIT_REMOVE, data);
      }
      break;

    case XMSG_NODES_TO_BLOCK_VERIFIERS_UPDATE_DELEGATE:
      if (server_limit_public_addresses(LIMIT_CHECK, data) == 1) {
        server_receive_data_socket_nodes_to_block_verifiers_update_delegates(client, data);
        server_limit_public_addresses(LIMIT_REMOVE, data);
      }
      break;

    case XMSG_NODES_TO_NODES_DATABASE_SYNC_REQ:
      if (server_limit_IP_addresses(LIMIT_CHECK, client->client_ip) == 1) {
        server_receive_data_socket_node_to_node_db_sync_req(client, data);
        server_limit_IP_addresses(LIMIT_REMOVE, client->client_ip);
      }
    break;

    case XMSG_NODES_TO_NODES_DATABASE_SYNC_DATA:
      if (server_limit_IP_addresses(LIMIT_CHECK, client->client_ip) == 1) {
        server_receive_data_socket_node_to_node_db_sync_data(data);
        server_limit_IP_addresses(LIMIT_REMOVE, client->client_ip);
      }
    break;

    case XMSG_XCASHD_TO_DPOPS_VERIFY:
      if (server_limit_IP_addresses(LIMIT_CHECK, client->client_ip) == 1) {
        server_receive_data_socket_nodes_to_block_verifiers_validate_block(client, data);
        server_limit_IP_addresses(LIMIT_REMOVE, client->client_ip);
      }
      break;

    case XMSG_SEED_TO_NODES_UPDATE_VOTE_COUNT:
      if (server_limit_IP_addresses(LIMIT_CHECK, client->client_ip) == 1) {
        server_receive_update_delegate_vote_count(data);
        server_limit_IP_addresses(LIMIT_REMOVE, client->client_ip);
      }
      break;

    case XMSG_SEED_TO_NODES_PAYOUT: {
      // Should always be false but just in case
      bool expected = false;
      if (!atomic_compare_exchange_strong_explicit(
              &payment_inprocess, &expected, true,
              memory_order_acq_rel,
              memory_order_relaxed))
      {
        ERROR_PRINT("Skipping payment processing, transaction currently in process");
        break;
      }

      if (server_limit_IP_addresses(LIMIT_CHECK, client->client_ip) == 1) {
        server_receive_payout(data); 
        server_limit_IP_addresses(LIMIT_REMOVE, client->client_ip);
      }

      // release the lock
      atomic_store_explicit(&payment_inprocess, false, memory_order_release);
      break;
    }

    case XMSG_SEED_TO_NODES_BANNED:
      if (server_limit_IP_addresses(LIMIT_CHECK, client->client_ip) == 1) {
        server_receive_banned_request(data);
        server_limit_IP_addresses(LIMIT_REMOVE, client->client_ip);
      }
      break;

    default:
      ERROR_PRINT("Unknown message type received: %s", data);
      break;
  }

  return;
}