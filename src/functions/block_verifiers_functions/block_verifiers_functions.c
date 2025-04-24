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
  size_t count;

  // Sync start
  INFO_STAGE_PRINT("Waiting for block synchronization start time...");
  if (sync_block_verifiers_minutes_and_seconds(1, 10) == XCASH_ERROR)
      return ROUND_SKIP;

  // Confirm block height hasn't drifted (this node may be behind the network)
  INFO_STAGE_PRINT("Part 5 - Confirm block height hasn't drifted");
  snprintf(current_round_part, sizeof(current_round_part), "%d", 5);
  if (get_current_block_height(data) == 1 && strncmp(current_block_height, data, BUFFER_SIZE) != 0) {
      WARNING_PRINT("Your block height is not synced correctly, waiting for next round");
      return ROUND_NEXT;
  }

  char block_blob[BUFFER_SIZE] = {0};
  // Only the block producer completes the following steps, producer_refs is an array in case we decide to add 
  // backup producers in the future
  if (strcmp(producer_refs[0].public_address, xcash_wallet_public_address) == 0) {

    // Part 3 - Create block template
    INFO_STAGE_PRINT("Part 6 - Create block template");
    snprintf(current_round_part, sizeof(current_round_part), "%d", 6);
    if (get_block_template(block_blob, BUFFER_SIZE) == 0) {
      return ROUND_NEXT;
    }

    if (sync_block_verifiers_minutes_and_seconds(1, 20) == XCASH_ERROR)
      return ROUND_SKIP;
    if (strncmp(block_blob, "", 1) == 0) {
      WARNING_PRINT("Did not receive block template");
      return ROUND_NEXT;
    }

    // Part 7 - Create block template
    INFO_STAGE_PRINT("Part 7 - Add VRF Data And Sign Block Blob");
    snprintf(current_round_part, sizeof(current_round_part), "%d", 7);
    if(!add_vrf_extra_and_sign(block_blob)) {
      return ROUND_NEXT;
    }

    if (sync_block_verifiers_minutes_and_seconds(1, 40) == XCASH_ERROR)
    return ROUND_SKIP;

    // Part 5 - Submit block
    if (!submit_block_template(block_with_hash) != XCASH_OK) {
      return ROUND_NEXT;
    }

    INFO_PRINT_STATUS_OK("Block signature sent");

    // update status, database (reserve_bytes and node online status)...
    
  }

  // Final step - Update DB
  INFO_STAGE_PRINT("Part 9 - Update DB");
  if (block_verifiers_create_block_and_update_database() == XCASH_ERROR)
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

  // Convert stored hex keys to binary
  if (!hex_to_byte_array(secret_key, sk_bin, sizeof(sk_bin))) {
    ERROR_PRINT("Invalid hex format for secret key");
    return XCASH_ERROR;
  }

  if (!hex_to_byte_array(vrf_public_key, pk_bin, sizeof(pk_bin))) {
    ERROR_PRINT("Invalid hex format for public key");
    return XCASH_ERROR;
  }

  // Validate the public key
  if (crypto_vrf_is_valid_key(pk_bin) != 1) {
    ERROR_PRINT("Public key failed validation");
    return XCASH_ERROR;
  }

  // Generate random binary string
  randombytes_buf(random_buf_bin, VRF_RANDOMBYTES_LENGTH);

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

  return XCASH_OK;
}
/*---------------------------------------------------------------------------------------------------------
Name: block_verifiers_create_vote_majority_results
Description: The block verifiers will create the vote majority results
Parameters:
  result - The result
  SETTINGS - The data settings
---------------------------------------------------------------------------------------------------------*/
void block_verifiers_create_vote_majority_results(char *result, const int SETTINGS) {
  const char *MESSAGE_HEADER = "{\r\n \"message_settings\": \"NODES_TO_NODES_VOTE_MAJORITY_RESULTS\",\r\n ";
  const char *VOTE_KEY_PREFIX = "\"vote_data_";
  const char *VOTE_KEY_SUFFIX = "\": \"";
  const char *VOTE_ENTRY_SUFFIX = "\",\r\n";

  size_t offset = 0;
  int count, count2;

  memset(result, 0, BUFFER_SIZE);

  // Reset majority vote memory
  pthread_mutex_lock(&majority_vote_lock);
  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
    for (count2 = 0; count2 < BLOCK_VERIFIERS_AMOUNT; count2++) {
      memset(current_block_verifiers_majority_vote.data[count][count2], 0,
             sizeof(current_block_verifiers_majority_vote.data[count][count2]));
    }
  }
  pthread_mutex_unlock(&majority_vote_lock);

  // Write the message header
  offset = snprintf(result, BUFFER_SIZE, "%s", MESSAGE_HEADER);

  // Compose JSON vote data
  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
    offset += snprintf(result + offset, BUFFER_SIZE - offset, "%s%d%s", VOTE_KEY_PREFIX, count + 1, VOTE_KEY_SUFFIX);

//    if (SETTINGS == 0) {
//      // VRF data vote
//      if (strlen(VRF_data.block_verifiers_vrf_secret_key_data[count]) == VRF_SECRET_KEY_LENGTH &&
//          strlen(VRF_data.block_verifiers_vrf_public_key_data[count]) == VRF_PUBLIC_KEY_LENGTH &&
//          strlen(VRF_data.block_verifiers_random_data[count]) == RANDOM_STRING_LENGTH) {
//
//        memcpy(result + offset, VRF_data.block_verifiers_vrf_secret_key_data[count], VRF_SECRET_KEY_LENGTH);
//        offset += VRF_SECRET_KEY_LENGTH;
//        memcpy(result + offset, VRF_data.block_verifiers_vrf_public_key_data[count], VRF_PUBLIC_KEY_LENGTH);
//        offset += VRF_PUBLIC_KEY_LENGTH;
//        memcpy(result + offset, VRF_data.block_verifiers_random_data[count], RANDOM_STRING_LENGTH);
//        offset += RANDOM_STRING_LENGTH;
//      } else {
//        offset += snprintf(result + offset, BUFFER_SIZE - offset, "%s", BLOCK_VERIFIER_MAJORITY_VRF_DATA_TEMPLATE);
//      }
//    } else {
//      // Signature vote
//      if (strlen(VRF_data.block_blob_signature[count]) == (VRF_PROOF_LENGTH + VRF_BETA_LENGTH)) {
//        memcpy(result + offset, VRF_data.block_blob_signature[count], VRF_PROOF_LENGTH + VRF_BETA_LENGTH);
//        offset += (VRF_PROOF_LENGTH + VRF_BETA_LENGTH);
//      } else {
//        offset += snprintf(result + offset, BUFFER_SIZE - offset, "%s", BLOCK_VERIFIER_MAJORITY_BLOCK_VERIFIERS_SIGNATURE_TEMPLATE);
//      }
//    }

    offset += snprintf(result + offset, BUFFER_SIZE - offset, "%s", VOTE_ENTRY_SUFFIX);
  }

  // Fix final trailing comma: Replace last ',\r\n' with closing }
  if (offset >= 3) {
    result[offset - 3] = '}';
    result[offset - 2] = '\0';
  }

  // Store vote into current_block_verifiers_majority_vote
  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
    if (strncmp(current_block_verifiers_list.block_verifiers_public_address[count],
                xcash_wallet_public_address, XCASH_WALLET_LENGTH) == 0) {
      break;
    }
  }

  pthread_mutex_lock(&majority_vote_lock);
//  for (count2 = 0; count2 < BLOCK_VERIFIERS_AMOUNT; count2++) {
//    memcpy(current_block_verifiers_majority_vote.data[count][count2],
//           VRF_data.block_verifiers_vrf_secret_key_data[count2], VRF_SECRET_KEY_LENGTH);
//    memcpy(current_block_verifiers_majority_vote.data[count][count2] + VRF_SECRET_KEY_LENGTH,
//           VRF_data.block_verifiers_vrf_public_key_data[count2], VRF_PUBLIC_KEY_LENGTH);
//    memcpy(current_block_verifiers_majority_vote.data[count][count2] + VRF_SECRET_KEY_LENGTH + VRF_PUBLIC_KEY_LENGTH,
//           VRF_data.block_verifiers_random_data[count2], RANDOM_STRING_LENGTH);
//  }
  pthread_mutex_unlock(&majority_vote_lock);
}

/*---------------------------------------------------------------------------------------------------------
Name: block_verifiers_create_VRF_data
Description: The block verifiers will create all of the VRF data
Return: 0 if an error has occured, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int block_verifiers_create_VRF_data(void)
{
  char hash_buf[SMALL_BUFFER_SIZE] = {0};
  char hex_buf[SMALL_BUFFER_SIZE] = {0};
  size_t count, hex_index;
  int selected_index = -1;
/*
  // Initialize vrf_alpha_string
  memset(VRF_data.vrf_alpha_string, 0, strlen((const char*)VRF_data.vrf_alpha_string));
  memcpy(VRF_data.vrf_alpha_string, previous_block_hash, BLOCK_HASH_LENGTH);

  // Append random data or placeholder
  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
    const char* rand_data = (strlen((const char*)VRF_data.block_verifiers_vrf_secret_key[count]) == crypto_vrf_SECRETKEYBYTES &&
                             strlen((const char*)VRF_data.block_verifiers_vrf_public_key[count]) == crypto_vrf_PUBLICKEYBYTES &&
                             strlen(VRF_data.block_verifiers_random_data[count]) == RANDOM_STRING_LENGTH)
                             ? VRF_data.block_verifiers_random_data[count]
                             : GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_RANDOM_STRING;

    strncat((char*)VRF_data.vrf_alpha_string, rand_data, RANDOM_STRING_LENGTH);
  }

  // Convert to hex string
  size_t alpha_len = strlen((const char*)VRF_data.vrf_alpha_string);
  for (hex_index = 0, count = 0; count < alpha_len; count++, hex_index += 2) {
    snprintf(VRF_data.vrf_alpha_string_data + hex_index, BUFFER_SIZE - hex_index, "%02x", VRF_data.vrf_alpha_string[count] & 0xFF);
  }

  // Hash alpha string
  crypto_hash_sha512((unsigned char*)hash_buf, (const unsigned char*)VRF_data.vrf_alpha_string_data, strlen(VRF_data.vrf_alpha_string_data));

  // Convert hash to hex
  for (hex_index = 0, count = 0; count < DATA_HASH_LENGTH / 2; count++, hex_index += 2) {
    snprintf(hex_buf + hex_index, sizeof(hex_buf) - hex_index, "%02x", hash_buf[count] & 0xFF);
  }

  // Choose index from hash
  for (count = 0; count < DATA_HASH_LENGTH; count += 2) {
    char byte_str[3] = {0};
    memcpy(byte_str, &hex_buf[count], 2);
    int idx = (int)strtol(byte_str, NULL, 16);

    if (idx >= MINIMUM_BYTE_RANGE && idx <= MAXIMUM_BYTE_RANGE) {
      idx %= BLOCK_VERIFIERS_AMOUNT;
      if (strncmp(VRF_data.block_verifiers_vrf_secret_key_data[idx], GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_SECRET_KEY_DATA, sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_SECRET_KEY_DATA) - 1) != 0 &&
          strncmp(VRF_data.block_verifiers_vrf_public_key_data[idx], GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_PUBLIC_KEY_DATA, sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_PUBLIC_KEY_DATA) - 1) != 0 &&
          strncmp(VRF_data.block_verifiers_random_data[idx], GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_RANDOM_STRING, sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_RANDOM_STRING) - 1) != 0) {
        selected_index = idx;
        break;
      }
    }
  }

  if (selected_index < 0) {
    ERROR_PRINT("Failed to select a valid block verifier for VRF");
    return XCASH_ERROR;
  }

  // Copy selected verifier's data
  memcpy(VRF_data.vrf_secret_key_data, VRF_data.block_verifiers_vrf_secret_key_data[selected_index], VRF_SECRET_KEY_LENGTH);
  memcpy(VRF_data.vrf_secret_key, VRF_data.block_verifiers_vrf_secret_key[selected_index], crypto_vrf_SECRETKEYBYTES);
  memcpy(VRF_data.vrf_public_key_data, VRF_data.block_verifiers_vrf_public_key_data[selected_index], VRF_PUBLIC_KEY_LENGTH);
  memcpy(VRF_data.vrf_public_key, VRF_data.block_verifiers_vrf_public_key[selected_index], crypto_vrf_PUBLICKEYBYTES);

  // Create VRF proof and beta string
  if (crypto_vrf_prove(VRF_data.vrf_proof, VRF_data.vrf_secret_key, (const unsigned char*)VRF_data.vrf_alpha_string_data, strlen((const char*)VRF_data.vrf_alpha_string_data)) != 0 ||
      crypto_vrf_proof_to_hash(VRF_data.vrf_beta_string, VRF_data.vrf_proof) != 0 ||
      crypto_vrf_verify(VRF_data.vrf_beta_string, VRF_data.vrf_public_key, VRF_data.vrf_proof, (const unsigned char*)VRF_data.vrf_alpha_string_data, strlen((const char*)VRF_data.vrf_alpha_string_data)) != 0) {
    ERROR_PRINT("VRF proof or verification failed");
    return XCASH_ERROR;
  }

  // Convert proof and beta to hex
  for (count = 0, hex_index = 0; count < crypto_vrf_PROOFBYTES; count++, hex_index += 2) {
    snprintf(VRF_data.vrf_proof_data + hex_index, BUFFER_SIZE_NETWORK_BLOCK_DATA - hex_index, "%02x", VRF_data.vrf_proof[count] & 0xFF);
  }
  for (count = 0, hex_index = 0; count < crypto_vrf_OUTPUTBYTES; count++, hex_index += 2) {
    snprintf(VRF_data.vrf_beta_string_data + hex_index, BUFFER_SIZE_NETWORK_BLOCK_DATA - hex_index, "%02x", VRF_data.vrf_beta_string[count] & 0xFF);
  }
*/
  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: block_verifiers_create_vote_results
Description: The block verifiers will create the vote results
Parameters:
  message - The message to send to the block verifiers
Return: 0 if an error has occured, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int block_verifiers_create_vote_results(char* message)
{
  char block_string[BUFFER_SIZE] = {0};
  unsigned char hash_raw[crypto_hash_sha512_BYTES] = {0};
  char hash_hex[DATA_HASH_LENGTH + 1] = {0};

  // Verify block signatures validity
  if (verify_network_block_data(1, 1, "0", BLOCK_VERIFIERS_AMOUNT) == 0) {
    ERROR_PRINT("The MAIN_NODES_TO_NODES_PART_4_OF_ROUND message is invalid");
    return XCASH_ERROR;
  }

  // Convert blockchain_data to network block string
  if (blockchain_data_to_network_block_string(block_string, BLOCK_VERIFIERS_AMOUNT) == 0) {
    ERROR_PRINT("Could not convert the blockchain_data to a network_block_string");
    return XCASH_ERROR;
  }

  // Copy block string to VRF block blob
  //strncpy(VRF_data.block_blob, block_string, sizeof(VRF_data.block_blob) - 1);

  // Hash the block string using SHA512
  crypto_hash_sha512(hash_raw, (const unsigned char*)block_string, strnlen(block_string, BUFFER_SIZE));

  // Convert SHA512 hash to hex string
  for (size_t i = 0; i < DATA_HASH_LENGTH / 2; i++) {
    snprintf(hash_hex + (i * 2), 3, "%02x", hash_raw[i]);
  }

  // Store the result in the global vote structure
  memset(current_round_part_vote_data.current_vote_results, 0, sizeof(current_round_part_vote_data.current_vote_results));
  strncpy(current_round_part_vote_data.current_vote_results, hash_hex, DATA_HASH_LENGTH);
  current_round_part_vote_data.vote_results_valid = 1;
  current_round_part_vote_data.vote_results_invalid = 0;

  // Construct JSON message
  snprintf(message, BUFFER_SIZE,
           "{\r\n \"message_settings\": \"NODES_TO_NODES_VOTE_RESULTS\",\r\n"
           " \"vote_settings\": \"valid\",\r\n \"vote_data\": \"%s\",\r\n}",
           current_round_part_vote_data.current_vote_results);

  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: block_verifiers_create_block_and_update_database
Description: The block verifiers will create the vote results
Parameters:
  message - The message to send to the block verifiers
Return: 0 if an error has occured, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int block_verifiers_create_block_and_update_database(void) {
  char block_with_hash[BUFFER_SIZE] = {0};
  char reserve_entry[BUFFER_SIZE] = {0};
  char reserve_key[BUFFER_SIZE] = {0};
  time_t now;
  struct tm utc;
  size_t reserve_index = 0;
  size_t block_height = 0;
/*
  // Add data hash to the network block string
  if (!add_data_hash_to_network_block_string(VRF_data.block_blob, block_with_hash)) {
    ERROR_PRINT("Failed to add data hash to block");
    return XCASH_ERROR;
  }

  // Add reserve bytes to database
  get_reserve_bytes_database(&reserve_index);
  snprintf(reserve_entry, sizeof(reserve_entry),
           "{\"block_height\":\"%s\",\"reserve_bytes_data_hash\":\"%s\",\"reserve_bytes\":\"%s\"}",
           current_block_height, VRF_data.reserve_bytes_data_hash, VRF_data.block_blob);
  snprintf(reserve_key, sizeof(reserve_key), "reserve_bytes_%zu", reserve_index);
  if (upsert_json_to_db(DATABASE_NAME, XCASH_DB_RESERVE_BYTES, reserve_index, reserve_entry, false) == XCASH_ERROR) {
    ERROR_PRINT("Failed to store reserve bytes to database");
    return XCASH_ERROR;
  }
*/
  // Run reserve proof checks
    sscanf(current_block_height, "%zu", &block_height);
    time(&now);
    gmtime_r(&now, &utc);
    reserve_proofs_delegate_check();

  // Wait until the end of block window
  if (sync_block_verifiers_minutes_and_seconds((BLOCK_TIME - 1), 0) != XCASH_OK)
    return XCASH_ERROR;

  // Submit block if this node is the producer
  bool is_producer = (strncmp(producer_refs[0].public_address, xcash_wallet_public_address, XCASH_WALLET_LENGTH) == 0);
  if (is_producer) {
      if (submit_block_template(block_with_hash) != XCASH_OK) {
          WARNING_PRINT("Failed to submit block to blockchain");
      }
  }

  // Allow other backup nodes to process
  sleep(BLOCK_VERIFIERS_SETTINGS);

  // Final wait to allow propagation
  INFO_STAGE_PRINT("Waiting for block propagation...");
  sync_block_verifiers_minutes_and_seconds((BLOCK_TIME - 1), 40);

  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: start_blocks_create_data
Description: Creates the data for the start block
Parameters:
  message - The data
  network_block_string - The network_block_string
Return: 0 if an error has occured, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int start_blocks_create_data(char* message, char* network_block_string)
{
  // Variables
  char data[BUFFER_SIZE];
  char data2[BUFFER_SIZE];
  char data3[BUFFER_SIZE];
  size_t count;

  const char DATABASE_COLLECTION[] = "reserve_bytes_1";
  memset(data,0,sizeof(data));
  memset(data2,0,sizeof(data2));
  memset(data3,0,sizeof(data3));

  // get a block template
  if (get_block_template(data, BUFFER_SIZE) == 0)
  {
    ERROR_PRINT("Could not get a block template");
    return XCASH_ERROR;
  }

  // convert the network_block_string to blockchain_data
  if (network_block_string_to_blockchain_data((const char*)data,"0",BLOCK_VERIFIERS_AMOUNT) == 0)
  {
    ERROR_PRINT("Could not convert the network_block_string to blockchain_data");
    return XCASH_ERROR;
  }

  // change the nonce to the CONSENSUS_NODE_NETWORK_BLOCK_NONCE
  memcpy(blockchain_data.nonce_data,CONSENSUS_NODE_NETWORK_BLOCK_NONCE,sizeof(CONSENSUS_NODE_NETWORK_BLOCK_NONCE)-1);

  // add the delegates data to the network_block_string
  memset(blockchain_data.blockchain_reserve_bytes.block_producer_delegates_name,0,strnlen(blockchain_data.blockchain_reserve_bytes.block_producer_delegates_name,BUFFER_SIZE));
  memcpy(blockchain_data.blockchain_reserve_bytes.block_producer_delegates_name, "NEWTORK_NODE_0", 13);
  memset(blockchain_data.blockchain_reserve_bytes.block_producer_public_address,0,strnlen(blockchain_data.blockchain_reserve_bytes.block_producer_public_address,BUFFER_SIZE));
  memcpy(blockchain_data.blockchain_reserve_bytes.block_producer_public_address, network_nodes[0].seed_public_address, XCASH_WALLET_LENGTH);
  memset(blockchain_data.blockchain_reserve_bytes.block_producer_node_backup_count,0,strnlen(blockchain_data.blockchain_reserve_bytes.block_producer_node_backup_count,BUFFER_SIZE));
  memcpy(blockchain_data.blockchain_reserve_bytes.block_producer_node_backup_count,"0",sizeof(char));
  memset(blockchain_data.blockchain_reserve_bytes.block_producer_backup_nodes_names, 0, strnlen(blockchain_data.blockchain_reserve_bytes.block_producer_backup_nodes_names, BUFFER_SIZE));
  char backup_nodes_string[128];
  snprintf(backup_nodes_string, sizeof(backup_nodes_string),
    "%s,%s,%s,%s,%s", NETWORK_NODE_0, NETWORK_NODE_0, NETWORK_NODE_0, NETWORK_NODE_0, NETWORK_NODE_0);
  size_t len = strlen(backup_nodes_string);
  memcpy(blockchain_data.blockchain_reserve_bytes.block_producer_backup_nodes_names, backup_nodes_string, len + 1);  // +1 for null terminator

  // add the VRF data
 // if (start_blocks_create_vrf_data() == 0)
 // {
 //   ERROR_PRINT("Could not add the VRF data");
 //   return XCASH_ERROR;
 // }
  
  // add the next block verifiers and add 0`s for the block_validation_node_signature
  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++)
  { 
    memcpy(blockchain_data.blockchain_reserve_bytes.next_block_verifiers_public_address[count],next_block_verifiers_list.block_verifiers_public_key[count],VRF_PUBLIC_KEY_LENGTH);
    memcpy(blockchain_data.blockchain_reserve_bytes.block_validation_node_signature_data[count],GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_SIGNATURE_DATA,sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_SIGNATURE_DATA)-1);
    memcpy(blockchain_data.blockchain_reserve_bytes.block_validation_node_signature[count],GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_SIGNATURE,sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_SIGNATURE)-1);
  }
  
  // convert the blockchain_data to a network_block_string
  memset(data,0,sizeof(data));
  if (blockchain_data_to_network_block_string(data,BLOCK_VERIFIERS_AMOUNT) == 0)
  {
    ERROR_PRINT("Could not convert the blockchain_data to a network_block_string");
    return XCASH_ERROR;
  }

  // sign the network block string
  if (sign_network_block_string(blockchain_data.blockchain_reserve_bytes.block_validation_node_signature[0],data) == 0)
  {
    ERROR_PRINT("Could not sign the network block string");
    return XCASH_ERROR;
  }

  // convert the blockchain_data to a network_block_string
  //memset(VRF_data.block_blob,0,strlen(VRF_data.block_blob));
  //if (blockchain_data_to_network_block_string(VRF_data.block_blob,BLOCK_VERIFIERS_AMOUNT) == 0)
  //{
  //  ERROR_PRINT("Could not convert the blockchain_data to a network_block_string");
  //  return XCASH_ERROR;
  //}

  // add the data hash to the network block string
  memset(network_block_string,0,strlen(network_block_string));
  //if (add_data_hash_to_network_block_string(VRF_data.block_blob,network_block_string) == 0)
  {
    ERROR_PRINT("Could not add the network block string data hash");
    return XCASH_ERROR;
  }

  // update the reserve bytes database
  memset(data2,0,sizeof(data2));
  memcpy(data2,"{\"block_height\":\"",17);
  memcpy(data2+17,current_block_height,strnlen(current_block_height,sizeof(current_block_height)));
  memcpy(data2+strlen(data2),"\",\"reserve_bytes_data_hash\":\"",29);
  //memcpy(data2+strlen(data2),VRF_data.reserve_bytes_data_hash,DATA_HASH_LENGTH);
  memcpy(data2+strlen(data2),"\",\"reserve_bytes\":\"",19);
  //memcpy(data2+strlen(data2),VRF_data.block_blob,strnlen(VRF_data.block_blob,sizeof(data2)));
  memcpy(data2+strlen(data2),"\"}",2);

  // add the network block string to the database
  if (insert_document_into_collection_json(DATABASE_NAME,DATABASE_COLLECTION,data2) == 0)
  {
    ERROR_PRINT("Could not add the new block to the database");
    return XCASH_ERROR;
  }

  // create the message
  memset(message,0,strlen(message));
  memcpy(message,"{\r\n \"message_settings\": \"MAIN_NETWORK_DATA_NODE_TO_BLOCK_VERIFIERS_START_BLOCK\",\r\n \"database_data\": \"",101);
  memcpy(message+101,data2,strnlen(data2,BUFFER_SIZE));
  memcpy(message+strlen(message),"\",\r\n \"reserve_bytes_data_hash\": \"",33);
  //memcpy(message+strlen(message),VRF_data.reserve_bytes_data_hash,DATA_HASH_LENGTH);
  memcpy(message+strlen(message),"\",\r\n}",5);
  
  // sign_data
  if (sign_data(message) == 0)
  { 
    ERROR_PRINT("Could not sign_data");
    return XCASH_ERROR;
  }

  // clear the VRF_data.block_blob so at the start of the next round, the main network data node does not try to update the databases
  //memset(VRF_data.block_blob,0,strlen(VRF_data.block_blob));

  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: start_blocks_create_vrf_data
Description: Creates the VRF data for the start block
Return: 0 if an error has occured, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int start_blocks_create_vrf_data(void)
{
  // Variables
  size_t count;
  size_t count2;
/*
  if (create_random_VRF_keys(VRF_data.vrf_public_key,VRF_data.vrf_secret_key) == 1 && crypto_vrf_is_valid_key((const unsigned char*)VRF_data.vrf_public_key) != 1)
  {
    ERROR_PRINT("Could not create the vrf_public_key or vrf_secret_key");
    return XCASH_ERROR;
  }

  memset(blockchain_data.blockchain_reserve_bytes.previous_block_hash_data,0,strlen(blockchain_data.blockchain_reserve_bytes.previous_block_hash_data));
  memset(blockchain_data.previous_block_hash_data,0,strlen(blockchain_data.previous_block_hash_data));
  memset(VRF_data.vrf_alpha_string,0,strlen((char*)VRF_data.vrf_alpha_string));    
  memcpy(blockchain_data.blockchain_reserve_bytes.previous_block_hash_data,previous_block_hash,BLOCK_HASH_LENGTH);
  memcpy(blockchain_data.previous_block_hash_data,previous_block_hash,BLOCK_HASH_LENGTH);
  memcpy(VRF_data.vrf_alpha_string,previous_block_hash,BLOCK_HASH_LENGTH);
  blockchain_data.previous_block_hash_data_length = BLOCK_HASH_LENGTH;
  blockchain_data.blockchain_reserve_bytes.previous_block_hash_data_length = BLOCK_HASH_LENGTH;

  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++)
  {
    memcpy(VRF_data.vrf_alpha_string+strlen((const char*)VRF_data.vrf_alpha_string),GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_RANDOM_STRING,sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_RANDOM_STRING)-1);
  }   

  // convert the vrf alpha string to a string
  for (count2 = 0, count = 0; count2 < (((RANDOM_STRING_LENGTH*2)*BLOCK_VERIFIERS_AMOUNT) + (BLOCK_HASH_LENGTH*2)) / 2; count2++, count += 2)
  {
    snprintf(VRF_data.vrf_alpha_string_data+count,BUFFER_SIZE-1,"%02x",VRF_data.vrf_alpha_string[count2] & 0xFF);
  }

  if (crypto_vrf_prove(VRF_data.vrf_proof,(const unsigned char*)VRF_data.vrf_secret_key,(const unsigned char*)VRF_data.vrf_alpha_string_data,(unsigned long long)strlen((const char*)VRF_data.vrf_alpha_string_data)) != 0)
  {
    ERROR_PRINT("Could not create the vrf proof");
    return XCASH_ERROR;
  }
  if (crypto_vrf_proof_to_hash(VRF_data.vrf_beta_string,(const unsigned char*)VRF_data.vrf_proof) != 0)
  {
    ERROR_PRINT("Could not create the vrf beta string");
    return XCASH_ERROR;
  }
  if (crypto_vrf_verify(VRF_data.vrf_beta_string,(const unsigned char*)VRF_data.vrf_public_key,(const unsigned char*)VRF_data.vrf_proof,(const unsigned char*)VRF_data.vrf_alpha_string_data,(unsigned long long)strlen((const char*)VRF_data.vrf_alpha_string_data)) != 0)
  {
    ERROR_PRINT("Could not create the VRF data");
    return XCASH_ERROR;
  }

  // convert all of the VRF data to a string
  for (count2 = 0, count = 0; count2 < crypto_vrf_SECRETKEYBYTES; count2++, count += 2)
  {
    snprintf(VRF_data.vrf_secret_key_data+count,BUFFER_SIZE_NETWORK_BLOCK_DATA-1,"%02x",VRF_data.vrf_secret_key[count2] & 0xFF);
  }
  for (count2 = 0, count = 0; count2 < crypto_vrf_PUBLICKEYBYTES; count2++, count += 2)
  {
    snprintf(VRF_data.vrf_public_key_data+count,BUFFER_SIZE_NETWORK_BLOCK_DATA-1,"%02x",VRF_data.vrf_public_key[count2] & 0xFF);
  }
  for (count2 = 0, count = 0; count2 < crypto_vrf_PROOFBYTES; count2++, count += 2)
  {
    snprintf(VRF_data.vrf_proof_data+count,BUFFER_SIZE_NETWORK_BLOCK_DATA-1,"%02x",VRF_data.vrf_proof[count2] & 0xFF);
  }
  for (count2 = 0, count = 0; count2 < crypto_vrf_OUTPUTBYTES; count2++, count += 2)
  {
    snprintf(VRF_data.vrf_beta_string_data+count,BUFFER_SIZE_NETWORK_BLOCK_DATA-1,"%02x",VRF_data.vrf_beta_string[count2] & 0xFF);
  }  

  // add all of the VRF data to the blockchain_data struct
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_secret_key,VRF_data.vrf_secret_key,crypto_vrf_SECRETKEYBYTES);
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_secret_key_data,VRF_data.vrf_secret_key_data,VRF_SECRET_KEY_LENGTH);
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_public_key,VRF_data.vrf_public_key,crypto_vrf_PUBLICKEYBYTES);
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_public_key_data,VRF_data.vrf_public_key_data,VRF_PUBLIC_KEY_LENGTH);
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_alpha_string,VRF_data.vrf_alpha_string,strnlen((const char*)VRF_data.vrf_alpha_string,BUFFER_SIZE));
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_alpha_string_data,VRF_data.vrf_alpha_string_data,strnlen(VRF_data.vrf_alpha_string_data,BUFFER_SIZE));
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_proof,VRF_data.vrf_proof,crypto_vrf_PROOFBYTES);
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_proof_data,VRF_data.vrf_proof_data,VRF_PROOF_LENGTH);
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_beta_string,VRF_data.vrf_beta_string,crypto_vrf_OUTPUTBYTES);
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_beta_string_data,VRF_data.vrf_beta_string_data,VRF_BETA_LENGTH);

  memcpy(blockchain_data.blockchain_reserve_bytes.block_verifiers_vrf_secret_key[0],blockchain_data.blockchain_reserve_bytes.vrf_secret_key,crypto_vrf_SECRETKEYBYTES);
  memcpy(blockchain_data.blockchain_reserve_bytes.block_verifiers_vrf_secret_key_data[0],blockchain_data.blockchain_reserve_bytes.vrf_secret_key_data,VRF_SECRET_KEY_LENGTH);
  memcpy(blockchain_data.blockchain_reserve_bytes.block_verifiers_vrf_public_key[0],blockchain_data.blockchain_reserve_bytes.vrf_public_key,crypto_vrf_PUBLICKEYBYTES);
  memcpy(blockchain_data.blockchain_reserve_bytes.block_verifiers_vrf_public_key_data[0],blockchain_data.blockchain_reserve_bytes.vrf_public_key_data,VRF_PUBLIC_KEY_LENGTH);
  memcpy(blockchain_data.blockchain_reserve_bytes.block_verifiers_random_data[0],GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_RANDOM_STRING,sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_RANDOM_STRING)-1);
  
  for (count = 1; count < BLOCK_VERIFIERS_AMOUNT; count++)
  {
    memcpy(blockchain_data.blockchain_reserve_bytes.block_verifiers_vrf_secret_key_data[count],GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_SECRET_KEY_DATA,sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_SECRET_KEY_DATA)-1);
    memcpy(blockchain_data.blockchain_reserve_bytes.block_verifiers_vrf_public_key_data[count],GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_PUBLIC_KEY_DATA,sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_PUBLIC_KEY_DATA)-1);
    memcpy(blockchain_data.blockchain_reserve_bytes.block_verifiers_random_data[count],GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_RANDOM_STRING,sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_RANDOM_STRING)-1);
  }
*/
  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: start_current_round_start_blocks
Description: Runs the round where the network data node will create the first block of the X-CASH proof of stake block on the network.
Return: 0 if an error has occured, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int start_current_round_start_blocks(void)
{
  // Variables
  char data[BUFFER_SIZE];
  char data2[BUFFER_SIZE];
  memset(data,0,sizeof(data));
  memset(data2,0,sizeof(data2));

  INFO_PRINT("Your block verifier is the main data network node so the first block will be created.");

  // wait until the non network data nodes have synced the previous current and next block verifiers list
  //sleep(30);
  
  // create the data
  if (start_blocks_create_data(data,data2) == 0)
  {
    ERROR_PRINT("Could not create the start blocks data");
    return XCASH_ERROR;
  }

  // set so the main network data node can create the block
  main_network_data_node_create_block = 1;

  // send the database data to all block verifiers
  sleep(BLOCK_VERIFIERS_SETTINGS);
  //block_verifiers_send_data_socket((const char*)data);

  // INFO_PRINT("Waiting for the block producer to submit the block to the network.");
  // sync_block_verifiers_minutes_and_seconds((BLOCK_TIME-1),SUBMIT_NETWORK_BLOCK_TIME_SECONDS);

  // have the main network data node submit the block to the network

  if (submit_block_template(data2) != XCASH_OK) {
    ERROR_PRINT("Could not create the starting block.");
    return XCASH_ERROR;
  }

  main_network_data_node_create_block = 0;

  return XCASH_OK;
}