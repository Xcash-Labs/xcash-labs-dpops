#include "block_verifiers_functions.h"

static size_t count = 0;

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
 * vrf_blob Layout (210 Bytes)
 *  Field	      Bytes   Description
 *  vrf_proof	  80	    Hex-decoded 80-byte VRF proof (e.g. from libsodium)
 *  vrf_beta	  64	    Hex-decoded 64-byte beta (VRF hash output)
 *  vrf_pubkey  32	    Hex-decoded 32-byte VRF public key
 *  total_votes  1      Hex-decoded 1-byte committee  vote total
 *  winning_vote 1      Hex-decoded 1-byte committee vote count for winner
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
  DEBUG_PRINT("Final vote hash 2: %s", vote_hash_hex);
  DEBUG_PRINT("total_vote: %u | winning_vote: %u", total_vote, winning_vote);

  size_t blob_len = strlen(block_blob_hex) / 2;   // bytes after hex→bin
  unsigned char* block_blob_bin = calloc(1, blob_len);

//  unsigned char* block_blob_bin = calloc(1, BUFFER_SIZE);

  if (!block_blob_bin) {
    ERROR_PRINT("Memory allocation failed for block_blob_bin");
    return false;
  }

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

  DEBUG_PRINT("Final block_blob_hex (length: %zu):", strlen(block_blob_hex));
  DEBUG_PRINT("%s", block_blob_hex);

  free(block_blob_bin);
  return true;
}

/*---------------------------------------------------------------------------------------------------------
Name: block_verifiers_create_block
Description: Runs the round where the block verifiers will create the block
Return: 0 if an error has occured, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int block_verifiers_create_block(const char* vote_hash_hex, uint8_t total_vote, uint8_t winning_vote) {
  char ck_block_height[BLOCK_HEIGHT_LENGTH + 1] = {0};

  // Confirm block height hasn't drifted (this node may be behind the network)
  INFO_STAGE_PRINT("Part 8 - Confirm block height hasn't drifted");
  snprintf(current_round_part, sizeof(current_round_part), "%d", 8);
  if (get_current_block_height(ck_block_height) == 1 && strncmp(current_block_height, ck_block_height, BLOCK_HEIGHT_LENGTH) != 0) {
    WARNING_PRINT("Your block height is not synced correctly, waiting for next round");
    return ROUND_ERROR;
  }

  char block_blob[BUFFER_SIZE] = {0};
  size_t reserved_offset = 0;
  // Only the block producer completes the following steps, producer_refs is an array in case we decide to add
  // backup producers in the future
  INFO_PRINT("Parts 9 thru 11 are only perfomed by the block producer");
  if (strcmp(producer_refs[0].public_address, xcash_wallet_public_address) == 0) {
    // Create block template
    INFO_STAGE_PRINT("Part 9 - Create block template");
    snprintf(current_round_part, sizeof(current_round_part), "%d", 9);
    if (get_block_template(block_blob, BUFFER_SIZE, &reserved_offset) == 0) {
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
    
    INFO_PRINT_STATUS_OK("Block signature sent");
  } else {
    if (++count >= 15) {
      INFO_PRINT("Get Banned Delegates...");
      get_banned_delegates();
      count = 0;
    }
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
  long   now_nsec = now_ts.tv_nsec;

  size_t seconds_per_block = (size_t)BLOCK_TIME * 60;
  size_t seconds_within_block = (size_t)(now_sec % (time_t)seconds_per_block);

  double target_seconds = (double)(MINUTES * 60 + SECONDS);
  double current_time_in_block = (double)seconds_within_block + (double)now_nsec / 1e9;
  double sleep_seconds = target_seconds - current_time_in_block;

  if (sleep_seconds <= 0.0) {
    DEBUG_PRINT("Missed sync point by %.3f seconds", -sleep_seconds);
    return XCASH_ERROR;
  }

  // Build relative timespec (no libm needed)
  time_t sec  = (time_t)sleep_seconds;        // floor for positive values
  double frac = sleep_seconds - (double)sec;
  long   nsec = (long)(frac * 1000000000.0);  // truncate to < 1e9

  struct timespec req = { .tv_sec = sec, .tv_nsec = nsec };
  struct timespec rem;

  INFO_PRINT("Sleeping for %.3f seconds to sync to target time...", sleep_seconds);

  // Resume remaining time on EINTR so signals don't abort the wait
  for (;;) {
    if (nanosleep(&req, &rem) == 0) break;    // slept fully
    if (errno != EINTR) {
      ERROR_PRINT("nanosleep failed: %s", strerror(errno));
      return XCASH_ERROR;
    }
    req = rem;
  }

  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: generate_and_request_vrf_data_sync
Description:
  Generates a VRF proof and beta for the current node (delegate) using:
    - The previous block hash
    - The current block height
    - The VRF public key of the intended block producer

  The VRF proof is generated with the node’s VRF secret key, and then converted to a VRF beta
  (the pseudorandom output). Both are validated immediately using the VRF public key to ensure correctness.

  Steps performed:
    1. Decode and validate the existing VRF public key.
    2. Decode the previous block hash into binary form.
    3. Append the current block height (as little-endian 8 bytes) and the VRF public key of the block producer
       to create the VRF alpha input (72 bytes total).
    4. Generate a VRF proof from the alpha input.
    5. Convert the VRF proof to a VRF beta value.
    6. Verify the VRF proof and ensure the computed beta matches.
    7. Store the VRF proof and beta in the delegate’s in-memory structure if this node is in the top
       BLOCK_VERIFIERS_AMOUNT list.
    8. Create and return a JSON message containing:
         - Public wallet address
         - VRF public key
         - VRF proof
         - VRF beta
         - Current block height
         - Delegates hash

Parameters:
  message - Output parameter. On success, set to a newly allocated string containing the
            JSON message to be broadcast to other block verifiers. Caller is responsible
            for freeing it.

Return:
  XCASH_OK (1)   - if all steps complete successfully.
  XCASH_ERROR (0) - if any validation, key decoding, proof generation, proof verification,
                    or message creation step fails.
---------------------------------------------------------------------------------------------------------*/
bool generate_and_request_vrf_data_sync(char** message) {
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
    return false;
  }

  // Validate the VRF public key
  if (crypto_vrf_is_valid_key(pk_bin) != 1) {
    ERROR_PRINT("Public key failed validation");
    return false;
  }

  // Decode 32-byte hash into alpha_input_bin[0..31]
  if (!hex_to_byte_array(previous_block_hash, previous_block_hash_bin, 32)) {
    ERROR_PRINT("Failed to decode previous block hash");
    return false;
  }
  memcpy(alpha_input_bin, previous_block_hash_bin, 32);

  // Convert current_block_height (char*) to binary
  uint64_t block_height = strtoull(current_block_height, NULL, 10);
  uint64_t height_le = htole64(block_height);
  memcpy(alpha_input_bin + 32, &height_le, sizeof(height_le));  // Write at offset 32

  // Add vrf_block_producer
  memcpy(alpha_input_bin + 40, pk_bin, 32);  // Write at offset 40

  // Generate VRF proof - the input data is previous block hash, the block height, and the vrf_public_key of the block producer
  if (crypto_vrf_prove(vrf_proof, secret_key_data, alpha_input_bin, sizeof(alpha_input_bin)) != 0) {
    ERROR_PRINT("Failed to generate VRF proof");
    return false;
  }

  // Convert proof to beta (random output)
  if (crypto_vrf_proof_to_hash(vrf_beta, vrf_proof) != 0) {
    ERROR_PRINT("Failed to convert VRF proof to beta");
    return false;
  }

  // Convert proof, beta, and random buffer to hex
  for (i = 0, offset = 0; i < crypto_vrf_PROOFBYTES; i++, offset += 2)
    snprintf(vrf_proof_hex + offset, 3, "%02x", vrf_proof[i]);
  for (i = 0, offset = 0; i < crypto_vrf_OUTPUTBYTES; i++, offset += 2)
    snprintf(vrf_beta_hex + offset, 3, "%02x", vrf_beta[i]);
  
  unsigned char computed_beta[crypto_vrf_OUTPUTBYTES];
  if (crypto_vrf_verify(computed_beta, pk_bin, vrf_proof, alpha_input_bin, 72) != 0) {
    DEBUG_PRINT("Failed to verify the VRF proof for this node");
    return false;
  } else {


    if (memcmp(computed_beta, vrf_beta, 64) != 0) {
      DEBUG_PRINT("Failed to match the computed VRF beta for this node");
      return false;
    }
  }

  // Compose outbound message (JSON)
  *message = create_message_param(
      XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_VRF_DATA,
      "public_address", xcash_wallet_public_address,
      "vrf_public_key", vrf_public_key,
      "vrf_proof", vrf_proof_hex,
      "vrf_beta", vrf_beta_hex,
      "block-height", current_block_height,
      "delegates_hash", delegates_hash,
      NULL);

  if (*message == NULL) {
    ERROR_PRINT("create_message_param returned NULL for VRF_DATA");
    return false;
  }

  return true;
}

// Helper for qsort
static int bytes32_cmp(const void *va, const void *vb) {
  const unsigned char *a = (const unsigned char *)va;
  const unsigned char *b = (const unsigned char *)vb;
  return memcmp(a, b, crypto_vrf_PUBLICKEYBYTES);
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

  int wait_milliseconds = 0;
  while (atomic_load(&wait_for_vrf_init) && wait_milliseconds < (DELAY_EARLY_TRANSACTIONS_MAX * 1000)) {
    usleep(500000);  // 0.5 seconds = 500,000 microseconds
    wait_milliseconds += 500;
  }
  if (atomic_load(&wait_for_vrf_init)) {
    ERROR_PRINT("Timed out waiting for vrf init in block_verifiers_create_vote_majority_result");
  }

  if (strlen(current_block_verifiers_list.block_verifiers_public_key[producer_indx]) == 0 ||
      strlen(current_block_verifiers_list.block_verifiers_vrf_proof_hex[producer_indx]) == 0 ||
      strlen(current_block_verifiers_list.block_verifiers_vrf_beta_hex[producer_indx]) == 0) {
    ERROR_PRINT("Missing VRF data for producer");
    return false;
  }

  size_t height_len = strlen(current_block_height);
  if (!hex_to_byte_array(current_block_verifiers_list.block_verifiers_public_key[producer_indx], pk_bin, sizeof(pk_bin))) {
    ERROR_PRINT("Invalid hex format for public key");
    return false;
  }

  if (!hex_to_byte_array(current_block_verifiers_list.block_verifiers_vrf_beta_hex[producer_indx], vrf_beta_bin, sizeof(vrf_beta_bin))) {
    ERROR_PRINT("Invalid hex format for beta");
    return false;
  }

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

  char* signature = calloc(XCASH_SIGN_DATA_LENGTH + 1, sizeof(char));
  char* request = calloc(MEDIUM_BUFFER_SIZE * 2, sizeof(char));
  if (!signature || !request) {
    FATAL_ERROR_EXIT("sign_data: Memory allocation failed");
  }

  unsigned char hash_input[160];  // height_len + 64 + 32 + 32
  memcpy(hash_input + offset, current_block_height, height_len);
  offset += height_len;

  memcpy(hash_input + offset, vrf_beta_bin, crypto_vrf_OUTPUTBYTES);
  offset += crypto_vrf_OUTPUTBYTES;

  memcpy(hash_input + offset, pk_bin, crypto_vrf_PUBLICKEYBYTES);
  offset += crypto_vrf_PUBLICKEYBYTES;

  memcpy(hash_input + offset, round_pk_hash_bin, crypto_vrf_PUBLICKEYBYTES);
  offset += crypto_vrf_PUBLICKEYBYTES;

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
                        request, HTTP_TIMEOUT_SETTINGS) <= 0 ||
      !parse_json_data(response, "result.signature", signature, XCASH_SIGN_DATA_LENGTH+1) ||
      strlen(signature) == 0 ||
      strncmp(signature, XCASH_SIGN_DATA_PREFIX, sizeof(XCASH_SIGN_DATA_PREFIX) - 1) != 0) {
    ERROR_PRINT("Function: block_verifiers_create_vote_majority_result - Wallet signature failed or format invalid");
    free(signature);
    free(request);
    return false;
  }

  // Save current block_verifiers data into structure if it is one of the top 50
  pthread_mutex_lock(&current_block_verifiers_lock);
  for (i = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
    if (strncmp(current_block_verifiers_list.block_verifiers_public_address[i], xcash_wallet_public_address, XCASH_WALLET_LENGTH) == 0) {
      memcpy(current_block_verifiers_list.block_verifiers_vote_signature[i], signature, XCASH_SIGN_DATA_LENGTH+1);
      break;
    }
  }
  pthread_mutex_unlock(&current_block_verifiers_lock);

  const char* params[] = {
      "public_address", xcash_wallet_public_address,
      "proposed_producer", current_block_verifiers_list.block_verifiers_public_address[producer_indx],
      "block_height", current_block_height,
      "vrf_beta", current_block_verifiers_list.block_verifiers_vrf_beta_hex[producer_indx],
      "vrf_proof", current_block_verifiers_list.block_verifiers_vrf_proof_hex[producer_indx],
      "vrf_public_key", current_block_verifiers_list.block_verifiers_public_key[producer_indx],
      "vote_signature", signature,
      NULL};
  *message = create_message_param_list(XMSG_NODES_TO_NODES_VOTE_MAJORITY_RESULTS, params);

  free(signature);
  signature = NULL;
  free(request);
  request = NULL;

  if (*message == NULL) {
    ERROR_PRINT("create_message_param returned NULL for VOTE_MAJORITY_RESULTS");
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
      "sync_token", sync_token,
      NULL};

  char* message = NULL;
  message = create_message_param_list(XMSG_NODES_TO_NODES_DATABASE_SYNC_REQ, params);
  if (!message) {
    WARNING_PRINT("create_message_param_list returned NULL for DATABASE_SYNC_REQ");
    return false;
  }

  const char* ip = delegates_all[selected_index].IP_address;
  if (send_message_to_ip_or_hostname(ip, XCASH_DPOPS_PORT, message) == XCASH_OK) {
    DEBUG_PRINT("Sync request sent to delegate %d (%s)", selected_index, ip);
    free(message);
    return true;
  }

  free(message);
  WARNING_PRINT("Failed to send sync request to delegate %d (%s)", selected_index, ip);
  return false;
}