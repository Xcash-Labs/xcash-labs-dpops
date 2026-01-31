#include "block_verifiers_server_functions.h"

void server_receive_data_socket_block_verifiers_to_block_verifiers_vrf_data(const char* MESSAGE)
{
  char public_address[XCASH_WALLET_LENGTH + 1] = {0};
  char vrf_public_key_data[VRF_PUBLIC_KEY_LENGTH + 1] = {0};
  char vrf_proof_hex[VRF_PROOF_LENGTH + 1] = {0};
  char vrf_beta_hex[VRF_BETA_LENGTH + 1] = {0};
  char block_height[BLOCK_HEIGHT_LENGTH + 1] = {0};
  char parsed_delegates_hash[SHA256_HASH_SIZE + 1] = {0};

  // parse
  if (parse_json_data(MESSAGE, "public_address", public_address, sizeof(public_address)) == XCASH_ERROR ||
      parse_json_data(MESSAGE, "vrf_public_key", vrf_public_key_data, sizeof(vrf_public_key_data)) == XCASH_ERROR ||
      parse_json_data(MESSAGE, "vrf_proof", vrf_proof_hex, sizeof(vrf_proof_hex)) == XCASH_ERROR ||
      parse_json_data(MESSAGE, "vrf_beta", vrf_beta_hex, sizeof(vrf_beta_hex)) == XCASH_ERROR ||
      parse_json_data(MESSAGE, "block-height", block_height, sizeof(block_height)) == XCASH_ERROR ||
      parse_json_data(MESSAGE, "delegates_hash", parsed_delegates_hash, sizeof(parsed_delegates_hash)) == XCASH_ERROR)
  {
    ERROR_PRINT("Could not parse the block_verifiers_to_block_verifiers_vrf_data");
    return;
  }

  if (strlen(public_address) < 5 || public_address[0] != 'X') {
    ERROR_PRINT("Invalid or missing delegate address: '%s'", public_address);
    return;
  }

  // ---- Phase 1: find delegate + snapshot globals quickly under lock
  size_t idx = (size_t)-1;

  // snapshots (so we don't read moving targets later)
  char local_block_height[BLOCK_HEIGHT_LENGTH + 1] = {0};
  char local_delegates_hash[SHA256_HASH_SIZE + 1] = {0};
  char local_prev_block_hash[BLOCK_HASH_LENGTH + 1] = {0};
  

  pthread_mutex_lock(&delegates_all_lock);

  // snapshot shared globals while under a lock you *know* is held consistently
  // (If these globals have their own lock, use that instead / additionally.)
  snprintf(local_block_height,  sizeof(local_block_height),  "%s", current_block_height);
  snprintf(local_delegates_hash, sizeof(local_delegates_hash), "%s", delegates_hash);
  snprintf(local_prev_block_hash, sizeof(local_prev_block_hash), "%s", previous_block_hash);

  for (size_t i = 0; i < BLOCK_VERIFIERS_TOTAL_AMOUNT; i++) {
    if (strncmp(delegates_all[i].public_address, public_address, XCASH_WALLET_LENGTH) == 0 &&
        delegates_all[i].verifiers_vrf_proof_hex[0] == '\0' &&
        delegates_all[i].verifiers_vrf_beta_hex[0] == '\0')
    {
      idx = i;
      break;
    }
  }

  pthread_mutex_unlock(&delegates_all_lock);

  if (idx == (size_t)-1) {
    if (startup_complete) {
      WARNING_PRINT("Delegate %s not found or already has VRF fields set.", public_address);
    }
    return;
  }

  // quick checks using snapshots
  if (strcmp(block_height, local_block_height) != 0) {
    WARNING_PRINT("Block height mismatch for %s: remote=%s, local=%s",
                  public_address, block_height, local_block_height);
    return;
  }

  if (strcmp(parsed_delegates_hash, local_delegates_hash) != 0) {
    // NOTE: delegate_db_hash_mismatch++ should be atomic or protected by a lock.
    pthread_mutex_lock(&delegates_all_lock);
    delegate_db_hash_mismatch++;
    strncpy(delegates_all[idx].online_status, "partial", sizeof(delegates_all[idx].online_status) - 1);
    pthread_mutex_unlock(&delegates_all_lock);

    WARNING_PRINT("Delegates hash mismatch for %s: remote=%s, local=%s",
                  public_address, parsed_delegates_hash, local_delegates_hash);
    return;
  }

  // ---- Phase 2: verify VRF OUTSIDE the lock
  unsigned char alpha_input_bin[72] = {0};
  unsigned char pk_bin[crypto_vrf_PUBLICKEYBYTES] = {0};
  unsigned char vrf_proof[crypto_vrf_PROOFBYTES] = {0};
  unsigned char vrf_beta[crypto_vrf_OUTPUTBYTES] = {0};
  unsigned char prev_hash_bin[32] = {0}; // if your block hash is always 32 bytes

  if (!hex_to_byte_array(vrf_public_key_data, pk_bin, sizeof(pk_bin)) ||
      !hex_to_byte_array(vrf_proof_hex, vrf_proof, sizeof(vrf_proof)) ||
      !hex_to_byte_array(vrf_beta_hex, vrf_beta, sizeof(vrf_beta)) ||
      !hex_to_byte_array(local_prev_block_hash, prev_hash_bin, sizeof(prev_hash_bin)))
  {
    ERROR_PRINT("Failed to decode one or more VRF fields from %s", public_address);
    return;
  }

  memcpy(alpha_input_bin, prev_hash_bin, 32);

  uint64_t height_num = strtoull(local_block_height, NULL, 10);
  uint64_t height_le = htole64(height_num);
  memcpy(alpha_input_bin + 32, &height_le, sizeof(height_le));
  memcpy(alpha_input_bin + 40, pk_bin, 32);

  unsigned char computed_beta[crypto_vrf_OUTPUTBYTES];
  if (crypto_vrf_verify(computed_beta, pk_bin, vrf_proof, alpha_input_bin, sizeof(alpha_input_bin)) != 0) {
    ERROR_PRINT("VRF proof failed verification from %s", public_address);
    return;
  }

  if (memcmp(computed_beta, vrf_beta, sizeof(vrf_beta)) != 0) {
    WARNING_PRINT("VRF beta mismatch from %s", public_address);
    return;
  }

  // ---- Phase 3: commit under lock (and re-check it's still empty)
  pthread_mutex_lock(&delegates_all_lock);

  if (delegates_all[idx].verifiers_vrf_proof_hex[0] == '\0' &&
      delegates_all[idx].verifiers_vrf_beta_hex[0] == '\0')
  {
    strncpy(delegates_all[idx].online_status, "true", sizeof(delegates_all[idx].online_status) - 1);
    memcpy(delegates_all[idx].verifiers_vrf_proof_hex, vrf_proof_hex, VRF_PROOF_LENGTH + 1);
    memcpy(delegates_all[idx].verifiers_vrf_beta_hex, vrf_beta_hex, VRF_BETA_LENGTH + 1);
  }

  pthread_mutex_unlock(&delegates_all_lock);
}





/*---------------------------------------------------------------------------------------------------------
Name: server_receive_data_socket_node_to_node_vote_majority
Description: Runs the code when the server receives the NODES_TO_NODES_VOTE_MAJORITY_RESULTS message
Parameters:
  MESSAGE - The message
---------------------------------------------------------------------------------------------------------*/
void server_receive_data_socket_node_to_node_vote_majority(const char* MESSAGE) {
  char public_address[XCASH_WALLET_LENGTH + 1] = {0};
  char public_address_producer[XCASH_WALLET_LENGTH + 1] = {0};
  char vrf_public_key_data[VRF_PUBLIC_KEY_LENGTH + 1] = {0};
  char vrf_proof_hex[VRF_PROOF_LENGTH + 1] = {0};
  char vrf_beta_hex[VRF_BETA_LENGTH + 1] = {0};
  char block_height[BLOCK_HEIGHT_LENGTH + 1] = {0};
  char vote_signature[XCASH_SIGN_DATA_LENGTH + 1] = {0};

  DEBUG_PRINT("received %s, %s", __func__, MESSAGE);

  // parse the message
  if (parse_json_data(MESSAGE, "public_address", public_address, sizeof(public_address)) == XCASH_ERROR ||
      parse_json_data(MESSAGE, "proposed_producer", public_address_producer, sizeof(public_address_producer)) == XCASH_ERROR ||
      parse_json_data(MESSAGE, "vrf_public_key", vrf_public_key_data, sizeof(vrf_public_key_data)) == XCASH_ERROR ||
      parse_json_data(MESSAGE, "vrf_proof", vrf_proof_hex, sizeof(vrf_proof_hex)) == XCASH_ERROR ||
      parse_json_data(MESSAGE, "vrf_beta", vrf_beta_hex, sizeof(vrf_beta_hex)) == XCASH_ERROR ||
      parse_json_data(MESSAGE, "block_height", block_height, sizeof(block_height)) == XCASH_ERROR ||
      parse_json_data(MESSAGE, "vote_signature", vote_signature, sizeof(vote_signature)) == XCASH_ERROR) {
    ERROR_PRINT("Could not parse the block_verifiers_to_block_verifiers_vrf_data");
    return;
  }

  if (strlen(vote_signature) == 0 || strncmp(vote_signature, XCASH_SIGN_DATA_PREFIX, sizeof(XCASH_SIGN_DATA_PREFIX) - 1) != 0) {
    ERROR_PRINT("Error with vote signature for %s", public_address);
    return;
  }

  bool found_voter = false;
  pthread_mutex_lock(&current_block_verifiers_lock);
  for (size_t i = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
    if (strcmp(public_address, current_block_verifiers_list.block_verifiers_public_address[i]) == 0) {
      found_voter = true;
      if (!verify_vrf_vote_signature(block_height, vrf_beta_hex, vrf_public_key_data, public_address, vote_signature)) {
        pthread_mutex_unlock(&current_block_verifiers_lock);
        WARNING_PRINT("Unable to verify the signature for vote from delegate %s", public_address);
        return;
      }
      if (current_block_verifiers_list.block_verifiers_voted[i] == 0) {
        current_block_verifiers_list.block_verifiers_voted[i] = 1;
        memcpy(current_block_verifiers_list.block_verifiers_vote_signature[i], vote_signature, XCASH_SIGN_DATA_LENGTH + 1);
        memcpy(current_block_verifiers_list.block_verifiers_selected_public_address[i], public_address_producer, XCASH_WALLET_LENGTH + 1);
        break;
      } else {
        pthread_mutex_unlock(&current_block_verifiers_lock);
        WARNING_PRINT("Verifier %s, has already voted and can not vote again", public_address);
        return;
      }
    }
  }
  pthread_mutex_unlock(&current_block_verifiers_lock);

  if (!found_voter) {
    WARNING_PRINT("Verifier %s not found in current_block_verifiers_list", public_address);
    return;
  }

  if (strcmp(block_height, current_block_height) != 0) {
    ERROR_PRINT("Mismatch in block height for verifier %s", public_address);
    return;
  }

  pthread_mutex_lock(&current_block_verifiers_lock);
  for (size_t i = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
    if (strcmp(public_address_producer, current_block_verifiers_list.block_verifiers_public_address[i]) != 0) {
      continue;
    }

    if (strcmp(vrf_public_key_data, current_block_verifiers_list.block_verifiers_public_key[i]) != 0) {
      pthread_mutex_unlock(&current_block_verifiers_lock);
      ERROR_PRINT("Mismatch in vrf_public_key for verifier %s", public_address_producer);
      return;
    }

    if (strcmp(vrf_proof_hex, current_block_verifiers_list.block_verifiers_vrf_proof_hex[i]) != 0) {
      pthread_mutex_unlock(&current_block_verifiers_lock);
      ERROR_PRINT("Mismatch in vrf_proof for verifier %s", public_address_producer);
      return;
    }

    if (strcmp(vrf_beta_hex, current_block_verifiers_list.block_verifiers_vrf_beta_hex[i]) != 0) {
      pthread_mutex_unlock(&current_block_verifiers_lock);
      ERROR_PRINT("Mismatch in vrf_beta for verifier %s", public_address_producer);
      return;
    }

    current_block_verifiers_list.block_verifiers_vote_total[i] += 1;
    pthread_mutex_unlock(&current_block_verifiers_lock);
    return;
  }

  pthread_mutex_unlock(&current_block_verifiers_lock);
  return;
}

// Helper for qsort
static int bytes32_cmp(const void *va, const void *vb) {
  const unsigned char *a = (const unsigned char *)va;
  const unsigned char *b = (const unsigned char *)vb;
  return memcmp(a, b, crypto_vrf_PUBLICKEYBYTES);
}

/*---------------------------------------------------------------------------------------------------------
 * @brief Verifies a delegate's vote for the block producer based on VRF output and signature.
 *
 * This function reconstructs the vote hash from the following inputs:
 *   - block_height (as ASCII string, e.g., "5")
 *   - vrf_beta (hex-encoded 32-byte VRF output)
 *   - vrf_public_key (hex-encoded 32-byte VRF public key)
 *
 * It hashes: block_height || vrf_beta || vrf_public_key || round_data_public_key hash
 * and verifies the provided signature (hex-encoded) was made by the delegate.
 *
 * @param block_height         Null-terminated ASCII string of the block height (e.g., "5")
 * @param vrf_beta_hex         Hex-encoded 32-byte VRF beta (64 hex characters)
 * @param vrf_pubkey_hex       Hex-encoded 32-byte VRF public key (64 hex characters)
 * @param vote_signature_hex   Hex-encoded 64-byte signature (128 hex characters)
 *
 * @return true if the signature is valid and matches the inputs; false otherwise
---------------------------------------------------------------------------------------------------------*/
bool verify_vrf_vote_signature(const char *block_height,
                          const char *vrf_beta_hex,
                          const char *vrf_pubkey_hex,
                          const char *public_wallet_address,
                          const char *vote_signature)
{
  const char *HTTP_HEADERS[] = {"Content-Type: application/json", "Accept: application/json"};
  const size_t HTTP_HEADERS_LENGTH = sizeof(HTTP_HEADERS) / sizeof(HTTP_HEADERS[0]);
  uint8_t vrf_beta_bin[crypto_vrf_OUTPUTBYTES] = {0};
  uint8_t vrf_pubkey_bin[crypto_vrf_PUBLICKEYBYTES] = {0};
  uint8_t hash[SHA256_EL_HASH_SIZE];
  char hash_hex[(SHA256_EL_HASH_SIZE * 2) + 1] = {0};
  uint8_t hash_input[160];
  size_t offset = 0;
  char request[MEDIUM_BUFFER_SIZE * 2] = {0};
  char response[MEDIUM_BUFFER_SIZE] = {0};

  if (!block_height || !vrf_beta_hex || !vrf_pubkey_hex || !vote_signature)
    return false;

  if (strlen(vrf_beta_hex) != crypto_vrf_OUTPUTBYTES * 2 ||
      strlen(vrf_pubkey_hex) != crypto_vrf_PUBLICKEYBYTES * 2)
    return false;

  if (!hex_to_byte_array(vrf_beta_hex, vrf_beta_bin, sizeof(vrf_beta_bin)) ||
      !hex_to_byte_array(vrf_pubkey_hex, vrf_pubkey_bin, sizeof(vrf_pubkey_bin)))
    return false;

  size_t block_height_len = strlen(block_height);
  if (block_height_len + crypto_vrf_OUTPUTBYTES + crypto_vrf_PUBLICKEYBYTES > sizeof(hash_input))
    return false;

  // collect valid pubkeys and create a hash
  uint8_t pks[BLOCK_VERIFIERS_AMOUNT][crypto_vrf_PUBLICKEYBYTES];
  memset(pks, 0, sizeof pks);
  size_t n = 0;

  for (size_t i = 0; i < BLOCK_VERIFIERS_AMOUNT; ++i) {
    const char* hex = current_block_verifiers_list.block_verifiers_public_key[i];
    if (!hex || hex[0] == '\0') continue;

    size_t len = strnlen(hex, (size_t)VRF_PUBLIC_KEY_LENGTH + 1);  // VRF_PUBLIC_KEY_LENGTH == 64
    if (len != (size_t)VRF_PUBLIC_KEY_LENGTH) {
      ERROR_PRINT("Pubkey[%zu] length %zu (expected %d)", i, len, VRF_PUBLIC_KEY_LENGTH);
      return false;  // or: continue;
    }

    if (!hex_to_byte_array(hex, pks[n], crypto_vrf_PUBLICKEYBYTES)) {
      ERROR_PRINT("Pubkey[%zu] invalid hex", i);
      return false;  // or: continue;
    }
    n++;
  }

  if (n == 0) {
    ERROR_PRINT("No valid public keys to hash");
    return false;
  }

  qsort(pks, n, crypto_vrf_PUBLICKEYBYTES, bytes32_cmp);

  // Domain-separate and bind to the round
  uint8_t round_pk_hash_bin[SHA256_EL_HASH_SIZE] = {0};  // SHA256_EL_HASH_SIZE must be 32
  {
    // buffer = "PKSET" || varint(len(height_bytes)) || height_bytes || varint(n) || concat(pks[0..n-1])
    uint8_t buf[5 + 16 + 8 + BLOCK_VERIFIERS_AMOUNT * crypto_vrf_PUBLICKEYBYTES];
    size_t off = 0;

    // domain tag
    memcpy(buf + off, "PKSET", 5);
    off += 5;

    // height as ASCII (or binary) with length-prefix to avoid ambiguity
    const uint8_t* h = (const uint8_t*)current_block_height;
    size_t hlen = strlen(current_block_height);
    buf[off++] = (uint8_t)hlen;  // simple 1-byte length (if height fits)
    memcpy(buf + off, h, hlen);
    off += hlen;

    // count
    buf[off++] = (uint8_t)n;

    // concatenated keys
    memcpy(buf + off, pks, n * crypto_vrf_PUBLICKEYBYTES);
    off += n * crypto_vrf_PUBLICKEYBYTES;

    sha256EL(buf, off, round_pk_hash_bin);
  }

  memcpy(hash_input + offset, block_height, block_height_len); offset += block_height_len;
  memcpy(hash_input + offset, vrf_beta_bin, crypto_vrf_OUTPUTBYTES); offset += crypto_vrf_OUTPUTBYTES;
  memcpy(hash_input + offset, vrf_pubkey_bin, crypto_vrf_PUBLICKEYBYTES); offset += crypto_vrf_PUBLICKEYBYTES;
  memcpy(hash_input + offset, round_pk_hash_bin, crypto_vrf_PUBLICKEYBYTES); offset += crypto_vrf_PUBLICKEYBYTES;

  sha256EL(hash_input, offset, hash);
  for (size_t i = 0; i < SHA256_EL_HASH_SIZE; i++)
    snprintf(hash_hex + i * 2, 3, "%02x", hash[i]);

  snprintf(request, sizeof(request),
           "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"verify\",\"params\":{"
           "\"data\":\"%s\","
           "\"address\":\"%s\","
           "\"signature\":\"%s\"}}",
           hash_hex, public_wallet_address, vote_signature);

  if (send_http_request(response, sizeof(response), XCASH_WALLET_IP, "/json_rpc", XCASH_WALLET_PORT,
                        "POST", HTTP_HEADERS, HTTP_HEADERS_LENGTH,
                        request, HTTP_TIMEOUT_SETTINGS) <= 0) {
    ERROR_PRINT("verify_vrf_vote_signature: HTTP request failed");
    return false;
  }

  // Parse response
  char result[8] = {0};
  if (parse_json_data(response, "result.good", result, sizeof(result)) == 1 && strcmp(result, "true") == 0) {
    return true;
  } else {
    return false;
  }

}

// Verifies a vote signature against data bound to:
//   block_height (ASCII) || vrf_beta (32 bytes) || vrf_pubkey (32 bytes) || sha256(concat(all round VRF pubkeys))
// Assumes helpers/constants exist: hex_to_byte_array, sha256EL, send_http_request, parse_json_data,
//   crypto_vrf_OUTPUTBYTES, crypto_vrf_PUBLICKEYBYTES, SHA256_EL_HASH_SIZE (== 32), VRF_PUBLIC_KEY_LENGTH (== 64),
//   BLOCK_VERIFIERS_AMOUNT, XCASH_WALLET_IP, XCASH_WALLET_PORT, HTTP_TIMEOUT_SETTINGS, MEDIUM_BUFFER_SIZE, etc.
// Uses current_block_verifiers_list.* already populated.

bool verify_vrf_vote_signature_bound(const char* block_height,
                                     const char* vrf_beta_hex,
                                     const char* vrf_pubkey_hex,
                                     const char* public_wallet_address,
                                     const char* vote_signature) {
  if (!block_height || !vrf_beta_hex || !vrf_pubkey_hex || !public_wallet_address || !vote_signature)
    return false;

  if (strlen(vrf_beta_hex) != crypto_vrf_OUTPUTBYTES * 2 ||
      strlen(vrf_pubkey_hex) != crypto_vrf_PUBLICKEYBYTES * 2) {
    return false;
  }

  // --- Decode beta/pubkey
  uint8_t vrf_beta_bin[crypto_vrf_OUTPUTBYTES] = {0};
  uint8_t vrf_pubkey_bin[crypto_vrf_PUBLICKEYBYTES] = {0};
  if (!hex_to_byte_array(vrf_beta_hex, vrf_beta_bin, sizeof(vrf_beta_bin)) ||
      !hex_to_byte_array(vrf_pubkey_hex, vrf_pubkey_bin, sizeof(vrf_pubkey_bin))) {
    return false;
  }
  // --- Collect round pubkeys from the in-memory list and hash them (order-sensitive: index order)
  uint8_t pks[BLOCK_VERIFIERS_AMOUNT][crypto_vrf_PUBLICKEYBYTES];
  memset(pks, 0, sizeof pks);
  size_t n = 0;

  for (size_t i = 0; i < BLOCK_VERIFIERS_AMOUNT; ++i) {
    const char* hex = current_block_verifiers_list.block_verifiers_public_key[i];
    if (!hex || hex[0] == '\0') continue;

    // Enforce exactly 64 hex chars (32 bytes)
    size_t len = strnlen(hex, VRF_PUBLIC_KEY_LENGTH + 1);
    if (len != VRF_PUBLIC_KEY_LENGTH) {
      ERROR_PRINT("verify_vrf_vote_signature_bound: pubkey[%zu] length %zu (expected %d)",
                  i, len, VRF_PUBLIC_KEY_LENGTH);
      return false;
    }

    if (!hex_to_byte_array(hex, pks[n], crypto_vrf_PUBLICKEYBYTES)) {
      ERROR_PRINT("verify_vrf_vote_signature_bound: pubkey[%zu] invalid hex", i);
      return false;
    }
    n++;
  }

  if (n == 0) {
    ERROR_PRINT("verify_vrf_vote_signature_bound: no round pubkeys collected");
    return false;
  }

  uint8_t round_pk_hash_bin[SHA256_EL_HASH_SIZE] = {0};
  // Hash concatenated pubkeys in index order
  sha256EL((const unsigned char*)pks, n * crypto_vrf_PUBLICKEYBYTES, round_pk_hash_bin);

  // --- Build the hash input we verify against
  // Layout: block_height (ASCII) || vrf_beta (32) || vrf_pubkey (32) || round_pk_hash (32)
  uint8_t hash_input[256];  // generous
  size_t offset = 0;

  const size_t block_height_len = strlen(block_height);
  if (block_height_len + crypto_vrf_OUTPUTBYTES + crypto_vrf_PUBLICKEYBYTES + SHA256_EL_HASH_SIZE > sizeof(hash_input))
    return false;

  memcpy(hash_input + offset, block_height, block_height_len);
  offset += block_height_len;
  memcpy(hash_input + offset, vrf_beta_bin, crypto_vrf_OUTPUTBYTES);
  offset += crypto_vrf_OUTPUTBYTES;
  memcpy(hash_input + offset, vrf_pubkey_bin, crypto_vrf_PUBLICKEYBYTES);
  offset += crypto_vrf_PUBLICKEYBYTES;
  memcpy(hash_input + offset, round_pk_hash_bin, SHA256_EL_HASH_SIZE);
  offset += SHA256_EL_HASH_SIZE;

  // sha256EL output (32 bytes)
  uint8_t hash[SHA256_EL_HASH_SIZE];
  sha256EL(hash_input, offset, hash);

  // Hex-encode the digest for wallet RPC
  char hash_hex[(SHA256_EL_HASH_SIZE * 2) + 1] = {0};
  for (size_t i = 0; i < SHA256_EL_HASH_SIZE; ++i)
    snprintf(hash_hex + (i * 2), 3, "%02x", hash[i]);

  // --- Call wallet verify
  const char* HTTP_HEADERS[] = {"Content-Type: application/json", "Accept: application/json"};
  const size_t HTTP_HEADERS_LENGTH = sizeof(HTTP_HEADERS) / sizeof(HTTP_HEADERS[0]);

  char request[MEDIUM_BUFFER_SIZE * 2] = {0};
  char response[MEDIUM_BUFFER_SIZE] = {0};

  snprintf(request, sizeof(request),
           "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"verify\",\"params\":{"
           "\"data\":\"%s\",\"address\":\"%s\",\"signature\":\"%s\"}}",
           hash_hex, public_wallet_address, vote_signature);

  if (send_http_request(response, sizeof(response),
                        XCASH_WALLET_IP, "/json_rpc", XCASH_WALLET_PORT,
                        "POST", HTTP_HEADERS, HTTP_HEADERS_LENGTH,
                        request, HTTP_TIMEOUT_SETTINGS) <= 0) {
    ERROR_PRINT("verify_vrf_vote_signature_bound: HTTP request failed");
    return false;
  }

  char result[8] = {0};
  if (parse_json_data(response, "result.good", result, sizeof(result)) == 1 &&
      strcmp(result, "true") == 0) {
    return true;
  }

  return false;
}