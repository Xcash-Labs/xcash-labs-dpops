#include "block_verifiers_functions.h"

size_t write_varint(uint8_t* out, size_t value) {
  size_t i = 0;
  while (value >= 0x80) {
    out[i++] = (value & 0x7F) | 0x80;
    value >>= 7;
  }
  out[i++] = value;
  return i;
}

/*---------------------------------------------------------------------------------------------------------
 * @brief Injects VRF-related data into the reserved section of a Monero-style blocktemplate blob
 *        and signs the original block blob using the producer's private key.
 *
 * This function performs the following steps:
 * 1. Converts the input hex-encoded `block_blob_hex` into binary.
 * 2. Constructs a 208-byte VRF blob
 * 3. Writes the VRF blob into `block_blob_bin` at the reserved offset, with a custom tag (0x07)
 *    and length prefix (1-byte varint).
 * 4. Converts the modified binary blob back into hex and stores it in `block_blob_hex`.
 *
 * vrf_blob Layout (274 Bytes)
 *  Field	      Bytes   Description
 *  vrf_proof	  80	    Hex-decoded 80-byte VRF proof (e.g. from libsodium)
 *  vrf_beta	  64	    Hex-decoded 64-byte beta (VRF hash output)
 *  vrf_pubkey  32	    Hex-decoded 32-byte VRF public key
 *  total_votes  1      Hex-decoded 1-byte vote total
 *  winning_vote 1      Hex-decoded 1-byte vote count for winner
 *  vote_hash	  32      Hex-decode 32-byte hash of all votes
 *
 * @param block_blob_hex The input and output hex-encoded blocktemplate blob.
 *                       Must contain reserved space as defined by get_block_template (e.g. 220 bytes).
 * @return true on success, false if any step fails (conversion, signing, or overflow).
 *
 * @note This function expects `producer_refs[0]` to be populated with all required hex strings.
 * @note Ensure the get_block_template reserve_size is at least 210–220 bytes to fit the full VRF blob.
 * @note The signature is calculated on the original (unpatched) block_blob_hex for consensus correctness.
---------------------------------------------------------------------------------------------------------*/
bool add_vrf_extra_and_sign(char* block_blob_hex, const char* vote_hash_hex, size_t reserved_offset, uint8_t total_vote, uint8_t winning_vote) {
  INFO_PRINT("Final vote hash 2: %s", vote_hash_hex);
  INFO_PRINT("total_vote: %u | winning_vote: %u", total_vote, winning_vote);

  unsigned char* block_blob_bin = calloc(1, BUFFER_SIZE);
  if (!block_blob_bin) {
    ERROR_PRINT("Memory allocation failed for block_blob_bin");
    return false;
  }

  size_t blob_len = strlen(block_blob_hex) / 2;
  if (!hex_to_byte_array(block_blob_hex, block_blob_bin, blob_len)) {
    ERROR_PRINT("Failed to convert block_blob_hex to binary");
    free(block_blob_bin);
    return false;
  }

  // Backoff 2 to overwrite the preset 0x02 trans (TX_EXTRA_NONCE) and length.  Update with new 07 trans (TX_EXTRA_VRF_SIGNATURE_TAG).
  size_t pos = reserved_offset - 2;

  // Construct the VRF blob
  uint8_t vrf_blob[VRF_BLOB_TOTAL_SIZE] = {0};
  size_t vrf_pos = 0;

  if (!hex_to_byte_array(producer_refs[0].vrf_proof_hex, vrf_blob + vrf_pos, VRF_PROOF_LENGTH / 2)) {
    ERROR_PRINT("Failed to decode VRF proof hex");
    free(block_blob_bin);
    return false;
  }
  vrf_pos += (VRF_PROOF_LENGTH / 2);

  if (!hex_to_byte_array(producer_refs[0].vrf_beta_hex, vrf_blob + vrf_pos, VRF_BETA_LENGTH / 2)) {
    ERROR_PRINT("Failed to decode VRF beta hex");
    free(block_blob_bin);
    return false;
  }
  vrf_pos += VRF_BETA_LENGTH / 2;

  if (!hex_to_byte_array(producer_refs[0].vrf_public_key, vrf_blob + vrf_pos, VRF_PUBLIC_KEY_LENGTH / 2)) {
    ERROR_PRINT("Failed to decode VRF public key hex");
    free(block_blob_bin);
    return false;
  }
  vrf_pos += VRF_PUBLIC_KEY_LENGTH / 2;

  // Add total_votes
  vrf_blob[vrf_pos++] = total_vote;

  // Add winning_vote
  vrf_blob[vrf_pos++] = winning_vote;

  // Add vote_hash (32-byte hex → 16-byte binary)
  if (!hex_to_byte_array(vote_hash_hex, vrf_blob + vrf_pos, VRF_PUBLIC_KEY_LENGTH / 2)) {
    ERROR_PRINT("Failed to decode vote hash hex");
    free(block_blob_bin);
    return false;
  }
  vrf_pos += 32;






  /*

  // Sign the original block blob (before patching)
  char blob_signature[XCASH_SIGN_DATA_LENGTH + 1] = {0};
  if (!sign_block_blob(block_blob_hex, blob_signature, sizeof(blob_signature))) {
    ERROR_PRINT("Failed to sign block blob");
    free(block_blob_bin);
    return false;
  }
  DEBUG_PRINT("Block Blob Signature: %s", blob_signature);

  const char* base64_part = blob_signature + 5;  // skip "SigV2"
  uint8_t sig_bytes[64] = {0};
  size_t sig_len = 0;

  if (!base64_decode(base64_part, sig_bytes, sizeof(sig_bytes), &sig_len)) {
    ERROR_PRINT("Base64 decode failed");
    free(block_blob_bin);
    return false;
  }

  if (sig_len != 64) {
    ERROR_PRINT("Decoded signature must be exactly 64 bytes");
    free(block_blob_bin);
    return false;
  }

  memcpy(vrf_blob + vrf_pos, sig_bytes, 64);
  vrf_pos += 64;
  DEBUG_PRINT("VRF proof decoded, vrf_pos now at: %zu", vrf_pos);

  */


  if (vrf_pos != VRF_BLOB_TOTAL_SIZE) {
    ERROR_PRINT("VRF blob constructed with incorrect size: %zu bytes", vrf_pos);
    free(block_blob_bin);
    return false;
  }

  block_blob_bin[pos++] = TX_EXTRA_VRF_SIGNATURE_TAG;
  size_t varint_len = write_varint(block_blob_bin + pos, VRF_BLOB_TOTAL_SIZE);
  pos += varint_len;
  memcpy(block_blob_bin + pos, vrf_blob, VRF_BLOB_TOTAL_SIZE);
  pos += VRF_BLOB_TOTAL_SIZE;

  if ((pos - reserved_offset) > BLOCK_RESERVED_SIZE) {
    ERROR_PRINT("VRF data exceeds reserved space: used %zu bytes, allowed %d", pos - reserved_offset, BLOCK_RESERVED_SIZE);
    free(block_blob_bin);
    return false;
  }

  bytes_to_hex(block_blob_bin, blob_len, block_blob_hex, BUFFER_SIZE);

  if (strlen(block_blob_hex) != blob_len * 2) {
    ERROR_PRINT("Hex conversion mismatch: expected %zu, got %zu", blob_len * 2, strlen(block_blob_hex));
    free(block_blob_bin);
    return false;
  }

  INFO_PRINT("Final block_blob_hex (length: %zu):", strlen(block_blob_hex));
  INFO_PRINT("%s", block_blob_hex);

  free(block_blob_bin);
  return true;
}

/*---------------------------------------------------------------------------------------------------------
Name: block_verifiers_create_block
Description: Runs the round where the block verifiers will create the block
Return: 0 if an error has occured, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int block_verifiers_create_block(const char* vote_hash_hex, uint8_t total_vote, uint8_t winning_vote) {
  char data[BUFFER_SIZE] = {0};

  // Confirm block height hasn't drifted (this node may be behind the network)
  INFO_STAGE_PRINT("Part 8 - Confirm block height hasn't drifted");
  snprintf(current_round_part, sizeof(current_round_part), "%d", 8);
  if (get_current_block_height(data) == 1 && strncmp(current_block_height, data, BUFFER_SIZE) != 0) {
    WARNING_PRINT("Your block height is not synced correctly, waiting for next round");
    return ROUND_ERROR;
  }

  char block_blob[BUFFER_SIZE] = {0};
  size_t reserved_offset = 0;
  // Only the block producer completes the following steps, producer_refs is an array in case we decide to add
  // backup producers in the future
  INFO_PRINT("Parts 9 thru 11 are only perfomed by the block producer");
  if (strcmp(producer_refs[0].public_address, xcash_wallet_public_address) == XCASH_ERROR) {
    // Create block template
    INFO_STAGE_PRINT("Part 9 - Create block template");
    snprintf(current_round_part, sizeof(current_round_part), "%d", 9);
    if (get_block_template(block_blob, BUFFER_SIZE, &reserved_offset) == XCASH_ERROR) {
      return ROUND_ERROR;
    }

    if (strncmp(block_blob, "", 1) == 0) {
      WARNING_PRINT("Did not receive block template");
      return ROUND_ERROR;
    }

    // Create block template
    INFO_STAGE_PRINT("Part 10 - Add VRF Data and Sign Block Blob");
    snprintf(current_round_part, sizeof(current_round_part), "%d", 10);
    if (!add_vrf_extra_and_sign(block_blob, vote_hash_hex, reserved_offset, total_vote, winning_vote)) {
      return ROUND_ERROR;
    }

    // Part 11 - Submit block
    INFO_STAGE_PRINT("Part 11 - Submit the Block");
    snprintf(current_round_part, sizeof(current_round_part), "%d", 11);
    if (!submit_block_template(block_blob)) {
      return ROUND_ERROR;
    }

    INFO_PRINT_STATUS_OK("Block signature sent ");
  }

  return ROUND_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: sync_block_verifiers_minutes_and_seconds
Description: Syncs the block verifiers to a specific minute and second
Parameters:
  minutes - The minutes
  seconds - The seconds
---------------------------------------------------------------------------------------------------------*/
int sync_block_verifiers_minutes_and_seconds(const int MINUTES, const int SECONDS) {
  if (MINUTES >= BLOCK_TIME || SECONDS >= 60) {
    ERROR_PRINT("Invalid sync time: MINUTES must be < BLOCK_TIME and SECONDS < 60");
    return XCASH_ERROR;
  }

  struct timespec now_ts;
  if (clock_gettime(CLOCK_REALTIME, &now_ts) != 0) {
    ERROR_PRINT("Failed to get high-resolution time");
    return XCASH_ERROR;
  }

  time_t now_sec = now_ts.tv_sec;
  long now_nsec = now_ts.tv_nsec;

  size_t seconds_per_block = BLOCK_TIME * 60;
  size_t seconds_within_block = now_sec % seconds_per_block;
  double target_seconds = (double)(MINUTES * 60 + SECONDS);
  double current_time_in_block = (double)seconds_within_block + (now_nsec / 1e9);
  double sleep_seconds = target_seconds - current_time_in_block;

  if (sleep_seconds <= 0) {
    WARNING_PRINT("Missed sync point by %.3f seconds", -sleep_seconds);
    return XCASH_ERROR;
  }

  struct timespec req = {
      .tv_sec = (time_t)sleep_seconds,
      .tv_nsec = (long)((sleep_seconds - (time_t)sleep_seconds) * 1e9)};

  INFO_PRINT("Sleeping for %.3f seconds to sync to target time...", sleep_seconds);
  if (nanosleep(&req, NULL) != 0) {
    ERROR_PRINT("nanosleep interrupted: %s", strerror(errno));
    return XCASH_ERROR;
  }

  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: block_verifiers_create_VRF_secret_key_and_VRF_public_key
Description:
  Generates a new VRF key pair (public and secret key) and a random alpha string to be used for verifiable
  randomness in the block producer selection process. The keys and random string are stored in the
  appropriate VRF_data structure fields and associated with the current node (block verifier).

  The function also prepares a JSON message that includes:
    - The public address of the sender (this node)
    - The VRF secret key (hex-encoded)
    - The VRF public key (hex-encoded)
    - The generated random alpha string

  This message is broadcast to other block verifiers to allow them to include this node’s randomness
  contribution in the verifiable selection round.

Parameters:
  message - Output buffer that receives the formatted JSON message to be broadcast to peers.

Return:
  XCASH_OK (1) if the key generation and message formatting succeed.
  XCASH_ERROR (0) if any step fails.
---------------------------------------------------------------------------------------------------------*/
bool generate_and_request_vrf_data_msg(char** message) {



  unsigned char alpha_input_bin[72] = {0};
  unsigned char pk_bin[crypto_vrf_PUBLICKEYBYTES] = {0};
  unsigned char vrf_proof[crypto_vrf_PROOFBYTES] = {0};
  unsigned char vrf_beta[crypto_vrf_OUTPUTBYTES] = {0};
  unsigned char previous_block_hash_bin[BLOCK_HASH_LENGTH / 2] = {0};
  char vrf_proof_hex[VRF_PROOF_LENGTH + 1] = {0};
  char vrf_beta_hex[VRF_BETA_LENGTH + 1] = {0};
  size_t i, offset;

  if (!hex_to_byte_array(vrf_public_key, pk_bin, sizeof(pk_bin))) {
    ERROR_PRINT("Invalid hex format for public key");
    return XCASH_ERROR;
  }

  // Validate the VRF public key
  if (crypto_vrf_is_valid_key(pk_bin) != 1) {
    ERROR_PRINT("Public key failed validation");
    return XCASH_ERROR;
  }

  // Decode 32-byte hash into alpha_input_bin[0..31]
  if (!hex_to_byte_array(previous_block_hash, previous_block_hash_bin, 32)) {
    ERROR_PRINT("Failed to decode previous block hash");
    return XCASH_ERROR;
  }
  memcpy(alpha_input_bin, previous_block_hash_bin, 32);

  // Convert current_block_height (char*) to binary
  uint64_t block_height = strtoull(current_block_height, NULL, 10);
  uint64_t height_le = htole64(block_height);
  memcpy(alpha_input_bin + 32, &height_le, sizeof(height_le));  // Write at offset 32

  // Add vrf_block_producer
  memcpy(alpha_input_bin + 40, pk_bin, 32);  // Write at offset 40

  // Generate VRF proof
  if (crypto_vrf_prove(vrf_proof, secret_key_data, alpha_input_bin, sizeof(alpha_input_bin)) != 0) {
    ERROR_PRINT("Failed to generate VRF proof");
    return XCASH_ERROR;
  }

  // Convert proof to beta (random output)
  if (crypto_vrf_proof_to_hash(vrf_beta, vrf_proof) != 0) {
    ERROR_PRINT("Failed to convert VRF proof to beta");
    return XCASH_ERROR;
  }

  // Convert proof, beta, and random buffer to hex
  for (i = 0, offset = 0; i < crypto_vrf_PROOFBYTES; i++, offset += 2)
    snprintf(vrf_proof_hex + offset, 3, "%02x", vrf_proof[i]);
  for (i = 0, offset = 0; i < crypto_vrf_OUTPUTBYTES; i++, offset += 2)
    snprintf(vrf_beta_hex + offset, 3, "%02x", vrf_beta[i]);
  
  unsigned char computed_beta[crypto_vrf_OUTPUTBYTES];
  if (crypto_vrf_verify(computed_beta, pk_bin, vrf_proof, alpha_input_bin, 72) != 0) {
    DEBUG_PRINT("Failed to verify the VRF proof for this node");
    return XCASH_ERROR;
  } else {
    if (memcmp(computed_beta, vrf_beta, 64) != 0) {
      DEBUG_PRINT("Failed to match the computed VRF beta for this node");
      return XCASH_ERROR;
    }
  }

  // Save current block_verifiers data into structure if it is one of the top 50
  pthread_mutex_lock(&majority_vrf_lock);
  for (i = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
    if (strncmp(current_block_verifiers_list.block_verifiers_public_address[i], xcash_wallet_public_address, XCASH_WALLET_LENGTH) == 0) {
      memcpy(current_block_verifiers_list.block_verifiers_public_address[i], xcash_wallet_public_address, XCASH_WALLET_LENGTH + 1);
      memcpy(current_block_verifiers_list.block_verifiers_vrf_public_key_hex[i], vrf_public_key, VRF_PUBLIC_KEY_LENGTH + 1);
      memcpy(current_block_verifiers_list.block_verifiers_vrf_proof_hex[i], vrf_proof_hex, VRF_PROOF_LENGTH + 1);
      memcpy(current_block_verifiers_list.block_verifiers_vrf_beta_hex[i], vrf_beta_hex, VRF_BETA_LENGTH + 1);
      current_block_verifiers_list.block_verifiers_vote_total[i] = 0;
      current_block_verifiers_list.block_verifiers_voted[i] = 0;
      break;
    }
  }
  pthread_mutex_unlock(&majority_vrf_lock);

  // Compose outbound message (JSON)
  *message = create_message_param(
      XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_VRF_DATA,
      "public_address", xcash_wallet_public_address,
      "vrf_public_key", vrf_public_key,
      "vrf_proof", vrf_proof_hex,
      "vrf_beta", vrf_beta_hex,
      "block-height", current_block_height,
      NULL);

  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
 * @brief Creates a JSON-formatted synchronization message containing the current node's
 *        block height, public address, and delegates table hash.
 *
 * This message is used during the network sync process, typically when a node wants to
 * compare its blockchain state and delegates table with others in the network.
 *
 * @param[out] message A pointer to a dynamically allocated JSON string. The caller is
 *                     responsible for freeing the memory.
 *
 * @return true (XCASH_OK) on success, or false (XCASH_ERROR) if memory allocation fails.
 *
---------------------------------------------------------------------------------------------------------*/
bool create_sync_msg(char** message) {
  // Only three key-value pairs + NULL terminator
  const int PARAM_COUNT = 4;
  const char** param_list = calloc(PARAM_COUNT * 2, sizeof(char*));  // key-value pairs

  if (!param_list) {
    ERROR_PRINT("Memory allocation failed for param_list");
    return XCASH_ERROR;
  }

  int param_index = 0;
  param_list[param_index++] = "block_height";
  param_list[param_index++] = current_block_height;

  param_list[param_index++] = "public_address";
  param_list[param_index++] = xcash_wallet_public_address;

  param_list[param_index++] = "delegates_hash";
  param_list[param_index++] = delegates_hash;

  param_list[param_index] = NULL;  // NULL terminate

  // Create the message
  *message = create_message_param_list(XMSG_XCASH_GET_SYNC_INFO, param_list);

  free(param_list);  // Clean up the key-value list

  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: block_verifiers_create_vote_majority_results
Description: The block verifiers will create the vote majority results
Parameters:
  result - The result
  SETTINGS - The data settings
---------------------------------------------------------------------------------------------------------*/
bool block_verifiers_create_vote_majority_result(char** message, int producer_indx) {
  const char* HTTP_HEADERS[] = {"Content-Type: application/json", "Accept: application/json"};
  const size_t HTTP_HEADERS_LENGTH = sizeof(HTTP_HEADERS) / sizeof(HTTP_HEADERS[0]);
  unsigned char pk_bin[crypto_vrf_PUBLICKEYBYTES] = {0};
  unsigned char vrf_beta_bin[crypto_vrf_OUTPUTBYTES] = {0};
  uint8_t hash[SHA256_EL_HASH_SIZE];
  char hash_hex[(SHA256_EL_HASH_SIZE * 2) + 1] = {0};
  size_t offset = 0;
  char response[MEDIUM_BUFFER_SIZE] = {0};

  if (!message)
    return false;

  if (strlen(current_block_verifiers_list.block_verifiers_vrf_public_key_hex[producer_indx]) == 0 ||
      strlen(current_block_verifiers_list.block_verifiers_vrf_proof_hex[producer_indx]) == 0 ||
      strlen(current_block_verifiers_list.block_verifiers_vrf_beta_hex[producer_indx]) == 0) {
    ERROR_PRINT("Missing VRF data for producer");
    return false;
  }

  size_t height_len = strlen(current_block_height);
  if (!hex_to_byte_array(current_block_verifiers_list.block_verifiers_vrf_public_key_hex[producer_indx], pk_bin, sizeof(pk_bin))) {
    ERROR_PRINT("Invalid hex format for public key");
    return false;
  }

  if (!hex_to_byte_array(current_block_verifiers_list.block_verifiers_vrf_beta_hex[producer_indx], vrf_beta_bin, sizeof(vrf_beta_bin))) {
    ERROR_PRINT("Invalid hex format for beta");
    return false;
  }

  char* signature = calloc(XCASH_SIGN_DATA_LENGTH+1, sizeof(char));
  char* request = calloc(MEDIUM_BUFFER_SIZE * 2, sizeof(char));
  if (!signature || !request) {
    FATAL_ERROR_EXIT("sign_data: Memory allocation failed");
  }

  unsigned char hash_input[128];  // height_len + 32 + 64
  memcpy(hash_input + offset, current_block_height, height_len);
  offset += height_len;

  memcpy(hash_input + offset, vrf_beta_bin, 64);
  offset += 64;

  memcpy(hash_input + offset, pk_bin, 32);
  offset += 32;

  size_t i = 0;
  sha256EL(hash_input, offset, hash);
  for (i = 0, offset = 0; i < SHA256_EL_HASH_SIZE; i++, offset += 2)
    snprintf(hash_hex + offset, 3, "%02x", hash[i]);

  snprintf(request, MEDIUM_BUFFER_SIZE * 2,
           "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"sign\",\"params\":{\"data\":\"%s\"}}",
           hash_hex);

  // Send signing request to wallet
  if (send_http_request(response, MEDIUM_BUFFER_SIZE, XCASH_WALLET_IP, "/json_rpc", XCASH_WALLET_PORT,
                        "POST", HTTP_HEADERS, HTTP_HEADERS_LENGTH,
                        request, SEND_OR_RECEIVE_SOCKET_DATA_TIMEOUT_SETTINGS) <= 0 ||
      !parse_json_data(response, "result.signature", signature, XCASH_SIGN_DATA_LENGTH+1) ||
      strlen(signature) == 0 ||
      strncmp(signature, XCASH_SIGN_DATA_PREFIX, sizeof(XCASH_SIGN_DATA_PREFIX) - 1) != 0) {
    ERROR_PRINT("Function: block_verifiers_create_vote_majority_result - Wallet signature failed or format invalid");
    free(signature);
    free(request);
    return false;
  }

  // Save current block_verifiers data into structure if it is one of the top 50
  pthread_mutex_lock(&majority_vrf_lock);
  for (i = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
    if (strncmp(current_block_verifiers_list.block_verifiers_public_address[i], xcash_wallet_public_address, XCASH_WALLET_LENGTH) == 0) {
      memcpy(current_block_verifiers_list.block_verifiers_vote_signature[i], signature, XCASH_SIGN_DATA_LENGTH+1);
      break;
    }
  }
  pthread_mutex_unlock(&majority_vrf_lock);

  const char* params[] = {
      "public_address", xcash_wallet_public_address,
      "proposed_producer", current_block_verifiers_list.block_verifiers_public_address[producer_indx],
      "block_height", current_block_height,
      "vrf_beta", current_block_verifiers_list.block_verifiers_vrf_beta_hex[producer_indx],
      "vrf_proof", current_block_verifiers_list.block_verifiers_vrf_proof_hex[producer_indx],
      "vrf_public_key", current_block_verifiers_list.block_verifiers_vrf_public_key_hex[producer_indx],
      "vote_signature", signature,
      NULL};
  *message = create_message_param_list(XMSG_NODES_TO_NODES_VOTE_MAJORITY_RESULTS, params);

  free(signature);
  signature = NULL;
  free(request);
  request = NULL;

  if (*message == NULL) {
    ERROR_PRINT("Function: block_verifiers_create_vote_majority_result - Failed to create message");
    return false;
  }

  return true;
}

/*---------------------------------------------------------------------------------------------------------
Name: create_delegates_db_sync_request
Description:
  Sends a database sync request to another delegate node.
  This is typically used by a block verifier to request an updated copy of the delegates database
  from a peer node (e.g., during startup, resync, or recovery).

Parameters:
  selected_index - Index of the delegate in the global delegates list. Used to resolve the target IP.

Returns:
  true  - if the sync request was sent successfully
  false - if the index is invalid or the message failed to send
---------------------------------------------------------------------------------------------------------*/
bool create_delegates_db_sync_request(int selected_index) {
  if (selected_index < 0 || selected_index >= BLOCK_VERIFIERS_TOTAL_AMOUNT) {
    ERROR_PRINT("Invalid delegate index: %d", selected_index);
    return false;
  }

  const char* params[] = {
      "public_address", xcash_wallet_public_address,
      NULL};

  char* message = NULL;
  message = create_message_param_list(XMSG_NODES_TO_NODES_DATABASE_SYNC_REQ, params);
  const char* ip = delegates_all[selected_index].IP_address;

  if (send_message_to_ip_or_hostname(ip, XCASH_DPOPS_PORT, message) == XCASH_OK) {
    DEBUG_PRINT("Sync request sent to delegate %d (%s)", selected_index, ip);
    return true;
  }

  WARNING_PRINT("Failed to send sync request to delegate %d (%s)", selected_index, ip);
  return false;
}