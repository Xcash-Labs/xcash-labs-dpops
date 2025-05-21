#include "block_verifiers_functions.h"

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
 * vrf_blob Layout (240 Bytes)
 *  Field	      Bytes   Description
 *  vrf_proof	  80	    Hex-decoded 80-byte VRF proof (e.g. from libsodium)
 *  vrf_beta	  64	    Hex-decoded 32-byte beta (VRF hash output)
 *  vrf_pubkey  32	    Hex-decoded 32-byte VRF public key
 *  signature	  64	    Hex-decoded 64-byte signature
 * 
 * @param block_blob_hex The input and output hex-encoded blocktemplate blob.
 *                       Must contain reserved space as defined by get_block_template (e.g. 220 bytes).
 * @return true on success, false if any step fails (conversion, signing, or overflow).
 *
 * @note This function expects `producer_refs[0]` to be populated with all required hex strings.
 * @note Ensure the get_block_template reserve_size is at least 210–220 bytes to fit the full VRF blob.
 * @note The signature is calculated on the original (unpatched) block_blob_hex for consensus correctness.
---------------------------------------------------------------------------------------------------------*/
bool add_vrf_extra_and_sign(char* block_blob_hex)
{
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

  size_t reserved_offset = 125;
  size_t pos = reserved_offset;

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

  // Sign the original block blob (before patching)
  char blob_signature[XCASH_SIGN_DATA_LENGTH + 1] = {0};
  if (!sign_block_blob(block_blob_hex, blob_signature, sizeof(blob_signature))) {
    ERROR_PRINT("Failed to sign block blob");
    free(block_blob_bin);
    return false;
  }
  DEBUG_PRINT("Block Blob Signature: %s", blob_signature);

  const char* base64_part = blob_signature + 5; // skip "SigV2"
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

  if (vrf_pos != 240) {
    ERROR_PRINT("VRF blob constructed with incorrect size: %zu bytes", vrf_pos);
    free(block_blob_bin);
    return false;
  }
  
  block_blob_bin[pos++] = TX_EXTRA_VRF_SIGNATURE_TAG;
  block_blob_bin[pos++] = 208;  // length of VRF blob (must match vrf_pos)
  memcpy(block_blob_bin + pos, vrf_blob, 208);
  pos += 208;

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
int block_verifiers_create_block(void) {
  char data[BUFFER_SIZE] = {0};

  // Confirm block height hasn't drifted (this node may be behind the network)
  INFO_STAGE_PRINT("Part 7 - Confirm block height hasn't drifted");
  snprintf(current_round_part, sizeof(current_round_part), "%d", 7);
  if (get_current_block_height(data) == 1 && strncmp(current_block_height, data, BUFFER_SIZE) != 0) {
      WARNING_PRINT("Your block height is not synced correctly, waiting for next round");
      return ROUND_ERROR;
  }


// will need to get consence vote befor adding nodes


  char block_blob[BUFFER_SIZE] = {0};
  // Only the block producer completes the following steps, producer_refs is an array in case we decide to add 
  // backup producers in the future
  if (strcmp(producer_refs[0].public_address, xcash_wallet_public_address) == 0) {

    // Create block template
    INFO_STAGE_PRINT("Part 8 - Create block template");
    snprintf(current_round_part, sizeof(current_round_part), "%d", 8);
    if (get_block_template(block_blob, BUFFER_SIZE) == 0) {
      return ROUND_ERROR;
    }

    if (strncmp(block_blob, "", 1) == 0) {
      WARNING_PRINT("Did not receive block template");
      return ROUND_ERROR;
    }

    // Create block template
    INFO_STAGE_PRINT("Part 9 - Add VRF Data And Sign Block Blob");
    snprintf(current_round_part, sizeof(current_round_part), "%d", 9);
    if(!add_vrf_extra_and_sign(block_blob)) {
      return ROUND_ERROR;
    }

    // Part 10 - Submit block
    if (!submit_block_template(block_blob)) {
      return ROUND_ERROR;
    }

    INFO_PRINT_STATUS_OK("Block signature sent");


  }


// sync .........

  // Final step - Update DB
  INFO_STAGE_PRINT("Part 9 - Update DB");
    // update status, database (reserve_bytes and node online status)...

    // how do other database get updated?  wait they all know the winning block producer

      return ROUND_ERROR;

  return ROUND_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: sync_block_verifiers_minutes_and_seconds
Description: Syncs the block verifiers to a specific minute and second
Parameters:
  minutes - The minutes
  seconds - The seconds
---------------------------------------------------------------------------------------------------------*/
int sync_block_verifiers_minutes_and_seconds(const int MINUTES, const int SECONDS)
{
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
    .tv_nsec = (long)((sleep_seconds - (time_t)sleep_seconds) * 1e9)
  };

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
bool generate_and_request_vrf_data_msg(char** message)
{
  unsigned char random_buf_bin[VRF_RANDOMBYTES_LENGTH] = {0};
  unsigned char alpha_input_bin[VRF_RANDOMBYTES_LENGTH * 2] = {0};
//  unsigned char sk_bin[crypto_vrf_SECRETKEYBYTES] = {0};


  unsigned char pk_bin[crypto_vrf_PUBLICKEYBYTES] = {0};
  unsigned char vrf_proof[crypto_vrf_PROOFBYTES] = {0};
  unsigned char vrf_beta[crypto_vrf_OUTPUTBYTES] = {0};
  unsigned char previous_block_hash_bin[BLOCK_HASH_LENGTH / 2] = {0};
  char vrf_proof_hex[VRF_PROOF_LENGTH + 1] = {0};  
  char vrf_beta_hex[VRF_BETA_LENGTH + 1] = {0};
  char random_buf_hex[(VRF_RANDOMBYTES_LENGTH * 2) + 1] = {0};
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

  // Generate random binary string
  if (!get_random_bytes(random_buf_bin, VRF_RANDOMBYTES_LENGTH)) {
    FATAL_ERROR_EXIT("Failed to generate VRF alpha input");
    return XCASH_ERROR;
  }

  // Form the alpha input = previous_block_hash || random_buf
  if (!hex_to_byte_array(previous_block_hash, previous_block_hash_bin, VRF_RANDOMBYTES_LENGTH)) {
    ERROR_PRINT("Failed to decode previous block hash");
    return XCASH_ERROR;
  }
  memcpy(alpha_input_bin, previous_block_hash_bin, VRF_RANDOMBYTES_LENGTH);
  memcpy(alpha_input_bin + VRF_RANDOMBYTES_LENGTH, random_buf_bin, VRF_RANDOMBYTES_LENGTH);

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
  for (i = 0, offset = 0; i < VRF_RANDOMBYTES_LENGTH; i++, offset += 2) {
      snprintf(random_buf_hex + offset, 3, "%02x",random_buf_bin[i]);
  }

  unsigned char computed_beta[crypto_vrf_OUTPUTBYTES];
  if (crypto_vrf_verify(computed_beta, pk_bin, vrf_proof, alpha_input_bin, 64) != 0) {
    DEBUG_PRINT("Failed to verify the VRF proof for this node");
    return XCASH_ERROR;
  } else {
    if (memcmp(computed_beta, vrf_beta, 64) != 0) {
      DEBUG_PRINT("Failed to match the computed VRF beta for this node");
      return XCASH_ERROR;
    }
  }

  // Save current block_verifiers data into structure
  pthread_mutex_lock(&majority_vrf_lock);
  for (i = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
    if (strncmp(current_block_verifiers_list.block_verifiers_public_address[i], xcash_wallet_public_address, XCASH_WALLET_LENGTH) == 0) {
      memcpy(current_block_verifiers_list.block_verifiers_public_address[i], xcash_wallet_public_address, XCASH_WALLET_LENGTH+1);
      memcpy(current_block_verifiers_list.block_verifiers_vrf_public_key_hex[i], vrf_public_key, VRF_PUBLIC_KEY_LENGTH+1);
      memcpy(current_block_verifiers_list.block_verifiers_random_hex[i], random_buf_hex, VRF_RANDOMBYTES_LENGTH * 2 + 1);
      memcpy(current_block_verifiers_list.block_verifiers_vrf_proof_hex[i], vrf_proof_hex, VRF_PROOF_LENGTH + 1); 
      memcpy(current_block_verifiers_list.block_verifiers_vrf_beta_hex[i], vrf_beta_hex, VRF_BETA_LENGTH + 1);
      break;
    }
  }
  pthread_mutex_unlock(&majority_vrf_lock);

  // Compose outbound message (JSON)
  *message = create_message_param(
      XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_VRF_DATA,
      "public_address", xcash_wallet_public_address,
      "vrf_public_key", vrf_public_key,
      "random_data", random_buf_hex,
      "vrf_proof", vrf_proof_hex,
      "vrf_beta", vrf_beta_hex,
      "block-height", current_block_height,
      NULL);

    return XCASH_OK;
}

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