#include "network_wallet_functions.h"

/*---------------------------------------------------------------------------------------------------------
Name: get_public_address
Description: Gets the public address of your wallet
Return: XCASH_OK (1) if successful, XCASH_ERROR (0) if an error occurs
---------------------------------------------------------------------------------------------------------*/
int get_public_address(void)
{
    // Constants
    const char* HTTP_HEADERS[] = {"Content-Type: application/json", "Accept: application/json"}; 
    const size_t HTTP_HEADERS_LENGTH = sizeof(HTTP_HEADERS) / sizeof(HTTP_HEADERS[0]);
    const char* GET_PUBLIC_ADDRESS_DATA = "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"get_address\"}";

    // Variables
    char data[SMALL_BUFFER_SIZE] = {0};

    // Send HTTP request to get the public address
    if (send_http_request(data, SMALL_BUFFER_SIZE, XCASH_WALLET_IP, "/json_rpc", XCASH_WALLET_PORT, "POST", 
                          HTTP_HEADERS, HTTP_HEADERS_LENGTH, GET_PUBLIC_ADDRESS_DATA, 
                          SEND_OR_RECEIVE_SOCKET_DATA_TIMEOUT_SETTINGS) <= 0) 
    {  
        ERROR_PRINT("Could not get the public address");
        return XCASH_ERROR;
    }

    // Clear the existing public address buffer
    memset(xcash_wallet_public_address, 0, sizeof(xcash_wallet_public_address));

    // Parse the JSON response to extract the address
    if (parse_json_data(data, "result.address", xcash_wallet_public_address, sizeof(xcash_wallet_public_address)) == 0) 
    {
        ERROR_PRINT("Could not parse the public address from the response");
        return XCASH_ERROR;
    }

    // Validate the public address length and prefix
    if (strnlen(xcash_wallet_public_address, sizeof(xcash_wallet_public_address)) != XCASH_WALLET_LENGTH || 
        strncmp(xcash_wallet_public_address, XCASH_WALLET_PREFIX, strlen(XCASH_WALLET_PREFIX)) != 0) 
    {
        ERROR_PRINT("Invalid public address format received");
        return XCASH_ERROR;
    }

    return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: check_reserve_proof
Description: Checks a reserve proof
Parameters:
  result - The amount for the reserve proof
  PUBLIC_ADDRESS - The public address that created the reserve proof
  RESERVE_PROOF - The reserve proof
Return:  0 if the reserve proof is invalid, 1 if the reserve proof is valid
---------------------------------------------------------------------------------------------------------*/
int check_reserve_proofs(uint64_t vote_atomic_amount, const char* PUBLIC_ADDRESS, const char* RESERVE_PROOF) {
  if (!PUBLIC_ADDRESS || !RESERVE_PROOF) {
    ERROR_PRINT("check_reserve_proofs: invalid arguments");
    return XCASH_ERROR;
  }

  static const char* HTTP_HEADERS[] = {"Content-Type: application/json", "Accept: application/json"};
  static const size_t HTTP_HEADERS_LENGTH = sizeof(HTTP_HEADERS) / sizeof(HTTP_HEADERS[0]);

  char request_payload[MEDIUM_BUFFER_SIZE] = {0};
  int n = snprintf(request_payload, sizeof(request_payload),
                   "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"check_reserve_proof\","
                   "\"params\":{\"address\":\"%.*s\",\"message\":\"\",\"signature\":\"%s\"}}",
                   XCASH_WALLET_LENGTH, PUBLIC_ADDRESS, RESERVE_PROOF);
  if (n < 0 || (size_t)n >= sizeof(request_payload)) {
    ERROR_PRINT("check_reserve_proofs: request payload too large");
    return XCASH_ERROR;
  }

  // Send
  char response[SMALL_BUFFER_SIZE] = {0};
  if (send_http_request(response, sizeof(response),
                        XCASH_WALLET_IP, "/json_rpc", XCASH_WALLET_PORT, "POST",
                        HTTP_HEADERS, HTTP_HEADERS_LENGTH,
                        request_payload, HTTP_TIMEOUT_SETTINGS) != XCASH_OK) {
    ERROR_PRINT("Could not validate the reserve proof (HTTP error)");
    return XCASH_ERROR;
  }

  // Parse required fields
  char good[8] = {0};
  char spent[32] = {0};
  char total_str[64] = {0};
  if (parse_json_data(response, "result.good", good, sizeof(good)) == 0 ||
      parse_json_data(response, "result.spent", spent, sizeof(spent)) == 0 ||
      parse_json_data(response, "result.total", total_str, sizeof(total_str)) == 0) {
    ERROR_PRINT("Reserve proof validation: missing fields");
    return XCASH_ERROR;
  }

  // Must be good and unspent
  if (good[0] != 't' || strcmp(spent, "0") != 0) {
    WARNING_PRINT("Reserve proof invalid or indicates spent outputs");
    return XCASH_ERROR;
  }

  // Convert total_str -> uint64_t (atomic units)
  errno = 0;
  char* end = NULL;
  unsigned long long total_val_ull = strtoull(total_str, &end, 10);
  if (errno != 0 || end == total_str || *end != '\0') {
    ERROR_PRINT("Failed to parse result.total as integer: '%s'", total_str);
    return XCASH_ERROR;
  }
  uint64_t proven_atomic = (uint64_t)total_val_ull;

  // Compare against requested amount
  if (proven_atomic < vote_atomic_amount) {
    WARNING_PRINT("Proof insufficient: proven=%" PRIu64 " requested=%" PRIu64,
                  proven_atomic, vote_atomic_amount);
    return XCASH_ERROR;
  }

  return XCASH_OK;
}