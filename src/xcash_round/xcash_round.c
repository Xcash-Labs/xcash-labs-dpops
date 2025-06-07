#include "xcash_round.h"

producer_ref_t producer_refs[] = {0};

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

  pthread_mutex_lock(&majority_vrf_lock);
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
  pthread_mutex_unlock(&majority_vrf_lock);

  if (selected_index != -1) {
    INFO_PRINT("Selected block producer: %s",
               current_block_verifiers_list.block_verifiers_name[selected_index]);
  } else {
    ERROR_PRINT("No valid block producer could be selected.");
  }

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

  if (vrf_public_key[0] == '\0') {
    WARNING_PRINT("Failed to read vrf_public_key for delegate, has this delegate been registered?");
    get_vrf_public_key();
    sleep(5);
    return ROUND_SKIP;
  }

  // Get the current block height
  INFO_STAGE_PRINT("Part 1 - Get Current Block Height");
  snprintf(current_round_part, sizeof(current_round_part), "%d", 2);
  if (get_current_block_height(current_block_height) != XCASH_OK) {
    ERROR_PRINT("Can't get current block height");
    return ROUND_ERROR;
  }
  atomic_store(&wait_for_block_height_init, false);
  INFO_STAGE_PRINT("Attempting To Create Block: %s", current_block_height);

  INFO_STAGE_PRINT("Part 2 - Check Delegates Data, Get Previous Block Hash, and Delegates Hash");
  snprintf(current_round_part, sizeof(current_round_part), "%d", 2);
  // delegates_all is loaded prior to start of round due to node timing issues
  int total_delegates = 0;
  for (size_t x = 0; x < BLOCK_VERIFIERS_AMOUNT; x++) {
    if (strlen(delegates_all[x].public_address) > 0) {
      total_delegates++;
    }
  }
  if (total_delegates == 0) {
    ERROR_PRINT("Can't get previous block hash");
    return ROUND_ERROR;
  }
  DEBUG_PRINT("Found %d active delegates out of %d total slots", total_delegates, BLOCK_VERIFIERS_AMOUNT);

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

  INFO_STAGE_PRINT("Part 3 - Send Sync message to all Delegates and wait for replies");
  snprintf(current_round_part, sizeof(current_round_part), "%d", 3);

  response_t** responses = NULL;
  char* sync_message = NULL;
  if (create_sync_msg(&sync_message)) {
    if (xnet_send_data_multi(XNET_DELEGATES_ALL, sync_message, &responses)) {
      free(sync_message);
      cleanup_responses(responses);
    } else {
      ERROR_PRINT("Failed to send SYNC message.");
      free(sync_message);
      cleanup_responses(responses);
      return ROUND_ERROR;
    }
  } else {
    ERROR_PRINT("Failed to generate SYNC message");
    free(sync_message);  // safe even if NULL
    return ROUND_ERROR;
  }

  INFO_STAGE_PRINT("Waiting for Delegates to sync...");
  if (sync_block_verifiers_minutes_and_seconds(0, 30) == XCASH_ERROR) {
    INFO_PRINT("Failed to sync Delegates in the aloted time, skipping round");
    return ROUND_SKIP;
  }

  INFO_STAGE_PRINT("Part 4 - Checking Block Verifiers Majority and Minimum Online Requirement");
  snprintf(current_round_part, sizeof(current_round_part), "%d", 4);
  // Fill block verifiers list with proven online nodes
  int nodes_majority_count = 0;

  pthread_mutex_lock(&delegates_mutex);
  pthread_mutex_lock(&majority_vrf_lock);
  memset(&current_block_verifiers_list, 0, sizeof(current_block_verifiers_list));
  for (size_t i = 0, j = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
    if (delegates_all[i].public_address[0] != '\0') {
      if (strcmp(delegates_all[i].online_status, "true") == 0) {
        strcpy(current_block_verifiers_list.block_verifiers_name[j], delegates_all[i].delegate_name);
        strcpy(current_block_verifiers_list.block_verifiers_public_address[j], delegates_all[i].public_address);
        strcpy(current_block_verifiers_list.block_verifiers_public_key[j], delegates_all[i].public_key);
        strcpy(current_block_verifiers_list.block_verifiers_IP_address[j], delegates_all[i].IP_address);
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
  pthread_mutex_unlock(&majority_vrf_lock);
  pthread_mutex_unlock(&delegates_mutex);
  atomic_store(&wait_for_vrf_init, false);

  if (nodes_majority_count < BLOCK_VERIFIERS_VALID_AMOUNT) {
    INFO_PRINT_STATUS_FAIL("Failed to reach the required number of online nodes: [%d/%d]", nodes_majority_count, BLOCK_VERIFIERS_VALID_AMOUNT);
    return ROUND_SKIP;
  }

  int required_majority = (total_delegates * MAJORITY_PERCENT + 99) / 100;

  if (nodes_majority_count < required_majority) {
    INFO_PRINT_STATUS_FAIL("Data majority not reached. Online Nodes: [%d/%d]", nodes_majority_count, required_majority);
    return ROUND_SKIP;
  }

  INFO_PRINT_STATUS_OK("Data majority reached. Online Nodes: [%d/%d]", nodes_majority_count, required_majority);

  INFO_STAGE_PRINT("Part 5 - Create VRF Data and Send To All Block Verifiers");
  snprintf(current_round_part, sizeof(current_round_part), "%d", 5);

  responses = NULL;
  char* vrf_message = NULL;
  if (generate_and_request_vrf_data_msg(&vrf_message)) {
    if (xnet_send_data_multi(XNET_DELEGATES_ALL_ONLINE, vrf_message, &responses)) {
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

  // Sync start
  if (sync_block_verifiers_minutes_and_seconds(1, 00) == XCASH_ERROR) {
    INFO_PRINT("Failed to sync VRF data in the aloted time, skipping roung");
    return ROUND_SKIP;
  }

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
  
  pthread_mutex_lock(&majority_vote_lock);
  current_block_verifiers_list.block_verifiers_vote_total[producer_indx] += 1;
  for (size_t i = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
    if (strcmp(xcash_wallet_public_address, current_block_verifiers_list.block_verifiers_public_address[i]) == 0) {
      current_block_verifiers_list.block_verifiers_voted[i] = 1;
      break;
    }
  }
  pthread_mutex_unlock(&majority_vote_lock);

  responses = NULL;
  char* vote_message = NULL;
  if (block_verifiers_create_vote_majority_result(&vote_message, producer_indx)) {
    if (xnet_send_data_multi(XNET_DELEGATES_ALL_ONLINE, vote_message, &responses)) {
      free(vote_message);
      cleanup_responses(responses);
    } else {
      ERROR_PRINT("Failed to send VRF message.");
      free(vote_message);
      cleanup_responses(responses);
      return ROUND_ERROR;
    }
  } else {
    ERROR_PRINT("Failed to generate VRF keys and message");
    if (vote_message != NULL) {
      free(vote_message);
    }
    return ROUND_ERROR;
  }

  // Sync start
  if (sync_block_verifiers_minutes_and_seconds(1, 30) == XCASH_ERROR) {
    INFO_PRINT("Failed to Confirm Block Creator in the aloted time, skipping roung");
    return ROUND_SKIP;
  }

  for (size_t i = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
    if (current_block_verifiers_list.block_verifiers_public_address[i][0] != '\0' &&
     current_block_verifiers_list.block_verifiers_voted[i] > 0) {
      DEBUG_PRINT(
          "Name: %s"
          " VRF Vote count: %d"
          " Voted: %u\n",
          current_block_verifiers_list.block_verifiers_name[i],
          current_block_verifiers_list.block_verifiers_vote_total[i],
          current_block_verifiers_list.block_verifiers_voted[i]);
    }
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
    INFO_PRINT("Most voted verifier: %s with %d votes",
      current_block_verifiers_list.block_verifiers_name[max_index],
      max_votes);
  } else {
    ERROR_PRINT("No votes recorded");
    return ROUND_ERROR;
  }

  if (max_index != producer_indx) {
    ERROR_PRINT("Producer selected by this delegate does not match consensus");
    return ROUND_ERROR;
  }

  if (max_votes < required_majority) {
    INFO_PRINT_STATUS_FAIL("Data majority not reached. Online Nodes: [%d/%d]", max_votes, required_majority);
    return ROUND_SKIP;
  }

  if (producer_indx >= 0) {
    pthread_mutex_lock(&majority_vrf_lock);
    // For now there is only one block producer and no backups
    memset(&producer_refs, 0, sizeof(producer_refs));
    // Populate the reference list with the selected producer
    strcpy(producer_refs[0].public_address, current_block_verifiers_list.block_verifiers_public_address[producer_indx]);
    strcpy(producer_refs[0].IP_address, current_block_verifiers_list.block_verifiers_IP_address[producer_indx]);
    strcpy(producer_refs[0].vrf_public_key, current_block_verifiers_list.block_verifiers_vrf_public_key_hex[producer_indx]);
    strcpy(producer_refs[0].random_buf_hex, current_block_verifiers_list.block_verifiers_random_hex[producer_indx]);
    strcpy(producer_refs[0].vrf_proof_hex, current_block_verifiers_list.block_verifiers_vrf_proof_hex[producer_indx]);
    strcpy(producer_refs[0].vrf_beta_hex, current_block_verifiers_list.block_verifiers_vrf_beta_hex[producer_indx]);
    pthread_mutex_unlock(&majority_vrf_lock);
  }

  INFO_STAGE_PRINT("Starting block production for block %s", current_block_height);
  int block_creation_result = block_verifiers_create_block();

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

    if (round_result == ROUND_OK) {
      for (size_t i = 0; i < BLOCK_VERIFIERS_TOTAL_AMOUNT; i++) {
        if (delegates_all[i].public_address != NULL && strlen(delegates_all[i].public_address) > 0) {

          char filter_json[SMALL_BUFFER_SIZE];
          char update_json[SMALL_BUFFER_SIZE];

          snprintf(filter_json, sizeof(filter_json), "{\"public_address\":\"%s\"}", delegates_all[i].public_address);

          uint64_t tmp_verifier_total_round = delegates_all[i].block_verifier_total_rounds;
          uint64_t tmp_verifier_online_total_rounds = delegates_all[i].block_verifier_online_total_rounds;
          if (strcmp(delegates_all[i].online_status, "true") == 0) {
            tmp_verifier_online_total_rounds += 1; 
            if (i <= 49) {
              tmp_verifier_total_round += 1; 
            }
          }

          uint64_t tmp_producer_total_rounds = delegates_all[i].block_producer_total_rounds;
          if (strcmp(delegates_all[i].public_address, producer_refs[0].public_address) == 0) {
            tmp_producer_total_rounds += 1;
          }

          snprintf(update_json, sizeof(update_json),
                   "{"
                   "\"online_status\":\"%s\","
                   "\"block_verifier_total_rounds\":%" PRIu64 ","
                   "\"block_verifier_online_total_rounds\":%" PRIu64 ","
                   "\"block_producer_total_rounds\":%" PRIu64 "}",
                   delegates_all[i].online_status,
                   tmp_verifier_total_round,
                   tmp_verifier_online_total_rounds,
                   tmp_producer_total_rounds);

          INFO_PRINT("Updated delegate %s: total=%" PRIu64 ", online=%" PRIu64 ", produced=%" PRIu64,
            delegates_all[i].public_address,
            tmp_verifier_total_round,
            tmp_verifier_online_total_rounds,
            tmp_producer_total_rounds);

          if (update_document_from_collection(DATABASE_NAME, DB_COLLECTION_DELEGATES, filter_json, update_json) != XCASH_OK) {
            ERROR_PRINT("Failed to update online_status for delegate %s", delegates_all[i].public_address);
          }
        }        
      }
    } else {
      if (delegate_db_hash_mismatch > 2) {
        // check whether we need a full resync, xcash_wallet_public_address don't pick self
        // TODO: call your sync routine here, e.g.: init_db_from_top();
      }
    }

    // set up delegates for next round
    if (!fill_delegates_from_db()) {
      FATAL_ERROR_EXIT("Failed to load and organize delegates for next round, Possible problem with Mongodb");
    }
  }
}