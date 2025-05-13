#include "block_verifiers_functions.h"

bool add_vrf_extra_and_sign(char* block_blob_hex)
{
  // Allocate working buffer for binary blob
  unsigned char* block_blob_bin = calloc(1, BUFFER_SIZE);
  if (!block_blob_bin) {
    ERROR_PRINT("Memory allocation failed for block_blob_bin");
    return false;
  }

  // Convert hex blob to binary
  size_t blob_len = strlen(block_blob_hex) / 2;
  if (!hex_to_byte_array(block_blob_hex, block_blob_bin, blob_len)) {
    ERROR_PRINT("Failed to convert block_blob_hex to binary");
    free(block_blob_bin);
    return false;
  }

  // Get reserved_offset from previous RPC call or define statically
  size_t reserved_offset = 320;
  
  // Validate offset doesn't overflow
  if (reserved_offset + 256 > BUFFER_SIZE) {
    ERROR_PRINT("Reserved offset too close to end of block blob");
    free(block_blob_bin);
    return false;
  }

  // Patch in the VRF extra fields at the reserved_offset
  size_t pos = reserved_offset;
  block_blob_bin[pos++] = 0x70;  // VRF proof tag
  pos += hex_to_byte_array(producer_refs[0].vrf_proof_hex, block_blob_bin + pos, VRF_PROOF_LENGTH / 2);

  block_blob_bin[pos++] = 0x71;  // VRF beta tag
  pos += hex_to_byte_array(producer_refs[0].vrf_beta_hex, block_blob_bin + pos, VRF_BETA_LENGTH / 2);

  block_blob_bin[pos++] = 0x73;  // VRF public key tag
  pos += hex_to_byte_array(producer_refs[0].vrf_public_key, block_blob_bin + pos, VRF_PUBLIC_KEY_LENGTH / 2);

  block_blob_bin[pos++] = 0x72;  // Public address tag
  pos += hex_to_byte_array(producer_refs[0].public_address, block_blob_bin + pos, XCASH_WALLET_LENGTH / 2);

  // Sign the full blob using Wallet RPC
  char signature_hex[XCASH_SIGN_DATA_LENGTH + 1] = {0};
  if (!sign_block_blob(block_blob_hex, signature_hex, sizeof(signature_hex))) {
    ERROR_PRINT("Failed to sign block blob");
    free(block_blob_bin);
    return false;
  }
  DEBUG_PRINT("Block Blob Signature: %s", signature_hex);

  block_blob_bin[pos++] = 0x74;  // Signature tag
  pos += hex_to_byte_array(signature_hex, block_blob_bin + pos, XCASH_SIGN_DATA_LENGTH / 2);

  // Re-encode the full blob to hex for submission
  bytes_to_hex(block_blob_bin, blob_len, block_blob_hex, BUFFER_SIZE);

  free(block_blob_bin);
  return true;
}

// Helper function: Restart logic if alone verifier
int check_restart_if_alone(size_t count) {
  if (count <= 1) {
      for (size_t i = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
          if (strncmp(current_block_verifiers_list.block_verifiers_public_address[i], xcash_wallet_public_address, XCASH_WALLET_LENGTH) == 0) {
              WARNING_PRINT("Restarting, could not process any other block verifiers data");
              return 1;
          }
      }
  }
  return 0;
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

  time_t now = time(NULL);
  if (now == ((time_t)-1)) {
    ERROR_PRINT("Failed to get current time");
    return XCASH_ERROR;
  }

  size_t seconds_per_block = BLOCK_TIME * 60;
  size_t seconds_within_block = now % seconds_per_block;
  size_t target_seconds = MINUTES * 60 + SECONDS;

  if (seconds_within_block >= target_seconds) {
    WARNING_PRINT("Missed sync point by %zu seconds", seconds_within_block - target_seconds);
    return XCASH_ERROR;
  }

  size_t sleep_seconds = target_seconds - seconds_within_block;
  DEBUG_PRINT("Sleeping for %zu seconds to sync to target time...", sleep_seconds);
  sleep(sleep_seconds);

  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Generate random binary string
---------------------------------------------------------------------------------------------------------*/
int get_random_bytes(unsigned char *buf, size_t len) {
    ssize_t ret = getrandom(buf, len, 0);
    if (ret < 0 || (size_t)ret != len) {
        ERROR_PRINT("getrandom() failed: %s", strerror(errno));
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
  unsigned char sk_bin[crypto_vrf_SECRETKEYBYTES] = {0};
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
  if (crypto_vrf_prove(vrf_proof, sk_bin, alpha_input_bin, sizeof(alpha_input_bin)) != 0) {
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

  // Save to block_verifiers index in struct (for signature tracking)
  pthread_mutex_lock(&majority_vote_lock);
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
  pthread_mutex_unlock(&majority_vote_lock);

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








  return XCASH_ERROR;
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