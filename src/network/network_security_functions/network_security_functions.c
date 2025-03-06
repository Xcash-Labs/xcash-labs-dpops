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
  const size_t MAXIMUM_AMOUNT = strlen(message) >= MAXIMUM_BUFFER_SIZE ? MAXIMUM_BUFFER_SIZE : strlen(message) + BUFFER_SIZE;

  // Memory allocation
  char *result = (char *)calloc(MAXIMUM_AMOUNT, sizeof(char));
  char *string = (char *)calloc(MAXIMUM_AMOUNT, sizeof(char));
  if (result == NULL || string == NULL)
  {
    FATAL_ERROR_EXIT("sign_data: Memory allocation failed.");
  }

  // Variables initialization
  char random_data[RANDOM_STRING_LENGTH + 1] = {0};
  char proof[VRF_PROOF_LENGTH + 1] = {0};
  char beta_string[VRF_BETA_LENGTH + 1] = {0};
  char data[BUFFER_SIZE] = {0};

  // Generate random data
  if (!random_string(random_data, RANDOM_STRING_LENGTH))
  {
    return handle_error("sign_data", "Failed to generate random data.", result, string);
  }

  // Read lock for previous block hash
  pthread_rwlock_rdlock(&rwlock);

  // Ensure previous block hash is set
  if (strlen(previous_block_hash) == 0)
  {
    char local_previous_block_hash[BLOCK_HASH_LENGTH + 1] = {0};
    if (!get_previous_block_hash(local_previous_block_hash))
    {
      pthread_rwlock_unlock(&rwlock);
      return handle_error("sign_data", "Previous block hash is empty. Cannot sign the message.", result, string);
    }
    strncpy(previous_block_hash, local_previous_block_hash, BLOCK_HASH_LENGTH);
  }

  // Build the initial message
  // create the message
  memcpy(result, message, strlen(message) - 1);
  memcpy(result + strlen(result), "\"public_address\": \"", 19);
  memcpy(result + strlen(result), xcash_wallet_public_address, XCASH_WALLET_LENGTH);
  memcpy(result + strlen(result), "\",\r\n \"previous_block_hash\": \"", 29);
  memcpy(result + strlen(result), previous_block_hash, strnlen(previous_block_hash, sizeof(previous_block_hash)));
  memcpy(result + strlen(result), "\",\r\n \"current_round_part\": \"", 28);
  memcpy(result + strlen(result), current_round_part, sizeof(char));
  memcpy(result + strlen(result), "\",\r\n \"current_round_part_backup_node\": \"", 40);
  memcpy(result + strlen(result), current_round_part_backup_node, sizeof(char));
  memcpy(result + strlen(result), "\",\r\n \"data\": \"", 14);
  memcpy(result + strlen(result), random_data, RANDOM_STRING_LENGTH);
  memcpy(result + strlen(result), "\",\r\n}", 5);

  pthread_rwlock_unlock(&rwlock);

  // Escape quotes in the message
  string_replace(result, MAXIMUM_AMOUNT, "\"", "\\\"");

  // Determine signature method
  int use_vrf_signing =
      strstr(message, "NODE_TO_NETWORK_DATA_NODES_GET_PREVIOUS_CURRENT_NEXT_BLOCK_VERIFIERS_LIST") == NULL &&
      strstr(message, "NODE_TO_BLOCK_VERIFIERS_ADD_RESERVE_PROOF") == NULL &&
      strstr(message, "NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE") == NULL &&
      strstr(message, "NODES_TO_BLOCK_VERIFIERS_UPDATE_DELEGATE") == NULL;

  if (use_vrf_signing)
  {
    if (!VRF_sign_data(beta_string, proof, result))
    {
      return handle_error("sign_data", "Failed to sign the message with VRF.", result, string);
    }

    pthread_rwlock_rdlock(&rwlock);
    // create the message
    memcpy(message + strlen(message) - 1, "\"public_address\": \"", 19);
    memcpy(message + strlen(message), xcash_wallet_public_address, XCASH_WALLET_LENGTH);
    memcpy(message + strlen(message), "\",\r\n \"previous_block_hash\": \"", 29);
    memcpy(message + strlen(message), previous_block_hash,
            strnlen(previous_block_hash, sizeof(previous_block_hash)));
    memcpy(message + strlen(message), "\",\r\n \"current_round_part\": \"", 28);
    memcpy(message + strlen(message), current_round_part, sizeof(char));
    memcpy(message + strlen(message), "\",\r\n \"current_round_part_backup_node\": \"", 40);
    memcpy(message + strlen(message), current_round_part_backup_node, sizeof(char));
    memcpy(message + strlen(message), "\",\r\n \"data\": \"", 14);
    memcpy(message + strlen(message), random_data, RANDOM_STRING_LENGTH);
    memcpy(message + strlen(message), "\",\r\n \"XCASH_DPOPS_signature\": \"", 31);
    memcpy(message + strlen(message), proof, VRF_PROOF_LENGTH);
    memcpy(message + strlen(message), beta_string, VRF_BETA_LENGTH);
    memcpy(message + strlen(message), "\",\r\n}", 5);
    pthread_rwlock_unlock(&rwlock);
  }
  else
  {
      // sign_data
      memcpy(string, "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"sign\",\"params\":{\"data\":\"", 60);
      memcpy(string + 60, result, strnlen(result, MAXIMUM_AMOUNT));
      memcpy(string + strlen(string), "\"}}", 3);
      memset(result, 0, strlen(result));

    if (send_http_request(data, XCASH_WALLET_IP, "/json_rpc", XCASH_WALLET_PORT, "POST", HTTP_HEADERS,
                          HTTP_HEADERS_LENGTH, string, SEND_OR_RECEIVE_SOCKET_DATA_TIMEOUT_SETTINGS) <= 0)
    {
      return handle_error("sign_data", "Failed to send HTTP request for signing.", result, string);
    }

    if (!parse_json_data(data, "signature", result, MAXIMUM_AMOUNT))
    {
      return handle_error("sign_data", "Failed to parse the signature from the response.", result, string);
    }

    if (strlen(result) != XCASH_SIGN_DATA_LENGTH ||
        strncmp(result, XCASH_SIGN_DATA_PREFIX, sizeof(XCASH_SIGN_DATA_PREFIX) - 1) != 0)
    {
      return handle_error("sign_data", "Invalid signature format.", result, string);
    }

    pthread_rwlock_rdlock(&rwlock);
    // create the message
    memcpy(message + strlen(message) - 1, "\"public_address\": \"", 19);
    memcpy(message + strlen(message), xcash_wallet_public_address, XCASH_WALLET_LENGTH);
    memcpy(message + strlen(message), "\",\r\n \"previous_block_hash\": \"", 29);
    memcpy(message + strlen(message), previous_block_hash,
            strnlen(previous_block_hash, sizeof(previous_block_hash)));
    memcpy(message + strlen(message), "\",\r\n \"current_round_part\": \"", 28);
    memcpy(message + strlen(message), current_round_part, sizeof(char));
    memcpy(message + strlen(message), "\",\r\n \"current_round_part_backup_node\": \"", 40);
    memcpy(message + strlen(message), current_round_part_backup_node, sizeof(char));
    memcpy(message + strlen(message), "\",\r\n \"data\": \"", 14);
    memcpy(message + strlen(message), random_data, RANDOM_STRING_LENGTH);
    memcpy(message + strlen(message), "\",\r\n \"XCASH_DPOPS_signature\": \"", 31);
    memcpy(message + strlen(message), result, XCASH_SIGN_DATA_LENGTH);
    memcpy(message + strlen(message), "\",\r\n}", 5);
    pthread_rwlock_unlock(&rwlock);
  }

  free(result);
  free(string);
  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: verify_data
Description: Verifies signed data, for receiving data securely
Parameters:
  message - The signed data
  VERIFY_CURRENT_ROUND_PART_AND_CURRENT_ROUND_PART_BACKUP_NODE_SETTINGS - 1 to verify the current_round_part and the current_round_part_backup_node, otherwise 0
Return: 0 if the signed data is not verified, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int verify_data(const char *MESSAGE, const int VERIFY_CURRENT_ROUND_PART_AND_CURRENT_ROUND_PART_BACKUP_NODE_SETTINGS)
{
  // Constants
  const char *HTTP_HEADERS[] = {"Content-Type: application/json", "Accept: application/json"};
  const size_t HTTP_HEADERS_LENGTH = sizeof(HTTP_HEADERS) / sizeof(HTTP_HEADERS[0]);
  const size_t MAXIMUM_AMOUNT = strlen(MESSAGE) >= MAXIMUM_BUFFER_SIZE ? MAXIMUM_BUFFER_SIZE : strlen(MESSAGE) + BUFFER_SIZE;

  // Variables
  char message_settings[BUFFER_SIZE] = {0};
  char public_address[XCASH_PUBLIC_ADDR_LENGTH+1] = {0};
  char message_previous_block_hash[BUFFER_SIZE] = {0};
  char message_current_round_part[BUFFER_SIZE] = {0};
  char message_current_round_part_backup_node[BUFFER_SIZE] = {0};
  char XCASH_DPOPS_signature[XCASH_SIGN_DATA_LENGTH+1] = {0};
  char public_key[VRF_PUBLIC_KEY_LENGTH + 1] = {0};
  char proof[VRF_PROOF_LENGTH + 1] = {0};
  char beta_string[VRF_BETA_LENGTH + 1] = {0};
  unsigned char public_key_data[crypto_vrf_PUBLICKEYBYTES + 1] = {0};
  unsigned char proof_data[crypto_vrf_PROOFBYTES + 1] = {0};
  unsigned char beta_string_data[crypto_vrf_OUTPUTBYTES + 1] = {0};
  char *result = (char *)calloc(MAXIMUM_AMOUNT, sizeof(char));
  char data[BUFFER_SIZE] = {0};
  char *string = (char *)calloc(MAXIMUM_AMOUNT, sizeof(char));
  size_t message_length;
  size_t count;
  long long int number = 0;

  // check if the memory needed was allocated on the heap successfully
  if (result == NULL || string == NULL)
  {
    FATAL_ERROR_EXIT("verify_data: Memory allocation failed.");
  }

  if (strstr(MESSAGE, "}") != NULL)
  {
    if (parse_json_data(MESSAGE, "message_settings", message_settings, sizeof(message_settings)) == 0)
    {
      return handle_error("verify_data", "Could not parse the message_settings", result, string);
    }
  }
  else
  {
    const char *delimiter_position = strstr(MESSAGE, "|");
    if (delimiter_position != NULL)
    {
      size_t length = delimiter_position - MESSAGE;
      length = length < sizeof(message_settings) ? length : sizeof(message_settings) - 1;
      memcpy(message_settings, MESSAGE, length);
      message_settings[length] = '\0';
    }
    else
    {
      return handle_error("verify_data", "Invalid message format", result, string);
    }
  }

  // Define message types that need special handling
  const char *special_message_types[] = {
      "NODE_TO_NETWORK_DATA_NODES_GET_PREVIOUS_CURRENT_NEXT_BLOCK_VERIFIERS_LIST",
      "NODE_TO_NETWORK_DATA_NODES_GET_CURRENT_BLOCK_VERIFIERS_LIST",
      "NODE_TO_BLOCK_VERIFIERS_GET_RESERVE_BYTES_DATABASE_HASH",
      "XCASH_PROOF_OF_STAKE_TEST_DATA",
      "XCASH_PROOF_OF_STAKE_TEST_DATA_2",
      "NODE_TO_BLOCK_VERIFIERS_ADD_RESERVE_PROOF",
      "NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE",
      "NODES_TO_BLOCK_VERIFIERS_UPDATE_DELEGATE"};
         // Check if message type requires special handling
         bool special_type = false;
  special_type = (is_valid_message_type(message_settings, special_message_types, 
    sizeof(special_message_types) / sizeof(special_message_types[0])));

  if (special_type)
  {
    if (strstr(MESSAGE, "}") != NULL)
    {
      if (parse_json_data(MESSAGE, "public_address", public_address, sizeof(public_address)) == 0 ||
          parse_json_data(MESSAGE, "XCASH_DPOPS_signature", XCASH_DPOPS_signature, sizeof(XCASH_DPOPS_signature)) == 0)
      {
        return handle_error("verify_data", "Could not parse the message", result, string);
      }
    }
    else
    {
      // Extract data based on message type
      if (strcmp(message_settings, "NODE_TO_BLOCK_VERIFIERS_ADD_RESERVE_PROOF") == 0)
      {
        if (string_count(MESSAGE, "|") == VOTE_PARAMETER_AMOUNT)
        {
          if (extract_data_between_delimiters(MESSAGE, 3, public_address, XCASH_WALLET_LENGTH) != XCASH_OK ||
              extract_data_between_delimiters(MESSAGE, 4, XCASH_DPOPS_signature, XCASH_SIGN_DATA_LENGTH) != XCASH_OK)
          {
            return handle_error("verify_data", "Invalid message data", result, string);
          }
        }
        else
        {
          return handle_error("verify_data", "Invalid message format", result, string);
        }
      }
      else if (strcmp(message_settings, "NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE") == 0)
      {
        if (string_count(MESSAGE, "|") == REGISTER_PARAMETER_AMOUNT)
        {
          if (extract_data_between_delimiters(MESSAGE, 4, public_address, XCASH_WALLET_LENGTH) != XCASH_OK ||
              extract_data_between_delimiters(MESSAGE, 5, XCASH_DPOPS_signature, XCASH_SIGN_DATA_LENGTH) != XCASH_OK)
          {
            return handle_error("verify_data", "Invalid message data", result, string);
          }
        }
        else
        {
          return handle_error("verify_data", "Invalid message format", result, string);
        }
      }
      else if (strcmp(message_settings, "NODES_TO_BLOCK_VERIFIERS_UPDATE_DELEGATE") == 0)
      {
        if (string_count(MESSAGE, "|") == UPDATE_PARAMETER_AMOUNT)
        {
          if (extract_data_between_delimiters(MESSAGE, 3, public_address, XCASH_WALLET_LENGTH) != XCASH_OK ||
              extract_data_between_delimiters(MESSAGE, 4, XCASH_DPOPS_signature, XCASH_SIGN_DATA_LENGTH) != XCASH_OK)
          {
            return handle_error("verify_data", "Invalid message data", result, string);
          }
        }
        else
        {
          return handle_error("verify_data", "Invalid message format", result, string);
        }
      }
      else if (strcmp(message_settings, "NODE_TO_BLOCK_VERIFIERS_GET_RESERVE_BYTES_DATABASE_HASH") == 0)
      {
        if (string_count(MESSAGE, "|") == GET_RESERVE_BYTES_DATABASE_HASH_PARAMETER_AMOUNT)
        {
          if (extract_data_between_delimiters(MESSAGE, 2, public_address, XCASH_WALLET_LENGTH) != XCASH_OK ||
              extract_data_between_delimiters(MESSAGE, 4, XCASH_DPOPS_signature, VRF_BETA_LENGTH + VRF_PROOF_LENGTH) != XCASH_OK)
          {
            return handle_error("verify_data", "Invalid message data", result, string);
          }
        }
        else
        {
          return handle_error("verify_data", "Invalid message format", result, string);
        }
      }
    }
  }
  else
  {
    // Standard JSON parsing for other message types
    if (parse_json_data(MESSAGE, "public_address", public_address, sizeof(public_address)) == 0 ||
        parse_json_data(MESSAGE, "previous_block_hash", message_previous_block_hash, sizeof(message_previous_block_hash)) == 0 ||
        parse_json_data(MESSAGE, "current_round_part", message_current_round_part, sizeof(message_current_round_part)) == 0 ||
        parse_json_data(MESSAGE, "current_round_part_backup_node", message_current_round_part_backup_node, sizeof(message_current_round_part_backup_node)) == 0 ||
        parse_json_data(MESSAGE, "XCASH_DPOPS_signature", XCASH_DPOPS_signature, sizeof(XCASH_DPOPS_signature)) == 0)
    {
      return handle_error("verify_data", "Could not parse the message", result, string);
    }
  }

  // Define valid message types for memset(data, 0, sizeof(data)) condition
  const char *valid_message_types[] = {
      "NETWORK_DATA_NODE_TO_NODE_SEND_CURRENT_BLOCK_VERIFIERS_LIST",
      "NODE_TO_NETWORK_DATA_NODES_GET_PREVIOUS_CURRENT_NEXT_BLOCK_VERIFIERS_LIST",
      "NODE_TO_NETWORK_DATA_NODES_GET_CURRENT_BLOCK_VERIFIERS_LIST",
      "NODE_TO_BLOCK_VERIFIERS_GET_RESERVE_BYTES_DATABASE_HASH",
      "BLOCK_VERIFIERS_TO_NETWORK_DATA_NODE_BLOCK_VERIFIERS_CURRENT_TIME",
      "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_ONLINE_STATUS",
      "NODE_TO_BLOCK_VERIFIERS_ADD_RESERVE_PROOF",
      "NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE",
      "NODES_TO_BLOCK_VERIFIERS_UPDATE_DELEGATE",
      "XCASH_PROOF_OF_STAKE_TEST_DATA",
      "XCASH_PROOF_OF_STAKE_TEST_DATA_2",
      "BLOCK_VERIFIERS_TO_NODES_RESERVE_BYTES_DATABASE_SYNC_CHECK_ALL_DOWNLOAD",
      "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_SYNC_CHECK_ALL_UPDATE",
      "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_SYNC_CHECK_ALL_DOWNLOAD",
      "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_SYNC_CHECK_DOWNLOAD",
      "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_DOWNLOAD_FILE_UPDATE",
      "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_DOWNLOAD_FILE_DOWNLOAD",
      "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_SYNC_CHECK_ALL_UPDATE",
      "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_SYNC_CHECK_ALL_DOWNLOAD",
      "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_DOWNLOAD_FILE_UPDATE",
      "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_DOWNLOAD_FILE_DOWNLOAD",
      "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_SYNC_CHECK_UPDATE",
      "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_SYNC_CHECK_DOWNLOAD",
      "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_DOWNLOAD_FILE_UPDATE",
      "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_DOWNLOAD_FILE_DOWNLOAD",
      "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_SYNC_CHECK_UPDATE",
      "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_SYNC_CHECK_DOWNLOAD",
      "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_DOWNLOAD_FILE_UPDATE",
      "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_DOWNLOAD_FILE_DOWNLOAD"};

  // Check if the block verifier that sent the data is the correct main node
  if (strcmp(message_settings, "MAIN_NODES_TO_NODES_PART_4_OF_ROUND") == 0 &&
      strcmp(public_address, main_nodes_list.block_producer_public_address) != 0)
  {
    return handle_error("verify_data", "Invalid MAIN_NODES_TO_NODES_PART_4_OF_ROUND message", result, string);
  }
  else if (is_valid_message_type(message_settings, valid_message_types, sizeof(valid_message_types) / sizeof(valid_message_types[0])))
  {
    memset(data, 0, sizeof(data));
  }
  else
  {
    bool pub_key_found = false;

    // Use is_seed_address function to check seed nodes
    if (is_seed_address(public_address))
    {
      pub_key_found = true;
    }
    else
    {
      // Check if the public address is in the current_block_verifiers_list struct
      for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++)
      {
        if (strncmp(public_address, delegates_all[count].public_address, XCASH_WALLET_LENGTH) == 0)
        {
          pub_key_found = true;
          break;
        }
      }
    }

    if (!pub_key_found)
    {
      return handle_error("verify_data", "Invalid message signature. Sender is unknown", result, string);
    }

    memset(data, 0, sizeof(data));
  }

// Define valid message types for previous block hash check
const char *valid_message_types_for_hash_check[] = {
  "NODE_TO_NETWORK_DATA_NODES_GET_PREVIOUS_CURRENT_NEXT_BLOCK_VERIFIERS_LIST",
  "NODE_TO_NETWORK_DATA_NODES_GET_CURRENT_BLOCK_VERIFIERS_LIST",
  "NODE_TO_BLOCK_VERIFIERS_GET_RESERVE_BYTES_DATABASE_HASH",
  "XCASH_PROOF_OF_STAKE_TEST_DATA",
  "XCASH_PROOF_OF_STAKE_TEST_DATA_2",
  "NODE_TO_BLOCK_VERIFIERS_ADD_RESERVE_PROOF",
  "NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE",
  "NODES_TO_BLOCK_VERIFIERS_UPDATE_DELEGATE",
  "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_SYNC_CHECK_ALL_UPDATE",
  "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_DOWNLOAD_FILE_UPDATE",
  "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_SYNC_CHECK_ALL_UPDATE",
  "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_DOWNLOAD_FILE_UPDATE",
  "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_DOWNLOAD_FILE_UPDATE",
  "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_DOWNLOAD_FILE_UPDATE",
  "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_SYNC_CHECK_ALL_DOWNLOAD",
  "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_DOWNLOAD_FILE_DOWNLOAD",
  "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_SYNC_CHECK_ALL_DOWNLOAD",
  "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_DOWNLOAD_FILE_DOWNLOAD",
  "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_DOWNLOAD_FILE_DOWNLOAD",
  "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_DOWNLOAD_FILE_DOWNLOAD",
  "NETWORK_DATA_NODE_TO_NODE_SEND_CURRENT_BLOCK_VERIFIERS_LIST",
  "NETWORK_DATA_NODE_TO_NODE_SEND_PREVIOUS_CURRENT_NEXT_BLOCK_VERIFIERS_LIST",
  "NODE_TO_NETWORK_DATA_NODES_GET_CURRENT_BLOCK_VERIFIERS_LIST",
  "NODE_TO_NETWORK_DATA_NODES_GET_PREVIOUS_CURRENT_NEXT_BLOCK_VERIFIERS_LIST"
};

// Verify if the previous block hash is correct
sscanf(current_block_height, "%lld", &number);
if (number >= XCASH_PROOF_OF_STAKE_BLOCK_HEIGHT) {
  // Check if message type is not in the valid list for hash check
  if (!is_valid_message_type(message_settings, valid_message_types_for_hash_check, 
      sizeof(valid_message_types_for_hash_check) / sizeof(valid_message_types_for_hash_check[0]))) 
  {
      // Validate previous block hash
      if (strcmp(previous_block_hash, message_previous_block_hash) != 0) {
          return handle_error("verify_data", "Invalid previous block hash", result, string);
      }

      // Verify if the current_round_part_backup_node is correct
      if (VERIFY_CURRENT_ROUND_PART_AND_CURRENT_ROUND_PART_BACKUP_NODE_SETTINGS == 1 &&
          (strncmp(current_round_part, message_current_round_part, 1) != 0 ||
           strncmp(current_round_part_backup_node, message_current_round_part_backup_node, 1) != 0)) 
      {
          return handle_error("verify_data", "Invalid current_round_part or current_round_part_backup_node", result, string);
      }
  }
}

// Check if the message type requires special handling
special_type = is_valid_message_type(message_settings, special_message_types, 
  sizeof(special_message_types) / sizeof(special_message_types[0]));

if (special_type && strstr(MESSAGE, "previous_block_hash") == NULL) {
  // Special handling for certain message types without previous block hash
  message_length = strlen(MESSAGE) - 94;
  safe_memcpy(result, MESSAGE, message_length);
} else {
  // Determine the message length based on the presence of signatures
  message_length = (strstr(MESSAGE, "\"XCASH_DPOPS_signature\": \"SigV1") == NULL &&
    strstr(MESSAGE, "|SigV1") == NULL) ? strlen(MESSAGE) - 320 : strlen(MESSAGE) - 125;

  // Copy message to result and append closing brace
  safe_memcpy(result, MESSAGE, message_length);
  safe_memcpy(result + message_length, "}", 2); 

  // Replace quotes with escaped quotes in the result string
  string_replace(result, MAXIMUM_AMOUNT, "\"", "\\\"");
}

if (strstr(MESSAGE, "\"XCASH_DPOPS_signature\": \"SigV1") == NULL && strstr(MESSAGE, "|SigV1") == NULL) {

  // Extract proof and beta_string
  safe_memcpy(proof, XCASH_DPOPS_signature, VRF_PROOF_LENGTH);
  safe_memcpy(beta_string, &XCASH_DPOPS_signature[VRF_PROOF_LENGTH], VRF_BETA_LENGTH);
      bool pub_key_found = false;

      // Check in seed nodes
      for (int i = 0; network_nodes[i].seed_public_address != NULL; i++) {
        if (strcmp(network_nodes[i].seed_public_address, public_address) == 0) {
          safe_memcpy(public_key, network_nodes[i].seed_public_key,
           VRF_PUBLIC_KEY_LENGTH);
          pub_key_found = true;
          break;
        }
      }
      // Check in block verifiers if not found in seed nodes
      if (!pub_key_found) {
          for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
              if (strncmp(current_block_verifiers_list.block_verifiers_public_address[count], public_address, XCASH_WALLET_LENGTH) == 0) {
                  safe_memcpy(public_key, current_block_verifiers_list.block_verifiers_public_key[count], VRF_PUBLIC_KEY_LENGTH);
                  pub_key_found = true;
                  break;
              }
          }
      }

      // Fetch from database if still not found
      if (!pub_key_found) {
          char query[BUFFER_SIZE] = {0};
          snprintf(query, sizeof(query), "{\"public_address\":\"%s\"}", public_address);

          if (count_documents_in_collection(DATABASE_NAME, "delegates", query) == 1) {
              if (read_document_field_from_collection(DATABASE_NAME, "delegates", query, "public_key", public_key) == 0) {
                  return handle_error("verify_data", "Could not find the public key to verify the message", result, string);
              }
          } else {
              return handle_error("verify_data", "Could not find the public key to verify the message", result, string);
          }
      }

      // Convert hex to binary
      if (hex_to_byte_array(public_key, public_key_data, sizeof(public_key_data)) != XCASH_OK ||
          hex_to_byte_array(proof, proof_data, sizeof(proof_data)) != XCASH_OK ||
          hex_to_byte_array(beta_string, beta_string_data, sizeof(beta_string_data)) != XCASH_OK)
      {
        return handle_error("verify_data", "Failed to convert hex to binary", result, string);
      }

  // Verify the message
  if (crypto_vrf_verify(beta_string_data, public_key_data, proof_data, (unsigned char *)result, strlen(result)) != 0) {
      return handle_error("verify_data", "Invalid message", result, string);
  }
} else {
  // Prepare JSON-RPC request
  message_length = strlen(result);
  memcpy(string, "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"verify\",\"params\":{\"data\":\"", 62);
  memcpy(string + 62, result, message_length);
  memcpy(string + 62 + message_length, "\",\"address\":\"", 13);
  memcpy(string + 75 + message_length, public_address, XCASH_WALLET_LENGTH);
  memcpy(string + 75 + message_length + XCASH_WALLET_LENGTH, "\",\"signature\":\"", 15);
  memcpy(string + 90 + message_length + XCASH_WALLET_LENGTH, XCASH_DPOPS_signature, XCASH_SIGN_DATA_LENGTH);
  memcpy(string + 90 + message_length + XCASH_WALLET_LENGTH + XCASH_SIGN_DATA_LENGTH, "\"}}", 3);

  // Clear result buffer
  memset(result, 0, strnlen(result, BUFFER_SIZE));

  // Send HTTP request
  if (send_http_request(result, XCASH_WALLET_IP, "/json_rpc", XCASH_WALLET_PORT,
                        "POST", HTTP_HEADERS, HTTP_HEADERS_LENGTH, string,
                        SEND_OR_RECEIVE_SOCKET_DATA_TIMEOUT_SETTINGS) <= 0) {
    return handle_error("verify_data", "Could not verify the data", result,
                        string);
  }

  if (parse_json_data(result, "good", data, sizeof(data)) == 0 || strncmp(data, "true", BUFFER_SIZE) != 0) {
      return handle_error("verify_data", "Invalid message", result, string);
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
int validate_data(const char *MESSAGE)
{
    // Variables
    char data[BUFFER_SIZE];

    memset(data, 0, sizeof(data));

    // check if the format is correct for each message
    if (strstr(MESSAGE, "NODE_TO_BLOCK_VERIFIERS_ADD_RESERVE_PROOF") != NULL ||
        strstr(MESSAGE, "XCASH_GET_BLOCK_PRODUCERS") != NULL ||
        strstr(MESSAGE, "NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE") != NULL ||
        strstr(MESSAGE, "NODE_TO_NETWORK_DATA_NODES_CHECK_VOTE_STATUS") != NULL ||
        strstr(MESSAGE, "NODES_TO_BLOCK_VERIFIERS_UPDATE_DELEGATE") != NULL ||
        strstr(MESSAGE, "NODES_TO_BLOCK_VERIFIERS_RECOVER_DELEGATE") != NULL ||
        strstr(MESSAGE, "NODE_TO_BLOCK_VERIFIERS_GET_RESERVE_BYTES_DATABASE_HASH") != NULL ||
        strstr(MESSAGE, "BLOCK_VERIFIERS_TO_NODES_RESERVE_BYTES_DATABASE_SYNC_CHECK_ALL_DOWNLOAD") != NULL)
    {
        return XCASH_OK;
    }

    if (strstr(MESSAGE, "XCASH_GET_SYNC_INFO") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "XCASH_GET_SYNC_INFO", sizeof(data)) != 0)
        {
            ERROR_PRINT("Invalid message");;
            return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "XCASH_GET_BLOCK_HASH") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "XCASH_GET_BLOCK_HASH", sizeof(data)) != 0)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "GET_CURRENT_BLOCK_HEIGHT") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "GET_CURRENT_BLOCK_HEIGHT", sizeof(data)) != 0)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "SEND_CURRENT_BLOCK_HEIGHT") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "SEND_CURRENT_BLOCK_HEIGHT", sizeof(data)) != 0 ||
            parse_json_data(MESSAGE, "block_height", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "public_address", data, sizeof(data)) == 0 || strlen(data) != XCASH_WALLET_LENGTH ||
            strncmp(data, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0 ||
            parse_json_data(MESSAGE, "previous_block_hash", data, sizeof(data)) == 0 ||
            strlen(data) != BLOCK_HASH_LENGTH ||
            parse_json_data(MESSAGE, "current_round_part", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "current_round_part_backup_node", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "data", data, sizeof(data)) == 0 || strlen(data) != RANDOM_STRING_LENGTH ||
            parse_json_data(MESSAGE, "XCASH_DPOPS_signature", data, sizeof(data)) == 0 ||
            strlen(data) != VRF_BETA_LENGTH + VRF_PROOF_LENGTH)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "MAIN_NODES_TO_NODES_PART_4_OF_ROUND_CREATE_NEW_BLOCK") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "MAIN_NODES_TO_NODES_PART_4_OF_ROUND_CREATE_NEW_BLOCK", sizeof(data)) != 0 ||
            parse_json_data(MESSAGE, "block_blob", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "public_address", data, sizeof(data)) == 0 || strlen(data) != XCASH_WALLET_LENGTH ||
            strncmp(data, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0 ||
            parse_json_data(MESSAGE, "previous_block_hash", data, sizeof(data)) == 0 ||
            strlen(data) != BLOCK_HASH_LENGTH ||
            parse_json_data(MESSAGE, "current_round_part", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "current_round_part_backup_node", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "data", data, sizeof(data)) == 0 || strlen(data) != RANDOM_STRING_LENGTH ||
            parse_json_data(MESSAGE, "XCASH_DPOPS_signature", data, sizeof(data)) == 0 ||
            strlen(data) != VRF_BETA_LENGTH + VRF_PROOF_LENGTH)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "MAIN_NETWORK_DATA_NODE_TO_BLOCK_VERIFIERS_START_BLOCK") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "MAIN_NETWORK_DATA_NODE_TO_BLOCK_VERIFIERS_START_BLOCK", sizeof(data)) != 0 ||
            parse_json_data(MESSAGE, "database_data", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_bytes_data_hash", data, sizeof(data)) == 0 ||
            strlen(data) != DATA_HASH_LENGTH || parse_json_data(MESSAGE, "public_address", data, sizeof(data)) == 0 ||
            strlen(data) != XCASH_WALLET_LENGTH ||
            strncmp(data, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0 ||
            parse_json_data(MESSAGE, "previous_block_hash", data, sizeof(data)) == 0 ||
            strlen(data) != BLOCK_HASH_LENGTH ||
            parse_json_data(MESSAGE, "current_round_part", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "current_round_part_backup_node", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "data", data, sizeof(data)) == 0 || strlen(data) != RANDOM_STRING_LENGTH ||
            parse_json_data(MESSAGE, "XCASH_DPOPS_signature", data, sizeof(data)) == 0 ||
            strlen(data) != VRF_BETA_LENGTH + VRF_PROOF_LENGTH)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_VRF_DATA") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_VRF_DATA", sizeof(data)) != 0 ||
            parse_json_data(MESSAGE, "vrf_secret_key", data, sizeof(data)) == 0 ||
            strlen(data) != VRF_SECRET_KEY_LENGTH ||
            parse_json_data(MESSAGE, "vrf_public_key", data, sizeof(data)) == 0 ||
            strlen(data) != VRF_PUBLIC_KEY_LENGTH || parse_json_data(MESSAGE, "random_data", data, sizeof(data)) == 0 ||
            strlen(data) != RANDOM_STRING_LENGTH || parse_json_data(MESSAGE, "public_address", data, sizeof(data)) == 0 ||
            strlen(data) != XCASH_WALLET_LENGTH ||
            strncmp(data, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0 ||
            parse_json_data(MESSAGE, "previous_block_hash", data, sizeof(data)) == 0 ||
            strlen(data) != BLOCK_HASH_LENGTH ||
            parse_json_data(MESSAGE, "current_round_part", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "current_round_part_backup_node", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "data", data, sizeof(data)) == 0 || strlen(data) != RANDOM_STRING_LENGTH ||
            parse_json_data(MESSAGE, "XCASH_DPOPS_signature", data, sizeof(data)) == 0 ||
            strlen(data) != VRF_BETA_LENGTH + VRF_PROOF_LENGTH)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;;
        }
    }
    else if (strstr(MESSAGE, "NODES_TO_NODES_VOTE_MAJORITY_RESULTS") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "NODES_TO_NODES_VOTE_MAJORITY_RESULTS", sizeof(data)) != 0 ||
            parse_json_data(MESSAGE, "vote_data_1", data, sizeof(data)) == 0 ||
            (strlen(data) != VRF_SECRET_KEY_LENGTH + VRF_PUBLIC_KEY_LENGTH + RANDOM_STRING_LENGTH &&
             strlen(data) != VRF_PROOF_LENGTH + VRF_BETA_LENGTH) ||
            parse_json_data(MESSAGE, "public_address", data, sizeof(data)) == 0 || strlen(data) != XCASH_WALLET_LENGTH ||
            strncmp(data, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0 ||
            parse_json_data(MESSAGE, "previous_block_hash", data, sizeof(data)) == 0 ||
            strlen(data) != BLOCK_HASH_LENGTH ||
            parse_json_data(MESSAGE, "current_round_part", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "current_round_part_backup_node", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "data", data, sizeof(data)) == 0 || strlen(data) != RANDOM_STRING_LENGTH ||
            parse_json_data(MESSAGE, "XCASH_DPOPS_signature", data, sizeof(data)) == 0 ||
            strlen(data) != VRF_BETA_LENGTH + VRF_PROOF_LENGTH)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "NODES_TO_NODES_VOTE_RESULTS") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "NODES_TO_NODES_VOTE_RESULTS", sizeof(data)) != 0 ||
            parse_json_data(MESSAGE, "vote_settings", data, sizeof(data)) == 0 ||
            (strncmp(data, "valid", 5) != 0 && strncmp(data, "invalid", 7) != 0) ||
            parse_json_data(MESSAGE, "vote_data", data, sizeof(data)) == 0 || strlen(data) != DATA_HASH_LENGTH ||
            parse_json_data(MESSAGE, "public_address", data, sizeof(data)) == 0 || strlen(data) != XCASH_WALLET_LENGTH ||
            strncmp(data, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0 ||
            parse_json_data(MESSAGE, "previous_block_hash", data, sizeof(data)) == 0 ||
            strlen(data) != BLOCK_HASH_LENGTH ||
            parse_json_data(MESSAGE, "current_round_part", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "current_round_part_backup_node", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "data", data, sizeof(data)) == 0 || strlen(data) != RANDOM_STRING_LENGTH ||
            parse_json_data(MESSAGE, "XCASH_DPOPS_signature", data, sizeof(data)) == 0 ||
            strlen(data) != VRF_BETA_LENGTH + VRF_PROOF_LENGTH)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_BLOCK_BLOB_SIGNATURE") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_BLOCK_BLOB_SIGNATURE", sizeof(data)) != 0 ||
            parse_json_data(MESSAGE, "block_blob_signature", data, sizeof(data)) == 0 ||
            strlen(data) != VRF_PROOF_LENGTH + VRF_BETA_LENGTH ||
            parse_json_data(MESSAGE, "public_address", data, sizeof(data)) == 0 || strlen(data) != XCASH_WALLET_LENGTH ||
            strncmp(data, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0 ||
            parse_json_data(MESSAGE, "previous_block_hash", data, sizeof(data)) == 0 ||
            strlen(data) != BLOCK_HASH_LENGTH ||
            parse_json_data(MESSAGE, "current_round_part", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "current_round_part_backup_node", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "data", data, sizeof(data)) == 0 || strlen(data) != RANDOM_STRING_LENGTH ||
            parse_json_data(MESSAGE, "XCASH_DPOPS_signature", data, sizeof(data)) == 0 ||
            strlen(data) != VRF_BETA_LENGTH + VRF_PROOF_LENGTH)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_INVALID_RESERVE_PROOFS") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_INVALID_RESERVE_PROOFS", sizeof(data)) != 0 ||
            parse_json_data(MESSAGE, "public_address_that_created_the_reserve_proof", data, sizeof(data)) == 0 ||
            strlen(data) != XCASH_WALLET_LENGTH || strncmp(data, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) ||
            parse_json_data(MESSAGE, "reserve_proof", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "public_address", data, sizeof(data)) == 0 || strlen(data) != XCASH_WALLET_LENGTH ||
            strncmp(data, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0 ||
            parse_json_data(MESSAGE, "previous_block_hash", data, sizeof(data)) == 0 ||
            strlen(data) != BLOCK_HASH_LENGTH ||
            parse_json_data(MESSAGE, "current_round_part", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "current_round_part_backup_node", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "data", data, sizeof(data)) == 0 || strlen(data) != RANDOM_STRING_LENGTH ||
            parse_json_data(MESSAGE, "XCASH_DPOPS_signature", data, sizeof(data)) == 0 ||
            strlen(data) != VRF_BETA_LENGTH + VRF_PROOF_LENGTH)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "NODE_TO_NETWORK_DATA_NODES_GET_PREVIOUS_CURRENT_NEXT_BLOCK_VERIFIERS_LIST") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "NODE_TO_NETWORK_DATA_NODES_GET_PREVIOUS_CURRENT_NEXT_BLOCK_VERIFIERS_LIST", sizeof(data)) !=
                0)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "NODE_TO_NETWORK_DATA_NODES_GET_CURRENT_BLOCK_VERIFIERS_LIST") != NULL)
    {
        if (strstr(MESSAGE, "\"public_address\"") != NULL &&
            (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
             strncmp(data, "NODE_TO_NETWORK_DATA_NODES_GET_CURRENT_BLOCK_VERIFIERS_LIST", sizeof(data)) != 0 ||
             parse_json_data(MESSAGE, "public_address", data, sizeof(data)) == 0 || strlen(data) != XCASH_WALLET_LENGTH ||
             strncmp(data, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0 ||
             parse_json_data(MESSAGE, "previous_block_hash", data, sizeof(data)) == 0 ||
             strlen(data) != BLOCK_HASH_LENGTH ||
             parse_json_data(MESSAGE, "current_round_part", data, sizeof(data)) == 0 || strlen(data) != 1 ||
             parse_json_data(MESSAGE, "current_round_part_backup_node", data, sizeof(data)) == 0 || strlen(data) != 1 ||
             parse_json_data(MESSAGE, "data", data, sizeof(data)) == 0 || strlen(data) != RANDOM_STRING_LENGTH ||
             parse_json_data(MESSAGE, "XCASH_DPOPS_signature", data, sizeof(data)) == 0 ||
             strlen(data) != VRF_BETA_LENGTH + VRF_PROOF_LENGTH))
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;;
        }
        else if (strstr(MESSAGE, "\"public_address\"") == NULL &&
                 (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
                  strncmp(data, "NODE_TO_NETWORK_DATA_NODES_GET_CURRENT_BLOCK_VERIFIERS_LIST", sizeof(data)) != 0))
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "NETWORK_DATA_NODE_TO_NODE_SEND_PREVIOUS_CURRENT_NEXT_BLOCK_VERIFIERS_LIST") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "NETWORK_DATA_NODE_TO_NODE_SEND_PREVIOUS_CURRENT_NEXT_BLOCK_VERIFIERS_LIST", sizeof(data)) !=
                0 ||
            parse_json_data(MESSAGE, "previous_block_verifiers_name_list", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "previous_block_verifiers_public_address_list", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "previous_block_verifiers_IP_address_list", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "current_block_verifiers_name_list", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "current_block_verifiers_public_address_list", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "current_block_verifiers_IP_address_list", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "next_block_verifiers_name_list", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "next_block_verifiers_public_address_list", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "next_block_verifiers_IP_address_list", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "public_address", data, sizeof(data)) == 0 || strlen(data) != XCASH_WALLET_LENGTH ||
            strncmp(data, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0 ||
            parse_json_data(MESSAGE, "previous_block_hash", data, sizeof(data)) == 0 ||
            strlen(data) != BLOCK_HASH_LENGTH ||
            parse_json_data(MESSAGE, "current_round_part", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "current_round_part_backup_node", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "data", data, sizeof(data)) == 0 || strlen(data) != RANDOM_STRING_LENGTH ||
            parse_json_data(MESSAGE, "XCASH_DPOPS_signature", data, sizeof(data)) == 0 ||
            strlen(data) != VRF_BETA_LENGTH + VRF_PROOF_LENGTH)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "NETWORK_DATA_NODE_TO_NODE_SEND_CURRENT_BLOCK_VERIFIERS_LIST") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "NETWORK_DATA_NODE_TO_NODE_SEND_CURRENT_BLOCK_VERIFIERS_LIST", sizeof(data)) != 0 ||
            parse_json_data(MESSAGE, "block_verifiers_public_address_list", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "block_verifiers_IP_address_list", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "public_address", data, sizeof(data)) == 0 || strlen(data) != XCASH_WALLET_LENGTH ||
            strncmp(data, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0 ||
            parse_json_data(MESSAGE, "previous_block_hash", data, sizeof(data)) == 0 ||
            strlen(data) != BLOCK_HASH_LENGTH ||
            parse_json_data(MESSAGE, "current_round_part", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "current_round_part_backup_node", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "data", data, sizeof(data)) == 0 || strlen(data) != RANDOM_STRING_LENGTH ||
            parse_json_data(MESSAGE, "XCASH_DPOPS_signature", data, sizeof(data)) == 0 ||
            strlen(data) != VRF_BETA_LENGTH + VRF_PROOF_LENGTH)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "BLOCK_VERIFIERS_TO_NETWORK_DATA_NODE_BLOCK_VERIFIERS_CURRENT_TIME") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "BLOCK_VERIFIERS_TO_NETWORK_DATA_NODE_BLOCK_VERIFIERS_CURRENT_TIME", sizeof(data)) != 0 ||
            parse_json_data(MESSAGE, "public_address", data, sizeof(data)) == 0 || strlen(data) != XCASH_WALLET_LENGTH ||
            strncmp(data, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0 ||
            parse_json_data(MESSAGE, "previous_block_hash", data, sizeof(data)) == 0 ||
            strlen(data) != BLOCK_HASH_LENGTH ||
            parse_json_data(MESSAGE, "current_round_part", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "current_round_part_backup_node", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "data", data, sizeof(data)) == 0 || strlen(data) != RANDOM_STRING_LENGTH ||
            parse_json_data(MESSAGE, "XCASH_DPOPS_signature", data, sizeof(data)) == 0 ||
            strlen(data) != VRF_BETA_LENGTH + VRF_PROOF_LENGTH)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "NETWORK_DATA_NODE_TO_BLOCK_VERIFIERS_BLOCK_VERIFIERS_CURRENT_TIME") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "NETWORK_DATA_NODE_TO_BLOCK_VERIFIERS_BLOCK_VERIFIERS_CURRENT_TIME", sizeof(data)) != 0 ||
            parse_json_data(MESSAGE, "current_time", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "public_address", data, sizeof(data)) == 0 || strlen(data) != XCASH_WALLET_LENGTH ||
            strncmp(data, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0 ||
            parse_json_data(MESSAGE, "previous_block_hash", data, sizeof(data)) == 0 ||
            strlen(data) != BLOCK_HASH_LENGTH ||
            parse_json_data(MESSAGE, "current_round_part", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "current_round_part_backup_node", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "data", data, sizeof(data)) == 0 || strlen(data) != RANDOM_STRING_LENGTH ||
            parse_json_data(MESSAGE, "XCASH_DPOPS_signature", data, sizeof(data)) == 0 ||
            strlen(data) != VRF_BETA_LENGTH + VRF_PROOF_LENGTH)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;;
        }
    }
    else if (strstr(MESSAGE, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_ONLINE_STATUS") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_ONLINE_STATUS", sizeof(data)) != 0)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "NODE_TO_BLOCK_VERIFIERS_CHECK_IF_CURRENT_BLOCK_VERIFIER") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "NODE_TO_BLOCK_VERIFIERS_CHECK_IF_CURRENT_BLOCK_VERIFIER", sizeof(data)) != 0)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "BLOCK_VERIFIERS_TO_NODE_SEND_RESERVE_BYTES") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "BLOCK_VERIFIERS_TO_NODE_SEND_RESERVE_BYTES", sizeof(data)) != 0 ||
            parse_json_data(MESSAGE, "reserve_bytes", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "public_address", data, sizeof(data)) == 0 || strlen(data) != XCASH_WALLET_LENGTH ||
            strncmp(data, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0 ||
            parse_json_data(MESSAGE, "previous_block_hash", data, sizeof(data)) == 0 ||
            strlen(data) != BLOCK_HASH_LENGTH ||
            parse_json_data(MESSAGE, "current_round_part", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "current_round_part_backup_node", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "data", data, sizeof(data)) == 0 || strlen(data) != RANDOM_STRING_LENGTH ||
            parse_json_data(MESSAGE, "XCASH_DPOPS_signature", data, sizeof(data)) == 0 ||
            strlen(data) != VRF_BETA_LENGTH + VRF_PROOF_LENGTH)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "NETWORK_DATA_NODES_TO_NETWORK_DATA_NODES_DATABASE_SYNC_CHECK") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "NETWORK_DATA_NODES_TO_NETWORK_DATA_NODES_DATABASE_SYNC_CHECK", sizeof(data)) != 0 ||
            parse_json_data(MESSAGE, "previous_blocks_reserve_bytes", data, sizeof(data)) == 0 ||
            (strncmp(data, "true", BUFFER_SIZE) != 0 && strncmp(data, "false", BUFFER_SIZE) != 0) ||
            parse_json_data(MESSAGE, "public_address", data, sizeof(data)) == 0 || strlen(data) != XCASH_WALLET_LENGTH ||
            strncmp(data, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0 ||
            parse_json_data(MESSAGE, "previous_block_hash", data, sizeof(data)) == 0 ||
            strlen(data) != BLOCK_HASH_LENGTH ||
            parse_json_data(MESSAGE, "current_round_part", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "current_round_part_backup_node", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "data", data, sizeof(data)) == 0 || strlen(data) != RANDOM_STRING_LENGTH ||
            parse_json_data(MESSAGE, "XCASH_DPOPS_signature", data, sizeof(data)) == 0 ||
            strlen(data) != VRF_BETA_LENGTH + VRF_PROOF_LENGTH)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_SYNC_CHECK_ALL_UPDATE") !=
             NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_SYNC_CHECK_ALL_UPDATE",
                    sizeof(data)) != 0 ||
            parse_json_data(MESSAGE, "reserve_bytes_settings", data, sizeof(data)) == 0 ||
            (strncmp(data, "0", 1) != 0 && strncmp(data, "1", 1) != 0) ||
            parse_json_data(MESSAGE, "reserve_bytes_data_hash", data, sizeof(data)) == 0 ||
            strlen(data) != DATA_HASH_LENGTH || parse_json_data(MESSAGE, "public_address", data, sizeof(data)) == 0 ||
            strlen(data) != XCASH_WALLET_LENGTH ||
            strncmp(data, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0 ||
            parse_json_data(MESSAGE, "previous_block_hash", data, sizeof(data)) == 0 ||
            strlen(data) != BLOCK_HASH_LENGTH ||
            parse_json_data(MESSAGE, "current_round_part", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "current_round_part_backup_node", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "data", data, sizeof(data)) == 0 || strlen(data) != RANDOM_STRING_LENGTH ||
            parse_json_data(MESSAGE, "XCASH_DPOPS_signature", data, sizeof(data)) == 0 ||
            strlen(data) != VRF_BETA_LENGTH + VRF_PROOF_LENGTH)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_SYNC_CHECK_ALL_DOWNLOAD") !=
             NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_SYNC_CHECK_ALL_DOWNLOAD",
                    sizeof(data)) != 0 ||
            parse_json_data(MESSAGE, "reserve_bytes_database", data, sizeof(data)) == 0 ||
            (strncmp(data, "true", 4) != 0 && strncmp(data, "false", 5) != 0) ||
            parse_json_data(MESSAGE, "public_address", data, sizeof(data)) == 0 || strlen(data) != XCASH_WALLET_LENGTH ||
            strncmp(data, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0 ||
            parse_json_data(MESSAGE, "previous_block_hash", data, sizeof(data)) == 0 ||
            strlen(data) != BLOCK_HASH_LENGTH ||
            parse_json_data(MESSAGE, "current_round_part", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "current_round_part_backup_node", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "data", data, sizeof(data)) == 0 || strlen(data) != RANDOM_STRING_LENGTH ||
            parse_json_data(MESSAGE, "XCASH_DPOPS_signature", data, sizeof(data)) == 0 ||
            strlen(data) != VRF_BETA_LENGTH + VRF_PROOF_LENGTH)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_SYNC_CHECK_DOWNLOAD") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_SYNC_CHECK_DOWNLOAD",
                    sizeof(data)) != 0 ||
            parse_json_data(MESSAGE, "reserve_bytes_database", data, sizeof(data)) == 0 ||
            (strncmp(data, "true", 4) != 0 && strncmp(data, "false", 5) != 0) ||
            parse_json_data(MESSAGE, "public_address", data, sizeof(data)) == 0 || strlen(data) != XCASH_WALLET_LENGTH ||
            strncmp(data, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0 ||
            parse_json_data(MESSAGE, "previous_block_hash", data, sizeof(data)) == 0 ||
            strlen(data) != BLOCK_HASH_LENGTH ||
            parse_json_data(MESSAGE, "current_round_part", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "current_round_part_backup_node", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "data", data, sizeof(data)) == 0 || strlen(data) != RANDOM_STRING_LENGTH ||
            parse_json_data(MESSAGE, "XCASH_DPOPS_signature", data, sizeof(data)) == 0 ||
            strlen(data) != VRF_BETA_LENGTH + VRF_PROOF_LENGTH)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_DOWNLOAD_FILE_UPDATE") !=
             NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_DOWNLOAD_FILE_UPDATE",
                    sizeof(data)) != 0 ||
            parse_json_data(MESSAGE, "file", data, sizeof(data)) == 0 || strstr(data, "reserve_bytes_") == NULL)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_DOWNLOAD_FILE_DOWNLOAD") !=
             NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_DOWNLOAD_FILE_DOWNLOAD",
                    sizeof(data)) != 0)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_SYNC_CHECK_ALL_UPDATE") !=
             NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_SYNC_CHECK_ALL_UPDATE",
                    sizeof(data)) != 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash", data, sizeof(data)) == 0 ||
            strlen(data) != DATA_HASH_LENGTH ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_1", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_2", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_3", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_4", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_5", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_6", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_7", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_8", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_9", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_10", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_11", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_12", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_13", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_14", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_15", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_16", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_17", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_18", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_19", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_20", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_20", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_21", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_22", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_23", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_24", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_25", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_26", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_27", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_28", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_29", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_30", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_31", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_32", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_33", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_34", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_35", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_36", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_37", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_38", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_39", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_40", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_41", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_42", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_43", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_44", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_45", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_46", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_47", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_48", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_49", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_data_hash_50", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "public_address", data, sizeof(data)) == 0 || strlen(data) != XCASH_WALLET_LENGTH ||
            strncmp(data, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0 ||
            parse_json_data(MESSAGE, "previous_block_hash", data, sizeof(data)) == 0 ||
            strlen(data) != BLOCK_HASH_LENGTH ||
            parse_json_data(MESSAGE, "current_round_part", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "current_round_part_backup_node", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "data", data, sizeof(data)) == 0 || strlen(data) != RANDOM_STRING_LENGTH ||
            parse_json_data(MESSAGE, "XCASH_DPOPS_signature", data, sizeof(data)) == 0 ||
            strlen(data) != VRF_BETA_LENGTH + VRF_PROOF_LENGTH)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_SYNC_CHECK_ALL_DOWNLOAD") !=
             NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_SYNC_CHECK_ALL_DOWNLOAD",
                    sizeof(data)) != 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database", data, sizeof(data)) == 0 ||
            (strncmp(data, "true", 4) != 0 && strncmp(data, "false", 5) != 0) ||
            parse_json_data(MESSAGE, "reserve_proofs_database_1", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_2", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_3", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_4", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_5", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_6", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_7", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_8", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_9", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_10", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_11", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_12", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_13", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_14", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_15", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_16", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_17", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_18", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_19", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_20", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_20", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_21", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_22", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_23", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_24", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_25", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_26", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_27", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_28", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_29", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_30", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_31", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_32", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_33", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_34", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_35", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_36", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_37", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_38", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_39", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_40", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_41", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_42", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_43", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_44", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_45", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_46", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_47", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_48", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_49", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "reserve_proofs_database_50", data, sizeof(data)) == 0 ||
            parse_json_data(MESSAGE, "public_address", data, sizeof(data)) == 0 || strlen(data) != XCASH_WALLET_LENGTH ||
            strncmp(data, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0 ||
            parse_json_data(MESSAGE, "previous_block_hash", data, sizeof(data)) == 0 ||
            strlen(data) != BLOCK_HASH_LENGTH ||
            parse_json_data(MESSAGE, "current_round_part", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "current_round_part_backup_node", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "data", data, sizeof(data)) == 0 || strlen(data) != RANDOM_STRING_LENGTH ||
            parse_json_data(MESSAGE, "XCASH_DPOPS_signature", data, sizeof(data)) == 0 ||
            strlen(data) != VRF_BETA_LENGTH + VRF_PROOF_LENGTH)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_DOWNLOAD_FILE_UPDATE") !=
             NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_DOWNLOAD_FILE_UPDATE",
                    sizeof(data)) != 0 ||
            parse_json_data(MESSAGE, "file", data, sizeof(data)) == 0 || strstr(data, "reserve_proofs_") == NULL)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_DOWNLOAD_FILE_DOWNLOAD") !=
             NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_DOWNLOAD_FILE_DOWNLOAD",
                    sizeof(data)) != 0)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_SYNC_CHECK_UPDATE") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_SYNC_CHECK_UPDATE", sizeof(data)) != 0 ||
            parse_json_data(MESSAGE, "data_hash", data, sizeof(data)) == 0 || strlen(data) != DATA_HASH_LENGTH ||
            parse_json_data(MESSAGE, "public_address", data, sizeof(data)) == 0 || strlen(data) != XCASH_WALLET_LENGTH ||
            strncmp(data, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0 ||
            parse_json_data(MESSAGE, "previous_block_hash", data, sizeof(data)) == 0 ||
            strlen(data) != BLOCK_HASH_LENGTH ||
            parse_json_data(MESSAGE, "current_round_part", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "current_round_part_backup_node", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "data", data, sizeof(data)) == 0 || strlen(data) != RANDOM_STRING_LENGTH ||
            parse_json_data(MESSAGE, "XCASH_DPOPS_signature", data, sizeof(data)) == 0 ||
            strlen(data) != VRF_BETA_LENGTH + VRF_PROOF_LENGTH)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_SYNC_CHECK_DOWNLOAD") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_SYNC_CHECK_DOWNLOAD", sizeof(data)) !=
                0 ||
            parse_json_data(MESSAGE, "delegates_database", data, sizeof(data)) == 0 ||
            (strncmp(data, "true", 4) != 0 && strncmp(data, "false", 5) != 0) ||
            parse_json_data(MESSAGE, "public_address", data, sizeof(data)) == 0 || strlen(data) != XCASH_WALLET_LENGTH ||
            strncmp(data, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0 ||
            parse_json_data(MESSAGE, "previous_block_hash", data, sizeof(data)) == 0 ||
            strlen(data) != BLOCK_HASH_LENGTH ||
            parse_json_data(MESSAGE, "current_round_part", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "current_round_part_backup_node", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "data", data, sizeof(data)) == 0 || strlen(data) != RANDOM_STRING_LENGTH ||
            parse_json_data(MESSAGE, "XCASH_DPOPS_signature", data, sizeof(data)) == 0 ||
            strlen(data) != VRF_BETA_LENGTH + VRF_PROOF_LENGTH)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_DOWNLOAD_FILE_UPDATE") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_DOWNLOAD_FILE_UPDATE", sizeof(data)) !=
                0)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_DOWNLOAD_FILE_DOWNLOAD") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_DOWNLOAD_FILE_DOWNLOAD", sizeof(data)) !=
                0)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_SYNC_CHECK_UPDATE") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_SYNC_CHECK_UPDATE", sizeof(data)) !=
                0 ||
            parse_json_data(MESSAGE, "data_hash", data, sizeof(data)) == 0 || strlen(data) != DATA_HASH_LENGTH ||
            parse_json_data(MESSAGE, "public_address", data, sizeof(data)) == 0 || strlen(data) != XCASH_WALLET_LENGTH ||
            strncmp(data, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0 ||
            parse_json_data(MESSAGE, "previous_block_hash", data, sizeof(data)) == 0 ||
            strlen(data) != BLOCK_HASH_LENGTH ||
            parse_json_data(MESSAGE, "current_round_part", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "current_round_part_backup_node", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "data", data, sizeof(data)) == 0 || strlen(data) != RANDOM_STRING_LENGTH ||
            parse_json_data(MESSAGE, "XCASH_DPOPS_signature", data, sizeof(data)) == 0 ||
            strlen(data) != VRF_BETA_LENGTH + VRF_PROOF_LENGTH)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_SYNC_CHECK_DOWNLOAD") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_SYNC_CHECK_DOWNLOAD", sizeof(data)) !=
                0 ||
            parse_json_data(MESSAGE, "statistics_database", data, sizeof(data)) == 0 ||
            (strncmp(data, "true", 4) != 0 && strncmp(data, "false", 5) != 0) ||
            parse_json_data(MESSAGE, "public_address", data, sizeof(data)) == 0 || strlen(data) != XCASH_WALLET_LENGTH ||
            strncmp(data, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0 ||
            parse_json_data(MESSAGE, "previous_block_hash", data, sizeof(data)) == 0 ||
            strlen(data) != BLOCK_HASH_LENGTH ||
            parse_json_data(MESSAGE, "current_round_part", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "current_round_part_backup_node", data, sizeof(data)) == 0 || strlen(data) != 1 ||
            parse_json_data(MESSAGE, "data", data, sizeof(data)) == 0 || strlen(data) != RANDOM_STRING_LENGTH ||
            parse_json_data(MESSAGE, "XCASH_DPOPS_signature", data, sizeof(data)) == 0 ||
            strlen(data) != VRF_BETA_LENGTH + VRF_PROOF_LENGTH)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_DOWNLOAD_FILE_UPDATE") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_DOWNLOAD_FILE_UPDATE", sizeof(data)) !=
                0)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_DOWNLOAD_FILE_DOWNLOAD") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_DOWNLOAD_FILE_DOWNLOAD",
                    sizeof(data)) != 0)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "XCASH_PROOF_OF_STAKE_TEST_DATA") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "XCASH_PROOF_OF_STAKE_TEST_DATA", sizeof(data)) != 0)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else if (strstr(MESSAGE, "XCASH_PROOF_OF_STAKE_TEST_DATA_2") != NULL)
    {
        if (parse_json_data(MESSAGE, "message_settings", data, sizeof(data)) == 0 ||
            strncmp(data, "XCASH_PROOF_OF_STAKE_TEST_DATA_2", sizeof(data)) != 0)
        {
          ERROR_PRINT("Invalid message");;
          return XCASH_ERROR;
        }
    }
    else
    {
      ERROR_PRINT("Invalid message");;
      return XCASH_ERROR;
    }
    return XCASH_OK;
}