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
        !parse_json_data(response, "result .signature", signature, MEDIUM_BUFFER_SIZE)) {
        return handle_error("sign_data", "Wallet signature failed", signature, payload, request);
    }

    if (strlen(signature) != XCASH_SIGN_DATA_LENGTH ||
        strncmp(signature, XCASH_SIGN_DATA_PREFIX, sizeof(XCASH_SIGN_DATA_PREFIX) - 1) != 0) {
        return handle_error("sign_data", "Invalid wallet signature format", signature, payload, request);
    }

    // Step 4: Append the signature to the original message
    snprintf(message + strlen(message) - 1, MEDIUM_BUFFER_SIZE - strlen(message),
        ",\"XCASH_DPOPS_signature\":\"%s\"}", signature);

    INFO_PRINT("Signed message:\n%s", message);

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
  char random_data[RANDOM_STRING_LENGTH + 1] = {0};
  char ck_previous_block_hash[BLOCK_HASH_LENGTH + 1] = {0};
  char raw_data[MEDIUM_BUFFER_SIZE] = {0};

  char request[MEDIUM_BUFFER_SIZE * 2] = {0};
  char response[MEDIUM_BUFFER_SIZE] = {0};

  // Extract all required fields
  if (parse_json_data(message, "XCASH_DPOPS_signature", signature, sizeof(signature)) != 1 ||
      parse_json_data(message, "public_address", ck_public_address, sizeof(ck_public_address)) != 1 ||
      parse_json_data(message, "v_previous_block_hash", ck_previous_block_hash, sizeof(ck_previous_block_hash)) != 1 ||
      parse_json_data(message, "v_current_round_part", ck_round_part, sizeof(ck_round_part)) != 1 ||
      parse_json_data(message, "v_random_data", random_data, sizeof(random_data)) != 1) {
    ERROR_PRINT("verify_data: Failed to parse one or more required fields.");
    return XCASH_ERROR;
  }

  // Rebuild original signed message
  snprintf(raw_data, sizeof(raw_data),
           "{"
           "\"v_previous_block_hash\":\"%s\","
           "\"v_current_round_part\":\"%s\","
           "\"v_random_data\":\"%s\""
           "}",
           ck_previous_block_hash,
           ck_round_part,
           random_data);

  // Prepare wallet verify request
  snprintf(request, sizeof(request),
           "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"verify\",\"params\":{"
           "\"data\":\"%s\","
           "\"address\":\"%s\","
           "\"signature\":\"%s\"}}",
           raw_data, ck_public_address, signature);

  if (send_http_request(response, sizeof(response), XCASH_WALLET_IP, "/json_rpc", XCASH_WALLET_PORT,
                        "POST", HTTP_HEADERS, HTTP_HEADERS_LENGTH,
                        request, SEND_OR_RECEIVE_SOCKET_DATA_TIMEOUT_SETTINGS) <= 0) {
    ERROR_PRINT("verify_data: HTTP request failed");
    return XCASH_ERROR;
  }

  // Parse response
  char result[8] = {0};
  if (parse_json_data(response, "verified", result, sizeof(result)) == 1 && strcmp(result, "true") == 0) {
    INFO_PRINT("Signature verified");
    return XCASH_OK;
  }

  ERROR_PRINT("Signature verification failed");
  return XCASH_ERROR;
}