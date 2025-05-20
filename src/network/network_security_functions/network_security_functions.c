#include "network_security_functions.h"

// Helper function for error handling
int handle_error(const char *function_name, const char *message, char *result, char *string)
{
  ERROR_PRINT("%s: %s", function_name, message);
  free(result);
  free(string);
  return XCASH_ERROR;
} 

// Helper function to check if a message type is in the valid list
bool is_valid_message_type(const char *message_settings, const char *valid_types[], size_t valid_types_count) {
  for (size_t i = 0; i < valid_types_count; i++) {
      if (strcmp(message_settings, valid_types[i]) == 0) {
          return true;
      }
  }
  return false;
}

// Helper function for memory copy with safety checks
void safe_memcpy(char *dest, const char *src, size_t length)
{
  if (dest != NULL && src != NULL && length > 0)
  {
    memcpy(dest, src, length);
  }
}

int extract_data_between_delimiters(const char *message, int delimiter_index, char *output, size_t output_size) {
  int count = 0;
  const char *start = message;
  const char *end;

  while (count < delimiter_index && (start = strstr(start, "|")) != NULL) {
      start++;
      count++;
  }

  if (start != NULL && (end = strstr(start, "|")) != NULL) {
      size_t length = end - start;
      if (length >= output_size) {
          return XCASH_ERROR;
      }
      memcpy(output, start, length);
      output[length] = '\0';
      return XCASH_OK;
  }
  return XCASH_ERROR;
}

/*---------------------------------------------------------------------------------------------------------
Name: sign_data
Description: Signs data with your XCA address, for sending data securely
Parameters:
  message - The sign_data
Return: 0 if an error has occured, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int sign_data(char *message)
{
  const char *HTTP_HEADERS[] = {"Content-Type: application/json", "Accept: application/json"}; 
  const size_t HTTP_HEADERS_LENGTH = sizeof(HTTP_HEADERS) / sizeof(HTTP_HEADERS[0]);
  const size_t MAXIMUM_AMOUNT = MAXIMUM_BUFFER_SIZE * 2;

  char *result = calloc(MAXIMUM_AMOUNT, sizeof(char));
  char *string = calloc(MAXIMUM_AMOUNT, sizeof(char));
  if (!result || !string) {
    FATAL_ERROR_EXIT("sign_data: Memory allocation failed.");
  }

  char random_data[RANDOM_STRING_LENGTH + 1] = {0};
  char data[BUFFER_SIZE] = {0};

  if (!random_string(random_data, RANDOM_STRING_LENGTH)) {
    return handle_error("sign_data", "Failed to generate random data.", result, string);
  }

  // Ensure previous_block_hash is set
  if (pthread_rwlock_tryrdlock(&rwlock) != 0) {
    return handle_error("sign_data", "Failed to acquire read lock.", result, string);
  }

  if (strlen(previous_block_hash) == 0) {
    char temp_hash[BLOCK_HASH_LENGTH + 1] = {0};
    pthread_rwlock_unlock(&rwlock);
    if (!get_previous_block_hash(temp_hash)) {
      return handle_error("sign_data", "Previous block hash missing.", result, string);
    }
    pthread_rwlock_wrlock(&rwlock);
    strncpy(previous_block_hash, temp_hash, BLOCK_HASH_LENGTH);
    pthread_rwlock_unlock(&rwlock);
  } else {
    pthread_rwlock_unlock(&rwlock);
  }

  // Compose JSON payload
  snprintf(result, MAXIMUM_AMOUNT,
    "%s"
    "\"public_address\": \"%s\",\r\n"
    "\"previous_block_hash\": \"%s\",\r\n"
    "\"current_round_part\": \"%s\",\r\n"
    "\"data\": \"%.*s\"\r\n"
    "}",
    message,
    xcash_wallet_public_address,
    previous_block_hash,
    current_round_part,
    RANDOM_STRING_LENGTH, random_data
  );

  string_replace(result, MAXIMUM_AMOUNT, "\"", "\\\"");

  // Build JSON-RPC request to wallet RPC for signing
  snprintf(string, MAXIMUM_AMOUNT,
    "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"sign\",\"params\":{\"data\":\"%s\"}}",
    result
  );
  memset(result, 0, MAXIMUM_AMOUNT);

  if (send_http_request(data, XCASH_WALLET_IP, "/json_rpc", XCASH_WALLET_PORT,
                        "POST", HTTP_HEADERS, HTTP_HEADERS_LENGTH,
                        string, SEND_OR_RECEIVE_SOCKET_DATA_TIMEOUT_SETTINGS) <= 0 ||
      !parse_json_data(data, "signature", result, MAXIMUM_AMOUNT)) {
    return handle_error("sign_data", "Wallet signature failed.", result, string);
  }

  if (strlen(result) != XCASH_SIGN_DATA_LENGTH ||
      strncmp(result, XCASH_SIGN_DATA_PREFIX, sizeof(XCASH_SIGN_DATA_PREFIX) - 1) != 0) {
    return handle_error("sign_data", "Invalid wallet signature format.", result, string);
  }

  pthread_rwlock_rdlock(&rwlock);
  snprintf(message + strlen(message) - 1, MAXIMUM_AMOUNT - strlen(message),
    "\"public_address\": \"%s\",\r\n"
    "\"previous_block_hash\": \"%s\",\r\n"
    "\"current_round_part\": \"%s\",\r\n"
    "\"data\": \"%.*s\",\r\n"
    "\"XCASH_DPOPS_signature\": \"%s\"\r\n}",
    xcash_wallet_public_address,
    previous_block_hash,
    current_round_part,
    RANDOM_STRING_LENGTH, random_data,
    result
  );
  pthread_rwlock_unlock(&rwlock);

  free(result);
  free(string);
  return XCASH_OK;
}

/*-----------------------------------------------------------------------------------------------------------
Name: sign_block_blob

Description:
  Signs a given block blob (hex-encoded string) using the X-Cash wallet RPC. This function sends the blob
  to the wallet daemon over JSON-RPC and retrieves the cryptographic signature. The signature can then be
  embedded in the block as part of the `extra` field or used for block verification.

Parameters:
  block_blob_hex   - A null-terminated string containing the block blob in hex format.
  signature_out    - A buffer to store the resulting signature string.
  sig_out_len      - The size of the signature_out buffer (should be at least XCASH_SIGN_DATA_LENGTH + 1).

Returns:
  true  - If the signing process succeeds and the signature is extracted successfully.
  false - If the HTTP request fails, the response is invalid, or the signature cannot be parsed.
-----------------------------------------------------------------------------------------------------------*/
bool sign_block_blob(const char* block_blob_hex, char* signature_out, size_t sig_out_len) {
  char request_json[BUFFER_SIZE + 256];
  char response[BUFFER_SIZE];

  snprintf(request_json, sizeof(request_json),
    "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"sign\",\"params\":{\"data\":\"%s\"}}",
    block_blob_hex
  );

  const char* headers[] = { "Content-Type: application/json", "Accept: application/json" };
  if (send_http_request(response, XCASH_WALLET_IP, "/json_rpc", XCASH_WALLET_PORT,
                        "POST", headers, 2, request_json, SEND_OR_RECEIVE_SOCKET_DATA_TIMEOUT_SETTINGS) <= 0) {
    return false;
  }

  return parse_json_data(response, "result.signature", signature_out, sig_out_len);
}

/*---------------------------------------------------------------------------------------------------------
 * Name: verify_data
 * Description:
 *   Verifies the authenticity and integrity of signed messages within the DPoPS protocol.
 *   Supports both VRF-based signatures and wallet-based JSON-RPC signatures.
 *
 * Parameters:
 *   message - The complete signed message (in JSON or delimited format).
 *   VERIFY_CURRENT_ROUND_PART_SETTING - 
 *     Set to 1 to validate the "current_round_part" field against the expected round part.
 *     Set to 0 to skip round part validation.
 *
 * Return:
 *   0 if the signed data is not verified, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int verify_data(const char *MESSAGE, const int VERIFY_CURRENT_ROUND_PART_SETTING) {
  // Setup
  const char *HTTP_HEADERS[] = {"Content-Type: application/json", "Accept: application/json"};
  const size_t HTTP_HEADERS_LENGTH = sizeof(HTTP_HEADERS) / sizeof(HTTP_HEADERS[0]);
  const size_t MAXIMUM_AMOUNT = strlen(MESSAGE) >= MAXIMUM_BUFFER_SIZE ? MAXIMUM_BUFFER_SIZE : strlen(MESSAGE) + BUFFER_SIZE;

  char message_settings[BUFFER_SIZE] = {0};
  char public_address[XCASH_PUBLIC_ADDR_LENGTH + 1] = {0};
  char message_previous_block_hash[BUFFER_SIZE] = {0};
  char message_current_round_part[BUFFER_SIZE] = {0};
  char XCASH_DPOPS_signature[XCASH_SIGN_DATA_LENGTH + 1] = {0};
  char public_key[VRF_PUBLIC_KEY_LENGTH + 1] = {0};
  char proof[VRF_PROOF_LENGTH + 1] = {0};
  char beta_string[VRF_BETA_LENGTH + 1] = {0};

  unsigned char public_key_data[crypto_vrf_PUBLICKEYBYTES + 1] = {0};
  unsigned char proof_data[crypto_vrf_PROOFBYTES + 1] = {0};
  unsigned char beta_string_data[crypto_vrf_OUTPUTBYTES + 1] = {0};

  char *result = calloc(MAXIMUM_AMOUNT, sizeof(char));
  char *string = calloc(MAXIMUM_AMOUNT, sizeof(char));
  char data[BUFFER_SIZE] = {0};
  if (!result || !string) FATAL_ERROR_EXIT("verify_data: Memory allocation failed.");

  size_t message_length = 0;
  long long int number = 0;

  // Detect message format and extract message_settings
  if (strstr(MESSAGE, "}") != NULL) {
    if (!parse_json_data(MESSAGE, "message_settings", message_settings, sizeof(message_settings))) {
      return handle_error("verify_data", "Could not parse message_settings", result, string);
    }
  } else {
    const char *delimiter = strstr(MESSAGE, "|");
    if (!delimiter) return handle_error("verify_data", "Invalid message format", result, string);
    size_t len = delimiter - MESSAGE;
    len = len < sizeof(message_settings) ? len : sizeof(message_settings) - 1;
    memcpy(message_settings, MESSAGE, len);
    message_settings[len] = '\0';
  }

  // Determine if it's a special format
  const char *special_types[] = {
    "NODE_TO_NETWORK_DATA_NODES_GET_PREVIOUS_CURRENT_NEXT_BLOCK_VERIFIERS_LIST",
    "NODE_TO_NETWORK_DATA_NODES_GET_CURRENT_BLOCK_VERIFIERS_LIST",
    "NODE_TO_BLOCK_VERIFIERS_GET_RESERVE_BYTES_DATABASE_HASH",
    "XCASH_PROOF_OF_STAKE_TEST_DATA",
    "XCASH_PROOF_OF_STAKE_TEST_DATA_2",
    "NODE_TO_BLOCK_VERIFIERS_ADD_RESERVE_PROOF",
    "NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE",
    "NODES_TO_BLOCK_VERIFIERS_UPDATE_DELEGATE"
  };
  bool special_type = is_valid_message_type(message_settings, special_types, sizeof(special_types)/sizeof(special_types[0]));

  // Extract public_address and signature from either JSON or delimited format
  if (special_type && strstr(MESSAGE, "}") == NULL) {
    if (strcmp(message_settings, "NODE_TO_BLOCK_VERIFIERS_ADD_RESERVE_PROOF") == 0 && string_count(MESSAGE, "|") == VOTE_PARAMETER_AMOUNT) {
      extract_data_between_delimiters(MESSAGE, 3, public_address, XCASH_WALLET_LENGTH);
      extract_data_between_delimiters(MESSAGE, 4, XCASH_DPOPS_signature, XCASH_SIGN_DATA_LENGTH);
    } else if (strcmp(message_settings, "NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE") == 0 && string_count(MESSAGE, "|") == REGISTER_PARAMETER_AMOUNT) {
      extract_data_between_delimiters(MESSAGE, 4, public_address, XCASH_WALLET_LENGTH);
      extract_data_between_delimiters(MESSAGE, 5, XCASH_DPOPS_signature, XCASH_SIGN_DATA_LENGTH);
    } else if (strcmp(message_settings, "NODES_TO_BLOCK_VERIFIERS_UPDATE_DELEGATE") == 0 && string_count(MESSAGE, "|") == UPDATE_PARAMETER_AMOUNT) {
      extract_data_between_delimiters(MESSAGE, 3, public_address, XCASH_WALLET_LENGTH);
      extract_data_between_delimiters(MESSAGE, 4, XCASH_DPOPS_signature, XCASH_SIGN_DATA_LENGTH);
    } else if (strcmp(message_settings, "NODE_TO_BLOCK_VERIFIERS_GET_RESERVE_BYTES_DATABASE_HASH") == 0) {
      extract_data_between_delimiters(MESSAGE, 2, public_address, XCASH_WALLET_LENGTH);
      extract_data_between_delimiters(MESSAGE, 4, XCASH_DPOPS_signature, VRF_PROOF_LENGTH + VRF_BETA_LENGTH);
    } else {
      return handle_error("verify_data", "Invalid special format message", result, string);
    }
  } else {
    if (!parse_json_data(MESSAGE, "public_address", public_address, sizeof(public_address)) ||
        !parse_json_data(MESSAGE, "previous_block_hash", message_previous_block_hash, sizeof(message_previous_block_hash)) ||
        !parse_json_data(MESSAGE, "current_round_part", message_current_round_part, sizeof(message_current_round_part)) ||
        !parse_json_data(MESSAGE, "XCASH_DPOPS_signature", XCASH_DPOPS_signature, sizeof(XCASH_DPOPS_signature))) {
      return handle_error("verify_data", "Could not parse standard message fields", result, string);
    }
  }

  // Round validation logic
  sscanf(current_block_height, "%lld", &number);
  if (number >= XCASH_PROOF_OF_STAKE_BLOCK_HEIGHT) {
    const char *valid_hash_types[] = {
      "NODE_TO_BLOCK_VERIFIERS_ADD_RESERVE_PROOF", "NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE",
      "NODES_TO_BLOCK_VERIFIERS_UPDATE_DELEGATE", "XCASH_PROOF_OF_STAKE_TEST_DATA", "XCASH_PROOF_OF_STAKE_TEST_DATA_2"
    };

    if (!is_valid_message_type(message_settings, valid_hash_types, sizeof(valid_hash_types)/sizeof(valid_hash_types[0]))) {
      if (strcmp(previous_block_hash, message_previous_block_hash) != 0) {
        return handle_error("verify_data", "Invalid previous block hash", result, string);
      }

      if (VERIFY_CURRENT_ROUND_PART_SETTING == 1 &&
          strncmp(current_round_part, message_current_round_part, 1) != 0) {
        return handle_error("verify_data", "Invalid round part", result, string);
      }
    }
  }

  // Construct unsigned message
  if (special_type && strstr(MESSAGE, "previous_block_hash") == NULL) {
    message_length = strlen(MESSAGE) - 94;
    safe_memcpy(result, MESSAGE, message_length);
  } else {
    message_length = (strstr(MESSAGE, "SigV1") == NULL) ? strlen(MESSAGE) - 320 : strlen(MESSAGE) - 125;
    safe_memcpy(result, MESSAGE, message_length);
    safe_memcpy(result + message_length, "}", 2);
    string_replace(result, MAXIMUM_AMOUNT, "\"", "\\\"");
  }

  // Signature verification
  if (strstr(MESSAGE, "SigV1") == NULL) {
    safe_memcpy(proof, XCASH_DPOPS_signature, VRF_PROOF_LENGTH);
    safe_memcpy(beta_string, &XCASH_DPOPS_signature[VRF_PROOF_LENGTH], VRF_BETA_LENGTH);

    // Lookup public key
    bool pub_key_found = false;
    for (int i = 0; network_nodes[i].seed_public_address; i++) {
      if (strcmp(network_nodes[i].seed_public_address, public_address) == 0) {
        strncpy(public_key, network_nodes[i].seed_public_key, VRF_PUBLIC_KEY_LENGTH);
        pub_key_found = true;
        break;
      }
    }
    if (!pub_key_found) {
      for (size_t i = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
        if (strncmp(current_block_verifiers_list.block_verifiers_public_address[i], public_address, XCASH_WALLET_LENGTH) == 0) {
          strncpy(public_key, current_block_verifiers_list.block_verifiers_public_key[i], VRF_PUBLIC_KEY_LENGTH);
          pub_key_found = true;
          break;
        }
      }
    }
    if (!pub_key_found) {
      char query[BUFFER_SIZE];
      snprintf(query, sizeof(query), "{\"public_address\":\"%s\"}", public_address);
      if (count_documents_in_collection(DATABASE_NAME, "delegates", query) == 1) {
        if (!read_document_field_from_collection(DATABASE_NAME, "delegates", query, "public_key", public_key)) {
          return handle_error("verify_data", "Could not read public key from DB", result, string);
        }
      } else {
        return handle_error("verify_data", "Public key not found", result, string);
      }
    }

    // Decode hex to binary and verify
    if (hex_to_byte_array(public_key, public_key_data, sizeof(public_key_data)) != XCASH_OK ||
        hex_to_byte_array(proof, proof_data, sizeof(proof_data)) != XCASH_OK ||
        hex_to_byte_array(beta_string, beta_string_data, sizeof(beta_string_data)) != XCASH_OK ||
        crypto_vrf_verify(beta_string_data, public_key_data, proof_data, (unsigned char *)result, strlen(result)) != 0) {
      return handle_error("verify_data", "Invalid signature (VRF)", result, string);
    }
  } else {
    snprintf(string, MAXIMUM_AMOUNT,
      "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"verify\",\"params\":{\"data\":\"%s\",\"address\":\"%s\",\"signature\":\"%s\"}}",
      result, public_address, XCASH_DPOPS_signature);

    if (send_http_request(result, XCASH_WALLET_IP, "/json_rpc", XCASH_WALLET_PORT,
                          "POST", HTTP_HEADERS, HTTP_HEADERS_LENGTH, string,
                          SEND_OR_RECEIVE_SOCKET_DATA_TIMEOUT_SETTINGS) <= 0 ||
        !parse_json_data(result, "good", data, sizeof(data)) ||
        strncmp(data, "true", 4) != 0) {
      return handle_error("verify_data", "Signature not valid (wallet RPC)", result, string);
    }
  }

  free(result);
  free(string);
  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: validate_data
Description: Validates that only certain nodes can request certain messages
Parameters:
  message - The data
Return: 0 if the data is not validated, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int validate_data(const char *MESSAGE) {
  char data[BUFFER_SIZE] = {0};
  const char *type;

  if ((type = strstr(MESSAGE, "NODE_TO_BLOCK_VERIFIERS_ADD_RESERVE_PROOF")) ||
      (type = strstr(MESSAGE, "XCASH_GET_BLOCK_PRODUCERS")) ||
      (type = strstr(MESSAGE, "NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE")) ||
      (type = strstr(MESSAGE, "NODE_TO_NETWORK_DATA_NODES_CHECK_VOTE_STATUS")) ||
      (type = strstr(MESSAGE, "NODES_TO_BLOCK_VERIFIERS_UPDATE_DELEGATE")) ||
      (type = strstr(MESSAGE, "NODES_TO_BLOCK_VERIFIERS_RECOVER_DELEGATE")) ||
      (type = strstr(MESSAGE, "NODE_TO_BLOCK_VERIFIERS_GET_RESERVE_BYTES_DATABASE_HASH")) ||
      (type = strstr(MESSAGE, "BLOCK_VERIFIERS_TO_NODES_RESERVE_BYTES_DATABASE_SYNC_CHECK_ALL_DOWNLOAD"))) {
      return XCASH_OK;
  }

  // Helper: validate string equals
  #define REQUIRE_STR(field, expected) \
      (parse_json_data(MESSAGE, field, data, sizeof(data)) == 0 || strncmp(data, expected, strlen(expected)) != 0)

  // Helper: validate length match
  #define REQUIRE_LEN(field, len) \
      (parse_json_data(MESSAGE, field, data, sizeof(data)) == 0 || strlen(data) != len)

  // Helper: validate prefix
  #define REQUIRE_PREFIX(field, prefix) \
      (parse_json_data(MESSAGE, field, data, sizeof(data)) == 0 || \
      strncmp(data, prefix, strlen(prefix)) != 0)

  if (strstr(MESSAGE, "XCASH_GET_SYNC_INFO") && REQUIRE_STR("message_settings", "XCASH_GET_SYNC_INFO")) goto error;
  if (strstr(MESSAGE, "XCASH_GET_BLOCK_HASH") && REQUIRE_STR("message_settings", "XCASH_GET_BLOCK_HASH")) goto error;
  if (strstr(MESSAGE, "GET_CURRENT_BLOCK_HEIGHT") && REQUIRE_STR("message_settings", "GET_CURRENT_BLOCK_HEIGHT")) goto error;

  if (strstr(MESSAGE, "SEND_CURRENT_BLOCK_HEIGHT")) {
      if (REQUIRE_STR("message_settings", "SEND_CURRENT_BLOCK_HEIGHT") ||
          REQUIRE_LEN("block_height", 1) ||
          REQUIRE_LEN("public_address", XCASH_WALLET_LENGTH) || 
          REQUIRE_PREFIX("public_address", XCASH_WALLET_PREFIX) ||
          REQUIRE_LEN("previous_block_hash", BLOCK_HASH_LENGTH) ||
          REQUIRE_LEN("current_round_part", 1) ||
          REQUIRE_LEN("data", RANDOM_STRING_LENGTH) ||
          REQUIRE_LEN("XCASH_DPOPS_signature", VRF_BETA_LENGTH + VRF_PROOF_LENGTH)) goto error;
  }

  // Add more refined, grouped handlers here as needed
  // For example: messages involving "block_blob", "database_data", "current_time", etc.

  // Fallback invalid
  return XCASH_OK;

error:
  ERROR_PRINT("Invalid message");
  return XCASH_ERROR;

  #undef REQUIRE_STR
  #undef REQUIRE_LEN
  #undef REQUIRE_PREFIX
}