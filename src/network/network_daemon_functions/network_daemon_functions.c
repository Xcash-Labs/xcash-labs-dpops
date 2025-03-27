#include "network_daemon_functions.h"

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
            sleep(INVALID_RESERVE_PROOFS_SETTINGS);
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
            sleep(INVALID_RESERVE_PROOFS_SETTINGS);
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
Return: 0 if an error has occured, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int get_block_template(char *result)
{
  // Constants
  const char* HTTP_HEADERS[] = {"Content-Type: application/json", "Accept: application/json"}; 
  const size_t HTTP_HEADERS_LENGTH = sizeof(HTTP_HEADERS) / sizeof(HTTP_HEADERS[0]);
  const char* RPC_ENDPOINT = "/json_rpc";
  const char* RPC_METHOD = "POST";
  const char* JSON_REQUEST_PREFIX = "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"get_block_template\",\"params\":{\"wallet_address\":\"";
  const char* JSON_REQUEST_SUFFIX = "\",\"reserve_size\":128}";

  // Variables
  char message[SMALL_BUFFER_SIZE];
  char response[BUFFER_SIZE];
  int retry_attempts = 2;

  for (int attempt = 0; attempt < retry_attempts; attempt++) 
  {
    // Clear buffers
    memset(message, 0, sizeof(message));
    memset(response, 0, sizeof(response));

    // Compose JSON request
    snprintf(message, sizeof(message), "%s%s%s", JSON_REQUEST_PREFIX, xcash_wallet_public_address, JSON_REQUEST_SUFFIX);

    // Send HTTP request
    if (send_http_request(response, XCASH_DAEMON_IP, RPC_ENDPOINT, XCASH_DAEMON_PORT, RPC_METHOD, HTTP_HEADERS, HTTP_HEADERS_LENGTH, message, SEND_OR_RECEIVE_SOCKET_DATA_TIMEOUT_SETTINGS) > 0 &&
        parse_json_data(response, "result.blocktemplate_blob", result, BUFFER_SIZE) == 1)
    {
      return XCASH_OK;
    }

    // On failure, sleep and retry
    if (attempt + 1 < retry_attempts) 
    {
      sleep(INVALID_RESERVE_PROOFS_SETTINGS);
    }
  }

  ERROR_PRINT("Could not get the block template");
  return XCASH_ERROR;
}

/*---------------------------------------------------------------------------------------------------------
Name: submit_block_template
Description: Adds a network block to the network
Parameters:
  DATA - The block_blob
Return: 0 if an error has occured, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int submit_block_template(const char* DATA)
{
  DEBUG_PRINT("Block template: %s", DATA);

  // Constants
  const char* HTTP_HEADERS[] = {"Content-Type: application/json","Accept: application/json"}; 
  const size_t HTTP_HEADERS_LENGTH = sizeof(HTTP_HEADERS)/sizeof(HTTP_HEADERS[0]);
  const size_t DATA_LENGTH = strnlen(DATA,BUFFER_SIZE);
  const char* RPC_ENDPOINT = "/json_rpc";

  // Variables
  char message[SMALL_BUFFER_SIZE];
  char data[SMALL_BUFFER_SIZE];
  char result[255];

  memset(data,0,sizeof(data));
  memset(message,0,sizeof(message));
  memset(result,0,sizeof(result));

  // create the message
  memcpy(message,"{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"submit_block\",\"params\":[\"",61);
  memcpy(message+61,DATA,DATA_LENGTH);
  memcpy(message+61+DATA_LENGTH,"\"]}",3);

  // Send HTTP request
  if (send_http_request(data, XCASH_DAEMON_IP, RPC_ENDPOINT, XCASH_DAEMON_PORT, "POST", HTTP_HEADERS, HTTP_HEADERS_LENGTH, message, SEND_OR_RECEIVE_SOCKET_DATA_TIMEOUT_SETTINGS) > 0 &&
      parse_json_data(data, "result.error.message", result, BUFFER_SIZE) == 1) {
    return XCASH_OK;
  } else {
    DEBUG_PRINT(result);
    ERROR_PRINT("Could not submit the block template");
    return XCASH_ERROR;
  }

}