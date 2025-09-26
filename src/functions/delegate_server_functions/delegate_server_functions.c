#include "delegate_server_functions.h"

/*---------------------------------------------------------------------------------------------------------
Name: check_for_valid_delegate_name
Description: Checks for a valid delegate name
Parameters:
  DELEGATE_NAME - The delegate name
Return: 0 if the delegate name is not valid, 1 if the delegate name is valid
---------------------------------------------------------------------------------------------------------*/
int check_for_valid_delegate_name(const char *DELEGATE_NAME) {
#define VALID_DATA "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-"

  size_t length = strlen(DELEGATE_NAME);

  // Check name length bounds
  if (length > MAXIMUM_BUFFER_SIZE_DELEGATES_NAME ||
      length < MINIMUM_BUFFER_SIZE_DELEGATES_NAME) {
    WARNING_PRINT("Attempt to register a delegate whose name is either too short or too long");
    return XCASH_ERROR;
  }

  // Validate all characters
  for (size_t i = 0; i < length; i++) {
    if (strchr(VALID_DATA, DELEGATE_NAME[i]) == NULL) {
      return XCASH_ERROR;
    }
  }

  return XCASH_OK;
#undef VALID_DATA
}

/*---------------------------------------------------------------------------------------------------------
Name: check_for_valid_delegate_fee
Description: Checks for a valid delegate fee
Parameters:
  DELEGATE_NAME - The delegate name
Return: 0 if the delegate fee is not valid, 1 if the delegate fee is valid
---------------------------------------------------------------------------------------------------------*/
int check_for_valid_delegate_fee(const char *MESSAGE) {
  const char *p, *q;
  char *endptr;
  int dot_seen = 0;
  int decimals = 0;
  double number;

  if (MESSAGE == NULL || *MESSAGE == '\0') {
    return XCASH_ERROR;
  }

  p = MESSAGE;

  /* 1) Format checks: only digits and at most one '.', not starting with '.' */
  if (*p == '.') {
    return XCASH_ERROR; /* disallow leading '.' */
  }

  for (q = p; *q; ++q) {
    if (*q == '.') {
      if (dot_seen) return 0; /* second dot -> invalid */
      dot_seen = 1;
      continue;
    }
    if (*q < '0' || *q > '9') {
      return XCASH_ERROR; /* any non-digit/non-dot char -> invalid (no spaces/signs/exponent) */
    }
    if (dot_seen) {
      ++decimals;
      if (decimals > 6) return 0; /* too many fractional digits */
    }
  }

  /* 2) Numeric parse (guaranteed to be plain decimal now) */
  errno = 0;
  number = strtod(p, &endptr);
  if (errno != 0 || endptr == p || *endptr != '\0' || !isfinite(number)) {
    return XCASH_ERROR;
  }

  /* 3) Range check: 0..100 inclusive */
  if (number < 0.0 || number > 100.0) {
    return XCASH_ERROR;
  }

  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name:        check_for_valid_ip_or_hostname
Description: Quick sanity check that HOST is either:
               - a numeric IPv4/IPv6 literal, or
               - a hostname that resolves via DNS (A/AAAA).
             No public/private filtering is performed here.
Parameters:
  host  - C string: IPv4/IPv6 literal or DNS hostname
Return:
  XCASH_OK    (1) if getaddrinfo() resolves to at least one address
  XCASH_ERROR (0) if null/empty or resolution fails
Notes:
  - Uses getaddrinfo(AF_UNSPEC) to support IPv4 and IPv6.
  - This performs DNS resolution and may block; call off the hot path.
  - Security enforcement (e.g., public-routable requirement) should be done on the server/seed.
---------------------------------------------------------------------------------------------------------*/
int check_for_valid_ip_or_hostname(const char *host) {
  if (!host || !*host) return XCASH_ERROR;
  struct addrinfo hints = {0}, *res = NULL;
  hints.ai_family = AF_UNSPEC;  // v4 or v6
  int rc = getaddrinfo(host, NULL, &hints, &res);
  if (rc != 0 || !res) return XCASH_ERROR;
  freeaddrinfo(res);
  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: server_receive_data_socket_nodes_to_block_verifiers_register_delegates
Description: Runs the code when the server receives the NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE message
Parameters:
  CLIENT_SOCKET - The socket to send data to
  MESSAGE - The message
---------------------------------------------------------------------------------------------------------*/
void server_receive_data_socket_nodes_to_block_verifiers_register_delegates(server_client_t *client, const char *MESSAGE) {
  char data[SMALL_BUFFER_SIZE] = {0};
  char delegate_name[MAXIMUM_BUFFER_SIZE_DELEGATES_NAME] = {0};
  char delegate_public_address[XCASH_WALLET_LENGTH + 1] = {0};
  char delegate_public_key[VRF_PUBLIC_KEY_LENGTH + 1] = {0};
  unsigned char delegate_public_key_data[crypto_vrf_PUBLICKEYBYTES + 1] = {0};
  char delegates_IP_address[BLOCK_VERIFIERS_IP_ADDRESS_TOTAL_LENGTH + 1] = {0};
  uint64_t registration_time = 0;

  // 1) Parse incoming MESSAGE as JSON
  cJSON *root = cJSON_Parse(MESSAGE);
  if (!root) {
    SERVER_ERROR("0|Could not verify the message");
  }

  // 2) Extract and validate each required field
  cJSON *msg_settings = cJSON_GetObjectItemCaseSensitive(root, "message_settings");
  cJSON *js_name = cJSON_GetObjectItemCaseSensitive(root, "delegate_name");
  cJSON *js_ip = cJSON_GetObjectItemCaseSensitive(root, "delegate_IP");
  cJSON *js_pubkey = cJSON_GetObjectItemCaseSensitive(root, "delegate_public_key");
  cJSON *js_address = cJSON_GetObjectItemCaseSensitive(root, "public_address");
  cJSON *js_reg_time = cJSON_GetObjectItemCaseSensitive(root, "registration_timestamp");

  if (!cJSON_IsString(msg_settings) || (msg_settings->valuestring == NULL) ||
      !cJSON_IsString(js_name) || (js_name->valuestring == NULL) ||
      !cJSON_IsString(js_ip) || (js_ip->valuestring == NULL) ||
      !cJSON_IsString(js_pubkey) || (js_pubkey->valuestring == NULL) ||
      !cJSON_IsString(js_address) || (js_address->valuestring == NULL) ||
      !cJSON_IsNumber(js_reg_time)) {
    cJSON_Delete(root);
    SERVER_ERROR("0|Could not verify the message");
  }

  // 2a) Ensure message_settings matches exactly
  if (strcmp(msg_settings->valuestring, "NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE") != 0) {
    cJSON_Delete(root);
    SERVER_ERROR("0|Invalid message_settings");
  }

  // 2b) Copy them into our local buffers (including null terminators)
  size_t name_len = strlen(js_name->valuestring);
  size_t ip_len = strlen(js_ip->valuestring);
  size_t pubkey_len = strlen(js_pubkey->valuestring);
  size_t address_len = strlen(js_address->valuestring);

  if (name_len == 0 || name_len >= sizeof(delegate_name) ||
      ip_len == 0 || ip_len >= sizeof(delegates_IP_address) ||
      pubkey_len != VRF_PUBLIC_KEY_LENGTH ||
      address_len != XCASH_WALLET_LENGTH) {
    cJSON_Delete(root);
    SERVER_ERROR("0|Invalid length for delegate name, delegate ip, public key, or public wallet address");
  }

  memcpy(delegate_name, js_name->valuestring, name_len);
  memcpy(delegates_IP_address, js_ip->valuestring, ip_len);
  memcpy(delegate_public_key, js_pubkey->valuestring, pubkey_len);
  memcpy(delegate_public_address, js_address->valuestring, address_len);
  registration_time = (uint64_t)js_reg_time->valuedouble;

  // 3) Convert hex string → raw bytes for VRF public key
  //    (each two hex chars → one byte)
  for (int i = 0, j = 0; i < (int)pubkey_len; i += 2, j++) {
    char byte_hex[3] = {delegate_public_key[i], delegate_public_key[i + 1], 0};
    delegate_public_key_data[j] = (unsigned char)strtol(byte_hex, NULL, 16);
  }
  delegate_public_key_data[crypto_vrf_PUBLICKEYBYTES] = 0;  // just in case

  // 4) Validate ranges and formats
  if (check_for_valid_delegate_name(delegate_name) == 0) {
    cJSON_Delete(root);
    SERVER_ERROR("0|Invalid delegate_name");
  }
  if (strlen(delegate_public_address) != XCASH_WALLET_LENGTH) {
    cJSON_Delete(root);
    SERVER_ERROR("0|Invalid public_address length");
  }
  if (strncmp(delegate_public_address, XCASH_WALLET_PREFIX,
              sizeof(XCASH_WALLET_PREFIX) - 1) != 0) {
    cJSON_Delete(root);
    SERVER_ERROR("0|Invalid public_address prefix");
  }
  if (check_for_valid_ip_or_hostname(delegates_IP_address) == XCASH_ERROR) {
    cJSON_Delete(root);
    SERVER_ERROR("0|Invalid delegate_IP (must be IP or resolvable hostname)");
  }
  if (crypto_vrf_is_valid_key(delegate_public_key_data) != 1) {
    cJSON_Delete(root);
    SERVER_ERROR("0|Invalid delegate_public_key");
  }

  cJSON_Delete(root);  // we no longer need the JSON tree

  // 5) Check uniqueness in database
  // 5a) public_address
  snprintf(data, sizeof(data), "{\"public_address\":\"%s\"}", delegate_public_address);
  if (count_documents_in_collection(DATABASE_NAME, DB_COLLECTION_DELEGATES, data) != 0) {
    if (is_seed_node) {
      // Seed node db uses replication so it alreay exists it has already been added
      send_data(client, (unsigned char *)"1|Registered the delegate}", strlen("1|Registered the delegate}"));
      return;
    } else {
      SERVER_ERROR("0|The delegates public address is already registered");
    }
  }

  // 5b) IP_address
  snprintf(data, sizeof(data), "{\"IP_address\":\"%s\"}", delegates_IP_address);
  if (count_documents_in_collection(DATABASE_NAME, DB_COLLECTION_DELEGATES, data) != 0) {
    SERVER_ERROR("0|The delegates IP address is already registered");
  }

  // 5c) public_key
  snprintf(data, sizeof(data), "{\"public_key\":\"%s\"}", delegate_public_key);
  if (count_documents_in_collection(DATABASE_NAME, DB_COLLECTION_DELEGATES, data) != 0) {
    SERVER_ERROR("0|The delegates public key is already registered");
  }

  // 5d) delegate_name
  snprintf(data, sizeof(data), "{\"delegate_name\":\"%s\"}", delegate_name);
  if (count_documents_in_collection(DATABASE_NAME, DB_COLLECTION_DELEGATES, data) != 0) {
    SERVER_ERROR("0|The delegates name is already registered");
  }

  // 6) Check overall delegate count
  int delegate_count = count_documents_in_collection(DATABASE_NAME, DB_COLLECTION_DELEGATES, "{}");
  if (delegate_count >= BLOCK_VERIFIERS_TOTAL_AMOUNT) {
    SERVER_ERROR("0|The maximum amount of delegates has been reached");
  }

  // 7) Finally insert a new document
  double set_delegate_fee = 0.00;
  uint64_t set_counts = 0;

  bson_t bson;
  bson_init(&bson);

  // Strings
  bson_append_utf8(&bson, "public_address", -1, delegate_public_address, -1);
  bson_append_utf8(&bson, "IP_address", -1, delegates_IP_address, -1);
  bson_append_utf8(&bson, "delegate_name", -1, delegate_name, -1);
  bson_append_utf8(&bson, "about", -1, "", -1);
  bson_append_utf8(&bson, "website", -1, "", -1);
  bson_append_utf8(&bson, "team", -1, "", -1);
  bson_append_utf8(&bson, "delegate_type", -1, "shared", -1);
  bson_append_utf8(&bson, "server_specs", -1, "", -1);
  bson_append_utf8(&bson, "online_status", -1, "false", -1);
  bson_append_utf8(&bson, "public_key", -1, delegate_public_key, -1);

  // Numbers
  bson_append_int64(&bson, "total_vote_count", -1, set_counts);
  bson_append_double(&bson, "delegate_fee", -1, set_delegate_fee);
  int64_t ms = (int64_t)registration_time * 1000;
  bson_append_date_time(&bson, "registration_timestamp", -1, ms);

  if (insert_document_into_collection_bson(DATABASE_NAME, DB_COLLECTION_DELEGATES, &bson) != XCASH_OK) {
    bson_destroy(&bson);
    SERVER_ERROR("0|Failed to insert the delegate document");
  }

  bson_destroy(&bson);

// Only update statics on seed nodes
#ifdef SEED_NODE_ON

  bson_t bson_statistics;
  bson_init(&bson_statistics);

  // Strings
  BSON_APPEND_UTF8(&bson_statistics, "_id", delegate_public_key);

  // Numbers
  bson_append_int64(&bson_statistics, "block_verifier_total_rounds", -1, set_counts);
  bson_append_int64(&bson_statistics, "block_verifier_online_total_rounds", -1, set_counts);
  bson_append_int64(&bson_statistics, "block_producer_total_rounds", -1, set_counts);

  // Guard watermark for exactly-once counting:
  bson_append_int64(&bson_statistics, "last_counted_block", -1, (int64_t)-1);

  // Insert into "statistics" collection
  if (insert_document_into_collection_bson(DATABASE_NAME, DB_COLLECTION_STATISTICS, &bson_statistics) != XCASH_OK) {
    bson_destroy(&bson_statistics);
    SERVER_ERROR("0|Failed to insert the statistics document");
  }

  bson_destroy(&bson_statistics);

#endif

  // 8) Success: reply back to the client
  send_data(client, (unsigned char *)"1|Registered the delegate", strlen("1|Registered the delegate"));
  return;
}

/*---------------------------------------------------------------------------------------------------------
Name: server_receive_data_socket_nodes_to_block_verifiers_validate_block
Description: Runs the code when the server receives the XCASHD_TO_DPOPS_VERIFY message.
             This function verifies the VRF proof and beta from a proposed block by reconstructing
             the alpha input (prev_block_hash || height || pubkey) and validating the cryptographic proof.
Parameters:
  CLIENT_SOCKET - The socket to send data to
  MESSAGE - The JSON message containing the VRF proof, beta, public key, block height, and previous hash
---------------------------------------------------------------------------------------------------------*/
void server_receive_data_socket_nodes_to_block_verifiers_validate_block(server_client_t *client, const char *MESSAGE) {
  char response[VSMALL_BUFFER_SIZE] = {0};

  // early at the top, before parsing JSON
  if (strcmp(client->client_ip, "127.0.0.1") != 0 && strcmp(client->client_ip, "::1") != 0) {
    send_data(client, (unsigned char *)"0|FORBIDDEN_NON_LOCAL", strlen("0|FORBIDDEN_NON_LOCAL"));
    INFO_PRINT("Non local");
    return;
  }

  // Parse the incoming JSON message
  cJSON *root = cJSON_Parse(MESSAGE);
  if (!root) {
    send_data(client, (unsigned char *)"0|INVALID_JSON", strlen("0|INVALID_JSON"));
    INFO_PRINT("Invalid json");
    return;
  }

  // Extract fields
  cJSON *msg_settings = cJSON_GetObjectItemCaseSensitive(root, "message_settings");
  cJSON *js_vrf_proof = cJSON_GetObjectItemCaseSensitive(root, "vrf_proof");
  cJSON *js_vrf_beta = cJSON_GetObjectItemCaseSensitive(root, "vrf_beta");
  cJSON *js_vrf_pubkey = cJSON_GetObjectItemCaseSensitive(root, "vrf_pubkey");
  cJSON *js_vote_hash = cJSON_GetObjectItemCaseSensitive(root, "vote_hash");
  cJSON *js_height = cJSON_GetObjectItemCaseSensitive(root, "height");
  cJSON *js_prev_hash = cJSON_GetObjectItemCaseSensitive(root, "prev_block_hash");

  if (!cJSON_IsString(msg_settings) || strcmp(msg_settings->valuestring, "XCASHD_TO_DPOPS_VERIFY") != 0 ||
      !cJSON_IsString(js_vrf_proof) || !cJSON_IsString(js_vrf_beta) || !cJSON_IsString(js_vrf_pubkey) ||
      !cJSON_IsString(js_vote_hash) || !cJSON_IsNumber(js_height) || !cJSON_IsString(js_prev_hash)) {
    cJSON_Delete(root);
    send_data(client, (unsigned char *)"0|BAD_FIELDS", strlen("0|BAD_FIELDS"));
    INFO_PRINT("Bad field fields");
    return;
  }

  // Extract strings and height
  const char *vrf_proof_str = js_vrf_proof->valuestring;
  const char *vrf_beta_str = js_vrf_beta->valuestring;
  const char *vrf_pubkey_str = js_vrf_pubkey->valuestring;
  const char *vote_hash_str = js_vote_hash->valuestring;
  const char *prev_hash_str = js_prev_hash->valuestring;
  uint64_t height = (uint64_t)js_height->valuedouble;

  if (!is_hex_len(vrf_proof_str, VRF_PROOF_LENGTH) ||
      !is_hex_len(vrf_beta_str, VRF_BETA_LENGTH) ||
      !is_hex_len(vrf_pubkey_str, VRF_PUBLIC_KEY_LENGTH) ||
      !is_hex_len(vote_hash_str, VOTE_HASH_LEN) ||
      !is_hex_len(prev_hash_str, BLOCK_HASH_LENGTH)) {
    cJSON_Delete(root);
    send_data(client, (unsigned char *)"0|BAD_FIELD_LEN_OR_NONHEX", strlen("0|BAD_FIELD_LEN_OR_NONHEX"));
    INFO_PRINT("Bad field lenght");
    return;
  }

  // If block_height being passed in is equal to the node block height do extra checks
  unsigned long long cheight = strtoull(current_block_height, NULL, 10);
  bool is_live_round = (height == cheight);

  pthread_mutex_lock(&producer_refs_lock);
  bool election_state_ready = is_hex_len(producer_refs[0].vrf_public_key, VRF_PUBLIC_KEY_LENGTH) &&
                              is_hex_len(producer_refs[0].vote_hash_hex, VOTE_HASH_LEN);
  pthread_mutex_unlock(&producer_refs_lock);

  DEBUG_PRINT("DPOPS dbg: height=%" PRIu64 " cheight=%llu live=%d state_ready=%d prev_in=%.*s prev_local=%.*s round_part %s",
              (uint64_t)height,
              (unsigned long long)cheight,
              is_live_round ? 1 : 0,
              election_state_ready ? 1 : 0,
              64, prev_hash_str,
              64, previous_block_hash,
              current_round_part);

  if (is_live_round && strcmp(current_round_part, "12")) {
    if (election_state_ready) {
      if (strncmp(prev_hash_str, previous_block_hash, 64) != 0) {
        cJSON_Delete(root);
        INFO_PRINT("Prev Hash mismatch: expected %s, got %s",
                   previous_block_hash, prev_hash_str);
        send_data(client, (unsigned char *)"0|PARENT_HASH_MISMATCH", strlen("0|PARENT_HASH_MISMATCH"));
        return;
      }

      // Parent matches our tip: enforce elected producer + vote hash
      if (strncmp(producer_refs[0].vrf_public_key, vrf_pubkey_str, VRF_PUBLIC_KEY_LENGTH) != 0) {
        INFO_PRINT("Public key mismatch: expected %s, got %s", producer_refs[0].vrf_public_key, vrf_pubkey_str);
        cJSON_Delete(root);
        send_data(client, (unsigned char *)"0|VRF_PUBKEY_MISMATCH", strlen("0|VRF_PUBKEY_MISMATCH"));
        return;
      }
      if (strncmp(producer_refs[0].vote_hash_hex, vote_hash_str, VOTE_HASH_LEN) != 0) {
        INFO_PRINT("Vote hash mismatch");
        cJSON_Delete(root);
        send_data(client, (unsigned char *)"0|VOTE_HASH_MISMATCH", strlen("0|VOTE_HASH_MISMATCH"));
        return;
      }

    } else {
      INFO_PRINT("No delegated selected, took too long");
      cJSON_Delete(root);
      send_data(client, (unsigned char *)"0|DELEGATE_SELECTION_TIMEOUT", strlen("0|DELEGATE_SELECTION_TIMEOUT"));
      return;
    }
  }

  // Buffers for binary data
  unsigned char pk_bin[crypto_vrf_PUBLICKEYBYTES] = {0};
  unsigned char proof_bin[crypto_vrf_PROOFBYTES] = {0};
  unsigned char beta_bin[crypto_vrf_OUTPUTBYTES] = {0};
  unsigned char prev_hash_bin[32] = {0};
  unsigned char alpha_input[32 + 8 + crypto_vrf_PUBLICKEYBYTES] = {0};
  unsigned char computed_beta[crypto_vrf_OUTPUTBYTES] = {0};

  // Convert hex → binary
  if (!hex_to_byte_array(vrf_pubkey_str, pk_bin, sizeof(pk_bin)) ||
      !hex_to_byte_array(vrf_proof_str, proof_bin, sizeof(proof_bin)) ||
      !hex_to_byte_array(vrf_beta_str, beta_bin, sizeof(beta_bin)) ||
      !hex_to_byte_array(prev_hash_str, prev_hash_bin, sizeof(prev_hash_bin))) {
    cJSON_Delete(root);
    send_data(client, (unsigned char *)"0|HEX_DECODING_FAIL", strlen("0|HEX_DECODING_FAIL"));
    return;
  }

  // Create alpha = prev_block_hash || height || pubkey
  memcpy(alpha_input, prev_hash_bin, 32);
  uint64_t height_le = htole64(height);
  memcpy(alpha_input + 32, &height_le, sizeof(height_le));
  memcpy(alpha_input + 40, pk_bin, crypto_vrf_PUBLICKEYBYTES);

  // Verify VRF
  bool valid_block = (crypto_vrf_verify(computed_beta, pk_bin, proof_bin, alpha_input, sizeof(alpha_input)) == 0) &&
                     (memcmp(computed_beta, beta_bin, sizeof(beta_bin)) == 0);

  if (valid_block) {
    snprintf(response, sizeof(response),
             "1|OK|%s",
             vote_hash_str);
    send_data(client, (unsigned char *)response, strlen(response));
  } else {
    snprintf(response, sizeof(response),
             "0|VERIFY_FAIL|%s",
             vote_hash_str);
    send_data(client, (unsigned char *)response, strlen(response));
  }

  cJSON_Delete(root);
  return;
}

/*---------------------------------------------------------------------------------------------------------
Name: server_receive_data_socket_nodes_to_block_verifiers_update_delegates
Description: Runs the code when the server receives the NODES_TO_BLOCK_VERIFIERS_UPDATE_DELEGATE message
Parameters:
  CLIENT_SOCKET - The socket to send data to
  MESSAGE - The message
---------------------------------------------------------------------------------------------------------*/
void server_receive_data_socket_nodes_to_block_verifiers_update_delegates(server_client_t *client, const char *MESSAGE) {
  char delegate_public_address[XCASH_WALLET_LENGTH + 1];
  uint64_t registration_time = 0;
  memset(delegate_public_address, 0, sizeof(delegate_public_address));

  // 1) Parse JSON
  if (MESSAGE == NULL || MESSAGE[0] == '\0') {
    SERVER_ERROR("0|Invalid message payload");
  }
  cJSON *root = cJSON_Parse(MESSAGE);
  if (!root) {
    SERVER_ERROR("0|Invalid JSON");
  }

  // Optional sanity: message_settings
  const cJSON *msg_settings = cJSON_GetObjectItemCaseSensitive(root, "message_settings");
  if (!cJSON_IsString(msg_settings) ||
      strncmp(msg_settings->valuestring, "NODES_TO_BLOCK_VERIFIERS_UPDATE_DELEGATE", 40) != 0) {
    cJSON_Delete(root);
    SERVER_ERROR("0|Invalid message settings");
  }

  // public_address
  const cJSON *jaddr = cJSON_GetObjectItemCaseSensitive(root, "public_address");
  if (!cJSON_IsString(jaddr)) {
    cJSON_Delete(root);
    SERVER_ERROR("0|public_address must be a string");
  }
  size_t addr_len = strnlen(jaddr->valuestring, XCASH_WALLET_LENGTH + 1);
  if (addr_len != XCASH_WALLET_LENGTH ||
      strncmp(jaddr->valuestring, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0) {
    cJSON_Delete(root);
    SERVER_ERROR("0|Invalid public_address (wrong length or prefix)");
  }
  memcpy(delegate_public_address, jaddr->valuestring, XCASH_WALLET_LENGTH);

  cJSON *js_reg_time = cJSON_GetObjectItemCaseSensitive(root, "registration_timestamp");

  if (!cJSON_IsNumber(js_reg_time) || !isfinite(js_reg_time->valuedouble) ||
      js_reg_time->valuedouble < 0.0) {
    cJSON_Delete(root);
    SERVER_ERROR("0|registration_timestamp invalid");
  }
  registration_time = (uint64_t)js_reg_time->valuedouble;

  // updates object (required)
  cJSON *updates = cJSON_GetObjectItemCaseSensitive(root, "updates");
  if (!cJSON_IsObject(updates)) {
    cJSON_Delete(root);
    SERVER_ERROR("0|'updates' must be an object");
  }

  // 2) Validate each field and build the BSON update doc
  static const char *const allowed_fields[] = {
      "IP_address", "about", "website", "team",
      "shared_delegate_status", "delegate_fee", "server_specs"};
  const size_t allowed_fields_count = sizeof(allowed_fields) / sizeof(allowed_fields[0]);

  // filter: { "public_address": "<addr>" }
  bson_t *filter_bson = bson_new();
  if (!filter_bson) {
    cJSON_Delete(root);
    SERVER_ERROR("0|Internal error (alloc filter)");
  }
  BSON_APPEND_UTF8(filter_bson, "public_address", delegate_public_address);

  // setdoc: fields to set
  bson_t *setdoc_bson = bson_new();
  if (!setdoc_bson) {
    bson_destroy(filter_bson);
    cJSON_Delete(root);
    SERVER_ERROR("0|Internal error (alloc update)");
  }

  size_t valid_kv_count = 0;

  for (cJSON *it = updates->child; it != NULL; it = it->next) {
    const char *key = it->string;
    if (!key) {
      bson_destroy(setdoc_bson);
      bson_destroy(filter_bson);
      cJSON_Delete(root);
      SERVER_ERROR("0|Missing update field name");
    }

    // allowlist check
    int ok_key = 0;
    for (size_t i = 0; i < allowed_fields_count; ++i) {
      if (strncmp(key, allowed_fields[i], VSMALL_BUFFER_SIZE) == 0) {
        ok_key = 1;
        break;
      }
    }
    if (!ok_key) {
      bson_destroy(setdoc_bson);
      bson_destroy(filter_bson);
      cJSON_Delete(root);
      SERVER_ERROR("0|Invalid update field (allowed: IP_address, about, website, team, shared_delegate_status, delegate_fee, server_specs)");
    }

    // ---- For ALL fields, require string on the wire ----
    if (!cJSON_IsString(it) || it->valuestring == NULL) {
      bson_destroy(setdoc_bson);
      bson_destroy(filter_bson);
      cJSON_Delete(root);
      SERVER_ERROR("0|Value for update field must be a string");
    }
    const char *val = it->valuestring;

    // Per-field constraints and storage
    if (strncmp(key, "IP_address", VSMALL_BUFFER_SIZE) == 0) {
      if (check_for_valid_ip_or_hostname(val) == 0) {
        bson_destroy(setdoc_bson);
        bson_destroy(filter_bson);
        cJSON_Delete(root);
        SERVER_ERROR("0|Invalid IP_address (must be IPv4 or domain, <=255 chars)");
      }
      BSON_APPEND_UTF8(setdoc_bson, key, val);
    } else if (strncmp(key, "about", VSMALL_BUFFER_SIZE) == 0) {
      if (strnlen(val, 512) > 511) {
        bson_destroy(setdoc_bson);
        bson_destroy(filter_bson);
        cJSON_Delete(root);
        SERVER_ERROR("0|'about' too long (max 512)");
      }
      BSON_APPEND_UTF8(setdoc_bson, key, val);
    } else if (strncmp(key, "website", VSMALL_BUFFER_SIZE) == 0) {
      if (strnlen(val, 256) > 255) {
        bson_destroy(setdoc_bson);
        bson_destroy(filter_bson);
        cJSON_Delete(root);
        SERVER_ERROR("0|'website' too long (max 255)");
      }
      BSON_APPEND_UTF8(setdoc_bson, key, val);
    } else if (strncmp(key, "team", VSMALL_BUFFER_SIZE) == 0) {
      if (strnlen(val, 256) > 255) {
        bson_destroy(setdoc_bson);
        bson_destroy(filter_bson);
        cJSON_Delete(root);
        SERVER_ERROR("0|'team' too long (max 255)");
      }
      BSON_APPEND_UTF8(setdoc_bson, key, val);
    } else if (strncmp(key, "shared_delegate_status", VSMALL_BUFFER_SIZE) == 0) {
      // the names are used in a sort and seed type needs to come
      if (strncmp(val, "solo", VSMALL_BUFFER_SIZE) != 0 &&
          strncmp(val, "shared", VSMALL_BUFFER_SIZE) != 0 &&
          strncmp(val, "team", VSMALL_BUFFER_SIZE) != 0) {
        bson_destroy(setdoc_bson);
        bson_destroy(filter_bson);
        cJSON_Delete(root);
        SERVER_ERROR("0|shared_delegate_status must be one of: solo, shared, or team");
      }
      BSON_APPEND_UTF8(setdoc_bson, key, val);
    } else if (strncmp(key, "delegate_fee", VSMALL_BUFFER_SIZE) == 0) {
      // Must be string on the wire; parse to number and store numeric
      if (check_for_valid_delegate_fee(val) == 0) {
        bson_destroy(setdoc_bson);
        bson_destroy(filter_bson);
        cJSON_Delete(root);
        SERVER_ERROR("0|Invalid delegate_fee (bad format or out of range)");
      }
      errno = 0;
      char *endp = NULL;
      double d = strtod(val, &endp);
      if (errno != 0 || endp == val || *endp != '\0' || !isfinite(d) || d < 0.0 || d > 100.0) {
        bson_destroy(setdoc_bson);
        bson_destroy(filter_bson);
        cJSON_Delete(root);
        SERVER_ERROR("0|Invalid delegate_fee (not numeric or out of range)");
      }
      // Store as numeric (Double). Switch to Decimal128 if you prefer exact fixed precision.
      BSON_APPEND_DOUBLE(setdoc_bson, "delegate_fee", d);
    } else if (strncmp(key, "server_specs", VSMALL_BUFFER_SIZE) == 0) {
      if (strnlen(val, 256) > 255) {
        bson_destroy(setdoc_bson);
        bson_destroy(filter_bson);
        cJSON_Delete(root);
        SERVER_ERROR("0|'server_specs' too long (max 255)");
      }
      BSON_APPEND_UTF8(setdoc_bson, key, val);
    } else {
      // Fallback (shouldn't hit due to allowlist)
      BSON_APPEND_UTF8(setdoc_bson, key, val);
    }

    ++valid_kv_count;
  }

  BSON_APPEND_DATE_TIME(setdoc_bson, "registration_timestamp", (int64_t)registration_time * 1000);

  if (valid_kv_count == 0) {
    bson_destroy(setdoc_bson);
    bson_destroy(filter_bson);
    cJSON_Delete(root);
    SERVER_ERROR("0|No valid updates provided");
  }

  // 3) Execute DB update
  if (update_document_from_collection_bson(DATABASE_NAME, DB_COLLECTION_DELEGATES, filter_bson, setdoc_bson) == 0) {
    bson_destroy(setdoc_bson);
    bson_destroy(filter_bson);
    cJSON_Delete(root);
    SERVER_ERROR("0|Database update failed");
  }

  bson_destroy(setdoc_bson);
  bson_destroy(filter_bson);
  cJSON_Delete(root);

  // 4) Success
  send_data(client, (unsigned char *)"1|Updated the delegate", strlen("1|Updated the delegate"));
  return;
}

/* --------------------------------------------------------------------------------------------------------
  Name: server_receive_data_socket_node_to_block_verifiers_add_reserve_proof
  Description: Runs the code when the server receives the NODE_TO_BLOCK_VERIFIERS_ADD_RESERVE_PROOF message 
    is received.
  Parameters:
  CLIENT_SOCKET - The socket to send data to
  MESSAGE - The message
----------------------------------------------------------------------------------------------------------- */
void server_receive_data_socket_node_to_block_verifiers_add_reserve_proof(server_client_t *client, const char *MESSAGE) {

#ifndef SEED_NODE_ON

  (void)MESSAGE;
  SERVER_ERROR("0|Transaction only available for seed nodes");

#else

  char voter_public_address[XCASH_WALLET_LENGTH + 1] = {0};
  char delegate_name_or_address[MAXIMUM_BUFFER_SIZE_DELEGATES_NAME + 1] = {0};
  char voted_for_public_address[XCASH_WALLET_LENGTH + 1] = {0};
  char proof_str[BUFFER_SIZE_RESERVE_PROOF + 1] = {0};
  char dbvoted_for[XCASH_WALLET_LENGTH + 1] = {0};
  char dbreserve_proof[BUFFER_SIZE_RESERVE_PROOF + 1] = {0};
  char json_filter[256] = {0};
  uint64_t vote_amount_atomic = 0;
  int64_t dbtotal_vote = 0;

  // ---- Parse JSON ----
  if (!MESSAGE || !*MESSAGE) {
    SERVER_ERROR("0|Invalid message payload");
  }
  cJSON *root = cJSON_Parse(MESSAGE);
  if (!root) {
    SERVER_ERROR("0|Invalid JSON");
  }

  // message_settings
  const cJSON *j_msg = cJSON_GetObjectItemCaseSensitive(root, "message_settings");
  if (!j_msg || !cJSON_IsString(j_msg)) {
    cJSON_Delete(root);
    SERVER_ERROR("0|Invalid message transaction type");
  }

  const char *ms = j_msg->valuestring;
  const bool is_vote = (strcmp(ms, "NODES_TO_BLOCK_VERIFIERS_VOTE") == 0);
  const bool is_revote = (strcmp(ms, "NODES_TO_BLOCK_VERIFIERS_REVOTE") == 0);

  if (!(is_vote || is_revote)) {
    cJSON_Delete(root);
    SERVER_ERROR("0|Invalid message transaction type");
  }

  // public_address (voter)
  const cJSON *j_addr = cJSON_GetObjectItemCaseSensitive(root, "public_address");
  if (!cJSON_IsString(j_addr)) {
    cJSON_Delete(root);
    SERVER_ERROR("0|public_address must be a string");
  }
  const size_t addr_len = strnlen(j_addr->valuestring, XCASH_WALLET_LENGTH + 1);
  if (addr_len != XCASH_WALLET_LENGTH ||
      strncmp(j_addr->valuestring, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0) {
    cJSON_Delete(root);
    SERVER_ERROR("0|Invalid public_address (wrong length or prefix)");
  }
  memcpy(voter_public_address, j_addr->valuestring, XCASH_WALLET_LENGTH);

    // delegate_name_or_address
  const cJSON *j_target = cJSON_GetObjectItemCaseSensitive(root, "delegate_name_or_address");
  if (!cJSON_IsString(j_target) || j_target->valuestring[0] == '\0') {
    cJSON_Delete(root);
    SERVER_ERROR("0|delegate_name_or_address must be a non-empty string");
  }
  strncpy(delegate_name_or_address, j_target->valuestring, sizeof(delegate_name_or_address) - 1);

  // vote_amount (STRING; atomic units)
  const cJSON *j_amount = cJSON_GetObjectItemCaseSensitive(root, "vote_amount");
  if (!cJSON_IsString(j_amount) || j_amount->valuestring[0] == '\0') {
    cJSON_Delete(root);
    SERVER_ERROR("0|vote_amount must be a non-empty string (atomic units)");
  }
  {
    const char *anum = j_amount->valuestring;
    for (const char *p = anum; *p; ++p) {
      if (*p < '0' || *p > '9') {
        cJSON_Delete(root);
        SERVER_ERROR("0|vote_amount must contain only digits (atomic units)");
      }
    }
    errno = 0;
    unsigned long long tmp = strtoull(anum, NULL, 10);
    if (errno != 0 || tmp == 0ULL) {
      cJSON_Delete(root);
      SERVER_ERROR("0|Invalid vote_amount");
    }
    vote_amount_atomic = (uint64_t)tmp;

    // Enforce per-vote minimum on the server too
    if (vote_amount_atomic < MIN_VOTE_ATOMIC) {
      cJSON_Delete(root);

      const unsigned long long min_vote_display =
          (unsigned long long)(MIN_VOTE_ATOMIC / XCASH_ATOMIC_UNITS);

      char err_msg[128];
      snprintf(err_msg, sizeof(err_msg),
               "0|Each vote must be at least %llu XCA", min_vote_display);

      SERVER_ERROR(err_msg);
    }
  }

  // reserve_proof
  const cJSON *j_proof = cJSON_GetObjectItemCaseSensitive(root, "reserve_proof");
  if (!cJSON_IsString(j_proof) || j_proof->valuestring[0] == '\0') {
    cJSON_Delete(root);
    SERVER_ERROR("0|reserve_proof must be a non-empty string");
  }
  const size_t proof_len = strnlen(j_proof->valuestring, sizeof(proof_str));
  if (proof_len == sizeof(proof_str)) {
    cJSON_Delete(root);
    SERVER_ERROR("0|reserve_proof too large");
  }
  memcpy(proof_str, j_proof->valuestring, proof_len);

  // ---- Resolve delegate target → public address ----
  if (strnlen(delegate_name_or_address, sizeof(delegate_name_or_address)) == XCASH_WALLET_LENGTH &&
      strncmp(delegate_name_or_address, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) == 0) {
    memcpy(voted_for_public_address, delegate_name_or_address, XCASH_WALLET_LENGTH);
  } else {
    // lookup by name (validate the name)
    size_t name_len = strnlen(delegate_name_or_address, sizeof(delegate_name_or_address));
    if (name_len == 0) {
      cJSON_Delete(root);
      SERVER_ERROR("0|delegate_name_or_address is empty");
    }

    // allow-list simple charset: letters, digits, underscore, hyphen, dot
    for (size_t i = 0; i < name_len; ++i) {
      unsigned char ch = (unsigned char)delegate_name_or_address[i];
      if (!((ch >= 'A' && ch <= 'Z') ||
            (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '_' || ch == '-' || ch == '.')) {
        cJSON_Delete(root);
        SERVER_ERROR("0|delegate_name_or_address contains invalid characters");
      }
    }

    snprintf(json_filter, sizeof(json_filter),
             "{\"delegate_name\":\"%.*s\"}",
             (int)name_len, delegate_name_or_address);

    char addr_buf[XCASH_WALLET_LENGTH + 1] = {0};
    if (read_document_field_from_collection(DATABASE_NAME, DB_COLLECTION_DELEGATES, json_filter,
                                            "public_address", addr_buf, sizeof(addr_buf)) != XCASH_OK ||
        strnlen(addr_buf, sizeof(addr_buf)) != XCASH_WALLET_LENGTH ||
        strncmp(addr_buf, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0) {
      cJSON_Delete(root);
      SERVER_ERROR("0|The delegate voted for is invalid");
    }

    memcpy(voted_for_public_address, addr_buf, XCASH_WALLET_LENGTH);
  }

  // ---- Disallow votes for seed/network data nodes ----
  if (is_seed_address(voted_for_public_address)) {
    cJSON_Delete(root);
    SERVER_ERROR("0|Cannot vote for a network seed node");
  }

  if (check_reserve_proofs(vote_amount_atomic, voter_public_address, proof_str) != XCASH_OK) {
    cJSON_Delete(root);
    SERVER_ERROR("0|Invalid reserve proof");
  }

  bson_error_t err;
  memset(&err, 0, sizeof(err));

  bool ok = fetch_reserve_proof_fields_by_id(
      voter_public_address,
      dbvoted_for, sizeof(dbvoted_for),
      &dbtotal_vote,
      dbreserve_proof, sizeof(dbreserve_proof),
      &err);

  if (!ok) {
    if (err.code != 0) {
      // A Mongo/cursor error occurred
      ERROR_PRINT("fetch_reserve_proof_fields_by_id failed: %s (%d)", err.message, err.code);
      cJSON_Delete(root);
      SERVER_ERROR("0|Database error fetching reserve_proof");
    } else {
      // Not found (doc missing)
      if (is_revote) {
        cJSON_Delete(root);
        SERVER_ERROR("0|No original vote exists for revote");
      }
    }
  } else {
    DEBUG_PRINT("voted_for=%s, total_vote=%lld, reserve_proof_len=%zu",
               dbvoted_for, (long long)dbtotal_vote, strlen(dbreserve_proof));
    if (strcmp(dbvoted_for, voted_for_public_address) == 0 &&
      strcmp(dbreserve_proof, proof_str) == 0 &&
      (uint64_t)dbtotal_vote == vote_amount_atomic)
    {
      // exact vote already exists, no need to continue (will only occur when checking for seed node replication)
      cJSON_Delete(root);
      send_data(client, (unsigned char *)"1|This vote already exists", strlen("1|This vote already exists"));
      return;
    }
  }

  snprintf(json_filter, sizeof(json_filter), "{\"_id\":\"%s\"}", voter_public_address);

  // One vote per wallet: delete previous (single collection)
  (void)delete_document_from_collection(DATABASE_NAME, DB_COLLECTION_RESERVE_PROOFS, json_filter);

  // Build BSON document
  bson_t doc;
  bson_init(&doc);

  // store _id as the public wallet address
  bson_append_utf8(&doc, "_id", -1, voter_public_address, XCASH_WALLET_LENGTH);

  // public_address_voted_for
  bson_append_utf8(&doc, "public_address_voted_for", -1,
                   voted_for_public_address, XCASH_WALLET_LENGTH);

  // total (64-bit int instead of "$numberLong")
  BSON_APPEND_INT64(&doc, "total_vote", (int64_t)vote_amount_atomic);

  // reserve_proof
  BSON_APPEND_UTF8(&doc, "reserve_proof", proof_str);

  // Insert into Mongo
  if (insert_document_into_collection_bson(DATABASE_NAME, DB_COLLECTION_RESERVE_PROOFS, &doc) != 1) {
    bson_destroy(&doc);
    cJSON_Delete(root);
    SERVER_ERROR("0|The vote could not be added to the database");
  }

  // Always free resources
  bson_destroy(&doc);

  // Done: hourly job will revalidate & aggregate totals
  cJSON_Delete(root);
  if (is_vote) {
    send_data(client, (unsigned char *)"1|The vote was successfully added to the database", strlen("1|The vote was successfully added to the database"));
  } else {
    send_data(client, (unsigned char *)"1|The revote was successful", strlen("1|The revote was successful"));
  }
  return;

  #endif  // SEED_NODE_ON

}
/* --------------------------------------------------------------------------------------------------------
  Name: server_receive_data_socket_node_to_block_verifiers_check_vote_status
  Description: Runs the code when the server receives the NODE_TO_BLOCK_VERIFIERS_CHECK_VOTE_STATUS message is received
  Parameters:
  CLIENT_SOCKET - The socket to send data to
  MESSAGE - The message
----------------------------------------------------------------------------------------------------------- */
void server_receive_data_socket_node_to_block_verifiers_check_vote_status(server_client_t *client, const char *MESSAGE) {
  if (!MESSAGE || !*MESSAGE) {
    SERVER_ERROR("0|Invalid message payload");
  }

  // Parse JSON
  cJSON *root = cJSON_Parse(MESSAGE);
  if (!root) {
    SERVER_ERROR("0|Invalid JSON");
  }

  const cJSON *addr = cJSON_GetObjectItemCaseSensitive(root, "public_address");
  if (!cJSON_IsString(addr) || !addr->valuestring || !*addr->valuestring) {
    cJSON_Delete(root);
    SERVER_ERROR("0|Missing public_address");
  }
  const char *public_address = addr->valuestring;

  // Basic sanity: prefix and length (adjust macros to your config)
  if (strlen(public_address) != XCASH_WALLET_LENGTH ||
      strncmp(public_address, XCASH_WALLET_PREFIX, strlen(XCASH_WALLET_PREFIX)) != 0) {
    cJSON_Delete(root);
    SERVER_ERROR("0|Invalid XCA public address");
  }

  // Basic sanity: prefix and length (adjust macros to your config)
  if (!str_is_base58(public_address)) {
    cJSON_Delete(root);
    SERVER_ERROR("0|Invalid XCA public address, not base58");
  }

  // Query Mongo via helper
  int64_t total_atomic = 0;
  char delegate_name[MAXIMUM_BUFFER_SIZE_DELEGATES_NAME + 1] = {0};

  if (!get_vote_total_and_delegate_name(public_address, &total_atomic, delegate_name)) {
    cJSON_Delete(root);
    send_data(client, (unsigned char*)"1|No Vote Found", strlen("1|No Vote Found"));
  }

  // Build success message
  char out[256];
  const char* name = (delegate_name[0] ? delegate_name : "(error)");
  double total_xca = (double)total_atomic / (double)ATOMIC_UNITS_PER_XCA;
  snprintf(out, sizeof(out), "1|Vote found: total:%.6f XCA, delegate:%s", total_xca, name);

  send_data(client, (unsigned char*)out, strlen(out));
  cJSON_Delete(root);
  return;
}