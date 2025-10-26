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
bool is_blockchain_synced(char *target_height, char *height)
{
  if (target_height == NULL || height == NULL) {
    ERROR_PRINT("is_blockchain_synced: null output buffer(s)");
    return false;
  }

  // Constants
  const char *HTTP_HEADERS[] = {"Content-Type: application/json", "Accept: application/json"};
  const size_t HTTP_HEADERS_LENGTH = sizeof(HTTP_HEADERS) / sizeof(HTTP_HEADERS[0]);
  const char *request_payload = "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"get_info\"}";

  char response[SMALL_BUFFER_SIZE] = {0};
  char synced_flag[16] = {0};
  char status_flag[16] = {0};
  char offline_flag[16] = {0};
  char bs_flag[16] = {0};
  target_height[0] = '\0';
  height[0] = '\0';

  if (send_http_request(response, SMALL_BUFFER_SIZE,
                        XCASH_DAEMON_IP, "/json_rpc", XCASH_DAEMON_PORT,
                        "POST", HTTP_HEADERS, HTTP_HEADERS_LENGTH, request_payload,
                        HTTP_TIMEOUT_SETTINGS) == XCASH_OK &&
      parse_json_data(response, "result.synchronized",  synced_flag,  sizeof(synced_flag))  != 0 &&
      parse_json_data(response, "result.status",        status_flag,  sizeof(status_flag))  != 0 &&
      parse_json_data(response, "result.busy_syncing",  bs_flag,  sizeof(bs_flag))  != 0 &&
      parse_json_data(response, "result.height",        height,       BLOCK_HEIGHT_LENGTH)  != 0 &&
      parse_json_data(response, "result.target_height", target_height,BLOCK_HEIGHT_LENGTH)  != 0 &&
      parse_json_data(response, "result.offline",       offline_flag, sizeof(offline_flag)) != 0)
  {

    INFO_PRINT("response: %s", response);

    if (strcmp(synced_flag, "true") == 0 &&
        strcmp(status_flag, "OK") == 0 &&
        strcmp(bs_flag, "false") == 0 &&
        strcmp(offline_flag, "false") == 0) {
      return true;
    }
    DEBUG_PRINT("Daemon not yet synced or status not OK: synchronized=%s, status=%s, offline=%s, busy_syncing=%s", synced_flag, status_flag, offline_flag, bs_flag);
    return false;
  }

  INFO_PRINT("is_blockchain_synced: failed to query or parse /get_info");
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
    if (send_http_request(response_data, SMALL_BUFFER_SIZE, XCASH_DAEMON_IP, "/json_rpc", XCASH_DAEMON_PORT,
                              "POST", HTTP_HEADERS, HTTP_HEADERS_LENGTH, request_payload,
                              HTTP_TIMEOUT_SETTINGS) == XCASH_OK &&
            parse_json_data(response_data, "result.count", result, SMALL_BUFFER_SIZE) != 0) {
            return XCASH_OK;
    }

    ERROR_PRINT("Could not get the current block height.");
    return XCASH_ERROR;
}


/*---------------------------------------------------------------------------------------------------------
Name: get_current_block_hash
Description: Gets the current block height and last block hash of the network
Parameters:
  result - The string where you want the current block height to be saved to
Return: 0 if an error has occurred, 1 if successful
---------------------------------------------------------------------------------------------------------*/
int get_current_block_hash(char *result_hash) {
    if (!result_hash) {
        ERROR_PRINT("Invalid argument: result_hash is NULL.");
        return XCASH_ERROR;
    }

    // Constants
    const char *HTTP_HEADERS[] = {"Content-Type: application/json", "Accept: application/json"};
    const size_t HTTP_HEADERS_LENGTH = sizeof(HTTP_HEADERS) / sizeof(HTTP_HEADERS[0]);
    const char *request_payload = "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"get_last_block_header\"}";

    // Buffer to store the response
    char response_data[MEDIUM_BUFFER_SIZE] = {0};
    if (send_http_request(response_data, MEDIUM_BUFFER_SIZE, XCASH_DAEMON_IP, "/json_rpc", XCASH_DAEMON_PORT,
                              "POST", HTTP_HEADERS, HTTP_HEADERS_LENGTH, request_payload,
                              HTTP_TIMEOUT_SETTINGS) == XCASH_OK &&        
            parse_json_data(response_data, "result.block_header.hash", result_hash, BLOCK_HASH_LENGTH+1) != 0) {
            return XCASH_OK;
    }

    ERROR_PRINT("Could not get the current block hash.");
    return XCASH_ERROR;
}


/*---------------------------------------------------------------------------------------------------------
Name: get_previous_block_hash
Description: Gets the previous block hash of the network
Parameters:
  result - The string where you want the previous block hash to be saved to
Return: XCASH_OK (1) if successful, XCASH_ERROR (0) if an error occurs
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
    if (send_http_request(data, SMALL_BUFFER_SIZE, XCASH_DAEMON_IP, "/json_rpc", XCASH_DAEMON_PORT, "POST",
                              HTTP_HEADERS, HTTP_HEADERS_LENGTH, REQUEST_PAYLOAD,
                              HTTP_TIMEOUT_SETTINGS) > 0 &&
            parse_json_data(data, "result.block_header.hash", result, BLOCK_HASH_LENGTH+1) > 0)
    {
      return XCASH_OK;
    }

    ERROR_PRINT("Could not get the previous block hash.");
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
int get_block_template(char* result, size_t result_size, size_t* reserved_offset_out) {
  // Constants
  const char* HTTP_HEADERS[] = {"Content-Type: application/json", "Accept: application/json"};
  const size_t HTTP_HEADERS_LENGTH = sizeof(HTTP_HEADERS) / sizeof(HTTP_HEADERS[0]);
  const char* RPC_ENDPOINT = "/json_rpc";
  const char* RPC_METHOD = "POST";
  const char* JSON_REQUEST_PREFIX = "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"get_block_template\",\"params\":{\"wallet_address\":\"";
  const char* JSON_REQUEST_SUFFIX = "\",\"reserve_size\":212}";

  // Variables
  char message[SMALL_BUFFER_SIZE] = {0};
  char* response = (char*)calloc(SMALL_BUFFER_SIZE, sizeof(char));
  char reserved_offset_str[16] = {0};

  if (!response || !reserved_offset_out) {
    ERROR_PRINT("Memory allocation failed or invalid output pointer");
    if (response) free(response);
    return XCASH_ERROR;
  }

  // Clear response buffer before each use
  memset(response, 0, SMALL_BUFFER_SIZE);

  // Compose JSON request
  snprintf(message, sizeof(message), "%s%s%s", JSON_REQUEST_PREFIX, xcash_wallet_public_address, JSON_REQUEST_SUFFIX);

  // Send HTTP request
  if (send_http_request(response, SMALL_BUFFER_SIZE, XCASH_DAEMON_IP, RPC_ENDPOINT, XCASH_DAEMON_PORT, RPC_METHOD,
                        HTTP_HEADERS, HTTP_HEADERS_LENGTH, message, HTTP_TIMEOUT_SETTINGS) > 0 &&
      parse_json_data(response, "result.blocktemplate_blob", result, result_size) == XCASH_OK &&
      parse_json_data(response, "result.reserved_offset", reserved_offset_str, sizeof(reserved_offset_str)) == XCASH_OK) {
    *reserved_offset_out = (size_t)strtoul(reserved_offset_str, NULL, 10);

    DEBUG_PRINT("Block Temp: %s", response);
    free(response);
    return XCASH_OK;
  }

  ERROR_PRINT("Could not get the block template or reserved_offset");
  free(response);
  return XCASH_ERROR;
}

/*---------------------------------------------------------------------------------------------------------
Name: get_block_by_height
Description: Gets the current block info by height
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

  char request_json[SMALL_BUFFER_SIZE] = {0};
  char response[SMALL_BUFFER_SIZE] = {0};
  char result[256] = {0};

  // Format JSON-RPC message to submit block
  snprintf(request_json, sizeof(request_json),
           "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"submit_block\",\"params\":[\"%s\"]}",
           DATA);

  // Send HTTP request
  if (send_http_request(response, SMALL_BUFFER_SIZE, XCASH_DAEMON_IP, RPC_ENDPOINT, XCASH_DAEMON_PORT,
                        "POST", HTTP_HEADERS, HTTP_HEADERS_LENGTH,
                        request_json, BLOCK_TIMEOUT_SECONDS) > 0)
  {
    // Check if there's an error in the response
    if (parse_json_data(response, "result.status", result, sizeof(result)) == 1) {
      if (strcmp(result, "OK") == 0) {
        return XCASH_OK;
      }
    }
  }

  ERROR_PRINT("Could not submit the block template.");
  return XCASH_ERROR;
}


/*---------------------------------------------------------------------------------------------------------
Name: get_block_info_by_height
Description: Fetches block info at a given height via daemon JSON-RPC get_block:
             returns hash, reward (atomic units), timestamp, and orphan_status.
Parameters:
  height         - Block height to query
  out_hash       - Buffer to receive the block hash (size >= BLOCK_HASH_LENGTH+1)
  out_hash_len   - Size of out_hash buffer
  out_reward     - (optional) pointer to uint64_t for block reward
  out_timestamp  - (optional) pointer to uint64_t for block timestamp
  out_orphan     - (optional) pointer to bool/int for orphan status (true/1 or false/0)
Return:
  XCASH_OK on success, XCASH_ERROR on failure
---------------------------------------------------------------------------------------------------------*/
int get_block_info_by_height(uint64_t height,
                             char *out_hash, size_t out_hash_len,
                             uint64_t *out_reward,
                             uint64_t *out_timestamp,
                             bool *out_orphan)
{
    if (!out_hash || out_hash_len < (BLOCK_HASH_LENGTH + 1)) {
        ERROR_PRINT("get_block_info_by_height: invalid hash buffer");
        return XCASH_ERROR;
    }

    // ---- Constants / request setup ----
    const char *HTTP_HEADERS[] = {"Content-Type: application/json", "Accept: application/json"};
    const size_t HTTP_HEADERS_LENGTH = sizeof(HTTP_HEADERS) / sizeof(HTTP_HEADERS[0]);

    char request_payload[256] = {0};
    // {"jsonrpc":"2.0","id":"0","method":"get_block","params":{"height":<height>}}
    int n = snprintf(request_payload, sizeof(request_payload),
                     "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"get_block\","
                     "\"params\":{\"height\":%" PRIu64 "}}", height);
    if (n < 0 || (size_t)n >= sizeof(request_payload)) {
        ERROR_PRINT("get_block_info_by_height: payload too large");
        return XCASH_ERROR;
    }

    char response_data[MEDIUM_BUFFER_SIZE] = {0};
    if (send_http_request(response_data, sizeof(response_data),
                          XCASH_DAEMON_IP, "/json_rpc", XCASH_DAEMON_PORT,
                          "POST", HTTP_HEADERS, HTTP_HEADERS_LENGTH,
                          request_payload, HTTP_TIMEOUT_SETTINGS) != XCASH_OK) {
        ERROR_PRINT("get_block_info_by_height: HTTP request failed");
        return XCASH_ERROR;
    }

    // hash (string)
    if (parse_json_data(response_data, "result.block_header.hash",
                        out_hash, out_hash_len) != XCASH_OK) {
        ERROR_PRINT("get_block_info_by_height: missing result.block_header.hash, Response: %s", response_data);
        return XCASH_ERROR;
    }

    // reward (uint64)
    if (out_reward) {
        char tmp[64] = {0};
        if (parse_json_data(response_data, "result.block_header.reward", tmp, sizeof(tmp)) != XCASH_OK) {
            ERROR_PRINT("get_block_info_by_height: missing result.block_header.reward");
            return XCASH_ERROR;
        }
        char *endp = NULL;
        unsigned long long v = strtoull(tmp, &endp, 10);
        if (!endp || *endp != '\0') {  // tmp should be an integer string (no .000000)
            ERROR_PRINT("get_block_info_by_height: invalid reward value '%s'", tmp);
            return XCASH_ERROR;
        }
        *out_reward = (uint64_t)v;
    }

    // timestamp (uint64)
    if (out_timestamp) {
        char tmp[32] = {0};
        if (parse_json_data(response_data, "result.block_header.timestamp", tmp, sizeof(tmp)) != XCASH_OK) {
            ERROR_PRINT("get_block_info_by_height: missing result.block_header.timestamp");
            return XCASH_ERROR;
        }
        char *endp = NULL;
        unsigned long long v = strtoull(tmp, &endp, 10);
        if (!endp || *endp != '\0') {
            ERROR_PRINT("get_block_info_by_height: invalid timestamp value '%s'", tmp);
            return XCASH_ERROR;
        }
        *out_timestamp = (uint64_t)v;
    }

    // orphan_status (bool)
    if (out_orphan) {
        char tmp[8] = {0};
        if (parse_json_data(response_data, "result.block_header.orphan_status", tmp, sizeof(tmp)) != XCASH_OK) {
            ERROR_PRINT("get_block_info_by_height: missing result.block_header.orphan_status");
            return XCASH_ERROR;
        }
        if (strcmp(tmp, "true") == 0 || strcmp(tmp, "TRUE") == 0 || strcmp(tmp, "1") == 0) {
            *out_orphan = true;
        } else if (strcmp(tmp, "false") == 0 || strcmp(tmp, "FALSE") == 0 || strcmp(tmp, "0") == 0) {
            *out_orphan = false;
        } else {
            ERROR_PRINT("get_block_info_by_height: invalid orphan_status '%s'", tmp);
            return XCASH_ERROR;
        }
    }

    return XCASH_OK;
}
