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
int check_reserve_proofs(uint64_t vote_amount_atomic, const char* PUBLIC_ADDRESS, const char* RESERVE_PROOF) {
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
  if (strcmp(good, "true") != 0 || strcmp(spent, "0") != 0) {
    WARNING_PRINT("Reserve proof invalid or indicates spent outputs (good=%s, spent=%s)",
                  good, spent);
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

  // Compare against requested amount (ensure you use the correct var name)
  if (proven_atomic < vote_amount_atomic) {
    WARNING_PRINT("Proof insufficient: proven=%" PRIu64 " requested=%" PRIu64,
                  proven_atomic, vote_amount_atomic);
    return XCASH_ERROR;
  }

  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: get_unlocked_balance
Description: Queries the wallet RPC `get_balance` (account_index = 0) and returns the unlocked balance.
Parameters:
  unlocked_balance_out - [out] Receives `result.unlocked_balance` (atomic units)
Return:  XCASH_OK (1) on success, XCASH_ERROR (0) on failure
---------------------------------------------------------------------------------------------------------*/
int get_unlocked_balance(uint64_t* unlocked_balance_out)
{
  if (!unlocked_balance_out) {
    ERROR_PRINT("get_unlocked_balance: unlocked_balance_out is NULL");
    return XCASH_ERROR;
  }

  // Static headers & payload; no formatting or size computations needed.
  static const char* HTTP_HEADERS[] = { "Content-Type: application/json", "Accept: application/json" };
  static const size_t HTTP_HEADERS_LENGTH = 2;

  // account_index hardcoded to 0; constant JSON payload
  static const char* REQUEST_PAYLOAD =
      "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"get_balance\","
      "\"params\":{\"account_index\":0}}";

  char response[SMALL_BUFFER_SIZE] = {0};
  if (send_http_request(response, sizeof(response),
                        XCASH_WALLET_IP, "/json_rpc", XCASH_WALLET_PORT, "POST",
                        HTTP_HEADERS, HTTP_HEADERS_LENGTH,
                        REQUEST_PAYLOAD, HTTP_TIMEOUT_SETTINGS) != XCASH_OK) {
    ERROR_PRINT("get_unlocked_balance: HTTP error");
    return XCASH_ERROR;
  }

  // Parse result.unlocked_balance
  char unlocked_str[64] = {0};
  if (parse_json_data(response, "result.unlocked_balance", unlocked_str, sizeof(unlocked_str)) == 0) {
    ERROR_PRINT("get_unlocked_balance: missing result.unlocked_balance");
    return XCASH_ERROR;
  }

  errno = 0;
  char* endp = NULL;
  unsigned long long v_unlocked = strtoull(unlocked_str, &endp, 10);
  if (errno != 0 || endp == unlocked_str || *endp != '\0') {
    ERROR_PRINT("get_unlocked_balance: parse failed: '%s'", unlocked_str);
    return XCASH_ERROR;
  }

  *unlocked_balance_out = (uint64_t)v_unlocked;
  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: wallet_payout_send
Description:
  Sends a payout for a single destination using monero-wallet-rpc `transfer`, with the fee
  deducted from the destination amount (subtract_fee_from_outputs:[0]).

Parameters:
  addr           - Destination public address (Base58)
  amount_atomic  - Amount to send, in atomic units
  reason         - Optional string for logging ("min_threshold" | "stale_7d" etc.)

Return:
  XCASH_OK (1) on success, XCASH_ERROR (0) on failure

Notes:
  - Uses account_index=0, priority=0, ring_size left to wallet default.
  - On success, extracts and logs result.tx_hash and result.fee if present.
  - Requires:
      send_http_request(...)
      parse_json_data(...)
      XCASH_WALLET_IP, XCASH_WALLET_PORT, HTTP_TIMEOUT_SETTINGS
      SMALL_BUFFER_SIZE
      INFO_PRINT / ERROR_PRINT macros
---------------------------------------------------------------------------------------------------------*/
int wallet_payout_send(const char* addr, int64_t amount_atomic, const char* reason)
{
  if (!addr || addr[0] == '\0') {
    ERROR_PRINT("wallet_payout_send: invalid address");
    return XCASH_ERROR;
  }
  if (amount_atomic <= 0) {
    ERROR_PRINT("wallet_payout_send: non-positive amount %" PRId64, amount_atomic);
    return XCASH_ERROR;
  }

  // Static headers
  static const char* HTTP_HEADERS[] = { "Content-Type: application/json", "Accept: application/json" };
  static const size_t HTTP_HEADERS_LENGTH = 2;

  // Build request payload:
  // - Single destination
  // - Fee taken from the only destination: subtract_fee_from_outputs:[0]
  // - priority:0, get_tx_key:true (harmless; useful for audit)
  char request[SMALL_BUFFER_SIZE] = {0};
  int n = snprintf(request, sizeof(request),
    "{"
      "\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"transfer\","
      "\"params\":{"
        "\"destinations\":[{\"amount\":%" PRId64 ",\"address\":\"%s\"}],"
        "\"account_index\":0,"
        "\"priority\":0,"
        "\"get_tx_key\":true,"
        "\"subtract_fee_from_outputs\":[0]"
      "}"
    "}",
    amount_atomic, addr
  );
  if (n < 0 || (size_t)n >= sizeof(request)) {
    ERROR_PRINT("wallet_payout_send: request too large");
    return XCASH_ERROR;
  }

  // Send
  char response[SMALL_BUFFER_SIZE] = {0};
  if (send_http_request(response, sizeof(response),
                        XCASH_WALLET_IP, "/json_rpc", XCASH_WALLET_PORT, "POST",
                        HTTP_HEADERS, HTTP_HEADERS_LENGTH,
                        request, HTTP_TIMEOUT_SETTINGS) != XCASH_OK) {
    ERROR_PRINT("wallet_payout_send: HTTP error");
    return XCASH_ERROR;
  }

  // Quick error check: many wallet-rpc errors include "error.code" / "error.message"
  char err_code_buf[32] = {0};
  if (parse_json_data(response, "error.code", err_code_buf, sizeof(err_code_buf)) != 0) {
    char err_msg_buf[256] = {0};
    parse_json_data(response, "error.message", err_msg_buf, sizeof(err_msg_buf)); // best-effort
    ERROR_PRINT("wallet_payout_send: RPC error code=%s msg=%s", err_code_buf,
                err_msg_buf[0] ? err_msg_buf : "(none)");
    return XCASH_ERROR;
  }

  // Parse success fields (best-effort)
  char tx_hash[129] = {0};        // 64 hex chars + safety
  char fee_str[64]  = {0};

  parse_json_data(response, "result.tx_hash", tx_hash, sizeof(tx_hash));   // optional
  parse_json_data(response, "result.fee",     fee_str, sizeof(fee_str));   // optional

  // Log success
  if (fee_str[0]) {
    INFO_PRINT("[payout] %s -> %s amount=%" PRId64 " (atomic) fee=%s tx=%s reason=%s",
               "account_index=0", addr, amount_atomic, fee_str,
               tx_hash[0] ? tx_hash : "(unknown)",
               (reason && reason[0]) ? reason : "(n/a)");
  } else {
    INFO_PRINT("[payout] %s -> %s amount=%" PRId64 " (atomic) tx=%s reason=%s",
               "account_index=0", addr, amount_atomic,
               tx_hash[0] ? tx_hash : "(unknown)",
               (reason && reason[0]) ? reason : "(n/a)");
  }

  return XCASH_OK;
}