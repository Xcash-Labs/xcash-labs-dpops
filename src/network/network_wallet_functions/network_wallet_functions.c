#include "network_wallet_functions.h"

// Helper: count items in result.tx_hash_list quickly (no allocations, tolerant of whitespace).
// Returns: >=0 item count, or -1 on malformed/missing array.
static int count_txids_in_response(const char* json)
{
  if (!json) return -1;
  const char* p = strstr(json, "\"tx_hash_list\"");
  if (!p) return -1;

  // find '[' after key
  p = strchr(p, '[');
  if (!p) return -1;
  ++p;

  // skip whitespace
  while (*p==' '||*p=='\t'||*p=='\r'||*p=='\n') ++p;

  if (*p == ']') return 0; // empty array

  int count = 0;
  for (;;) {
    while (*p==' '||*p=='\t'||*p=='\r'||*p=='\n') ++p;
    if (*p != '\"') return -1; // expect string element
    ++p;

    // scan to closing quote (txids are hex strings; escapes not expected)
    while (*p && *p != '\"') ++p;
    if (*p != '\"') return -1;
    ++count; ++p;

    while (*p==' '||*p=='\t'||*p=='\r'||*p=='\n') ++p;
    if (*p == ',') { ++p; continue; }
    if (*p == ']') break;
    return -1; // malformed
  }
  return count;
}

// Helper, quiet JSON-RPC error precheck (prevents noisy "not found" logs from parser)
static int jsonrpc_has_error_top(const char* s)
{
  if (!s) return 0;
  const char* p = strstr(s, "\"error\"");
  if (!p) return 0;
  p += 7;
  while (*p==' '||*p=='\t'||*p=='\r'||*p=='\n') ++p;
  if (*p != ':') return 0;
  ++p;
  while (*p==' '||*p=='\t'||*p=='\r'||*p=='\n') ++p;
  return (*p=='{'); // top-level error object present
}

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
                          HTTP_TIMEOUT_SETTINGS) <= 0) 
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
  Sends a payout for a single destination using monero-wallet-rpc `transfer_split`, with the fee
  deducted from the destination amount (subtract_fee_from_outputs:[0]). Handles multiple txs.

Semantics (important):
  - first_tx_hash_out = first transaction hash
  - txids_out = ONLY sibling txids (2nd..N). No duplication of the first.
  - tx_count_out = number of sibling txids stored in txids_out
      * If only one tx is created, tx_count_out==0 and txids_out may be NULL.

Parameters:
  addr                  - Destination public address (Base58)
  amount_atomic         - Amount to send, in atomic units (requested amount before fee subtraction)
  reason                - Optional string for logging ("min_threshold" | "stale_7d" etc.)
  first_tx_hash_out     - (out) First tx hash (for quick reference; may be NULL)
  first_tx_hash_out_len - Size of first_tx_hash_out buffer
  fee_out               - (out) Total fee across all split transactions (may be NULL)
  created_at_ms_out     - (out) Milliseconds since epoch when request was made (may be NULL)
  amount_sent_out       - (out) Total amount delivered to destination (sum of all per-tx amounts) (may be NULL)
  txids_out             - (out) Array of buffers to receive sibling txids only; each buffer must be size (TRANSACTION_HASH_LENGTH+1).
                          May be NULL if you do not need siblings or expect only one tx.
  txids_out_cap         - Capacity (number of elements) in txids_out. Ignored if txids_out==NULL.
  tx_count_out          - (out) Count of sibling txids actually present (0..txids_out_cap, or 0 if only one tx)

Return:
  XCASH_OK (1) on success, XCASH_ERROR (0) on failure

Notes:
  - Uses account_index=0, priority=0, ring_size left to wallet default.
  - Aggregates result.tx_hash_list and result.fee_list.
  - Prefers per-destination net amounts when available (with subtract_fee_from_outputs),
    otherwise falls back to result.amount_list.
---------------------------------------------------------------------------------------------------------*/
int wallet_payout_send(const char* addr, int64_t amount_atomic, const char* reason,
                       char* first_tx_hash_out, size_t first_tx_hash_out_len,
                       uint64_t* fee_out, int64_t* created_at_ms_out, uint64_t* amount_sent_out,
                       char (*txids_out)[TRANSACTION_HASH_LENGTH + 1], size_t txids_out_cap, size_t* tx_count_out)
{
  if (!addr || addr[0] == '\0') {
    ERROR_PRINT("wallet_payout_send: invalid address");
    return XCASH_ERROR;
  }
  if (amount_atomic <= 0) {
    ERROR_PRINT("wallet_payout_send: non-positive amount %" PRId64, amount_atomic);
    return XCASH_ERROR;
  }

  static const char* HTTP_HEADERS[] = { "Content-Type: application/json", "Accept: application/json" };
  static const size_t HTTP_HEADERS_LENGTH = 2;

  // Build wallet-rpc request (single dest; fee from output; split enabled)
  char request[SMALL_BUFFER_SIZE] = {0};
  int n = snprintf(request, sizeof(request),
    "{"
      "\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"transfer_split\","
      "\"params\":{"
        "\"destinations\":[{\"amount\":%" PRId64 ",\"address\":\"%s\"}],"
        "\"account_index\":0,"
        "\"priority\":0,"
        "\"get_tx_keys\":false,"
        "\"subtract_fee_from_outputs\":[0]"
      "}"
    "}",
    amount_atomic, addr
  );
  if (n < 0 || (size_t)n >= sizeof(request)) {
    ERROR_PRINT("wallet_payout_send: request too large");
    return XCASH_ERROR;
  }

  // Send request
  char response[SMALL_BUFFER_SIZE] = {0};
  if (send_http_request(response, sizeof(response),
                        XCASH_WALLET_IP, "/json_rpc", XCASH_WALLET_PORT, "POST",
                        HTTP_HEADERS, HTTP_HEADERS_LENGTH,
                        request, SEND_PAYMENT_TIMEOUT_SETTINGS) != XCASH_OK) {
    ERROR_PRINT("wallet_payout_send: HTTP error");
    return XCASH_ERROR;
  }

  // Quiet JSON-RPC error detection
  if (jsonrpc_has_error_top(response)) {
    char err_code_buf[32] = {0};
    char err_msg_buf[256] = {0};
    if (parse_json_data(response, "error.code", err_code_buf, sizeof(err_code_buf)) == 0) {
      parse_json_data(response, "error.message", err_msg_buf, sizeof(err_msg_buf)); // best-effort
      ERROR_PRINT("wallet_payout_send: RPC error code=%s msg=%s",
                  err_code_buf, err_msg_buf[0] ? err_msg_buf : "(none)");
    } else {
      ERROR_PRINT("wallet_payout_send: RPC error (no code/message parsed)");
    }
    return XCASH_ERROR;
  }

  // Count txs once (fast, no logging)
  int tx_count = count_txids_in_response(response);
  if (tx_count <= 0) {
    ERROR_PRINT("wallet_payout_send: tx_hash_list empty or missing");
    return XCASH_ERROR;
  }

  const int MAX_SPLIT_TX = 128; // defensive parser cap
  int to_process = tx_count;
  bool too_many_txs = false;
  if (to_process > MAX_SPLIT_TX) {
    to_process = MAX_SPLIT_TX;
    too_many_txs = true;
  }

  uint64_t total_fee = 0;
  uint64_t total_sent_net = 0;

  // Timestamp
  int64_t created_at_ms = (int64_t)time(NULL) * 1000;
  if (created_at_ms_out) *created_at_ms_out = created_at_ms;

  char first_tx_hash[TRANSACTION_HASH_LENGTH + 1] = {0};
  size_t siblings_total = (tx_count > 0) ? (size_t)(tx_count - 1) : 0;
  size_t siblings_stored = 0;

  for (int i = 0; i < to_process; ++i) {
    // tx_hash_list[i]
    char txh[TRANSACTION_HASH_LENGTH + 1] = {0};
    char key_txhash[64]; snprintf(key_txhash, sizeof(key_txhash), "result.tx_hash_list[%d]", i);
    if (parse_json_data(response, key_txhash, txh, sizeof(txh)) != 0 || txh[0] == '\0') {
      ERROR_PRINT("wallet_payout_send: failed to read tx_hash_list[%d]", i);
      return XCASH_ERROR;
    }

    // fee_list[i]
    char fee_str[64] = {0};
    char key_fee[64]; snprintf(key_fee, sizeof(key_fee), "result.fee_list[%d]", i);
    if (parse_json_data(response, key_fee, fee_str, sizeof(fee_str)) != 0) {
      ERROR_PRINT("wallet_payout_send: missing result.fee_list[%d]", i);
      return XCASH_ERROR;
    }
    errno = 0; char* endp = NULL;
    unsigned long long fee_u = strtoull(fee_str, &endp, 10);
    if (errno || endp == fee_str || *endp != '\0') {
      ERROR_PRINT("wallet_payout_send: bad fee_list[%d] '%s'", i, fee_str);
      return XCASH_ERROR;
    }

    // amount_list[i] (net to destination for this tx)
    uint64_t sent_net_i = 0;
    char amt_str[64] = {0};
    char key_amt[64]; snprintf(key_amt, sizeof(key_amt), "result.amount_list[%d]", i);
    if (parse_json_data(response, key_amt, amt_str, sizeof(amt_str)) == 0) {
      errno = 0; endp = NULL;
      unsigned long long v = strtoull(amt_str, &endp, 10);
      if (!errno && endp && *endp == '\0') sent_net_i = (uint64_t)v;
    }

    // Collect ids
    if (i == 0) {
      snprintf(first_tx_hash, sizeof(first_tx_hash), "%s", txh); // first
    } else {
      if (txids_out && siblings_stored < txids_out_cap) {
        snprintf(txids_out[siblings_stored], TRANSACTION_HASH_LENGTH + 1, "%s", txh);
        ++siblings_stored;
      }
      // If no buffer or buffer full, we still track the total via siblings_total
    }

    total_fee += (uint64_t)fee_u;
    total_sent_net += sent_net_i;

    WARNING_PRINT("[payout/split #%d] acct=0 -> %s req=%" PRId64 " sent=%" PRIu64
                  " fee=%" PRIu64 " tx=%s reason=%s",
                  i, addr, amount_atomic, sent_net_i, (uint64_t)fee_u, txh,
                  (reason && reason[0]) ? reason : "(n/a)");
  }

  // Safety: if wallet produced more txs than we processed, report & fail after setting outs
  if (too_many_txs) {
    if (first_tx_hash_out && first_tx_hash_out_len)
      snprintf(first_tx_hash_out, first_tx_hash_out_len, "%s", first_tx_hash);
    if (fee_out) *fee_out = total_fee;
    if (amount_sent_out) *amount_sent_out = total_sent_net;
    if (tx_count_out) *tx_count_out = siblings_total; // report total siblings from wallet
    ERROR_PRINT("wallet_payout_send: too many split txs (wallet=%d, max_supported=%d)",
                tx_count, MAX_SPLIT_TX);
    return XCASH_ERROR;
  }

  // Capacity check: if caller's sibling buffer was too small, you can either fail or succeed truncated.
  // Here we choose to FAIL to force the caller to resize (keeps auditing exact).
  if (txids_out && siblings_stored < siblings_total) {
    if (first_tx_hash_out && first_tx_hash_out_len)
      snprintf(first_tx_hash_out, first_tx_hash_out_len, "%s", first_tx_hash);
    if (fee_out) *fee_out = total_fee;
    if (amount_sent_out) *amount_sent_out = total_sent_net;
    if (tx_count_out) *tx_count_out = siblings_total;
    ERROR_PRINT("wallet_payout_send: txids_out capacity too small (siblings_total=%zu, stored=%zu, cap=%zu)",
                siblings_total, siblings_stored, txids_out_cap);
    return XCASH_ERROR;
  }

  // Outs (success)
  if (first_tx_hash_out && first_tx_hash_out_len)
    snprintf(first_tx_hash_out, first_tx_hash_out_len, "%s", first_tx_hash);
  if (fee_out) *fee_out = total_fee;
  if (amount_sent_out) *amount_sent_out = total_sent_net;
  if (tx_count_out) *tx_count_out = siblings_total; // <-- total siblings (0 when only 1 tx)

  WARNING_PRINT("[payout/split] acct=0 -> %s req=%" PRId64 " total_sent=%" PRIu64
                " total_fee=%" PRIu64 " txs=%d (siblings=%zu, tx_count_out=%zu) first_tx=%s reason=%s",
                addr, amount_atomic, total_sent_net, total_fee,
                tx_count, siblings_total, (tx_count_out ? *tx_count_out : siblings_total),
                first_tx_hash[0] ? first_tx_hash : "(none)",
                (reason && reason[0]) ? reason : "(n/a)");

  return XCASH_OK;
}