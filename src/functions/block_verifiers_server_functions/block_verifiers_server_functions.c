#include "block_verifiers_server_functions.h"

void server_receive_data_socket_block_verifiers_to_block_verifiers_vrf_data(const char* MESSAGE)
{
  char public_address[XCASH_WALLET_LENGTH + 1] = {0};
  char vrf_public_key_data[VRF_PUBLIC_KEY_LENGTH + 1] = {0};
  char vrf_proof_hex[VRF_PROOF_LENGTH + 1] = {0};  
  char vrf_beta_hex[VRF_BETA_LENGTH + 1] = {0};
  char block_height[BLOCK_HEIGHT_LENGTH] = {0};
  char parsed_delegates_hash[MD5_HASH_SIZE + 1] = {0};

  DEBUG_PRINT("received %s, %s", __func__, MESSAGE);

  // parse the message
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
    DEBUG_PRINT("Invalid or missing delegate address: '%s'", public_address);
    return;
  }

  DEBUG_PRINT("Parsed remote public_address: %s, block_height: %s, delegates_hash: %s", public_address, block_height, 
    parsed_delegates_hash);

  int wait_seconds = 0;
  while (atomic_load(&wait_for_block_height_init) && wait_seconds < DELAY_EARLY_TRANSACTIONS_MAX) {
    sleep(1);
    wait_seconds++;
  }

  if (atomic_load(&wait_for_block_height_init)) {
    ERROR_PRINT("Timed out waiting for current_block_height in server_receive_data_socket_block_verifiers_to_block_verifiers_vrf_data");
  }

  pthread_mutex_lock(&delegates_all_lock);
  bool found = false;

  for (size_t i = 0; i < BLOCK_VERIFIERS_TOTAL_AMOUNT; i++) {

    if (strncmp(delegates_all[i].public_address, public_address, XCASH_WALLET_LENGTH) == 0 &&
        delegates_all[i].verifiers_vrf_proof_hex[0] == '\0' &&
        delegates_all[i].verifiers_vrf_beta_hex[0] == '\0') {

      found = true;
      if (strcmp(block_height, current_block_height) != 0) {
        DEBUG_PRINT("Block height mismatch for %s: remote=%s, local=%s",
                    public_address, block_height, current_block_height);
        break;
      }

      // Compare delegate list hash
      if (strcmp(parsed_delegates_hash, delegates_hash) != 0) {
        DEBUG_PRINT("Delegates hash mismatch for %s: remote=%s, local=%s",
                    public_address, parsed_delegates_hash, delegates_hash);
        delegate_db_hash_mismatch = delegate_db_hash_mismatch + 1;
        // Online and a partial match
        strncpy(delegates_all[i].online_status, "partial", sizeof(delegates_all[i].online_status));
        delegates_all[i].online_status[sizeof(delegates_all[i].online_status) - 1] = '\0';
        break;
      }

      // All checks passed — mark online
      strncpy(delegates_all[i].online_status, "true", sizeof(delegates_all[i].online_status));
      delegates_all[i].online_status[sizeof(delegates_all[i].online_status) - 1] = '\0';
      INFO_PRINT("Marked delegate %s as online (ck)", public_address);

      unsigned char alpha_input_bin[72] = {0};
      unsigned char pk_bin[crypto_vrf_PUBLICKEYBYTES] = {0};
      unsigned char vrf_proof[crypto_vrf_PROOFBYTES] = {0};
      unsigned char vrf_beta[crypto_vrf_OUTPUTBYTES] = {0};
      unsigned char previous_block_hash_bin[BLOCK_HASH_LENGTH / 2] = {0};

      if (!hex_to_byte_array(vrf_public_key_data, pk_bin, sizeof(pk_bin)) ||
        !hex_to_byte_array(vrf_proof_hex, vrf_proof, sizeof(vrf_proof)) ||
        !hex_to_byte_array(vrf_beta_hex, vrf_beta, sizeof(vrf_beta)) ||
        !hex_to_byte_array(previous_block_hash, previous_block_hash_bin, sizeof(previous_block_hash_bin))) {
          ERROR_PRINT("Failed to decode one or more fields in VRF message from %s", public_address);
          break;
      }

      memcpy(alpha_input_bin, previous_block_hash_bin, 32);

      // Convert current_block_height (char*) to binary
      uint64_t block_height_num = strtoull(current_block_height, NULL, 10);
      uint64_t height_le = htole64(block_height_num);
      memcpy(alpha_input_bin + 32, &height_le, sizeof(height_le));

      // Add vrf_block_producer
      memcpy(alpha_input_bin + 40, pk_bin, 32);  // Write at offset 40

      // Verify VRF proof
      unsigned char computed_beta[crypto_vrf_OUTPUTBYTES];
      if (crypto_vrf_verify(computed_beta, pk_bin, vrf_proof, alpha_input_bin, sizeof(alpha_input_bin)) != 0) {
        ERROR_PRINT("VRF proof failed verification from %s", public_address);
        break;
      }

      if (memcmp(computed_beta, vrf_beta, sizeof(vrf_beta)) != 0) {
        WARNING_PRINT("VRF beta mismatch from %s", public_address);
        break;
      }

      memcpy(delegates_all[i].verifiers_vrf_proof_hex, vrf_proof_hex, VRF_PROOF_LENGTH + 1); 
      memcpy(delegates_all[i].verifiers_vrf_beta_hex, vrf_beta_hex, VRF_BETA_LENGTH + 1);

      break;

    }
  }
  pthread_mutex_unlock(&delegates_all_lock);
  if (!found) {
    INFO_PRINT("Delegate not found in delegates_all: %s", public_address);
  }

  return;
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
  char block_height[BLOCK_HEIGHT_LENGTH] = {0};
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

  if (!verify_vrf_vote_signature(block_height, vrf_beta_hex, vrf_public_key_data, public_address, vote_signature)) {
    ERROR_PRINT("Unable to verigy the signature for the vote %s", public_address);
    return;
  }

  for (size_t i = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
    if (strcmp(public_address, current_block_verifiers_list.block_verifiers_public_address[i]) == 0) {
      if (current_block_verifiers_list.block_verifiers_voted[i] == 0) {
        pthread_mutex_lock(&current_block_verifiers_lock);
        current_block_verifiers_list.block_verifiers_voted[i] = 1;
        memcpy(current_block_verifiers_list.block_verifiers_vote_signature[i], vote_signature, XCASH_SIGN_DATA_LENGTH+1);
        memcpy(current_block_verifiers_list.block_verifiers_selected_public_address[i], public_address_producer, XCASH_WALLET_LENGTH+1);
        pthread_mutex_unlock(&current_block_verifiers_lock);
      } else {
        WARNING_PRINT("Verifier %s, has already voted and can not vote again", public_address);
        return;
      }
    }
  }

  if (strcmp(block_height, current_block_height) != 0) {
    ERROR_PRINT("Mismatch in block height for verifier %s", public_address);
    return;
  }

  for (size_t i = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
    if (strcmp(public_address_producer, current_block_verifiers_list.block_verifiers_public_address[i]) != 0) {
      continue;
    }

    if (strcmp(vrf_public_key_data, current_block_verifiers_list.block_verifiers_public_key[i]) != 0) {
      ERROR_PRINT("Mismatch in vrf_public_key for verifier %s", public_address_producer);
      return;
    }

    if (strcmp(vrf_proof_hex, current_block_verifiers_list.block_verifiers_vrf_proof_hex[i]) != 0) {
      ERROR_PRINT("Mismatch in vrf_proof for verifier %s", public_address_producer);
      return;
    }

    if (strcmp(vrf_beta_hex, current_block_verifiers_list.block_verifiers_vrf_beta_hex[i]) != 0) {
      ERROR_PRINT("Mismatch in vrf_beta for verifier %s", public_address_producer);
      return;
    }

    pthread_mutex_lock(&current_block_verifiers_lock);
    current_block_verifiers_list.block_verifiers_vote_total[i] += 1;
    pthread_mutex_unlock(&current_block_verifiers_lock);
    return;
  }

  return;
}

/*---------------------------------------------------------------------------------------------------------
 * @brief Verifies a delegate's vote for the block producer based on VRF output and signature.
 *
 * This function reconstructs the vote hash from the following inputs:
 *   - block_height (as ASCII string, e.g., "5")
 *   - vrf_beta (hex-encoded 32-byte VRF output)
 *   - vrf_public_key (hex-encoded 32-byte VRF public key)
 *
 * It hashes: block_height || vrf_beta || vrf_public_key
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
  uint8_t hash_input[128];
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

  memcpy(hash_input + offset, block_height, block_height_len); offset += block_height_len;
  memcpy(hash_input + offset, vrf_beta_bin, crypto_vrf_OUTPUTBYTES); offset += crypto_vrf_OUTPUTBYTES;
  memcpy(hash_input + offset, vrf_pubkey_bin, crypto_vrf_PUBLICKEYBYTES); offset += crypto_vrf_PUBLICKEYBYTES;
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
                        request, SEND_OR_RECEIVE_SOCKET_DATA_TIMEOUT_SETTINGS) <= 0) {
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