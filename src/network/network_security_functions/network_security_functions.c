#include "network_security_functions.h"

// Helper function for error handling
void handle_error(const char* function_name, const char* message, char* buf1, char* buf2, char* buf3) {
  ERROR_PRINT("%s: %s", function_name, message);
  if (buf1) free(buf1);
  if (buf2) free(buf2);
  if (buf3) free(buf3);
  return;
}

/*---------------------------------------------------------------------------------------------------------
Name: sign_data
Description: Signs data with your XCA address, for sending data securely
Parameters:
  message - The sign_data
Return: 0 if an error has occured, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int sign_data(char* message) {
  const char* HTTP_HEADERS[] = {"Content-Type: application/json", "Accept: application/json"};
  const size_t HTTP_HEADERS_LENGTH = sizeof(HTTP_HEADERS) / sizeof(HTTP_HEADERS[0]);
  char* signature = calloc(XCASH_SIGN_DATA_LENGTH + 1, sizeof(char));
  char* payload = calloc(MEDIUM_BUFFER_SIZE, sizeof(char));
  char* request = calloc(MEDIUM_BUFFER_SIZE * 2, sizeof(char));
  char response[MEDIUM_BUFFER_SIZE] = {0};
  char random_data[RANDOM_STRING_LENGTH + 1] = {0};
  char cur_round_part[3] = {0};
  char trans_type[128] = {0};

  // Extract all required fields
  if (parse_json_data(message, "message_settings", trans_type, sizeof(trans_type)) != 1) {
    ERROR_PRINT("sign_data: Failed to parse the message_settings fields.");
    return XCASH_ERROR;
  }

  strcpy(cur_round_part, current_round_part);
  if (strcmp(trans_type, "SEED_TO_NODES_UPDATE_VOTE_COUNT") == 0 || strcmp(trans_type, "SEED_TO_NODES_PAYOUT") == 0) {
    snprintf(cur_round_part, sizeof cur_round_part, "70");
  }

  if (!signature || !payload || !request) {
    FATAL_ERROR_EXIT("sign_data: Memory allocation failed");
  }

  // Generate random data
  if (!random_string(random_data, RANDOM_STRING_LENGTH)) {
    handle_error("sign_data", "Failed to generate random data", signature, payload, request);
    return XCASH_ERROR;
  }

  // Step 1: Build the full JSON message to be signed
  snprintf(message + strlen(message) - 1, MEDIUM_BUFFER_SIZE - strlen(message),
           ",\"v_previous_block_hash\":\"%s\","
           "\"v_current_round_part\":\"%s\","
           "\"v_random_data\":\"%.*s\""
           "}",
           previous_block_hash,
           cur_round_part,
           RANDOM_STRING_LENGTH, random_data);

  // Step 2: Escape the message for the JSON-RPC request
  snprintf(payload, MEDIUM_BUFFER_SIZE, "%s", message);
  string_replace(payload, MEDIUM_BUFFER_SIZE, "\"", "\\\"");

  snprintf(request, MEDIUM_BUFFER_SIZE * 2,
           "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"sign\",\"params\":{\"data\":\"%s\"}}",
           payload);

  // Step 3: Send signing request to wallet
  if (send_http_request(response, MEDIUM_BUFFER_SIZE, XCASH_WALLET_IP, "/json_rpc", XCASH_WALLET_PORT,
                        "POST", HTTP_HEADERS, HTTP_HEADERS_LENGTH,
                        request, HTTP_TIMEOUT_SETTINGS) <= 0 ||
      !parse_json_data(response, "result.signature", signature, XCASH_SIGN_DATA_LENGTH + 1)) {
    handle_error("sign_data", "Wallet signature failed", signature, payload, request);
    return XCASH_ERROR;
  }

  if (strlen(signature) != XCASH_SIGN_DATA_LENGTH ||
      strncmp(signature, XCASH_SIGN_DATA_PREFIX, strlen(XCASH_SIGN_DATA_PREFIX)) != 0) {
    handle_error("sign_data", "Invalid wallet signature format", signature, payload, request);
    return XCASH_ERROR;
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
Name: sign_txt_string
Description:
  Signs a given text string using the X-Cash wallet RPC. This function sends the blob
  to the wallet daemon over JSON-RPC and retrieves the cryptographic signature.

Parameters:
  txt_string       - A null-terminated string containing the block blob in hex format.
  signature_out    - A buffer to store the resulting signature string.
  sig_out_len      - The size of the signature_out buffer (should be at least XCASH_SIGN_DATA_LENGTH + 1).

Returns:
  true  - If the signing process succeeds and the signature is extracted successfully.
  false - If the HTTP request fails, the response is invalid, or the signature cannot be parsed.
-----------------------------------------------------------------------------------------------------------*/
bool sign_txt_string(const char* txt_string, char* signature_out, size_t sig_out_len) {
  if (!txt_string || !signature_out || sig_out_len == 0) return false;

  // Compute required length first (snprintf with NULL just counts)
  int need = snprintf(NULL, 0,
                      "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"sign\",\"params\":{\"data\":\"%s\"}}",
                      txt_string);
  if (need < 0) {
    ERROR_PRINT("sign_block_blob: snprintf(size) failed while sizing JSON request");
    return false;
  }

  // Overflow/sanity guard (e.g., cap at 5 MiB)
  size_t req_len = (size_t)need + 1;
  if (req_len < (size_t)need || req_len > (BUFFER_SIZE)) {
    ERROR_PRINT("sign_block_blob: request size suspicious (%zu bytes)", req_len);
    return false;
  }

  char* request_json = (char*)malloc(req_len);
  if (!request_json) {
    ERROR_PRINT("sign_block_blob: allocating %zu bytes failed", req_len);
    return false;
  }

  int wrote = snprintf(request_json, req_len,
                       "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"sign\",\"params\":{\"data\":\"%s\"}}",
                       txt_string);
  if (wrote < 0 || (size_t)wrote >= req_len) {  // truncated or error
    free(request_json);
    return false;
  }

  const char* headers[] = {
      "Content-Type: application/json",
      "Accept: application/json"};

  char response[SMALL_BUFFER_SIZE];
  int http_ok = send_http_request(response, sizeof response,
                                  XCASH_WALLET_IP, "/json_rpc", XCASH_WALLET_PORT,
                                  "POST", headers, 2, request_json, HTTP_TIMEOUT_SETTINGS);

  free(request_json);

  if (http_ok <= 0) {
    ERROR_PRINT("sign_block_blob: HTTP request failed");
    return false;
  }

  if (!parse_json_data(response, "result.signature", signature_out, sig_out_len)) {
    ERROR_PRINT("sign_block_blob: signature not found/parsed");
    return false;
  }

  return true;
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
int verify_data(const char* message, xcash_msg_t msg_type) {
  const char* HTTP_HEADERS[] = {"Content-Type: application/json", "Accept: application/json"};
  const size_t HTTP_HEADERS_LENGTH = sizeof(HTTP_HEADERS) / sizeof(HTTP_HEADERS[0]);

  char data[SMALL_BUFFER_SIZE] = {0};
  char signature[XCASH_SIGN_DATA_LENGTH + 1] = {0};
  char ck_public_address[XCASH_WALLET_LENGTH + 1] = {0};
  char ck_round_part[3] = {0};
  char ck_previous_block_hash[BLOCK_HASH_LENGTH + 1] = {0};
  char raw_data[MEDIUM_BUFFER_SIZE] = {0};
  char request[MEDIUM_BUFFER_SIZE * 2] = {0};
  char response[MEDIUM_BUFFER_SIZE] = {0};
  char cur_round_part[3] = {0};

  strcpy(cur_round_part, current_round_part);
  if (msg_type == XMSG_SEED_TO_NODES_UPDATE_VOTE_COUNT || msg_type == XMSG_SEED_TO_NODES_PAYOUT) {
    snprintf(cur_round_part, sizeof cur_round_part, "70");
  }

  // Extract all required fields
  if (parse_json_data(message, "XCASH_DPOPS_signature", signature, sizeof(signature)) != 1 ||
      parse_json_data(message, "public_address", ck_public_address, sizeof(ck_public_address)) != 1 ||
      parse_json_data(message, "v_previous_block_hash", ck_previous_block_hash, sizeof(ck_previous_block_hash)) != 1 ||
      parse_json_data(message, "v_current_round_part", ck_round_part, sizeof(ck_round_part)) != 1) {
    ERROR_PRINT("verify_data: Failed to parse one or more required fields.");
    return XCASH_ERROR;
  }

  if (strcmp(cur_round_part, ck_round_part) != 0) {
    if (startup_complete) {
      WARNING_PRINT("Failed Signature Verification, round part timing issue: current round %s - message round %s.", cur_round_part, ck_round_part);
    }
    return XCASH_ERROR;
  }

  if (strcmp(previous_block_hash, ck_previous_block_hash) != 0) {
    ERROR_PRINT("Failed Signature Verification, previous block hash is not valid");
    return XCASH_ERROR;
  }

  snprintf(data, sizeof(data), "{\"public_address\":\"%s\"}", ck_public_address);
  if (count_documents_in_collection(DATABASE_NAME, DB_COLLECTION_DELEGATES, data) == 0) {
    DEBUG_PRINT("The delegates public address in this transaction does not exist");
    return XCASH_ERROR;
  }

  snprintf(raw_data, sizeof(raw_data), "%s", message);

  char* sig_pos = strstr(raw_data, ",\"XCASH_DPOPS_signature\"");
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
                        request, HTTP_TIMEOUT_SETTINGS) <= 0) {
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

// Helper function
static bool is_local_address(const char* ip) {
  if (!ip) return false;

  // Loopback quick checks
  if (strncmp(ip, "127.", 4) == 0) return true;  // 127.0.0.0/8
  if (strcmp(ip, "::1") == 0) return true;

  struct ifaddrs *ifaddr = NULL, *ifa = NULL;
  if (getifaddrs(&ifaddr) != 0) return false;

  bool match = false;
  for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
    if (!ifa->ifa_addr) continue;
    int family = ifa->ifa_addr->sa_family;
    if (family != AF_INET && family != AF_INET6) continue;

    char host[NI_MAXHOST];
    if (getnameinfo(ifa->ifa_addr,
                    (family == AF_INET) ? sizeof(struct sockaddr_in)
                                        : sizeof(struct sockaddr_in6),
                    host, sizeof(host), NULL, 0, NI_NUMERICHOST) == 0) {
      if (strcmp(host, ip) == 0) {
        match = true;
        break;
      }
    }
  }
  freeifaddrs(ifaddr);
  return match;
}

/*---------------------------------------------------------------------------------------------------------
 * @brief Verifies a signed action message using the wallet's verify endpoint.
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
int verify_action_data(const char* message, const char* client_ip, xcash_msg_t msg_type) {
  const char* HTTP_HEADERS[] = {"Content-Type: application/json", "Accept: application/json"};
  const size_t HTTP_HEADERS_LENGTH = sizeof(HTTP_HEADERS) / sizeof(HTTP_HEADERS[0]);

  char signature[XCASH_SIGN_DATA_LENGTH + 1] = {0};
  char ck_public_address[XCASH_WALLET_LENGTH + 1] = {0};
  char raw_data[MEDIUM_BUFFER_SIZE] = {0};
  char request[MEDIUM_BUFFER_SIZE * 2] = {0};
  char response[MEDIUM_BUFFER_SIZE] = {0};

  // Extract all required fields
  if (parse_json_data(message, "signature", signature, sizeof(signature)) != 1 ||
      parse_json_data(message, "public_address", ck_public_address, sizeof(ck_public_address)) != 1) {
    ERROR_PRINT("verify_data: Failed to parse one or more required fields.");
    return XCASH_ERROR;
  }

  // allow local: loopback or any interface on this host, can't check sign due to wallet process being down for delegate registration
  if ((msg_type == XMSG_NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE ||
       msg_type == XMSG_NODES_TO_BLOCK_VERIFIERS_UPDATE_DELEGATE) &&
      is_local_address(client_ip)) {
    DEBUG_PRINT("Internal loopback connection ok from: %s", client_ip);
    return XCASH_OK;
  }

  snprintf(raw_data, sizeof(raw_data), "%s", message);

  char* sig_pos = strstr(raw_data, ",\"signature\"");
  if (sig_pos) {
    *sig_pos = '}';         // Replace start of signature field with closing brace
    *(sig_pos + 1) = '\0';  // Null-terminate the string
  } else {
    ERROR_PRINT("Signature field not found in action message.");
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
                        request, HTTP_TIMEOUT_SETTINGS) <= 0) {
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

// helper: sockaddr -> numeric string
static int sockaddr_to_numhost(const struct sockaddr* sa, char* out, size_t outsz) {
  if (!sa || !out || outsz == 0) return 0;
  int rc = getnameinfo(sa,
                       (sa->sa_family == AF_INET) ? sizeof(struct sockaddr_in)
                                                  : sizeof(struct sockaddr_in6),
                       out, outsz, NULL, 0, NI_NUMERICHOST);
  return rc == 0;
}

/*---------------------------------------------------------------------------------------------------------
 * Name: verify_ip
 * Description:
 *   Verifies that the IP address of the message sender matches the registered IP or resolved hostname
 *   of a known delegate from the current verifier list.
 *
 * Parameters:
 *   message - The complete message (in JSON or delimited key-value format).
 *   client_ip - The IP address from which the message was received.
 *
 * Return:
 *   XCASH_OK (1) if the IP matches a known delegate and (optionally) round part is valid.
 *   XCASH_ERROR (0) if the delegate is unknown, the IP does not match, or data is invalid.
---------------------------------------------------------------------------------------------------------*/
int verify_the_ip(const char* message, const char* client_ip, bool seed_only) {
  if (!message || !client_ip || client_ip[0] == '\0') {
    ERROR_PRINT("verify_ip: Null or empty client_ip passed");
    return XCASH_ERROR;
  }

  char ck_public_address[XCASH_WALLET_LENGTH + 1] = {0};
  char ip_address_trans[IP_LENGTH + 1] = {0};
  char filter_json[256] = {0};
  char resolved_ip[INET_ADDRSTRLEN] = {0};   // v4 only, as before
  char client_canon[INET_ADDRSTRLEN] = {0};  // v4 only, as before

  // allow local: loopback or any interface on this host
  if (is_local_address(client_ip)) {
    DEBUG_PRINT("Internal loopback connection ok from: %s", client_ip);
    return XCASH_OK;
  }

  // Extract the public address
  if (parse_json_data(message, "public_address", ck_public_address, sizeof(ck_public_address)) != XCASH_OK) {
    ERROR_PRINT("verify_ip: Failed to parse public_address field");
    return XCASH_ERROR;
  }

  if (seed_only) {
    if (!is_seed_address(ck_public_address)) {
      ERROR_PRINT("verify_ip: The public_address for this ip must be sent from a seed delegate");
      return XCASH_ERROR;
    }
  }

  // Get the IP/hostname from DB
  snprintf(filter_json, sizeof(filter_json), "{ \"public_address\": \"%s\" }", ck_public_address);
  if (read_document_field_from_collection(DATABASE_NAME, DB_COLLECTION_DELEGATES,
                                          filter_json, "IP_address",
                                          ip_address_trans, sizeof(ip_address_trans)) != XCASH_OK) {
    ERROR_PRINT("Delegate '%s' not found in DB or missing IP_address", ck_public_address);
    return XCASH_ERROR;
  }
  ip_address_trans[sizeof(ip_address_trans) - 1] = '\0';

  // Canonicalize client IPv4 (keeps your v4-only contract)
  {
    struct addrinfo hints = {0}, *cres = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(client_ip, NULL, &hints, &cres) == 0 && cres) {
      if (!sockaddr_to_numhost(cres->ai_addr, client_canon, sizeof(client_canon))) {
        snprintf(client_canon, sizeof(client_canon), "%s", client_ip);
      }
      freeaddrinfo(cres);
    } else {
      snprintf(client_canon, sizeof(client_canon), "%s", client_ip);
    }
  }

  // Resolve the expected hostname/IP (IPv4 only, as before)
  struct addrinfo hints = {0}, *res = NULL;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  int gai_ret = getaddrinfo(ip_address_trans, NULL, &hints, &res);
  bool match = false;

  if (gai_ret == 0 && res != NULL) {
    // check ALL results, not just the first
    for (struct addrinfo* p = res; p; p = p->ai_next) {
      if (!p->ai_addr || p->ai_addr->sa_family != AF_INET) continue;
      if (!sockaddr_to_numhost(p->ai_addr, resolved_ip, sizeof(resolved_ip))) continue;
      if (strcmp(resolved_ip, client_canon) == 0) {
        match = true;
        break;
      }
    }
    freeaddrinfo(res);
  } else {
    WARNING_PRINT("getaddrinfo failed for '%s': %s", ip_address_trans, gai_strerror(gai_ret));
    // fallback: treat DB value as a literal IPv4 and compare
    snprintf(resolved_ip, sizeof(resolved_ip), "%s", ip_address_trans);
    match = (strcmp(resolved_ip, client_canon) == 0);
  }

  if (!match) {
    ERROR_PRINT("IP verification failed: Delegate '%s' expects '%s' (resolved/any: %s), got: %s",
                ck_public_address, ip_address_trans, resolved_ip, client_canon);
    return XCASH_ERROR;
  }

  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
 * Name: wallet_verify_signature
 *
 * Description:
 *   Calls the local XCASH wallet JSON-RPC `verify` to check that `in_signature` is valid
 *   for (`sign_str`, `in_public_address`). Returns XCASH_OK on valid, XCASH_ERROR otherwise.
 *
 * Parameters:
 *   sign_str          - Canonical string that was signed (exact bytes).
 *   in_public_address - XCA public address of the claimed signer.
 *   in_signature      - Signature text to verify.
 *
 * Return:
 *   XCASH_OK    -> wallet says signature is valid
 *   XCASH_ERROR -> HTTP/parse error or wallet says invalid
 *
 * Notes:
 *   - If sign_str may contain quotes/backslashes/newlines, JSON-escape it before calling.
 *   - Uses XCASH_WALLET_IP / XCASH_WALLET_PORT / HTTP_TIMEOUT_SETTINGS.
 *---------------------------------------------------------------------------------------------------------*/
int wallet_verify_signature(const char *sign_str, const char *in_public_address, const char *in_signature)
{
  if (!sign_str || !in_public_address || !in_signature) {
    return XCASH_ERROR;
  }

  static const char *HTTP_HEADERS[] = {
    "Content-Type: application/json",
    "Accept: application/json"
  };
  static const size_t HTTP_HEADERS_LENGTH =
      sizeof(HTTP_HEADERS) / sizeof(HTTP_HEADERS[0]);

  char request[MEDIUM_BUFFER_SIZE * 2] = {0};
  char response[MEDIUM_BUFFER_SIZE] = {0};

  // NOTE: JSON-escape sign_str/address/signature if they can contain special chars.
  int nw = snprintf(request, sizeof(request),
      "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"verify\",\"params\":{"
        "\"data\":\"%s\","
        "\"address\":\"%s\","
        "\"signature\":\"%s\"}}",
      sign_str, in_public_address, in_signature);
  if (nw < 0 || (size_t)nw >= sizeof(request)) {
    return XCASH_ERROR;
  }

  int sent = send_http_request(response, sizeof(response),
                               XCASH_WALLET_IP, "/json_rpc", XCASH_WALLET_PORT,
                               "POST", HTTP_HEADERS, HTTP_HEADERS_LENGTH,
                               request, HTTP_TIMEOUT_SETTINGS);
  if (sent <= 0) {
    return XCASH_ERROR;
  }

  char result[8] = {0};
  int parsed_ok = parse_json_data(response, "result.good", result, sizeof(result));
  if (parsed_ok != XCASH_OK) {
    return XCASH_ERROR;
  }

  return (strcmp(result, "true") == 0) ? XCASH_OK : XCASH_ERROR;
}

/*---------------------------------------------------------------------------------------------------------
  dnssec_helper.c — DNSSEC helper for X-Cash

  Public API (see prototypes below):
    dnssec_ctx_t*   dnssec_init(void);
    void            dnssec_destroy(dnssec_ctx_t*);
    dnssec_status_t dnssec_query(dnssec_ctx_t*, const char* name, int rrtype, bool* out_havedata);
    bool            dnssec_rr_secure(const char* name, int rrtype);


dnssec_ctx_t*  dnssec_init(void);
void           dnssec_destroy(dnssec_ctx_t* h);
dnssec_status_t dnssec_query(dnssec_ctx_t* h, const char* name, int rrtype, bool* out_havedata);
bool           dnssec_rr_secure(const char* name, int rrtype);

*---------------------------------------------------------------------------------------------------------*/
/* ICANN KSK DS anchors (2010:19036 and 2017:20326), same as Monero ships.    */
static const char * const* get_builtin_ds(void) {
  static const char * const ds[] = {
    ". IN DS 19036 8 2 49AAC11D7B6F6446702E54A1607371607A1A41855200FD2CE1CDDE32F24E8FB5\n",
    ". IN DS 20326 8 2 E06D44B80B8F1D39A95C0B0D7C65D08458E880409BBC683457104237C7F8EC8D\n",
    NULL
  };
  return ds;
}

static int dnssec_add_trust_anchor(struct ub_ctx* ctx) {
  int added = 0;

  /* 1) Built-in DS anchors */
  const char * const *ds = get_builtin_ds();
  for (; *ds; ++ds) {
    if (ub_ctx_add_ta(ctx, *ds) == 0) ++added;
  }

  /* 2) Optional env/file anchors */
  const char* env = getenv("DNS_TRUST_ANCHOR");
  const char* candidates[] = {
    (env && *env) ? env : NULL,
    "/var/lib/unbound/root.key",
    "/etc/unbound/root.key",
    "/usr/local/etc/unbound/root.key",
    NULL
  };
  int i;
  for (i = 0; candidates[i]; ++i) {
    if (candidates[i] && ub_ctx_add_ta_file(ctx, candidates[i]) == 0) ++added;
  }

  return (added > 0) ? 0 : -1;
}

/* Support DNS_PUBLIC=tcp://9.9.9.9 (or udp://1.1.1.1, or plain IP). */
static void dnssec_maybe_set_public_forwarder(struct ub_ctx* ctx) {
  const char* dns_pub = getenv("DNS_PUBLIC");
  if (!dns_pub || !*dns_pub) return;

  const char* s = dns_pub;
  const char* addr = s;
  int use_tcp = 0;
  if      (strncmp(s, "tcp://", 6) == 0) { use_tcp = 1; addr = s + 6; }
  else if (strncmp(s, "udp://", 6) == 0) { use_tcp = 0; addr = s + 6; }

  (void)ub_ctx_set_fwd(ctx, addr);
  if (use_tcp) (void)ub_ctx_set_option(ctx, "do-tcp:", "yes");
}

dnssec_ctx_t* dnssec_init(void) {
  dnssec_ctx_t* h = (dnssec_ctx_t*)calloc(1, sizeof(*h));
  if (!h) return NULL;

  h->ctx = ub_ctx_create();
  if (!h->ctx) { free(h); return NULL; }

  /* Sensible defaults */
  (void)ub_ctx_set_option(h->ctx, "val-permissive-mode:", "no");
  (void)ub_ctx_set_option(h->ctx, "qname-minimisation:",  "yes");
  (void)ub_ctx_set_option(h->ctx, "infra-cache-numhosts:", "2000");
  (void)ub_ctx_set_option(h->ctx, "infra-keep-probing:",   "yes");
  (void)ub_ctx_set_option(h->ctx, "harden-below-nxdomain:","yes");
  (void)ub_ctx_set_option(h->ctx, "do-ip4:",               "yes");
  (void)ub_ctx_set_option(h->ctx, "do-ip6:",               "no");
  (void)ub_ctx_set_option(h->ctx, "timeout:",              "3000");

  /* Use system resolvers unless DNS_PUBLIC overrides */
  (void)ub_ctx_resolvconf(h->ctx, NULL);
  dnssec_maybe_set_public_forwarder(h->ctx);

  /* Add anchors (built-in DS first, then optional files) */
  if (dnssec_add_trust_anchor(h->ctx) != 0) {
    (void)ub_ctx_delete(h->ctx);
    free(h);
    return NULL;
  }

  return h;
}

void dnssec_destroy(dnssec_ctx_t* h) {
  if (!h) return;
  if (h->ctx) (void)ub_ctx_delete(h->ctx);
  free(h);
}

dnssec_status_t dnssec_query(dnssec_ctx_t* h, const char* name, int rrtype, bool* out_havedata) {
  if (out_havedata) *out_havedata = false;
  if (!h || !h->ctx || !name || !*name) return DNSSEC_ERR;

  struct ub_result* res = NULL;
  if (ub_resolve(h->ctx, name, rrtype, 1, &res) != 0 || !res) {
    return DNSSEC_ERR;
  }

  dnssec_status_t status;
  if (res->bogus) {
    DEBUG_PRINT("DNSSEC BOGUS for %s (rr=%d): %s", name, rrtype,
                res->why_bogus ? res->why_bogus : "(no reason)");
    status = DNSSEC_BOGUS;
  } else if (res->secure) {
    status = DNSSEC_SECURE;
  } else {
    status = DNSSEC_UNSIGNED;
  }

  if (out_havedata) *out_havedata = (res->havedata != 0);

  (void)ub_resolve_free(res);
  return status;
}

static bool txt_rdata_to_string(const unsigned char* rdata, size_t len, char** out_str) {
  if (!rdata || !len || !out_str) return false;
  char* s = (char*)malloc(len + 1);
  if (!s) return false;
  size_t i = 0, pos = 0;
  while (i < len) {
    unsigned int n = rdata[i++];
    if (i + n > len) { free(s); return false; }
    memcpy(s + pos, rdata + i, n);
    pos += n; i += n;
  }
  s[pos] = '\0';
  *out_str = s;
  return true;
}

// Collect all validated updpops entries from a host
size_t dnssec_get_all_updpops(dnssec_ctx_t* dctx, const char* host,
                              updpops_entry_t* out, size_t cap) {
  if (!dctx || !dctx->ctx || !host || !out || cap == 0) return 0;

  struct ub_result* res = NULL;
  if (ub_resolve(dctx->ctx, host, RR_TXT, RR_IN, &res) != 0 || !res) return 0;

  size_t n = 0;
  if (res->secure && !res->bogus && res->havedata && res->data) {
    for (size_t i = 0; res->data[i] && n < cap; ++i) {
      char* str = NULL;
      if (txt_rdata_to_string((const unsigned char*)res->data[i],
                              (size_t)res->len[i], &str)) {
        updpops_entry_t e;
        if (parse_updpops_entry(str, &e)) out[n++] = e;
        free(str);
      }
    }
  }

  ub_resolve_free(res);
  return n;
}

bool digest_allowed(const char* self_hex, const updpops_entry_t* list, size_t n, const updpops_entry_t** matched) {
  for (size_t i = 0; i < n; ++i) {
    if (strcmp(self_hex, list[i].digest) == 0) {
      if (matched) *matched = &list[i];
      return true;
    }
  }
  return false;
}