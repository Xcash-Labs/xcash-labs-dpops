#include "xcash_round.h"

producer_ref_t producer_refs[] = {0};
static int total_delegates = 0;

/**
 * @brief Selects the block producer from the current round’s verifiers using VRF beta comparison.
 *
 * This function scans through the list of block verifiers who submitted valid VRF data,
 * and deterministically selects the one with the lowest VRF beta value as the block producer.
 * The comparison is lexicographic and assumes all submitted beta strings are valid hex strings.
 *
 * @return int The index in `current_block_verifiers_list` of the selected block producer,
 *             or -1 if no valid VRF beta values are found.
 */
int select_block_producer_from_vrf(void) {
  int selected_index = -1;
  char lowest_beta[VRF_BETA_LENGTH + 1] = {0};

  pthread_mutex_lock(&current_block_verifiers_lock);
  for (size_t i = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
    // Skip if no beta submitted or is a seed node

    if (strlen(current_block_verifiers_list.block_verifiers_vrf_beta_hex[i]) != VRF_BETA_LENGTH) {
      continue;
    }

//    Include seed nodes in block production for now
//    if (is_seed_address(current_block_verifiers_list.block_verifiers_public_address[i])) {
//      continue;
//    }

    if (selected_index == -1 ||
        strcmp(current_block_verifiers_list.block_verifiers_vrf_beta_hex[i], lowest_beta) < 0) {
      selected_index = (int)i;
      strncpy(lowest_beta, current_block_verifiers_list.block_verifiers_vrf_beta_hex[i], VRF_BETA_LENGTH);
    }
  }

  if (selected_index != -1) {
    INFO_PRINT("Selected block producer: %s",
               current_block_verifiers_list.block_verifiers_name[selected_index]);
  } else {
    ERROR_PRINT("No valid block producer could be selected.");
  }
  pthread_mutex_unlock(&current_block_verifiers_lock);


  return selected_index;
}

/**
 * @brief Runs a single round of the DPoPS consensus process.
 *
 * This function coordinates the full lifecycle of a consensus round, including:
 *  1. Retrieving the current block height and previous block hash
 *  2. Synchronizing block verifier databases and verifying node majority
 *  3. Broadcasting locally generated VRF data and collecting it from other block verifiers
 *  4. Selecting the block producer using VRF-based randomness
 *  5. Initiating block production on the selected producer node
 *
 * Each stage is sequenced using time-based synchronization (sync_block_verifiers_minutes_and_seconds),
 * and uses `current_round_part` for identification and message signing context.
 *
 * @return xcash_round_result_t - ROUND_OK if block was created and broadcast successfully,
 *                                ROUND_SKIP if round was skipped due to node majority failure,
 *                                ROUND_ERROR on critical errors.
 */
xcash_round_result_t process_round(void) {

for (int j = 0; j < BLOCK_VERIFIERS_TOTAL_AMOUNT; j++) {
    if (strnlen(delegates_all[j].public_address, XCASH_WALLET_LENGTH) > 0) {
        INFO_PRINT("Delegate %d:", j);
        INFO_PRINT("  Name: %s", delegates_all[j].delegate_name);
        INFO_PRINT("  Public Address: %s", delegates_all[j].public_address);
        INFO_PRINT("  Public Key: %s", delegates_all[j].public_key);
        INFO_PRINT("  IP Address: %s", delegates_all[j].IP_address);
        INFO_PRINT("  VRF Proof Hex: %s", delegates_all[j].verifiers_vrf_proof_hex);
        INFO_PRINT("  VRF Beta Hex: %s", delegates_all[j].verifiers_vrf_beta_hex);
        INFO_PRINT("  Vote Total: %d", delegates_all[j].total_vote_count);  // adjust if field name differs
        INFO_PRINT("  Online Status: %s", delegates_all[j].online_status);
    }
}



  INFO_STAGE_PRINT("Part 1 - Check for Delegate Initialization");
  snprintf(current_round_part, sizeof(current_round_part), "%d", 1);
  if (strlen(vrf_public_key) == 0) {
    WARNING_PRINT("Failed to read vrf_public_key for delegate, has this delegate been registered?");
    return ROUND_SKIP;
  }

  if (!is_blockchain_synced()) {
    WARNING_PRINT("Delegate is still syncing, skipping round");
    return ROUND_SKIP;
  }

  // Get the current block height
  INFO_STAGE_PRINT("Part 2 - Get Current Block Height");
  snprintf(current_round_part, sizeof(current_round_part), "%d", 2);
  if (get_current_block_height(current_block_height) != XCASH_OK) {
    ERROR_PRINT("Can't get current block height");
    atomic_store(&wait_for_block_height_init, false);
    return ROUND_ERROR;
  }

  atomic_store(&wait_for_block_height_init, false);
  INFO_STAGE_PRINT("Createing Block: %s", current_block_height);

  INFO_STAGE_PRINT("Part 3 - Check Delegates, Get Previous Block Hash, and Delegates Collection Hash");
  snprintf(current_round_part, sizeof(current_round_part), "%d", 3);
  // delegates_all is loaded prior to start of round due to node timing issues
  total_delegates = 0;
  for (size_t x = 0; x < BLOCK_VERIFIERS_TOTAL_AMOUNT; x++) {
    if (strlen(delegates_all[x].public_address) > 0) {
      total_delegates++;
    }
  }
  if (total_delegates == 0) {
    ERROR_PRINT("No delegates were loaded from the database");
    return ROUND_ERROR;
  }
  DEBUG_PRINT("Found %d active delegates out of %d total slots", total_delegates, BLOCK_VERIFIERS_TOTAL_AMOUNT);

  // Get the previous block hash
  memset(previous_block_hash, 0, BLOCK_HASH_LENGTH);
  if (get_previous_block_hash(previous_block_hash) != XCASH_OK) {
    ERROR_PRINT("Can't get previous block hash");
    return ROUND_SKIP;
  }

  // Get hash for delegates collection
  memset(delegates_hash, 0, sizeof(delegates_hash));
  if (!hash_delegates_collection(delegates_hash)) {
    ERROR_PRINT("Failed to create delegates MD5 hash");
    return ROUND_ERROR;
  }

  INFO_STAGE_PRINT("Part 4 - Sync & Create VRF Data and Send To All Delegates");
  snprintf(current_round_part, sizeof(current_round_part), "%d", 4);

  response_t** responses = NULL;
  char* vrf_message = NULL;
  if (generate_and_request_vrf_data_sync(&vrf_message)) {
    if (xnet_send_data_multi(XNET_DELEGATES_ALL, vrf_message, &responses)) {
      free(vrf_message);
      cleanup_responses(responses);
    } else {
      ERROR_PRINT("Failed to send VRF message.");
      free(vrf_message);
      cleanup_responses(responses);
      return ROUND_ERROR;
    }
  } else {
    ERROR_PRINT("Failed to generate VRF keys and message");
    if (vrf_message != NULL) {
      free(vrf_message);
    }
    return ROUND_ERROR;
  }

  INFO_STAGE_PRINT("Waiting for Sync and VRF Data from all nodes...");
  if (sync_block_verifiers_minutes_and_seconds(0, 30) == XCASH_ERROR) {
    INFO_PRINT("Failed to sync Delegates in the allotted  time, skipping round");
    return ROUND_SKIP;
  }

  INFO_STAGE_PRINT("Part 5 - Checking Block Verifiers Majority and Minimum Online Requirement");
  snprintf(current_round_part, sizeof(current_round_part), "%d", 5);
  // Fill block verifiers list with proven online nodes
  int nodes_majority_count = 0;

  pthread_mutex_lock(&current_block_verifiers_lock);
  memset(&current_block_verifiers_list, 0, sizeof(current_block_verifiers_list));
  for (size_t i = 0, j = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
    if (delegates_all[i].public_address[0] != '\0') {
      if (strcmp(delegates_all[i].online_status, "true") == 0) {
        strcpy(current_block_verifiers_list.block_verifiers_name[j], delegates_all[i].delegate_name);
        strcpy(current_block_verifiers_list.block_verifiers_public_address[j], delegates_all[i].public_address);
        strcpy(current_block_verifiers_list.block_verifiers_public_key[j], delegates_all[i].public_key);
        strcpy(current_block_verifiers_list.block_verifiers_IP_address[j], delegates_all[i].IP_address);
        strcpy(current_block_verifiers_list.block_verifiers_vrf_proof_hex[j], delegates_all[i].verifiers_vrf_proof_hex);
        strcpy(current_block_verifiers_list.block_verifiers_vrf_beta_hex[j], delegates_all[i].verifiers_vrf_beta_hex);
        current_block_verifiers_list.block_verifiers_vote_total[j] = 0;
        current_block_verifiers_list.block_verifiers_voted[j] = 0;
        INFO_PRINT_STATUS_OK("Delegate: %s, Online Status: ", delegates_all[i].delegate_name);
        nodes_majority_count++;
        j++;
      } else {
        INFO_PRINT_STATUS_FAIL("Delegate: %s, Online Status: ", delegates_all[i].delegate_name);
      }
    }
  }
  pthread_mutex_unlock(&current_block_verifiers_lock);
  atomic_store(&wait_for_vrf_init, false);



for (int j = 0; j < BLOCK_VERIFIERS_TOTAL_AMOUNT; j++) {
    if (strnlen(current_block_verifiers_list.block_verifiers_public_address[j], XCASH_WALLET_LENGTH) > 0) {
        INFO_PRINT("Verifier %d:", j);
        INFO_PRINT("  Name: %s", current_block_verifiers_list.block_verifiers_name[j]);
        INFO_PRINT("  Public Address: %s", current_block_verifiers_list.block_verifiers_public_address[j]);
        INFO_PRINT("  Public Key: %s", current_block_verifiers_list.block_verifiers_public_key[j]);
        INFO_PRINT("  IP Address: %s", current_block_verifiers_list.block_verifiers_IP_address[j]);
        INFO_PRINT("  VRF Proof Hex: %s", current_block_verifiers_list.block_verifiers_vrf_proof_hex[j]);
        INFO_PRINT("  VRF Beta Hex: %s", current_block_verifiers_list.block_verifiers_vrf_beta_hex[j]);
        INFO_PRINT("  Vote Total: %d", current_block_verifiers_list.block_verifiers_vote_total[j]);
        INFO_PRINT("  Voted: %d", current_block_verifiers_list.block_verifiers_voted[j]);
    }
}



  // Need at least BLOCK_VERIFIERS_VALID_AMOUNT delegates to start things off, delegates data needs to match for first delegates
  if (nodes_majority_count < BLOCK_VERIFIERS_VALID_AMOUNT) {
    INFO_PRINT_STATUS_FAIL("Failed to reach the required number of online nodes: [%d/%d]", nodes_majority_count, BLOCK_VERIFIERS_VALID_AMOUNT);
    return ROUND_SKIP;
  }

  int delegates_num = (total_delegates < BLOCK_VERIFIERS_AMOUNT) ? total_delegates : BLOCK_VERIFIERS_AMOUNT;
  int required_majority = (delegates_num * MAJORITY_PERCENT + 99) / 100;

  if (nodes_majority_count < required_majority) {
    INFO_PRINT_STATUS_FAIL("Data majority not reached. Online Nodes: [%d/%d]", nodes_majority_count, required_majority);
    return ROUND_SKIP;
  }

  INFO_PRINT_STATUS_OK("Data majority reached. Online Nodes: [%d/%d]", nodes_majority_count, required_majority);

  INFO_STAGE_PRINT("Part 6 - Select Block Creator From VRF Data");
  snprintf(current_round_part, sizeof(current_round_part), "%d", 6);

  // PoS bootstrapping block
  int producer_indx = -1;
  if (strtoull(current_block_height, NULL, 10) == XCASH_PROOF_OF_STAKE_BLOCK_HEIGHT) {
    INFO_PRINT("Seednode 0 will Create first DPOPS block.");
    producer_indx = 0;
  } else {
    producer_indx = select_block_producer_from_vrf();
  }

  if (producer_indx < 0) {
    INFO_STAGE_PRINT("Block Producer not selected, skipping round");
    return ROUND_ERROR;
  }
  
  INFO_STAGE_PRINT("Part 7 - Wait for Block Creator Confirmation by Consensus Vote");
  snprintf(current_round_part, sizeof(current_round_part), "%d", 7);
  
  pthread_mutex_lock(&current_block_verifiers_lock);
  current_block_verifiers_list.block_verifiers_vote_total[producer_indx] += 1;
  for (size_t i = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
    if (strcmp(xcash_wallet_public_address, current_block_verifiers_list.block_verifiers_public_address[i]) == 0) {
      current_block_verifiers_list.block_verifiers_voted[i] = 1;
      memcpy(current_block_verifiers_list.block_verifiers_selected_public_address[i],
        current_block_verifiers_list.block_verifiers_public_address[producer_indx], XCASH_WALLET_LENGTH+1);
      break;
    }
  }
  pthread_mutex_unlock(&current_block_verifiers_lock);

  responses = NULL;
  char* vote_message = NULL;
  if (block_verifiers_create_vote_majority_result(&vote_message, producer_indx)) {
    if (xnet_send_data_multi(XNET_DELEGATES_ALL_ONLINE, vote_message, &responses)) {
      free(vote_message);
      cleanup_responses(responses);
    } else {
      ERROR_PRINT("Failed to send VRF vote result message.");
      free(vote_message);
      cleanup_responses(responses);
      return ROUND_ERROR;
    }
  } else {
    ERROR_PRINT("Failed to generate Vote Majority Result message");
    if (vote_message != NULL) {
      free(vote_message);
    }
    return ROUND_ERROR;
  }

  // Sync start
  if (sync_block_verifiers_minutes_and_seconds(1, 00) == XCASH_ERROR) {
    INFO_PRINT("Failed to Confirm Block Creator in the allotted  time, skipping round");
    return ROUND_SKIP;
  }

  int max_index = -1;
  int max_votes = -1;

  for (size_t i = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
    int votes = current_block_verifiers_list.block_verifiers_vote_total[i];
    if (votes > max_votes) {
      max_votes = votes;
      max_index = (int)i;
    }
  }

  if (max_index != -1) {
    INFO_PRINT("Confirmed Block Winner: %s with %d votes", current_block_verifiers_list.block_verifiers_name[max_index], max_votes);
  } else {
    ERROR_PRINT("No votes recorded");
    return ROUND_ERROR;
  }

  uint8_t vote_hashes[BLOCK_VERIFIERS_AMOUNT][SHA256_EL_HASH_SIZE];
  uint8_t final_vote_hash[SHA256_EL_HASH_SIZE] = {0};
  size_t valid_vote_count = 0;


  pthread_mutex_lock(&current_block_verifiers_lock);
  for (size_t i = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {

    if ( (current_block_verifiers_list.block_verifiers_voted[i] > 0) &&
     (strncmp(current_block_verifiers_list.block_verifiers_selected_public_address[i], 
      current_block_verifiers_list.block_verifiers_public_address[max_index], XCASH_WALLET_LENGTH) == 0) &&
      (current_block_verifiers_list.block_verifiers_public_address[i][0] != '\0')){
      uint8_t signature_bin[64] = {0};
      const char* encoded_sig = current_block_verifiers_list.block_verifiers_vote_signature[i];

      if (strncmp(encoded_sig, "SigV2", 5) == 0) {
        encoded_sig += 5;  // Skip prefix
      }
      size_t decoded_len = 0;
      if (!base64_decode(encoded_sig, signature_bin, SIGNATURE_BIN_LEN, &decoded_len)) {
        ERROR_PRINT("Base64 decode failed");
        pthread_mutex_unlock(&current_block_verifiers_lock);
        return ROUND_ERROR;
      }

      if (decoded_len != SIGNATURE_BIN_LEN) {
        ERROR_PRINT("Unexpected decoded signature length: got %zu, expected %d", decoded_len, SIGNATURE_BIN_LEN);
        pthread_mutex_unlock(&current_block_verifiers_lock);
        return ROUND_ERROR;
      }

      uint8_t hash_input[crypto_vrf_OUTPUTBYTES + crypto_vrf_PUBLICKEYBYTES + 64];
      size_t offset = 0;
      if (!hex_to_byte_array(current_block_verifiers_list.block_verifiers_vrf_beta_hex[i],
                             hash_input + offset,
                             crypto_vrf_OUTPUTBYTES)) {
        ERROR_PRINT("Invalid hex for vrf_beta");
        return ROUND_ERROR;
      }
      offset += crypto_vrf_OUTPUTBYTES;

      // Decode and copy VRF pubkey (32 bytes from 64-char hex)
      if (!hex_to_byte_array(current_block_verifiers_list.block_verifiers_vrf_public_key_hex[i],
                             hash_input + offset,
                             crypto_vrf_PUBLICKEYBYTES)) {
        ERROR_PRINT("Invalid hex for vrf_pubkey");
        pthread_mutex_unlock(&current_block_verifiers_lock);
        return ROUND_ERROR;
      }
      offset += crypto_vrf_PUBLICKEYBYTES;

      memcpy(hash_input + offset,
             signature_bin,
             sizeof(signature_bin));
      offset += sizeof(signature_bin);

      if (offset != sizeof(hash_input)) {
        ERROR_PRINT("Vote hash input length mismatch: got %zu, expected %zu", offset, sizeof(hash_input));
        pthread_mutex_unlock(&current_block_verifiers_lock);
        return ROUND_ERROR;
      }

      sha256EL(hash_input, offset, vote_hashes[valid_vote_count]);
      valid_vote_count++;
    }
  }
  pthread_mutex_unlock(&current_block_verifiers_lock);

  if (valid_vote_count != (size_t)max_votes) {
    INFO_PRINT("Unexpected vote count when creating final vote hash: valid_vote_count = %zu, max_votes = %d",
                valid_vote_count, max_votes);
    return ROUND_SKIP;
  }

  int compare_hashes(const void* a, const void* b) {
    return memcmp(a, b, SHA256_EL_HASH_SIZE);
  }

  qsort(vote_hashes, valid_vote_count, SHA256_EL_HASH_SIZE, compare_hashes);

  if (max_index != producer_indx) {
    ERROR_PRINT("Producer selected by this delegate does not match consensus");
    return ROUND_ERROR;
  }

  // Concatenate all vote_hashes into a buffer
  uint8_t all_hashes_concat[valid_vote_count * SHA256_EL_HASH_SIZE];
  size_t concat_len = valid_vote_count * SHA256_EL_HASH_SIZE;

  for (size_t i = 0; i < valid_vote_count; i++) {
    memcpy(all_hashes_concat + (i * SHA256_EL_HASH_SIZE), vote_hashes[i], SHA256_EL_HASH_SIZE);
  }

  // Final hash of all vote hashes
  sha256EL(all_hashes_concat, concat_len, final_vote_hash);

  char final_vote_hash_hex[SHA256_EL_HASH_SIZE * 2 + 1] = {0};
  for (size_t i = 0; i < SHA256_EL_HASH_SIZE; i++) {
    snprintf(final_vote_hash_hex + (i * 2), 3, "%02x", final_vote_hash[i]);
  }

  DEBUG_PRINT("Final vote hash: %s", final_vote_hash_hex);

  if (max_votes < required_majority) {
    INFO_PRINT_STATUS_FAIL("Data majority not reached. Online Nodes: [%d/%d]", max_votes, required_majority);
    return ROUND_SKIP;
  }

  if (producer_indx >= 0) {
    pthread_mutex_lock(&producer_refs_lock);
    // For now there is only one block producer and no backups
    memset(&producer_refs, 0, sizeof(producer_refs));
    // Populate the reference list with the selected producer
    strcpy(producer_refs[0].public_address, current_block_verifiers_list.block_verifiers_public_address[producer_indx]);
    strcpy(producer_refs[0].IP_address, current_block_verifiers_list.block_verifiers_IP_address[producer_indx]);
    strcpy(producer_refs[0].vrf_public_key, current_block_verifiers_list.block_verifiers_vrf_public_key_hex[producer_indx]);
    strcpy(producer_refs[0].vrf_proof_hex, current_block_verifiers_list.block_verifiers_vrf_proof_hex[producer_indx]);
    strcpy(producer_refs[0].vrf_beta_hex, current_block_verifiers_list.block_verifiers_vrf_beta_hex[producer_indx]);
    strcpy(producer_refs[0].vote_hash_hex, final_vote_hash_hex);
    pthread_mutex_unlock(&producer_refs_lock);
  }

  int block_creation_result = block_verifiers_create_block(final_vote_hash_hex, (uint8_t)valid_vote_count, (uint8_t)nodes_majority_count);

  if (block_creation_result == ROUND_OK) {
    INFO_PRINT_STATUS_OK("Round Successfully Completed For Block %s", current_block_height);
  } else {
    INFO_PRINT("Round skipped by delegate or block round %s was unsuccessful.", current_block_height);
  }

  return (xcash_round_result_t)block_creation_result;
}

/*---------------------------------------------------------------------------------------------------------
Name: start_block_production
Description:
  Main loop for initiating and coordinating block production in the X-Cash DPoPS system.

  - Waits until the local node is fully synchronized with the blockchain before starting.
  - Every BLOCK_TIME window, attempts to create a new round and produce a block.
  - If within the PoS bootstrap phase, only the designated seed node can initiate the round.
  - Handles retry logic, round failures, and optional database reinitialization if needed.
  - Uses the current block height and timing intervals to align with the DPoPS round schedule.

  This function is designed to be run continuously as part of the main production thread.

Parameters:
  None

Returns:
  None
---------------------------------------------------------------------------------------------------------*/
void start_block_production(void) {
  struct timeval current_time;
  xcash_round_result_t round_result;
  bool current_block_healthy = false;

  // Wait for node to be fully synced
  while (!current_block_healthy) {
    if (is_blockchain_synced()) {
      current_block_healthy = true;
    } else {
      WARNING_PRINT("Node is still syncing. Waiting for recovery...");
      sleep(5);
    }
  }

  // set up delegates for first round
  if (!fill_delegates_from_db()) {
    FATAL_ERROR_EXIT("Failed to load and organize delegates for starting round, Possible problem with Mongodb");
  }

  // Start production loop
  while (true) {
    gettimeofday(&current_time, NULL);
    size_t seconds_within_block = current_time.tv_sec % (BLOCK_TIME * 60);
    size_t minute_within_block = (current_time.tv_sec / 60) % BLOCK_TIME;

    // Skip production if outside initial window
    if (seconds_within_block > 5) {
      if (seconds_within_block % 10 == 0) {
        // only print every 10 seconds
        INFO_PRINT("Next round starts in [%ld:%02ld]",
                   BLOCK_TIME - 1 - minute_within_block,
                   59 - (current_time.tv_sec % 60));
      }
      sleep(1);
      continue;
    }

    current_block_height[0] = '\0';
    delegate_db_hash_mismatch = 0;
    atomic_store(&wait_for_vrf_init, true);
    atomic_store(&wait_for_block_height_init, true);
    round_result = ROUND_OK;

    round_result = process_round();

    // Final step - Update DB
    snprintf(current_round_part, sizeof(current_round_part), "%d", 12);

    if (round_result == ROUND_OK) {

      INFO_STAGE_PRINT("Waiting To Verify Block Creation...");
      if (sync_block_verifiers_minutes_and_seconds(1, 51) == XCASH_ERROR) {
        DEBUG_PRINT("Failed to sync in the allotted time");
      }

      for (size_t i = 0; i < BLOCK_VERIFIERS_TOTAL_AMOUNT; i++) {
        if (strlen(delegates_all[i].public_address) > 0 && strlen(delegates_all[i].public_key) > 0) {
          if (strcmp(delegates_all[i].online_status, delegates_all[i].online_status_orginal) == 0) {
            DEBUG_PRINT("No change to online status, update skipped...");
          } else {
            bool is_primary = false;

#ifdef SEED_NODE_ON
            if (is_primary_node()) {
              is_primary = true;
            }
#endif

            if (!is_seed_node || is_primary) {
              char tmp_status[6] = "false";
              if (strcmp(delegates_all[i].online_status, "true") == 0) {
                strcpy(tmp_status, "true");
              }

              bson_t filter;
              bson_t update_fields;
              bson_init(&filter);
              BSON_APPEND_UTF8(&filter, "public_key", delegates_all[i].public_key);
              bson_init(&update_fields);
              BSON_APPEND_UTF8(&update_fields, "online_status", tmp_status);
              if (update_document_from_collection_bson(DATABASE_NAME, DB_COLLECTION_DELEGATES, &filter, &update_fields) != XCASH_OK) {
                ERROR_PRINT("Failed to update online_status for delegate %s", delegates_all[i].public_address);
              }

              bson_destroy(&filter);
              bson_destroy(&update_fields);
           }
          }

// Only update statics on seed nodes
#ifdef SEED_NODE_ON

          if (is_primary_node()) {

            INFO_PRINT("Updating Statistics");
            char ck_block_height[BLOCK_HEIGHT_LENGTH + 1] = {0};
            if (get_current_block_height(ck_block_height) != XCASH_OK) {
              ERROR_PRINT("Can't get current block height");
              goto end_of_round_skip_block;
            }

            uint64_t ck_height = strtoull(ck_block_height, NULL, 10);
            uint64_t cur_height = strtoull(current_block_height, NULL, 10);

            if (ck_height != cur_height + 1) {
              ERROR_PRINT("New block was not created by the selected block producer");
              goto end_of_round_skip_block;
            }

            if (!is_blockchain_synced()) {
              ERROR_PRINT("Blockchain is not synced");
              goto end_of_round_skip_block;
            }

            uint64_t tmp_verifier_total_round = 0;
            uint64_t tmp_verifier_online_total_rounds = 0;
            uint64_t tmp_producer_total_rounds = 0;

            if (get_statistics_totals_by_public_key(delegates_all[i].public_key, &tmp_verifier_total_round, &tmp_verifier_online_total_rounds,
                                                    &tmp_producer_total_rounds) == XCASH_OK) {

              if (strcmp(delegates_all[i].online_status, "true") == 0) {
                tmp_verifier_online_total_rounds += 1;
                if (i < BLOCK_VERIFIERS_AMOUNT) {
                  tmp_verifier_total_round += 1;

                  if (strcmp(delegates_all[i].public_address, producer_refs[0].public_address) == 0) {
                    tmp_producer_total_rounds += 1;
                  }
                }
              }
              
              bson_t filter_stat;
              bson_t update_fields_stat;
              bson_init(&filter_stat);
              bson_init(&update_fields_stat);
              
              BSON_APPEND_UTF8(&filter_stat, "public_key", delegates_all[i].public_key);

              BSON_APPEND_INT64(&update_fields_stat, "block_verifier_total_rounds", tmp_verifier_total_round);
              BSON_APPEND_INT64(&update_fields_stat, "block_verifier_online_total_rounds", tmp_verifier_online_total_rounds);
              BSON_APPEND_INT64(&update_fields_stat, "block_producer_total_rounds", tmp_producer_total_rounds);

              if (update_document_from_collection_bson(DATABASE_NAME, DB_COLLECTION_STATISTICS, &filter_stat, &update_fields_stat) != XCASH_OK) {
                ERROR_PRINT("Failed to update statistics for delegate %s", delegates_all[i].public_address);
              }

              bson_destroy(&filter_stat);
              bson_destroy(&update_fields_stat);


              DEBUG_PRINT("Updated delegate %s: total=%" PRIu64 ", online=%" PRIu64 ", produced=%" PRIu64,
                         delegates_all[i].public_address,
                         tmp_verifier_total_round,
                         tmp_verifier_online_total_rounds,
                         tmp_producer_total_rounds);
            } else {
              ERROR_PRINT("Failed retrieve and update of statistics for delegate %s", delegates_all[i].public_address);
            }
          }

#endif

        }
      }
    } else {
      // Check if registered, if not check db again
      if (strlen(vrf_public_key) == 0) {
        get_vrf_public_key();
      }
      // if not registered no need to continue
      if (strlen(vrf_public_key) != 0) {
        INFO_STAGE_PRINT("Round skipped or delegate still initializing - waiting to sync...");
        if (sync_block_verifiers_minutes_and_seconds(1, 51) == XCASH_ERROR) {
          DEBUG_PRINT("Failed to sync in the allotted time");
        }

        // If more that a 30% mismatch lets resync the node
        if ((delegate_db_hash_mismatch * 100) > (total_delegates * 30)) {
          int selected_index;
          pthread_mutex_lock(&delegates_all_lock);
          selected_index = select_random_online_delegate();
          pthread_mutex_unlock(&delegates_all_lock);

          if (create_sync_token() == XCASH_OK) {
            if (!create_delegates_db_sync_request(selected_index)) {
              ERROR_PRINT("Error occured while syncing delegates");
            }
            if (sync_block_verifiers_minutes_and_seconds(1, 58) == XCASH_ERROR) {
              ERROR_PRINT("Failed to sync in the allotted time");
            }
          } else {
            ERROR_PRINT("Error creating sync token"); 
          }

        }
      }
    }

  end_of_round_skip_block:
    // set up delegates for next round
    if (!fill_delegates_from_db()) {
      FATAL_ERROR_EXIT("Failed to load and organize delegates for next round, Possible problem with Mongodb");
    }
  }

}