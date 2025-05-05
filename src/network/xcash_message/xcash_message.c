#include "xcash_message.h"

const xcash_msg_t WALLET_SIGN_MESSAGES[] = {
    XMSG_NODE_TO_NETWORK_DATA_NODES_GET_PREVIOUS_CURRENT_NEXT_BLOCK_VERIFIERS_LIST,
    XMSG_NODE_TO_BLOCK_VERIFIERS_ADD_RESERVE_PROOF,
    XMSG_NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE,
    XMSG_NODES_TO_BLOCK_VERIFIERS_UPDATE_DELEGATE,
    XMSG_NONE};
const size_t WALLET_SIGN_MESSAGES_COUNT = ARRAY_SIZE(WALLET_SIGN_MESSAGES) - 1;

const xcash_msg_t UNSIGNED_MESSAGES[] = {
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_ONLINE_STATUS,
    XMSG_GET_CURRENT_BLOCK_HEIGHT,
    XMSG_XCASH_GET_SYNC_INFO,
    XMSG_NONE};
const size_t UNSIGNED_MESSAGES_COUNT = ARRAY_SIZE(UNSIGNED_MESSAGES) - 1;

const xcash_msg_t NONRETURN_MESSAGES[] = {
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_ONLINE_STATUS,
    XMSG_NETWORK_DATA_NODES_TO_NETWORK_DATA_NODES_DATABASE_SYNC_CHECK,
    XMSG_SEND_CURRENT_BLOCK_HEIGHT,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_VRF_DATA,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_BLOCK_BLOB_SIGNATURE,
    XMSG_NODES_TO_NODES_VOTE_RESULTS,
    XMSG_NODES_TO_NODES_VOTE_MAJORITY_RESULTS,
    XMSG_MAIN_NODES_TO_NODES_PART_4_OF_ROUND_CREATE_NEW_BLOCK,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_INVALID_RESERVE_PROOFS,
    XMSG_MAIN_NETWORK_DATA_NODE_TO_BLOCK_VERIFIERS_START_BLOCK,
    XMSG_NONE};
const size_t NONRETURN_MESSAGES_COUNT = ARRAY_SIZE(NONRETURN_MESSAGES) - 1;

// Checks if a message is unsigned
bool is_unsigned_type(xcash_msg_t msg) {
  for (size_t i = 0; i < UNSIGNED_MESSAGES_COUNT; i++) {
    if (msg == UNSIGNED_MESSAGES[i]) {
      return true;
    }
  }
  return false;
}

// Checks if a message requires wallet signature
bool is_walletsign_type(xcash_msg_t msg) {
  for (size_t i = 0; i < WALLET_SIGN_MESSAGES_COUNT; i++) {
    if (msg == WALLET_SIGN_MESSAGES[i]) {
      return true;
    }
  }
  return false;
}

// Checks if a message does not require a return (nonblocking)
bool is_nonreturn_type(xcash_msg_t msg) {
  for (size_t i = 0; i < NONRETURN_MESSAGES_COUNT; i++) {
    if (msg == NONRETURN_MESSAGES[i]) {
      return true;
    }
  }
  return false;
}

// Sign a message in message_buf using local node's private key
bool sign_message(char* message_buf, size_t message_buf_size) {
  (void)message_buf_size;               // Currently unused
  int result = sign_data(message_buf);  // sign_data presumably modifies message_buf
  return (result == 1);
}

// Sign a message in message_buf using wallet's private key
bool sign_message_by_wallet(char* message_buf, size_t message_buf_size) {
  (void)message_buf_size;  // Currently unused
  int result = sign_data(message_buf);
  return (result == 1);
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
      DEBUG_PRINT("Mismatched param key/value for message %s", xcash_net_messages[msg]);
      break;
    }
    snprintf(message_buf + message_offset, sizeof(message_buf) - message_offset,
             ",\r\n \"%s\": \"%s\"", param_key, param_value);
    message_offset = (int)strlen(message_buf);
  }

  strncat(message_buf, "\r\n}\r\n", sizeof(message_buf) - strlen(message_buf) - 1);

  // If message is signed
  if (!is_unsigned_type(msg)) {
    if (!sign_message(message_buf, sizeof(message_buf))) {
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
    DEBUG_PRINT("Received JSON message");
  
    cJSON *json_obj = cJSON_Parse(data);
    if (!json_obj) {
      ERROR_PRINT("Invalid message received, JSON parsing error in handle_srv_message: %s", cJSON_GetErrorPtr());
      return;
    }
  
    cJSON *settings_obj = cJSON_GetObjectItemCaseSensitive(json_obj, "message_settings");
    if (!cJSON_IsString(settings_obj) || (settings_obj->valuestring == NULL)) {
      ERROR_PRINT("Invalid message received, missing or invalid message_settings");
      cJSON_Delete(json_obj);
      return;
    }
  
    snprintf(trans_type, sizeof(trans_type), "%s", settings_obj->valuestring);
    cJSON_Delete(json_obj);
  }
  else if (strstr(data, "|")) {  

    DEBUG_PRINT("........ADD BAR DATATYPE HERE.............");

    // set trans_type 

  } else {
    ERROR_PRINT("Message does not match one of the expected format");
    return;
  }

  DEBUG_PRINT("Transaction Type: %s", trans_type);

  xcash_msg_t msg_type = get_message_type(trans_type);
  switch (msg_type) {
  //  case XMSG_XCASH_GET_BLOCK_HASH:
  //    if (server_limit_IP_addresses(1, client->client_ip) == 1) {
  //      server_received_msg_get_block_hash(client, data);
  //      server_limit_IP_addresses(0, client->client_ip);
  //    }
  //    break;

    case XMSG_XCASH_GET_SYNC_INFO:
//      if (server_limit_IP_addresses(1, client->client_ip) == 1) {
        server_received_msg_get_sync_info(client, data);
//        server_limit_IP_addresses(0, client->client_ip);
//      }
      break;

    case XMSG_XCASH_GET_BLOCK_PRODUCERS:
      if (server_limit_IP_addresses(1, client->client_ip) == 1) {
        server_received_msg_get_block_producers(client, data);
        server_limit_IP_addresses(0, client->client_ip);
      }
      break;

//    case XMSG_GET_CURRENT_BLOCK_HEIGHT:
//      if (server_limit_IP_addresses(1, client->client_ip) == 1) {
//        server_receive_data_socket_get_current_block_height(client->client_ip);
//        server_limit_IP_addresses(0, client->client_ip);
//      }
//      break;

//    case XMSG_SEND_CURRENT_BLOCK_HEIGHT:
//      if (server_limit_IP_addresses(1, client->client_ip) == 1) {
//        pthread_mutex_lock(&update_current_block_height_lock);
//        server_receive_data_socket_send_current_block_height(data);
//        pthread_mutex_unlock(&update_current_block_height_lock);
//        server_limit_IP_addresses(0, client->client_ip);
//      }
//      break;

//    case XMSG_NODE_TO_NETWORK_DATA_NODES_GET_PREVIOUS_CURRENT_NEXT_BLOCK_VERIFIERS_LIST:
//      if (is_seed_node) {
//        if (server_limit_public_addresses(1, data) == 1) {
//          server_receive_data_socket_node_to_network_data_nodes_get_previous_current_next_block_verifiers_list(client);
//          server_limit_public_addresses(3, data);
//        }
//      }
//      break;

//    case XMSG_NODE_TO_NETWORK_DATA_NODES_GET_CURRENT_BLOCK_VERIFIERS_LIST:
//      if ((strstr(data, "\"public_address\"") != NULL && server_limit_public_addresses(1, data) == 1) ||
//          (strstr(data, "\"public_address\"") == NULL && server_limit_IP_addresses(1, client->client_ip) == 1)) {
//        server_receive_data_socket_node_to_network_data_nodes_get_current_block_verifiers_list(client);
//        strstr(data, "\"public_address\"") != NULL ? server_limit_public_addresses(3, data)
//                                                   : server_limit_IP_addresses(0, client->client_ip);
//      }
//    break;

//    case XMSG_NETWORK_DATA_NODES_TO_NETWORK_DATA_NODES_DATABASE_SYNC_CHECK:
//      if (server_limit_IP_addresses(1, client->client_ip) == 1) {
//        server_receive_data_socket_network_data_nodes_to_network_data_nodes_database_sync_check(data);
//        server_limit_IP_addresses(0, client->client_ip);
//      }
//      break;

//    case XMSG_NODE_TO_BLOCK_VERIFIERS_GET_RESERVE_BYTES_DATABASE_HASH:
//      if ((strstr(data, "|") != NULL && server_limit_public_addresses(2, data) == 1) ||
//          (strstr(data, "|") == NULL && server_limit_IP_addresses(1, client->client_ip) == 1)) {
//        server_receive_data_socket_node_to_block_verifiers_get_reserve_bytes_database_hash(client, data);
//        strstr(data, "|") != NULL ? server_limit_public_addresses(4, data)
//                                  : server_limit_IP_addresses(0, client->client_ip);
//      }
//      break;

//    case XMSG_NODE_TO_BLOCK_VERIFIERS_CHECK_IF_CURRENT_BLOCK_VERIFIER:
//      if (server_limit_IP_addresses(1, client->client_ip) == 1) {
//        server_receive_data_socket_node_to_block_verifiers_check_if_current_block_verifier(client);
//        server_limit_IP_addresses(0, client->client_ip);
//      }
//      break;

//    case XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_SYNC_CHECK_ALL_UPDATE:
//      if (server_limit_public_addresses(1, data) == 1) {
//        server_receive_data_socket_block_verifiers_to_block_verifiers_reserve_proofs_database_sync_check_all_update(client, data);
//        server_limit_public_addresses(3, data);
//      }
//      break;

//    case XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_DOWNLOAD_FILE_UPDATE:
//      if (server_limit_public_addresses(1, data) == 1) {
//        server_receive_data_socket_block_verifiers_to_block_verifiers_reserve_proofs_database_download_file_update(client, data);
//        server_limit_public_addresses(3, data);
//      }
//      break;

//    case XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_SYNC_CHECK_ALL_UPDATE:
//      if (server_limit_public_addresses(1, data) == 1) {
//        server_receive_data_socket_block_verifiers_to_block_verifiers_reserve_bytes_database_sync_check_all_update(client, data);
//        server_limit_public_addresses(3, data);
//      }
//      break;

//    case XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_DOWNLOAD_FILE_UPDATE:
//      if (server_limit_public_addresses(1, data) == 1) {
//        server_receive_data_socket_block_verifiers_to_block_verifiers_reserve_bytes_database_download_file_update(client, data);
//        server_limit_public_addresses(3, data);
//      }
//      break;

//    case XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_SYNC_CHECK_UPDATE:
//      if (server_limit_public_addresses(1, data) == 1) {
//        server_receive_data_socket_block_verifiers_to_block_verifiers_delegates_database_sync_check_update(client, data);
//        server_limit_public_addresses(3, data);
//      }
//      break;

//    case XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_DOWNLOAD_FILE_UPDATE:
//      if (server_limit_public_addresses(1, data) == 1) {
//        server_receive_data_socket_block_verifiers_to_block_verifiers_delegates_database_download_file_update(client);
//        server_limit_public_addresses(3, data);
//      }
//      break;

//    case XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_SYNC_CHECK_UPDATE:
//      if (server_limit_public_addresses(1, data) == 1) {
//        server_receive_data_socket_block_verifiers_to_block_verifiers_statistics_database_sync_check_update(client, data);
//        server_limit_public_addresses(3, data);
//      }
//      break;

//    case XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_DOWNLOAD_FILE_UPDATE:
//      if (server_limit_public_addresses(1, data) == 1) {
//        server_receive_data_socket_block_verifiers_to_block_verifiers_statistics_database_download_file_update(client);
//        server_limit_public_addresses(3, data);
//      }
//      break;

//    case XMSG_NODE_TO_BLOCK_VERIFIERS_ADD_RESERVE_PROOF:
//      if (server_limit_IP_addresses(1, client->client_ip) == 1) {
//        pthread_mutex_lock(&add_reserve_proof_lock);
//        server_receive_data_socket_node_to_block_verifiers_add_reserve_proof(client, data);
//        pthread_mutex_unlock(&add_reserve_proof_lock);
//        server_limit_IP_addresses(0, client->client_ip);
//      }
//      break;

//    case XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_INVALID_RESERVE_PROOFS:
//      if (server_limit_public_addresses(1, data) == 1) {
//        pthread_mutex_lock(&invalid_reserve_proof_lock);
//        server_receive_data_socket_block_verifiers_to_block_verifiers_invalid_reserve_proofs(data);
//        pthread_mutex_unlock(&invalid_reserve_proof_lock);
//        server_limit_public_addresses(3, data);
//      }
//      break;

//    case XMSG_NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE:
//      if (server_limit_IP_addresses(1, client->client_ip) == 1) {
//        server_receive_data_socket_nodes_to_block_verifiers_register_delegates(client, data);
//        server_limit_IP_addresses(0, client->client_ip);
//      }
//      break;

//    case XMSG_NODES_TO_BLOCK_VERIFIERS_UPDATE_DELEGATE:
//      if (server_limit_IP_addresses(1, client->client_ip) == 1) {
//        server_receive_data_socket_nodes_to_block_verifiers_update_delegates(client, data);
//        server_limit_IP_addresses(0, client->client_ip);
//      }
//      break;

//    case XMSG_NODE_TO_NETWORK_DATA_NODES_CHECK_VOTE_STATUS:
//      if (server_limit_IP_addresses(1, client->client_ip) == 1) {
//       server_receive_data_socket_nodes_to_network_data_nodes_check_vote_status(client, data);
//        server_limit_IP_addresses(0, client->client_ip);
//      }
//      break;

//    case XMSG_BLOCK_VERIFIERS_TO_NETWORK_DATA_NODE_BLOCK_VERIFIERS_CURRENT_TIME:
//      if (server_limit_public_addresses(1, data) == 1) {
//        server_receive_data_socket_block_verifiers_to_network_data_nodes_block_verifiers_current_time(client);
//        server_limit_public_addresses(3, data);
//      }
//      break;

//    case XMSG_MAIN_NETWORK_DATA_NODE_TO_BLOCK_VERIFIERS_START_BLOCK:
//      if (main_network_data_node_create_block == 1 && server_limit_public_addresses(1, data) == 1) {
//        server_receive_data_socket_main_network_data_node_to_block_verifier_start_block(data);
//        server_limit_public_addresses(3, data);
//      }
//      break;

//    case XMSG_MAIN_NODES_TO_NODES_PART_4_OF_ROUND_CREATE_NEW_BLOCK:
//      if (server_limit_public_addresses(1, data) == 1) {
//        server_receive_data_socket_main_node_to_node_message_part_4(data);
//        server_limit_public_addresses(3, data);
//      }
//      break;

    case XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_VRF_DATA:
    if (server_limit_IP_addresses(1, client->client_ip) == 1) {
      server_receive_data_socket_block_verifiers_to_block_verifiers_vrf_data(data);
      server_limit_IP_addresses(3, client->client_ip);
    }
    break;

//    case XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_BLOCK_BLOB_SIGNATURE:
//      if (server_limit_public_addresses(1, data) == 1) {
//        server_receive_data_socket_block_verifiers_to_block_verifiers_block_blob_signature(data);
//        server_limit_public_addresses(3, data);
//      }
//      break;

//    case XMSG_NODES_TO_NODES_VOTE_MAJORITY_RESULTS:
//      if (server_limit_public_addresses(1, data) == 1) {
//        server_receive_data_socket_node_to_node_majority(data);
//        server_limit_public_addresses(3, data);
//      }
//     break;

//    case XMSG_NODES_TO_NODES_VOTE_RESULTS:
//      if (server_limit_public_addresses(1, data) == 1) {
//        server_receive_data_socket_node_to_node(data);
//        server_limit_public_addresses(3, data);
//      }
//      break;

    default:
      ERROR_PRINT("Unknown message type received: %s", data);
      break;
  }

  return;
}