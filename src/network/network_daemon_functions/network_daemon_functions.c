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


  PRINT_INFO("HERE.............................................");

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
  char outc_str[16] = {0}, inc_str[16] = {0};
  target_height[0] = '\0';
  height[0] = '\0';

  if (send_http_request(response, SMALL_BUFFER_SIZE,
                        XCASH_DAEMON_IP, "/json_rpc", XCASH_DAEMON_PORT,
                        "POST", HTTP_HEADERS, HTTP_HEADERS_LENGTH, request_payload,
                        HTTP_TIMEOUT_SETTINGS) == XCASH_OK &&
      parse_json_data(response, "result.synchronized",  synced_flag,  sizeof(synced_flag))  != 0 &&
      parse_json_data(response, "result.status",        status_flag,  sizeof(status_flag))  != 0 &&
      parse_json_data(response, "result.height",        height,       BLOCK_HEIGHT_LENGTH)  != 0 &&
      parse_json_data(response, "result.target_height", target_height,BLOCK_HEIGHT_LENGTH)  != 0 &&
      parse_json_data(response, "result.outgoing_connections_count", outc_str, sizeof(outc_str)) != 0 &&
      parse_json_data(response, "result.incoming_connections_count", inc_str, sizeof(inc_str))   != 0 &&
      parse_json_data(response, "result.offline",       offline_flag, sizeof(offline_flag)) != 0)
  {
    if (strcmp(synced_flag, "true") == 0 &&
        strcmp(status_flag, "OK") == 0 &&
        strcmp(offline_flag, "false") == 0) {

        return true;
    }

  INFO_PRINT("outc: %s     inc: %s", outc_str, inc_str);

    INFO_PRINT("Daemon not yet synced or status not OK: synchronized=%s, status=%s, offline=%s", synced_flag, status_flag, offline_flag);
    return false;
  }

  INFO_PRINT("outc: %s     inc: %s", outc_str, inc_str);

  INFO_PRINT("is_blockchain_synced: failed to query or parse /get_info");
  return false;
}




// expects: send_http_request(), parse_json_data() already available
// BLOCK_TIME_MINUTES defined; adjust thresholds as you like
#define LAG_OK 2
#define STALE_BLOCK_SECS (3 * (BLOCK_TIME) * 60)

bool is_blockchain_synced__NEW__(char* target_height, char* height) {
  if (!target_height || !height) return false;
  target_height[0] = '\0';
  height[0] = '\0';

  const char* HTTP_HEADERS[] = {"Content-Type: application/json", "Accept: application/json"};
  const size_t HTTP_HEADERS_LENGTH = sizeof(HTTP_HEADERS) / sizeof(HTTP_HEADERS[0]);
  char info[SMALL_BUFFER_SIZE] = {0};

  char status[16] = {0}, offline[16] = {0}, synced[16] = {0};
  char outc_str[16] = {0}, inc_str[16] = {0};

  if (send_http_request(info, sizeof(info),
                        XCASH_DAEMON_IP, "/get_info", XCASH_DAEMON_PORT,
                        "GET", HTTP_HEADERS, HTTP_HEADERS_LENGTH, NULL,
                        HTTP_TIMEOUT_SETTINGS) != XCASH_OK) {
    return false;
  }

  if (parse_json_data(info, "status", status, sizeof(status)) == 0 ||
      parse_json_data(info, "offline", offline, sizeof(offline)) == 0 ||
      parse_json_data(info, "height", height, BLOCK_HEIGHT_LENGTH) == 0 ||
      parse_json_data(info, "target_height", target_height, BLOCK_HEIGHT_LENGTH) == 0) {
    return false;
  }

  // optional, if available in your build of get_info:
  parse_json_data(info, "synchronized", synced, sizeof(synced));  // ignore if missing
  parse_json_data(info, "outgoing_connections_count", outc_str, sizeof(outc_str));
  parse_json_data(info, "incoming_connections_count", inc_str, sizeof(inc_str));

  const bool ok_status = (strcmp(status, "OK") == 0);
  const bool not_offline = (strcmp(offline, "false") == 0);
  const long long h = (long long)strtoull(height, NULL, 10);
  const long long th = (long long)strtoull(target_height, NULL, 10);
  const int outc = (int)strtol(outc_str[0] ? outc_str : "0", NULL, 10);
  const int inc = (int)strtol(inc_str[0] ? inc_str : "0", NULL, 10);

  if (!ok_status || !not_offline) return false;
  if (outc + inc < 2) return false;   // not enough peers yet
  if (th == 0) return false;          // target unknown
  if (h + LAG_OK < th) return false;  // too far behind

  // Freshness check via /get_last_block_header
  {
    char lbh[SMALL_BUFFER_SIZE] = {0};
    if (send_http_request(lbh, sizeof(lbh),
                          XCASH_DAEMON_IP, "/get_last_block_header", XCASH_DAEMON_PORT,
                          "GET", HTTP_HEADERS, HTTP_HEADERS_LENGTH, NULL,
                          HTTP_TIMEOUT_SETTINGS) == XCASH_OK) {
      // naive parse of nested "timestamp": your parse_json_data may already find it
      char ts_str[32] = {0};
      if (parse_json_data(lbh, "timestamp", ts_str, sizeof(ts_str)) != 0) {
        const long long ts = (long long)strtoll(ts_str, NULL, 10);
        const time_t now_s = time(NULL);
        if (ts <= 0 || (now_s - ts) > STALE_BLOCK_SECS) {
          return false;  // tip is stale compared to current time
        }
      }
    }
  }

  return true;
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
    if (send_http_request(data, SMALL_BUFFER_SIZE, XCASH_DAEMON_IP, "/json_rpc", XCASH_DAEMON_PORT, "POST",
                              HTTP_HEADERS, HTTP_HEADERS_LENGTH, REQUEST_PAYLOAD,
                              HTTP_TIMEOUT_SETTINGS) > 0 &&
            parse_json_data(data, "result.block_header.hash", result, BLOCK_HASH_LENGTH+1
            ) > 0)
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