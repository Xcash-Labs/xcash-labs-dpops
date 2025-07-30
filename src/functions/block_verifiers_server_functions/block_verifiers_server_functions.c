#include "block_verifiers_server_functions.h"

/*---------------------------------------------------------------------------------------------------------
Name: server_receive_data_socket_block_verifiers_to_block_verifiers_vrf_data
Description: Runs the code when the server receives the BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_VRF_DATA message
Parameters:
  MESSAGE - The message
---------------------------------------------------------------------------------------------------------*/
/*
void server_receive_data_socket_block_verifiers_to_block_verifiers_vrf_data__OLD__(const char* MESSAGE)
{
  char public_address[XCASH_WALLET_LENGTH + 1] = {0};
  char vrf_public_key_data[VRF_PUBLIC_KEY_LENGTH + 1] = {0};
  char vrf_proof_hex[VRF_PROOF_LENGTH + 1] = {0};  
  char vrf_beta_hex[VRF_BETA_LENGTH + 1] = {0};
  char random_buf_hex[(VRF_RANDOMBYTES_LENGTH * 2) + 1] = {0};
  char block_height[BLOCK_HEIGHT_LENGTH] = {0};
  int count;

  DEBUG_PRINT("received %s, %s", __func__, MESSAGE);


  int wait_seconds = 0;
  while (atomic_load(&wait_for_vrf_init) && wait_seconds < DELAY_EARLY_TRANSACTIONS_MAX) {
    sleep(1);
    wait_seconds++;
  }
  if (atomic_load(&wait_for_vrf_init)) {
    ERROR_PRINT("Timed out waiting for vrf init in server_receive_data_socket_block_verifiers_to_block_verifiers_vrf_data");
  }

  // parse the message
  if (parse_json_data(MESSAGE, "public_address", public_address, sizeof(public_address)) == XCASH_ERROR || 
    parse_json_data(MESSAGE, "vrf_public_key", vrf_public_key_data, sizeof(vrf_public_key_data)) == XCASH_ERROR ||
    parse_json_data(MESSAGE, "random_data", random_buf_hex, sizeof(random_buf_hex)) == XCASH_ERROR ||
    parse_json_data(MESSAGE, "vrf_proof", vrf_proof_hex, sizeof(vrf_proof_hex)) == XCASH_ERROR ||
    parse_json_data(MESSAGE, "vrf_beta", vrf_beta_hex, sizeof(vrf_beta_hex)) == XCASH_ERROR ||
    parse_json_data(MESSAGE, "block-height", block_height, sizeof(block_height)) == XCASH_ERROR)
  {
    ERROR_PRINT("Could not parse the block_verifiers_to_block_verifiers_vrf_data");
    return;
  }

  if (strcmp(block_height, current_block_height) != 0) {
      ERROR_PRINT("Skipping VRF data: current block_height [%s] does not match expected [%s]", current_block_height, block_height);
      return; 
  }

  pthread_mutex_lock(&majority_vrf_lock);
  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
    if (strncmp(current_block_verifiers_list.block_verifiers_public_address[count], public_address, XCASH_WALLET_LENGTH) == 0 &&
        strncmp(current_block_verifiers_list.block_verifiers_vrf_public_key_hex[count], "", 1) == 0 &&
        strncmp(current_block_verifiers_list.block_verifiers_random_hex[count], "", 1) == 0 &&
        strncmp(current_block_verifiers_list.block_verifiers_vrf_proof_hex[count], "", 1) == 0 &&
        strncmp(current_block_verifiers_list.block_verifiers_vrf_beta_hex[count], "", 1) == 0) {

      unsigned char random_buf_bin[VRF_RANDOMBYTES_LENGTH] = {0};
      unsigned char alpha_input_bin[VRF_RANDOMBYTES_LENGTH * 2] = {0};
      unsigned char pk_bin[crypto_vrf_PUBLICKEYBYTES] = {0};
      unsigned char vrf_proof[crypto_vrf_PROOFBYTES] = {0};
      unsigned char vrf_beta[crypto_vrf_OUTPUTBYTES] = {0};
      unsigned char previous_block_hash_bin[BLOCK_HASH_LENGTH / 2] = {0};

      if (!hex_to_byte_array(vrf_public_key_data, pk_bin, sizeof(pk_bin)) ||

        !hex_to_byte_array(vrf_proof_hex, vrf_proof, sizeof(vrf_proof)) ||
        !hex_to_byte_array(vrf_beta_hex, vrf_beta, sizeof(vrf_beta)) ||
        !hex_to_byte_array(random_buf_hex, random_buf_bin, sizeof(random_buf_bin)) ||
        !hex_to_byte_array(previous_block_hash, previous_block_hash_bin, sizeof(previous_block_hash_bin))) {

          ERROR_PRINT("Failed to decode one or more fields in VRF message from %s", public_address);
          break;
      }

      // Form alpha input = previous_block_hash || random_buf
      memcpy(alpha_input_bin, previous_block_hash_bin, VRF_RANDOMBYTES_LENGTH);
      memcpy(alpha_input_bin + VRF_RANDOMBYTES_LENGTH, random_buf_bin, VRF_RANDOMBYTES_LENGTH);

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

      memcpy(current_block_verifiers_list.block_verifiers_vrf_public_key_hex[count], vrf_public_key_data, VRF_PUBLIC_KEY_LENGTH+1);
      memcpy(current_block_verifiers_list.block_verifiers_random_hex[count], random_buf_hex, VRF_RANDOMBYTES_LENGTH * 2 + 1);
      memcpy(current_block_verifiers_list.block_verifiers_vrf_proof_hex[count], vrf_proof_hex, VRF_PROOF_LENGTH + 1); 
      memcpy(current_block_verifiers_list.block_verifiers_vrf_beta_hex[count], vrf_beta_hex, VRF_BETA_LENGTH + 1);

      break;
    }
  }
  pthread_mutex_unlock(&majority_vrf_lock);

  return;
}
*/

void server_receive_data_socket_block_verifiers_to_block_verifiers_vrf_data(const char* MESSAGE)
{
  char public_address[XCASH_WALLET_LENGTH + 1] = {0};
  char vrf_public_key_data[VRF_PUBLIC_KEY_LENGTH + 1] = {0};
  char vrf_proof_hex[VRF_PROOF_LENGTH + 1] = {0};  
  char vrf_beta_hex[VRF_BETA_LENGTH + 1] = {0};
  char block_height[BLOCK_HEIGHT_LENGTH] = {0};
  int count;

  DEBUG_PRINT("received %s, %s", __func__, MESSAGE);

  int wait_seconds = 0;
  while (atomic_load(&wait_for_vrf_init) && wait_seconds < DELAY_EARLY_TRANSACTIONS_MAX) {
    sleep(1);
    wait_seconds++;
  }
  if (atomic_load(&wait_for_vrf_init)) {
    ERROR_PRINT("Timed out waiting for vrf init in server_receive_data_socket_block_verifiers_to_block_verifiers_vrf_data");
  }

  // parse the message
  if (parse_json_data(MESSAGE, "public_address", public_address, sizeof(public_address)) == XCASH_ERROR || 
    parse_json_data(MESSAGE, "vrf_public_key", vrf_public_key_data, sizeof(vrf_public_key_data)) == XCASH_ERROR ||
    parse_json_data(MESSAGE, "vrf_proof", vrf_proof_hex, sizeof(vrf_proof_hex)) == XCASH_ERROR ||
    parse_json_data(MESSAGE, "vrf_beta", vrf_beta_hex, sizeof(vrf_beta_hex)) == XCASH_ERROR ||
    parse_json_data(MESSAGE, "block-height", block_height, sizeof(block_height)) == XCASH_ERROR)
  {
    ERROR_PRINT("Could not parse the block_verifiers_to_block_verifiers_vrf_data");
    return;
  }

  if (strcmp(block_height, current_block_height) != 0) {
      ERROR_PRINT("Skipping VRF data: current block_height [%s] does not match expected [%s]", current_block_height, block_height);
      return; 
  }

  pthread_mutex_lock(&majority_vrf_lock);
  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
    if (strncmp(current_block_verifiers_list.block_verifiers_public_address[count], public_address, XCASH_WALLET_LENGTH) == 0 &&
        strncmp(current_block_verifiers_list.block_verifiers_vrf_public_key_hex[count], "", 1) == 0 &&
        strncmp(current_block_verifiers_list.block_verifiers_vrf_proof_hex[count], "", 1) == 0 &&
        strncmp(current_block_verifiers_list.block_verifiers_vrf_beta_hex[count], "", 1) == 0) {

      unsigned char alpha_input_bin[40] = {0};
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

      // Form alpha input = previous_block_hash || block_height
      if (!hex_to_byte_array(previous_block_hash, previous_block_hash_bin, 32)) {
        ERROR_PRINT("Failed to decode previous block hash");
        break;
      }
      memcpy(alpha_input_bin, previous_block_hash_bin, 32);

      // Convert current_block_height (char*) to binary
      uint64_t block_height_num = strtoull(current_block_height, NULL, 10);
      uint64_t height_le = htole64(block_height_num);
      memcpy(alpha_input_bin + 32, &height_le, sizeof(height_le));

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

      memcpy(current_block_verifiers_list.block_verifiers_vrf_public_key_hex[count], vrf_public_key_data, VRF_PUBLIC_KEY_LENGTH+1);
      memcpy(current_block_verifiers_list.block_verifiers_vrf_proof_hex[count], vrf_proof_hex, VRF_PROOF_LENGTH + 1); 
      memcpy(current_block_verifiers_list.block_verifiers_vrf_beta_hex[count], vrf_beta_hex, VRF_BETA_LENGTH + 1);

      break;
    }
  }
  pthread_mutex_unlock(&majority_vrf_lock);

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
        pthread_mutex_lock(&majority_vote_lock);
        current_block_verifiers_list.block_verifiers_voted[i] = 1;
        memcpy(current_block_verifiers_list.block_verifiers_vote_signature[i], vote_signature, XCASH_SIGN_DATA_LENGTH+1);
        memcpy(current_block_verifiers_list.block_verifiers_selected_public_address[i], public_address_producer, XCASH_WALLET_LENGTH+1);
        pthread_mutex_unlock(&majority_vote_lock);
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

    if (strcmp(vrf_public_key_data, current_block_verifiers_list.block_verifiers_vrf_public_key_hex[i]) != 0) {
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

    pthread_mutex_lock(&majority_vote_lock);
    current_block_verifiers_list.block_verifiers_vote_total[i] += 1;
    pthread_mutex_unlock(&majority_vote_lock);
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