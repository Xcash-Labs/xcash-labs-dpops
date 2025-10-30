#include "xcash_round.h"

producer_ref_t producer_refs[] = {0};
static int total_delegates = 0;
static char previous_round_block_hash[BLOCK_HASH_LENGTH + 1] = {0};
static bool last_round_success = false;

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

    // Do not include seed nodes in block production
    if (is_seed_address(current_block_verifiers_list.block_verifiers_public_address[i])) {
      continue;
    }

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

// Helper routine
static int compare_hashes(const void* a, const void* b) {
  return memcmp(a, b, SHA256_EL_HASH_SIZE);
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
 *                                ROUND_SKIP if round was skipped due to xcashd error or round stage took too long
 *                                ROUND_ERROR on critical errors attempt refresh
 */
xcash_round_result_t process_round(void) {
  memset(&producer_refs, 0, sizeof(producer_refs));

  INFO_STAGE_PRINT("Part 1 - Check Delegate Registration");
  snprintf(current_round_part, sizeof(current_round_part), "%d", 1);
  if (strlen(vrf_public_key) == 0) {
    WARNING_PRINT("Failed to read vrf_public_key, has this delegate been registered?");
    return ROUND_SKIP;
  }

  INFO_STAGE_PRINT("Part 2 - Check Delegates, Get Previous Block Hash, and Delegates Collection Hash");
  snprintf(current_round_part, sizeof(current_round_part), "%d", 2);
  // delegates_all is loaded prior to start of round due to node timing issues
  bool delegate_not_found = true;
  total_delegates = 0;
  pthread_mutex_lock(&delegates_all_lock);
  for (size_t x = 0; x < BLOCK_VERIFIERS_TOTAL_AMOUNT; x++) {
    if (strlen(delegates_all[x].public_address) > 0) {
      total_delegates++;
      if (strcmp(delegates_all[x].public_address, xcash_wallet_public_address) == 0) {
        delegate_not_found = false;
      }
    }
  }
  pthread_mutex_unlock(&delegates_all_lock);
  if (total_delegates == 0) {
    ERROR_PRINT("No delegates were loaded from the database");
    return ROUND_ERROR;
  }
  DEBUG_PRINT("Found %d active delegates out of %d total slots", total_delegates, BLOCK_VERIFIERS_TOTAL_AMOUNT);

  // Check if this node is active
  if (delegate_not_found) {
    WARNING_PRINT("This delegate was not found in delegates_all. If recently registered or updated, activation can take up to 10 minutes.");
    return ROUND_SKIP;
  }

  if (last_round_success) {
    // Get the previous block hash and check to make sure it changed from last round
    snprintf(previous_round_block_hash, sizeof previous_round_block_hash, "%s", previous_block_hash);
    for (int attempt = 1; attempt <= 2; ++attempt) {
      memset(previous_block_hash, 0, BLOCK_HASH_LENGTH + 1);
      if (get_previous_block_hash(previous_block_hash) != XCASH_OK) {
        ERROR_PRINT("Can't get previous block hash");
        return ROUND_SKIP;
      }
      // Success condition: the previous hash changed vs what we had
      if (strcmp(previous_block_hash, previous_round_block_hash) != 0) {
        break;
      }
      if (attempt >= 2) {
        ERROR_PRINT("Still showing Previous Block Hash, Block did not advance");
        return ROUND_SKIP;
      }
      INFO_PRINT("Previous hash unchanged, retrying");
      sleep(2);
    }
  } else {
    // No majority last round -> chain may be stalled
    memset(previous_block_hash, 0, sizeof previous_block_hash);
    if (get_previous_block_hash(previous_block_hash) != XCASH_OK) {
      ERROR_PRINT("Can't get previous block hash");
      return ROUND_SKIP;
    }
    INFO_PRINT("No majority last round; skipping hash tip-advance check");
  }

  // Get hash for delegates collection
  memset(delegates_hash, 0, sizeof(delegates_hash));
  if (!hash_delegates_collection(delegates_hash)) {
    ERROR_PRINT("Failed to create delegates MD5 hash");
    return ROUND_SKIP;
  }

  // Get the current block height and wait to complete before sending or reading transactions
  INFO_STAGE_PRINT("Part 3 - Get Current Block Height");
  snprintf(current_round_part, sizeof(current_round_part), "%d", 3);

  char target_height[BLOCK_HEIGHT_LENGTH + 1] = {0};
  char cheight[BLOCK_HEIGHT_LENGTH + 1] = {0};
  if (!is_blockchain_synced(target_height, cheight)) {
    ERROR_PRINT("Blockchain is not synced, skipping round");
    return ROUND_SKIP;
  }

  if (get_current_block_height(current_block_height) != XCASH_OK) {
    ERROR_PRINT("Can't get current block height");
    atomic_store(&wait_for_block_height_init, false);
    return ROUND_SKIP;
  }

  atomic_store(&wait_for_block_height_init, false);
  INFO_STAGE_PRINT("Creating Block: %s", current_block_height);

  INFO_STAGE_PRINT("Part 4 - Sync & Create VRF Data and Send To All Delegates");
  snprintf(current_round_part, sizeof(current_round_part), "%d", 4);
  sleep(1);  // needed for nodes time not being exact
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
  if (sync_block_verifiers_minutes_and_seconds(0, 15) == XCASH_ERROR) {
    INFO_PRINT("Failed to sync Delegates in the allotted  time, skipping round");
    return ROUND_SKIP;
  }

  INFO_STAGE_PRINT("Part 5 - Checking Block Verifiers Majority and Minimum Online Requirement");
  snprintf(current_round_part, sizeof(current_round_part), "%d", 5);
  // Fill block verifiers list with proven online nodes
  int online_count = 0;

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
        online_count++;
        j++;
      } else {
        INFO_PRINT_STATUS_FAIL("Delegate: %s, Online Status: ", delegates_all[i].delegate_name);
      }
    }
  }
  pthread_mutex_unlock(&current_block_verifiers_lock);
  atomic_store(&wait_for_vrf_init, false);

  // Need at least BLOCK_VERIFIERS_VALID_AMOUNT delegates to start things off, delegates data needs to match for first delegates
  if (online_count < BLOCK_VERIFIERS_VALID_AMOUNT) {
    INFO_PRINT_STATUS_FAIL("Failed to reach the required number of online nodes: %d  Minimum Required: %d", online_count, BLOCK_VERIFIERS_VALID_AMOUNT);
    return ROUND_ERROR;
  }

  int delegates_num = (total_delegates < BLOCK_VERIFIERS_AMOUNT) ? total_delegates : BLOCK_VERIFIERS_AMOUNT;
  int quorum_needed = (delegates_num * MAJORITY_PERCENT + 99) / 100;  // 70% of nodes online
  int agreement_needed = (2 * delegates_num + 2) / 3;


  if (online_count < quorum_needed) {
    INFO_PRINT_STATUS_FAIL("Quorum not reached. Total: %d  Online: %d  Need: %d (>= %d%%)", delegates_num, online_count, quorum_needed, MAJORITY_PERCENT);
    return ROUND_ERROR;
  }

  INFO_PRINT_STATUS_OK("Quorum reached. Online: %d/%d  Need: %d", online_count, delegates_num, quorum_needed);


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
             current_block_verifiers_list.block_verifiers_public_address[producer_indx], XCASH_WALLET_LENGTH + 1);
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
  if (sync_block_verifiers_minutes_and_seconds(0, 30) == XCASH_ERROR) {
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
    if ((current_block_verifiers_list.block_verifiers_voted[i] > 0) &&
        (strncmp(current_block_verifiers_list.block_verifiers_selected_public_address[i],
                 current_block_verifiers_list.block_verifiers_public_address[max_index], XCASH_WALLET_LENGTH) == 0) &&
        (current_block_verifiers_list.block_verifiers_public_address[i][0] != '\0')) {
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
        pthread_mutex_unlock(&current_block_verifiers_lock);
        return ROUND_ERROR;
      }
      offset += crypto_vrf_OUTPUTBYTES;

      // Decode and copy VRF pubkey (32 bytes from 64-char hex)
      if (!hex_to_byte_array(current_block_verifiers_list.block_verifiers_public_key[i],
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

  char final_vote_hash_hex[VOTE_HASH_LEN + 1] = {0};
  for (size_t i = 0; i < SHA256_EL_HASH_SIZE; i++) {
    snprintf(final_vote_hash_hex + (i * 2), 3, "%02x", final_vote_hash[i]);
  }

  DEBUG_PRINT("Final vote hash: %s", final_vote_hash_hex);

  if (max_votes < agreement_needed) {
    INFO_PRINT_STATUS_FAIL("Consensus not reached: Votes: %d (need ≥ %d of N=%d)", max_votes, agreement_needed, delegates_num);
    return ROUND_ERROR;
  }
  INFO_PRINT_STATUS_OK("Consensus reached: Votes: %d (required %d)", max_votes, agreement_needed);

  if (producer_indx >= 0) {
    pthread_mutex_lock(&producer_refs_lock);
    // For now there is only one block producer and no backups. Populate the reference list with the selected producer.
    strcpy(producer_refs[0].public_address, current_block_verifiers_list.block_verifiers_public_address[producer_indx]);
    strcpy(producer_refs[0].IP_address, current_block_verifiers_list.block_verifiers_IP_address[producer_indx]);
    strcpy(producer_refs[0].vrf_public_key, current_block_verifiers_list.block_verifiers_public_key[producer_indx]);
    strcpy(producer_refs[0].vrf_proof_hex, current_block_verifiers_list.block_verifiers_vrf_proof_hex[producer_indx]);
    strcpy(producer_refs[0].vrf_beta_hex, current_block_verifiers_list.block_verifiers_vrf_beta_hex[producer_indx]);
    strcpy(producer_refs[0].vote_hash_hex, final_vote_hash_hex);
    pthread_mutex_unlock(&producer_refs_lock);
  }

  int block_creation_result = block_verifiers_create_block(final_vote_hash_hex, (uint8_t)valid_vote_count, (uint8_t)online_count);

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
  char target_height[BLOCK_HEIGHT_LENGTH + 1] = {0};
  char cheight[BLOCK_HEIGHT_LENGTH + 1] = {0};

  // Wait for node to be fully synced
  bool not_synced = true;
  while (not_synced && !atomic_load(&shutdown_requested)) {
    if (!is_blockchain_synced(target_height, cheight)) {
      INFO_PRINT("Synchronizing blockchain");
      sleep(5);
      continue;
    }
    unsigned long long node_h = strtoull(cheight, NULL, 10);
    unsigned long long target_h = strtoull(target_height, NULL, 10);
    if (target_height[0] == '\0' || target_h == 0 || cheight[0] == '\0' || node_h == 0) {
      INFO_PRINT("Synchronizing blockchain: current height / target height: %s / %s", cheight, target_height);
      sleep(5);
      continue;
    }

    INFO_PRINT("current height / target height: %s / %s", cheight, target_height);

 
    if (target_h == 0ULL || cheight == 0ULL) {
      ERROR_PRINT("Error converting string heights to number");
      atomic_store(&shutdown_requested, true);
    }
    if (node_h < target_h) {
      INFO_PRINT("Delegate is still syncing, node is at %s and the target height is %s", cheight, target_height);
      sleep(5);
      continue;
    }
    not_synced = false;
  }

  INFO_PRINT("Waiting for block production to start");
  sync_block_verifiers_minutes_and_seconds(0, 58);
  // set up delegates for first round
  if (!fill_delegates_from_db()) {
    ERROR_PRINT("Failed to load and organize delegates for starting round, Possible problem with Mongodb");
    return;
  }

  // Start production loop
  static bool printed_on_enter = false;
  static time_t last_log_sec = 0;
  startup_complete = true;
  // Might not be used yet but lets initialize 
  if (create_sync_token() == XCASH_ERROR) {
    ERROR_PRINT("Error creating sync token");
  }

  while (!atomic_load(&shutdown_requested)) {

    for (;;) {
      gettimeofday(&current_time, NULL);
      long within = current_time.tv_sec % BLOCK_TIME_SEC;

      if (within <= 1) {  // entry window: 0..1 sec
        printed_on_enter = false;
        break;
      }

      long remain = BLOCK_TIME_SEC - within;  // time until boundary

      if (!printed_on_enter || (current_time.tv_sec - last_log_sec) >= 1) {
        INFO_PRINT("Next round starts in [%ld:%02ld]", remain / 60, remain % 60);
        printed_on_enter = true;
        last_log_sec = current_time.tv_sec;
      }

      // 1s sleep that resumes after EINTR
      unsigned int left = 1;
      while ((left = sleep(left)) > 0) { /* continue on signal */
      }
    }

    current_block_height[0] = '\0';
    delegate_db_hash_mismatch = 0;
    atomic_store(&wait_for_vrf_init, true);
    atomic_store(&wait_for_block_height_init, true);
    round_result = ROUND_OK;

    round_result = process_round();

    // Final step - Wait for block creation/DB Updates or Node clean-up
    snprintf(current_round_part, sizeof(current_round_part), "%d", 12);
    if (round_result == ROUND_OK) {
      last_round_success = true;
      INFO_STAGE_PRINT("Part 12 - Wait for Block Creation");
    } else  {
      last_round_success = false;
      INFO_STAGE_PRINT("Part 12 - Wait for Node clean-up / sync");
    }

    // 10 secs to perform cleanup or add stats and other info
    if (sync_block_verifiers_minutes_and_seconds(0, 50) == XCASH_ERROR) {
      INFO_PRINT("Failed to create block in the allotted time, skipping round");
      goto end_of_round_skip_block;
    }

    if (round_result == ROUND_SKIP) {
      if (strlen(vrf_public_key) == 0) {
        get_vrf_public_key();
      }
      goto end_of_round_skip_block;
    }

    if (round_result == ROUND_OK) {
      // Update online status
      for (size_t i = 0; i < BLOCK_VERIFIERS_TOTAL_AMOUNT; i++) {
        if (strlen(delegates_all[i].public_address) > 0 && strlen(delegates_all[i].public_key) > 0) {
          if (strcmp(delegates_all[i].online_status, delegates_all[i].online_status_orginal) != 0) {
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
              goto end_of_round_skip_block;
            }

            bson_destroy(&filter);
            bson_destroy(&update_fields);
          }
        }
      }

// If not a seed node - Add block record only on delegate that found block.  Seed nodes are be part of block create process
#ifndef SEED_NODE_ON

    const bool block_found = (strcmp(xcash_wallet_public_address, producer_refs[0].public_address) == 0);
    if (block_found) {
      char nblock_hash[BLOCK_HASH_LENGTH + 1] = {0};
      uint64_t reward_atomic = 0;
      uint64_t ts_epoch = 0;
      bool is_orphan = false;
      uint64_t block_create_height = strtoull(current_block_height, NULL, 10);  // block that was just created
      int rc = get_block_info_by_height(block_create_height, nblock_hash, sizeof(nblock_hash), &reward_atomic, &ts_epoch, &is_orphan);
      if (rc == XCASH_OK) {
        if (!is_orphan) {
          bson_t doc;
          bson_init(&doc);
          BSON_APPEND_UTF8(&doc, "_id", nblock_hash);
          BSON_APPEND_INT64(&doc, "block_height", (int64_t)block_create_height);
          BSON_APPEND_INT64(&doc, "block_reward", (int64_t)reward_atomic);
          BSON_APPEND_BOOL(&doc, "processed", false);
          BSON_APPEND_DATE_TIME(&doc, "timestamp", (int64_t)ts_epoch * 1000);
          if (insert_document_into_collection_bson(DATABASE_NAME, DB_COLLECTION_BLOCKS_FOUND, &doc) != 1) {
            ERROR_PRINT("Failed to record block: hash=%s height=%llu reward=%llu (epoch=%llu) collection=%s",
                        nblock_hash, (unsigned long long)block_create_height, (unsigned long long)reward_atomic,
                        (unsigned long long)ts_epoch,
                        DB_COLLECTION_BLOCKS_FOUND);
          }
          bson_destroy(&doc);
        }

      } else {
        ERROR_PRINT("Function get_block_info_by_height (%llu) failed", (unsigned long long)block_create_height);
        goto end_of_round_skip_block;
      }
    }

#endif

#ifdef SEED_NODE_ON

    char current_block_hash[BLOCK_HASH_LENGTH + 1] = {0};
    char ck_block_height[BLOCK_HEIGHT_LENGTH + 1] = {0};
    unsigned long long cbheight = strtoull(current_block_height, NULL, 10);

    if (get_current_block_height(ck_block_height) != XCASH_OK) {
      ERROR_PRINT("Failed to get current block height");
      goto end_of_round_skip_block;
    }

    unsigned long long ck_height = strtoull(ck_block_height, NULL, 10);
    if (ck_height <= cbheight) {
      sleep(4);

      memset(ck_block_height, 0, sizeof ck_block_height);
      if (get_current_block_height(ck_block_height) != XCASH_OK) {
        ERROR_PRINT("Failed to get current block height (retry)");
        goto end_of_round_skip_block;
      }

      // If block height does not advance after a second try, skip (on of other seed nodes will update)
      ck_height = strtoull(ck_block_height, NULL, 10);
      if (ck_height <= cbheight) {
        ERROR_PRINT("Block did not advance (still at %llu)", (unsigned long long)cbheight);
        goto end_of_round_skip_block;
      }
    }

    if (get_current_block_hash(current_block_hash) != XCASH_OK) {
      ERROR_PRINT("Can't get current block hash");
      goto end_of_round_skip_block;
    }

    mongoc_client_t* c = mongoc_client_pool_pop(database_client_thread_pool);
    if (!c) {
      ERROR_PRINT("Mongo client pop failed");
      goto end_of_round_skip_block;
    }

    // ** update the statistics collection (one increment per height, keyed by public_key) **
    {
      mongoc_collection_t* stats =
          mongoc_client_get_collection(c, DATABASE_NAME, DB_COLLECTION_STATISTICS);
      if (!stats) {
        ERROR_PRINT("get_collection(%s) failed", DB_COLLECTION_STATISTICS);
        mongoc_client_pool_push(database_client_thread_pool, c);
        goto end_of_round_skip_block;
      }

      for (size_t i = 0; i < BLOCK_VERIFIERS_TOTAL_AMOUNT; i++) {
        if (!delegates_all[i].public_key[0]) continue;
        if (!delegates_all[i].public_address[0]) continue;

        const bool online = (strcmp(delegates_all[i].online_status, "true") == 0);
        const bool is_verifier = (i < BLOCK_VERIFIERS_AMOUNT);
        const bool is_producer = is_verifier &&
                                 (strcmp(delegates_all[i].public_address, producer_refs[0].public_address) == 0);

        // Filter: by public_key AND only if we haven't counted this height yet
        bson_t filter;
        bson_init(&filter);
        BSON_APPEND_UTF8(&filter, "_id", delegates_all[i].public_key);

        bson_t arr, or0, or1, lt, exists;
        bson_append_array_begin(&filter, "$or", -1, &arr);

        // { last_counted_block: { $lt: cbheight } }
        bson_init(&or0);
        bson_init(&lt);
        BSON_APPEND_INT64(&lt, "$lt", (int64_t)cbheight);
        BSON_APPEND_DOCUMENT(&or0, "last_counted_block", &lt);
        bson_destroy(&lt);
        bson_append_document(&arr, "0", -1, &or0);
        bson_destroy(&or0);

        // { last_counted_block: { $exists: false } }
        bson_init(&or1);
        bson_init(&exists);
        BSON_APPEND_BOOL(&exists, "$exists", false);
        BSON_APPEND_DOCUMENT(&or1, "last_counted_block", &exists);
        bson_destroy(&exists);
        bson_append_document(&arr, "1", -1, &or1);
        bson_destroy(&or1);

        bson_append_array_end(&filter, &arr);

        // $inc counters (Mongo is fine with 0)
        bson_t inc;
        bson_init(&inc);
        BSON_APPEND_INT64(&inc, "block_verifier_total_rounds", is_verifier ? 1 : 0);
        BSON_APPEND_INT64(&inc, "block_verifier_online_total_rounds", (is_verifier && online) ? 1 : 0);
        BSON_APPEND_INT64(&inc, "block_producer_total_rounds", is_producer ? 1 : 0);

        // $set watermark
        bson_t set;
        bson_init(&set);
        BSON_APPEND_INT64(&set, "last_counted_block", (int64_t)cbheight);

        // update = { $inc: {...}, $set: {...} }
        bson_t update;
        bson_init(&update);
        BSON_APPEND_DOCUMENT(&update, "$inc", &inc);
        BSON_APPEND_DOCUMENT(&update, "$set", &set);

        // IMPORTANT: no upsert here (docs are created at startup/registration)
        bson_error_t err;
        if (!mongoc_collection_update_one(stats, &filter, &update, /*opts*/ NULL, NULL, &err)) {
          ERROR_PRINT("stats update failed pk=%.12s… h=%llu: %s",
                      delegates_all[i].public_key, (unsigned long long)cbheight, err.message);
        }

        // cleanup
        bson_destroy(&update);
        bson_destroy(&set);
        bson_destroy(&inc);
        bson_destroy(&filter);
      }

      mongoc_collection_destroy(stats);
    }

    // ** update the consensus_rounds collection **
    {
      if (producer_refs[0].public_address[0] == '\0' ||
          !is_hex_len(producer_refs[0].vrf_public_key, VRF_PUBLIC_KEY_LENGTH)) {
        ERROR_PRINT("[round write] invariant: missing/invalid winner at height=%llu",
                    (unsigned long long)cbheight);
        mongoc_client_pool_push(database_client_thread_pool, c);
        goto end_of_round_skip_block;
      }

      // ----- get collection in outer scope so it's visible in cleanup -----
      mongoc_collection_t* coll = mongoc_client_get_collection(c, DATABASE_NAME, DB_COLLECTION_ROUNDS);
      if (!coll) {
        ERROR_PRINT("get_collection(%s) failed", DB_COLLECTION_ROUNDS);
        mongoc_client_pool_push(database_client_thread_pool, c);
        goto end_of_round_skip_block;
      }

      // Filter: { block_height: <cbheight> }
      bson_t filter;
      bson_init(&filter);
      BSON_APPEND_INT64(&filter, "block_height", (int64_t)cbheight);

      // --- before hex→bin, validate hex sizes ---
      if (!is_hex_len(previous_block_hash, BLOCK_HASH_LENGTH) ||
          !is_hex_len(current_block_hash, BLOCK_HASH_LENGTH) ||
          !is_hex_len(producer_refs[0].vote_hash_hex, 64)) {
        ERROR_PRINT("[round write] bad hex length(s) at height=%llu",
                    (unsigned long long)cbheight);
        bson_destroy(&filter);
        mongoc_collection_destroy(coll);
        mongoc_client_pool_push(database_client_thread_pool, c);
        goto end_of_round_skip_block;
      }

      // --- decode round-level hex to binary ---
      uint8_t prev_hash_bin[32], block_hash_bin[32], vote_hash_bin[32];
      if (!hex_to_byte_array(previous_block_hash, prev_hash_bin, sizeof prev_hash_bin) ||
          !hex_to_byte_array(current_block_hash, block_hash_bin, sizeof block_hash_bin) ||
          !hex_to_byte_array(producer_refs[0].vote_hash_hex, vote_hash_bin, sizeof vote_hash_bin)) {
        ERROR_PRINT("[round write] hex→bin decode failed at height=%llu", (unsigned long long)cbheight);
        bson_destroy(&filter);
        mongoc_collection_destroy(coll);
        mongoc_client_pool_push(database_client_thread_pool, c);
        goto end_of_round_skip_block;
      }

      // $setOnInsert with round data (one-time fields)
      bson_t soi;
      bson_init(&soi);
      BSON_APPEND_INT64(&soi, "block_height", (int64_t)cbheight);  // REQUIRED
      BSON_APPEND_BINARY(&soi, "prev_block_hash", BSON_SUBTYPE_BINARY, prev_hash_bin, sizeof prev_hash_bin);
      BSON_APPEND_BINARY(&soi, "block_hash", BSON_SUBTYPE_BINARY, block_hash_bin, sizeof block_hash_bin);
      BSON_APPEND_BINARY(&soi, "vote_hash", BSON_SUBTYPE_BINARY, vote_hash_bin, sizeof vote_hash_bin);

      // ts_decided ONLY on insert
      int64_t now_ms = 0;
      {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        now_ms = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
      }
      BSON_APPEND_DATE_TIME(&soi, "ts_decided", now_ms);

      // block_verifiers array (skip empty), VRF fields stored as binary
      bson_t arr;
      if (!bson_append_array_begin(&soi, "block_verifiers", -1, &arr)) {
        ERROR_PRINT("append_array_begin(block_verifiers) failed");
        goto build_fail;
      }

      uint32_t out = 0;  // make sure this is initialized before the loop
      for (uint32_t k = 0; k < BLOCK_VERIFIERS_AMOUNT; ++k) {
        const char* addr = current_block_verifiers_list.block_verifiers_public_address[k];
        if (!addr || addr[0] == '\0') continue;

        uint8_t pk_bin[32], proof_bin[80], beta_bin[64];
        if (!hex_to_byte_array(current_block_verifiers_list.block_verifiers_public_key[k], pk_bin, 32) ||
            !hex_to_byte_array(current_block_verifiers_list.block_verifiers_vrf_proof_hex[k], proof_bin, 80) ||
            !hex_to_byte_array(current_block_verifiers_list.block_verifiers_vrf_beta_hex[k], beta_bin, 64)) {
          WARNING_PRINT("[round write] verifier hex→bin decode failed (k=%u) height=%llu",
                        k, (unsigned long long)cbheight);
          continue;
        }
        // Build array element key safely
        const char* keyptr = NULL;
        char keybuf[16];
        bson_uint32_to_string(out, &keyptr, keybuf, sizeof keybuf);
        bson_t item;
        if (!bson_append_document_begin(&arr, keyptr, -1, &item)) {
          ERROR_PRINT("append_document_begin failed for index=%u", out);
          // close array to keep doc consistent, then bail to cleanup
          bson_append_array_end(&soi, &arr);
          goto build_fail;
        }
        // Bound addr length to avoid strlen walks
        size_t addrlen = strnlen(addr, XCASH_WALLET_LENGTH + 1);
        if (addrlen == 0 || addrlen > XCASH_WALLET_LENGTH) {
          ERROR_PRINT("bad public_address length=%zu at k=%u", addrlen, k);
          bson_append_document_end(&arr, &item);
          continue;
        }
        if (!bson_append_utf8(&item, "public_address", -1, addr, (int)addrlen) ||
            !bson_append_binary(&item, "vrf_public_key", -1, BSON_SUBTYPE_BINARY, pk_bin, 32) ||
            !bson_append_binary(&item, "vrf_proof", -1, BSON_SUBTYPE_BINARY, proof_bin, 80) ||
            !bson_append_binary(&item, "vrf_beta", -1, BSON_SUBTYPE_BINARY, beta_bin, 64)) {
          ERROR_PRINT("append field(s) failed at k=%u", k);
          bson_append_document_end(&arr, &item);
          continue;
        }
        bson_append_document_end(&arr, &item);
        ++out;
      }

      if (!bson_append_array_end(&soi, &arr)) {
        ERROR_PRINT("append_array_end(block_verifiers) failed");
        goto build_fail;
      }

      // winner subdoc (no index stored; keep address string, key binary)
      {
        if (producer_refs[0].public_address[0] == '\0' ||
            !is_hex_len(producer_refs[0].vrf_public_key, VRF_PUBLIC_KEY_LENGTH)) {
          ERROR_PRINT("[round write] invariant: missing/invalid winner at height=%llu",
                      (unsigned long long)cbheight);
          goto build_fail;
        }

        const char* waddr = producer_refs[0].public_address;
        const char* wkeyh = producer_refs[0].vrf_public_key;
        size_t wlen = strnlen(waddr, XCASH_WALLET_LENGTH + 1);
        if (wlen == 0 || wlen > XCASH_WALLET_LENGTH) {
          ERROR_PRINT("[round write] winner address length invalid");
          goto build_fail;
        }

        uint8_t wkey_bin[32] = {0};
        if (!hex_to_byte_array(wkeyh, wkey_bin, 32)) {
          ERROR_PRINT("[round write] winner key decode failed");
          goto build_fail;
        }

        bson_t wdoc;
        if (!bson_append_document_begin(&soi, "winner", -1, &wdoc)) {
          ERROR_PRINT("append_document_begin(winner) failed");
          goto build_fail;
        }
        if (!bson_append_utf8(&wdoc, "public_address", -1, waddr, (int)wlen) ||
            !bson_append_binary(&wdoc, "vrf_public_key", -1, BSON_SUBTYPE_BINARY, wkey_bin, 32)) {
          ERROR_PRINT("append fields(winner) failed");
          bson_append_document_end(&soi, &wdoc);
          goto build_fail;
        }
        bson_append_document_end(&soi, &wdoc);
      }

      // ---- Validate final doc BEFORE update ----
      {
        size_t bad_off = 0;
        if (!bson_validate(&soi, BSON_VALIDATE_NONE, &bad_off)) {
          char* dump = bson_as_canonical_extended_json(&soi, NULL);
          ERROR_PRINT("BSON validate failed at offset=%zu; dump=%s",
                      bad_off, dump ? dump : "(null)");
          if (dump) bson_free(dump);
          goto build_fail;
        }
      }

      // Update: { $setOnInsert: soi }
      bson_t update;
      bson_init(&update);
      BSON_APPEND_DOCUMENT(&update, "$setOnInsert", &soi);

      // Upsert: true
      bson_t opts;
      bson_init(&opts);
      BSON_APPEND_BOOL(&opts, "upsert", true);

      // One atomic call
      bson_error_t err;
      bson_t reply;
      bson_init(&reply);

      {
        bool ok = mongoc_collection_update_one(coll, &filter, &update, &opts, &reply, &err);
        if (!ok) {
          const bool is_dup =
              mongoc_error_has_label(&reply, "DuplicateKey") ||
              (err.domain == MONGOC_ERROR_SERVER &&
               (err.code == 11000 || err.code == 11001 || err.code == 12582));

          if (!is_dup) {
            WARNING_PRINT("[round write] upsert %s height=%llu failed: %s",
                          DB_COLLECTION_ROUNDS, (unsigned long long)cbheight, err.message);
            // fallthrough to cleanup as error
          }
        }
      }

      // cleanup success
      bson_destroy(&reply);
      bson_destroy(&opts);
      bson_destroy(&update);
      bson_destroy(&soi);
      bson_destroy(&filter);
      mongoc_collection_destroy(coll);
      mongoc_client_pool_push(database_client_thread_pool, c);
      goto end_of_round_skip_block;

    // ------------- unified error cleanup -------------
    build_fail:
      bson_destroy(&soi);
      bson_destroy(&filter);
      mongoc_collection_destroy(coll);
      mongoc_client_pool_push(database_client_thread_pool, c);
      goto end_of_round_skip_block;
    }

#endif

    } else {
      // If >20% of delegates report a DB hash mismatch, trigger a resync.
      if (delegate_db_hash_mismatch > 0) {
        if ((delegate_db_hash_mismatch * 100) > (total_delegates * 20)) {
          if (is_seed_node && strncmp(xcash_wallet_public_address, network_nodes[0].seed_public_address, XCASH_WALLET_LENGTH) != 0) {
            DEBUG_PRINT("Skipping resync (not seed node #1)");
          } else {
            INFO_STAGE_PRINT("Delegates Collection is out of sync, attempting to update");
            int selected_index;
            pthread_mutex_lock(&delegates_all_lock);
            selected_index = select_random_online_delegate();
            pthread_mutex_unlock(&delegates_all_lock);
            if (create_sync_token() == XCASH_OK) {
              if (create_delegates_db_sync_request(selected_index)) {
                INFO_PRINT("Waiting for DB sync");
              } else {
                ERROR_PRINT("Error occured while syncing delegates");
              }
            } else {
              ERROR_PRINT("Error creating sync token");
            }
          }
        }
      }
    }

  end_of_round_skip_block:
    sync_block_verifiers_minutes_and_seconds(0, 58);
    // set up delegates for next round; retry on transient failure
    bool ok = false;
    pthread_mutex_lock(&delegates_all_lock);
    ok = fill_delegates_from_db();
    pthread_mutex_unlock(&delegates_all_lock);

    if (!ok) {
      ERROR_PRINT("Failed to load and organize delegates for next round, MongoDB or network error");
      sleep(5);  // make sure we miss the next block window and wait for next round
    }

  }
}