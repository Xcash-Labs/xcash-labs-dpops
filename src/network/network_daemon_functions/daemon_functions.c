#include "daemon_functions.h"

/*---------------------------------------------------------------------------------------------------------
Name: get_current_block_height
Description: Gets the current block height of the network
Parameters:
  result - The string where you want the current block height to be saved to
Return: 0 if an error has occurred, 1 if successful
---------------------------------------------------------------------------------------------------------*/
int get_current_block_height(char *result)
{
    // Constants
    const char *HTTP_HEADERS[] = {"Content-Type: application/json", "Accept: application/json"};
    const size_t HTTP_HEADERS_LENGTH = sizeof(HTTP_HEADERS) / sizeof(HTTP_HEADERS[0]);
    const char *request_payload = "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"get_block_count\"}";

    // Buffer to store the response
    char response_data[BUFFER_SIZE] = {0};

    // First attempt to fetch block height
    if (send_http_request(response_data, XCASH_DAEMON_IP, "/json_rpc", XCASH_DAEMON_PORT,
                          "POST", HTTP_HEADERS, HTTP_HEADERS_LENGTH, request_payload, 
                          SEND_OR_RECEIVE_SOCKET_DATA_TIMEOUT_SETTINGS) != XCASH_OK ||
        parse_json_data(response_data, "count", result, BUFFER_SIZE) == 0)
    {  
        memset(response_data, 0, sizeof(response_data));
        memset(result, 0, BUFFER_SIZE);
        sleep(INVALID_RESERVE_PROOFS_SETTINGS);

        // Retry if the first attempt failed
        if (send_http_request(response_data, XCASH_DAEMON_IP, "/json_rpc", XCASH_DAEMON_PORT,
                              "POST", HTTP_HEADERS, HTTP_HEADERS_LENGTH, request_payload, 
                              SEND_OR_RECEIVE_SOCKET_DATA_TIMEOUT_SETTINGS) != XCASH_OK ||
            parse_json_data(response_data, "count", result, BUFFER_SIZE) == 0)
        {
            ERROR_PRINT("Could not get the current block height");
            return XCASH_ERROR;
        }
    }

    return XCASH_OK;
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
    // Constants
    const char *HTTP_HEADERS[] = {"Content-Type: application/json", "Accept: application/json"};
    const size_t HTTP_HEADERS_LENGTH = sizeof(HTTP_HEADERS) / sizeof(HTTP_HEADERS[0]);
    const char *REQUEST_PAYLOAD = "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"get_last_block_header\"}";

    // Variables
    char data[SMALL_BUFFER_SIZE] = {0};

    // Function to send request and parse result
    for (int attempt = 0; attempt < 2; attempt++) {
        if (send_http_request(data, XCASH_DAEMON_IP, "/json_rpc", XCASH_DAEMON_PORT, "POST",
                              HTTP_HEADERS, HTTP_HEADERS_LENGTH, REQUEST_PAYLOAD,
                              SEND_OR_RECEIVE_SOCKET_DATA_TIMEOUT_SETTINGS) > 0 &&
            parse_json_data(data, "hash", result, BUFFER_SIZE) > 0) {
            return XCASH_OK;
        }

        // First attempt failed, retry after delay
        if (attempt == 0) {
            WARNING_PRINT("Retrying to fetch previous block hash...");
            sleep(INVALID_RESERVE_PROOFS_SETTINGS);
        }
    }

    ERROR_PRINT("Could not get the previous block hash after multiple attempts.");
    return XCASH_ERROR;
}