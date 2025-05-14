#include "network_daemon_functions.h"

/*---------------------------------------------------------------------------------------------------------
Name: is_blockchain_synced
Description:
  Checks whether the local xcashd daemon is fully synced with the network.
  It validates both the "synchronized" flag and that height == target_height.
Parameters:
  result - The string where you want the current block height to be saved t
Return:
  true if the blockchain is synced, false otherwise.
---------------------------------------------------------------------------------------------------------*/
bool is_blockchain_synced(void) {
  const char* HTTP_HEADERS[] = {"Content-Type: application/json", "Accept: application/json"};
  const size_t HTTP_HEADERS_LENGTH = sizeof(HTTP_HEADERS) / sizeof(HTTP_HEADERS[0]);
  const char* RPC_ENDPOINT = "/get_info";

  char response[SMALL_BUFFER_SIZE] = {0};
  char synced_flag[16] = {0};
  char status_flag[16] = {0};
  char offline_flag[16] = {0};

  // Retry mechanism
  for (int attempt = 0; attempt < 2; ++attempt) {
    if (send_http_request(response, XCASH_DAEMON_IP, RPC_ENDPOINT, XCASH_DAEMON_PORT,
                          "GET", HTTP_HEADERS, HTTP_HEADERS_LENGTH, NULL,
                          SEND_OR_RECEIVE_SOCKET_DATA_TIMEOUT_SETTINGS) == XCASH_OK &&
        parse_json_data(response, "synchronized", synced_flag, sizeof(synced_flag)) != 0 &&
        parse_json_data(response, "status", status_flag, sizeof(status_flag)) != 0 &&
        parse_json_data(response, "offline", offline_flag, sizeof(offline_flag)) != 0) {
      if (strcmp(synced_flag, "true") == 0 &&
          strcmp(status_flag, "OK") == 0 &&
          strcmp(offline_flag, "false") == 0) {
        return true;
      }

      DEBUG_PRINT("Daemon not yet synced or status not OK: synchronized=%s, status=%s, offline=%s", synced_flag, status_flag, offline_flag);
      return false;
    }

    memset(response, 0, sizeof(response));
    memset(synced_flag, 0, sizeof(synced_flag));
    memset(status_flag, 0, sizeof(status_flag));
    memset(offline_flag, 0, sizeof(offline_flag));

    if (attempt == 0) {
      WARNING_PRINT("Retrying blockchain sync check...");
      sleep(RETRY_SECONDS);
    }
  }

  ERROR_PRINT("Could not determine blockchain sync status.");
  return false;
}

/*---------------------------------------------------------------------------------------------------------
Name: get_current_block_height
Description: Gets the current block height of the network
Parameters:
  result - The string where you want the current block height to be saved to
Return: 0 if an error has occurred, 1 if successful
---------------------------------------------------------------------------------------------------------*/
int get_current_block_height(char *result) {
    if (!result) {
        ERROR_PRINT("Invalid argument: result is NULL.");
        return XCASH_ERROR;
    }

    // Constants
    const char *HTTP_HEADERS[] = {"Content-Type: application/json", "Accept: application/json"};
    const size_t HTTP_HEADERS_LENGTH = sizeof(HTTP_HEADERS) / sizeof(HTTP_HEADERS[0]);
    const char *request_payload = "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"get_block_count\"}";

    // Buffer to store the response
    char response_data[SMALL_BUFFER_SIZE] = {0};

    // Retry mechanism
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (send_http_request(response_data, XCASH_DAEMON_IP, "/json_rpc", XCASH_DAEMON_PORT,
                              "POST", HTTP_HEADERS, HTTP_HEADERS_LENGTH, request_payload,
                              SEND_OR_RECEIVE_SOCKET_DATA_TIMEOUT_SETTINGS) == XCASH_OK &&
            parse_json_data(response_data, "result.count", result, SMALL_BUFFER_SIZE) != 0) {
            return XCASH_OK;
        }

        // Clear buffers before retry
        memset(response_data, 0, sizeof(response_data));
        memset(result, 0, SMALL_BUFFER_SIZE);
        
        // Sleep only if this is not the last attempt
        if (attempt == 0) {
            WARNING_PRINT("Retrying to fetch of block height...");
            sleep(RETRY_SECONDS);
        }
    }

    ERROR_PRINT("Could not get the current block height.");
    return XCASH_ERROR;
}

/*---------------------------------------------------------------------------------------------------------
Name: get_previous_block_hash
Description: Gets the previous block hash of the network
Parameters:
  result - The string where you want the previous block hash to be saved to
Return: XCASH_OK (1) if successful, XCASH_ERROR (-1) if an error occurs
---------------------------------------------------------------------------------------------------------*/
int get_previous_block_hash(char *result)
{
    if (!result) {
        ERROR_PRINT("Invalid argument: result is NULL.");
        return XCASH_ERROR;
    }
    
    // Constants
    const char *HTTP_HEADERS[] = {"Content-Type: application/json", "Accept: application/json"};
    const size_t HTTP_HEADERS_LENGTH = sizeof(HTTP_HEADERS) / sizeof(HTTP_HEADERS[0]);
    const char *REQUEST_PAYLOAD = "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"get_last_block_header\"}";

    // Variables
    char data[SMALL_BUFFER_SIZE] = {0};

    // Function to send request and parse result
    for (int attempt = 0; attempt < 2; attempt++)
    {
        if (send_http_request(data, XCASH_DAEMON_IP, "/json_rpc", XCASH_DAEMON_PORT, "POST",
                              HTTP_HEADERS, HTTP_HEADERS_LENGTH, REQUEST_PAYLOAD,
                              SEND_OR_RECEIVE_SOCKET_DATA_TIMEOUT_SETTINGS) > 0 &&
            parse_json_data(data, "result.block_header.hash", result, BLOCK_HASH_LENGTH+1
            ) > 0)
        {
            return XCASH_OK;
        }

        // First attempt failed, retry after delay
        if (attempt == 0)
        {
            WARNING_PRINT("Retrying to fetch previous block hash...");
            sleep(RETRY_SECONDS);
        }
    }

    ERROR_PRINT("Could not get the previous block hash after multiple attempts.");
    return XCASH_ERROR;
}

/*---------------------------------------------------------------------------------------------------------
Name: get_block_template
Description: Gets the block template for creating a new block
Parameters:
  result - The block template
  result_size - The size of the block_template
Return: 0 if an error has occured, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int get_block_template(char* result, size_t result_size)
{
  // Constants
  const char* HTTP_HEADERS[] = {"Content-Type: application/json", "Accept: application/json"}; 
  const size_t HTTP_HEADERS_LENGTH = sizeof(HTTP_HEADERS) / sizeof(HTTP_HEADERS[0]);
  const char* RPC_ENDPOINT = "/json_rpc";
  const char* RPC_METHOD = "POST";
  const char* JSON_REQUEST_PREFIX = "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"get_block_template\",\"params\":{\"wallet_address\":\"";
  const char* JSON_REQUEST_SUFFIX = "\",\"reserve_size\":320}";

  // Variables
  char message[VSMALL_BUFFER_SIZE] = {0};
  char* response = (char*)calloc(BUFFER_SIZE, sizeof(char));
  int retry_attempts = 2;

  if (!response) {
    ERROR_PRINT("Memory allocation failed for response buffer");
    return XCASH_ERROR;
  }

  for (int attempt = 0; attempt < retry_attempts; attempt++) 
  {
    // Clear response buffer before each use
    memset(response, 0, BUFFER_SIZE);

    // Compose JSON request
    snprintf(message, sizeof(message), "%s%s%s", JSON_REQUEST_PREFIX, xcash_wallet_public_address, JSON_REQUEST_SUFFIX);

    // Send HTTP request
    if (send_http_request(response, XCASH_DAEMON_IP, RPC_ENDPOINT, XCASH_DAEMON_PORT, RPC_METHOD, HTTP_HEADERS, HTTP_HEADERS_LENGTH, message, SEND_OR_RECEIVE_SOCKET_DATA_TIMEOUT_SETTINGS) > 0 &&
        parse_json_data(response, "result.blocktemplate_blob", result, result_size) == 1)
    {
      free(response);
      return XCASH_OK;
    }

    // On failure, sleep and retry
    if (attempt + 1 < retry_attempts) 
    {
      sleep(RETRY_SECONDS);
    }
  }

  ERROR_PRINT("Could not get the block template");
  free(response);

  return XCASH_ERROR;
}

/*---------------------------------------------------------------------------------------------------------
Name: submit_block_template
Description: Submits the final block blob to the xcashd daemon via JSON-RPC using `submit_block`.
Parameters:
  DATA - Hex-encoded block blob string to be submitted.
Return:
  XCASH_OK on success, XCASH_ERROR on failure.
---------------------------------------------------------------------------------------------------------*/
bool submit_block_template(const char* DATA)
{
  if (!DATA || strlen(DATA) == 0) {
    ERROR_PRINT("Invalid block data for submission.");
    return XCASH_ERROR;
  }

  const char* HTTP_HEADERS[] = {"Content-Type: application/json", "Accept: application/json"};
  const size_t HTTP_HEADERS_LENGTH = sizeof(HTTP_HEADERS) / sizeof(HTTP_HEADERS[0]);
  const char* RPC_ENDPOINT = "/json_rpc";

  char request_json[BUFFER_SIZE] = {0};
  char response[BUFFER_SIZE] = {0};
  char result[256] = {0};

  // Format JSON-RPC message to submit block
  snprintf(request_json, sizeof(request_json),
           "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"submit_block\",\"params\":[\"%s\"]}",
           DATA);

  // Send HTTP request
  if (send_http_request(response, XCASH_DAEMON_IP, RPC_ENDPOINT, XCASH_DAEMON_PORT,
                        "POST", HTTP_HEADERS, HTTP_HEADERS_LENGTH,
                        request_json, SEND_OR_RECEIVE_SOCKET_DATA_TIMEOUT_SETTINGS) > 0)
  {
    // Check if there's an error in the response
    if (parse_json_data(response, "error.message", result, sizeof(result)) == 1) {
      DEBUG_PRINT("Block submission returned error: %s", result);
      return XCASH_ERROR;
    }

    DEBUG_PRINT("Block submitted successfully.");
    return XCASH_OK;
  }

  ERROR_PRINT("Could not submit the block template.");
  return XCASH_ERROR;
}