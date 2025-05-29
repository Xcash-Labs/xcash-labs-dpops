#include "network_security_functions.h"

// Helper function for error handling
int handle_error(const char *function_name, const char *message, char *buf1, char *buf2, char *buf3)
{
  ERROR_PRINT("%s: %s", function_name, message);
  if (buf1) free(buf1);
  if (buf2) free(buf2);
  if (buf3) free(buf3);
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

    char *signature = calloc(MEDIUM_BUFFER_SIZE, sizeof(char));
    char *payload = calloc(MEDIUM_BUFFER_SIZE, sizeof(char));
    char *request = calloc(MEDIUM_BUFFER_SIZE * 2, sizeof(char));
    char response[MEDIUM_BUFFER_SIZE] = {0};
    char random_data[RANDOM_STRING_LENGTH + 1] = {0};

    if (!signature || !payload || !request) {
        FATAL_ERROR_EXIT("sign_data: Memory allocation failed");
    }

    // Generate random data
    if (!random_string(random_data, RANDOM_STRING_LENGTH)) {
        return handle_error("sign_data", "Failed to generate random data", signature, payload, request);
    }

    // Step 1: Build the full JSON message to be signed
    snprintf(message + strlen(message) - 1, MEDIUM_BUFFER_SIZE - strlen(message),
      ",\"v_previous_block_hash\":\"%s\","
      "\"v_current_round_part\":\"%s\","
      "\"v_random_data\":\"%.*s\""
      "}",
      previous_block_hash,
      current_round_part,
      RANDOM_STRING_LENGTH, random_data);

    // Step 2: Escape the message for the JSON-RPC request
    strncpy(payload, message, MEDIUM_BUFFER_SIZE);
    string_replace(payload, MEDIUM_BUFFER_SIZE, "\"", "\\\"");

    snprintf(request, MEDIUM_BUFFER_SIZE * 2,
        "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"sign\",\"params\":{\"data\":\"%s\"}}",
        payload
    );

    // Step 3: Send signing request to wallet
    if (send_http_request(response, MEDIUM_BUFFER_SIZE, XCASH_WALLET_IP, "/json_rpc", XCASH_WALLET_PORT,
                          "POST", HTTP_HEADERS, HTTP_HEADERS_LENGTH,
                          request, SEND_OR_RECEIVE_SOCKET_DATA_TIMEOUT_SETTINGS) <= 0 ||
        !parse_json_data(response, "result.signature", signature, MEDIUM_BUFFER_SIZE)) {
        return handle_error("sign_data", "Wallet signature failed", signature, payload, request);
    }

    if (strlen(signature) != XCASH_SIGN_DATA_LENGTH ||
        strncmp(signature, XCASH_SIGN_DATA_PREFIX, sizeof(XCASH_SIGN_DATA_PREFIX) - 1) != 0) {
        return handle_error("sign_data", "Invalid wallet signature format", signature, payload, request);
    }

    // Step 4: Append the signature to the original message
    snprintf(message + strlen(message) - 1, MEDIUM_BUFFER_SIZE - strlen(message),
        ",\"XCASH_DPOPS_signature\":\"%s\"}", signature);

    free(signature);
    free(payload);
    free(request);
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
  char response[SMALL_BUFFER_SIZE];

  snprintf(request_json, sizeof(request_json),
    "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"sign\",\"params\":{\"data\":\"%s\"}}",
    block_blob_hex
  );

  const char* headers[] = { "Content-Type: application/json", "Accept: application/json" };
  if (send_http_request(response, SMALL_BUFFER_SIZE, XCASH_WALLET_IP, "/json_rpc", XCASH_WALLET_PORT,
                        "POST", headers, 2, request_json, HTTP_TIMEOUT_SETTINGS) <= 0) {
    return false;
  }

  return parse_json_data(response, "result.signature", signature_out, sig_out_len);
}

/*---------------------------------------------------------------------------------------------------------
 * Name: verify_data
 * Description:
 *   Verifies the authenticity and integrity of signed messages within the DPoPS protocol.
 *   Supports wallet-based JSON-RPC signatures.
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
int verify_data(const char *message)
{
  const char *HTTP_HEADERS[] = {"Content-Type: application/json", "Accept: application/json"};
  const size_t HTTP_HEADERS_LENGTH = sizeof(HTTP_HEADERS) / sizeof(HTTP_HEADERS[0]);

  char signature[XCASH_SIGN_DATA_LENGTH + 1] = {0};
  char ck_public_address[XCASH_WALLET_LENGTH + 1] = {0};
  char ck_round_part[3] = {0};
  char ck_previous_block_hash[BLOCK_HASH_LENGTH + 1] = {0};
  char raw_data[MEDIUM_BUFFER_SIZE] = {0};
  char request[MEDIUM_BUFFER_SIZE * 2] = {0};
  char response[MEDIUM_BUFFER_SIZE] = {0};

  // Extract all required fields
  if (parse_json_data(message, "XCASH_DPOPS_signature", signature, sizeof(signature)) != 1 ||
      parse_json_data(message, "public_address", ck_public_address, sizeof(ck_public_address)) != 1 ||
      parse_json_data(message, "v_previous_block_hash", ck_previous_block_hash, sizeof(ck_previous_block_hash)) != 1 ||
      parse_json_data(message, "v_current_round_part", ck_round_part, sizeof(ck_round_part)) != 1) {
    ERROR_PRINT("verify_data: Failed to parse one or more required fields.");
    return XCASH_ERROR;
  }

  if (strcmp(current_round_part, ck_round_part) != 0) {
    ERROR_PRINT("Failed Signature Verification, round part timing issue: current round %s - message round %s.", current_round_part, ck_round_part);
    return XCASH_ERROR;
  }

  if (strcmp(previous_block_hash, ck_previous_block_hash) != 0) {
    ERROR_PRINT("Failed Signature Verification, previous block hash is not valid");
    return XCASH_ERROR;
  }
  
  strncpy(raw_data, message, sizeof(raw_data));

  char *sig_pos = strstr(raw_data, ",\"XCASH_DPOPS_signature\"");
  if (sig_pos) {
    *sig_pos = '}';         // Replace start of signature field with closing brace
    *(sig_pos + 1) = '\0';  // Null-terminate the string
  } else {
    ERROR_PRINT("Signature field not found.");
    return XCASH_ERROR;
  }

  char escaped[MEDIUM_BUFFER_SIZE] = {0};
  strncpy(escaped, raw_data, MEDIUM_BUFFER_SIZE);
  string_replace(escaped, MEDIUM_BUFFER_SIZE, "\"", "\\\"");

  // Prepare wallet verify request
  snprintf(request, sizeof(request),
           "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"verify\",\"params\":{"
           "\"data\":\"%s\","
           "\"address\":\"%s\","
           "\"signature\":\"%s\"}}",
           escaped, ck_public_address, signature);

  if (send_http_request(response, sizeof(response), XCASH_WALLET_IP, "/json_rpc", XCASH_WALLET_PORT,
                        "POST", HTTP_HEADERS, HTTP_HEADERS_LENGTH,
                        request, SEND_OR_RECEIVE_SOCKET_DATA_TIMEOUT_SETTINGS) <= 0) {
    ERROR_PRINT("verify_data: HTTP request failed");
    return XCASH_ERROR;
  }

  // Parse response
  char result[8] = {0};
  if (parse_json_data(response, "result.good", result, sizeof(result)) == 1 && strcmp(result, "true") == 0) {
    return XCASH_OK;
  }

  WARNING_PRINT("Signature verification failed for transaction");
  return XCASH_ERROR;
}

/*---------------------------------------------------------------------------------------------------------
 * @brief Verifies a signed RPC message using the wallet's verify endpoint.
 *
 * This function is used to validate the authenticity of a message that follows
 * a `|`-delimited format, where the final field is a signature generated by the
 * sender's wallet. The function:
 *   - Extracts the signature from the end of the message.
 *   - Extracts the public address from the 5th field (index 4).
 *   - Escapes the message contents for safe embedding in a JSON payload.
 *   - Constructs a JSON-RPC request and sends it to the wallet's `verify` endpoint.
 *   - Parses the response to confirm the validity of the signature.
 *
 * Expected message format:
 *   "TYPE|param1|param2|...|<public_address>|...|<signature>"
 *
 * @param message The original `|`-delimited message string including the signature.
 *
 * @return XCASH_OK if the signature is valid, otherwise XCASH_ERROR.
---------------------------------------------------------------------------------------------------------*/
int verify_bar_data(const char *message) {
  const char *HTTP_HEADERS[] = {"Content-Type: application/json", "Accept: application/json"};
  const size_t HTTP_HEADERS_LENGTH = sizeof(HTTP_HEADERS) / sizeof(HTTP_HEADERS[0]);

  char message_copy[MEDIUM_BUFFER_SIZE * 2] = {0};
  char public_address[XCASH_WALLET_LENGTH + 1] = {0};
  char signature[XCASH_SIGN_DATA_LENGTH + 1] = {0};
  char *last_delim = NULL;

  // Copy input to avoid modifying original
  strncpy(message_copy, message, sizeof(message_copy) - 1);

  // Extract signature
  last_delim = strrchr(message_copy, '|');
  if (!last_delim || (last_delim - message_copy) <= 0) {
    ERROR_PRINT("verify_rpc_data: Signature delimiter not found or too short.");
    return XCASH_ERROR;
  }

  // Extract and null-terminate the message at the signature
  strncpy(signature, last_delim + 1, sizeof(signature) - 1);
  *last_delim = '\0';  // Trim message to exclude signature

  // Extract public address (4th field)
  const int PUBLIC_ADDRESS_INDEX = 4;
  const char *start = message_copy;
  int pipe_count = 0;

  while (*start && pipe_count < PUBLIC_ADDRESS_INDEX) {
    if (*start == '|') pipe_count++;
    start++;
  }

  if (pipe_count != PUBLIC_ADDRESS_INDEX) {
    ERROR_PRINT("verify_rpc_data: Failed to extract public address.");
    return XCASH_ERROR;
  }

  const char *end = strchr(start, '|');
  if (!end || (end - start) >= XCASH_WALLET_LENGTH) {
    ERROR_PRINT("verify_rpc_data: Invalid or missing public address.");
    return XCASH_ERROR;
  }

  strncpy(public_address, start, end - start);
  public_address[end - start] = '\0';

  // Escape the message
  char escaped[MEDIUM_BUFFER_SIZE * 2] = {0};
  strncpy(escaped, message_copy, sizeof(escaped) - 1);
  string_replace(escaped, sizeof(escaped), "\"", "\\\"");

  // Build verification request
  char request[MEDIUM_BUFFER_SIZE * 4] = {0};
  snprintf(request, sizeof(request),
           "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"verify\",\"params\":{"
           "\"data\":\"%s\","
           "\"address\":\"%s\","
           "\"signature\":\"%s\"}}",
           escaped, public_address, signature);

  // Send HTTP request
  char response[MEDIUM_BUFFER_SIZE * 2] = {0};
  if (send_http_request(response, sizeof(response), XCASH_WALLET_IP, "/json_rpc", XCASH_WALLET_PORT,
                        "POST", HTTP_HEADERS, HTTP_HEADERS_LENGTH, request,
                        SEND_OR_RECEIVE_SOCKET_DATA_TIMEOUT_SETTINGS) <= 0) {
    ERROR_PRINT("verify_rpc_data: HTTP request failed");
    return XCASH_ERROR;
  }

  // Parse response
  char result[8] = {0};
  if (parse_json_data(response, "result.good", result, sizeof(result)) == 1 &&
      strcmp(result, "true") == 0) {
    return XCASH_OK;
  }

  WARNING_PRINT("verify_rpc_data: Signature check failed");
  return XCASH_ERROR;
}