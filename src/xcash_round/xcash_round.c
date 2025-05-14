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

  pthread_mutex_lock(&majority_vote_lock);
  for (size_t i = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
    // Skip if no beta submitted or is a seed node
    if (strncmp(current_block_verifiers_list.block_verifiers_vrf_beta_hex[i], "", 1) == 0 ||
        is_seed_address(current_block_verifiers_list.block_verifiers_public_address[i])) {
      continue;
    }

    if (selected_index == -1 ||
        strcmp(current_block_verifiers_list.block_verifiers_vrf_beta_hex[i], lowest_beta) < 0) {
      selected_index = (int)i;
      strncpy(lowest_beta, current_block_verifiers_list.block_verifiers_vrf_beta_hex[i], VRF_BETA_LENGTH);
    }
  }
  pthread_mutex_unlock(&majority_vote_lock);

  if (selected_index != -1) {
    INFO_PRINT("Selected block producer: %s",
               current_block_verifiers_list.block_verifiers_public_address[selected_index]);
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
  // last, current, and next delegtes load in fill_delegates_from_db - clean up not needed --------------------
  //  INFO_STAGE_PRINT("Part 1 - Initial Network Block Verifiers Sync");
  //  snprintf(current_round_part, sizeof(current_round_part), "%d", 1);
  // Update with fresh delegates list
  //  if (!fill_delegates_from_db()) {
  //   ERROR_PRINT("Can't read delegates list from DB");
  //    return ROUND_ERROR_RD;
  //  }

  //  delegates_loaded = true; // This is set back to false that the end of the round

  // Get the current block height
  INFO_STAGE_PRINT("Part 1 - Get Current Block Height");
  snprintf(current_round_part, sizeof(current_round_part), "%d", 2);
  if (get_current_block_height(current_block_height) != XCASH_OK) {
    ERROR_PRINT("Can't get current block height");
    return ROUND_SKIP;
  }
  INFO_PRINT("Creating Block: %s", current_block_height);

  INFO_STAGE_PRINT("Part 2 - Check Delegates Data, Get Previous Block Hash, and Delegates Hash");
  snprintf(current_round_part, sizeof(current_round_part), "%d", 2);
  // delegates_all is loaded prior to start of round due to node timing issues
  int total_delegates = 0;
  for (size_t x = 0; x < BLOCK_VERIFIERS_TOTAL_AMOUNT; x++) {
    if (strlen(delegates_all[x].public_address) > 0) {
      total_delegates++;
    }
  }
  if (total_delegates == 0) {
    ERROR_PRINT("Can't get previous block hash");
    return ROUND_SKIP;
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
  if (sync_block_verifiers_minutes_and_seconds(0, 25) == XCASH_ERROR) {
    INFO_PRINT("Failed to sync Delegates in the aloted time");
    return ROUND_ERROR;
  }

  INFO_STAGE_PRINT("Part 4 - Checking Block Verifiers Majority and Minimum Online Requirement");
  snprintf(current_round_part, sizeof(current_round_part), "%d", 4);
  // Fill block verifiers list with proven online nodes
  int nodes_majority_count = 0;
  pthread_mutex_lock(&majority_vote_lock);

  for (size_t i = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
    if (strlen(delegates_all[i].delegate_name) == 0) {
      continue;  // Skip uninitialized entries
    }
  }

  memset(&current_block_verifiers_list, 0, sizeof(current_block_verifiers_list));
  for (size_t i = 0, j = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
    if (delegates_all[i].public_address != NULL && delegates_all[i].public_address[0] != '\0') {
      if (strcmp(delegates_all[i].online_status_ck, "true") == 0) {
        strcpy(current_block_verifiers_list.block_verifiers_name[j], delegates_all[i].delegate_name);
        strcpy(current_block_verifiers_list.block_verifiers_public_address[j], delegates_all[i].public_address);
        strcpy(current_block_verifiers_list.block_verifiers_public_key[j], delegates_all[i].public_key);
        strcpy(current_block_verifiers_list.block_verifiers_IP_address[j], delegates_all[i].IP_address);
        INFO_PRINT_STATUS_OK("Delegate: %s, Online Status: ", delegates_all[i].delegate_name);
        nodes_majority_count++;
        j++;
      } else {
        INFO_PRINT_STATUS_FAIL("Delegate: %s, Online Status: ", delegates_all[i].delegate_name);
      }
    }
  }
  pthread_mutex_unlock(&majority_vote_lock);
  DEBUG_PRINT("Received sync info from %d delegates", nodes_majority_count);

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
  // This message is defines as NONRETURN and no responses are expected
  if (generate_and_request_vrf_data_msg(&vrf_message)) {
    DEBUG_PRINT("Generated VRF message: %s", vrf_message);
    if (xnet_send_data_multi(XNET_DELEGATES_ALL_ONLINE, vrf_message, &responses)) {
      DEBUG_PRINT("Message sent to all online delegates.");
    } else {
      ERROR_PRINT("Failed to send VRF message.");
    }
    free(vrf_message);
  } else {
    ERROR_PRINT("Failed to generate VRF keys and message");
    if (vrf_message != NULL) {
      free(vrf_message);
    }
    return ROUND_ERROR;
  }

  
  return ROUND_SKIP;

  // Sync start
  if (sync_block_verifiers_minutes_and_seconds(0, 30) == XCASH_ERROR) {
    INFO_PRINT("Failed to sync VRF data in the aloted time");
    return ROUND_ERROR;
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
    return ROUND_SKIP;
  } else {
    pthread_mutex_lock(&majority_vote_lock);

    // For now there is only one block producer and no backups
    memset(&producer_refs, 0, sizeof(producer_refs));
    // Populate the reference list with the selected producer
    strcpy(producer_refs[0].public_address, current_block_verifiers_list.block_verifiers_public_address[producer_indx]);
    strcpy(producer_refs[0].IP_address, current_block_verifiers_list.block_verifiers_IP_address[producer_indx]);
    strcpy(producer_refs[0].vrf_public_key, current_block_verifiers_list.block_verifiers_vrf_public_key_hex[producer_indx]);
    strcpy(producer_refs[0].random_buf_hex, current_block_verifiers_list.block_verifiers_random_hex[producer_indx]);
    strcpy(producer_refs[0].vrf_proof_hex, current_block_verifiers_list.block_verifiers_vrf_proof_hex[producer_indx]);
    strcpy(producer_refs[0].vrf_beta_hex, current_block_verifiers_list.block_verifiers_vrf_beta_hex[producer_indx]);

    pthread_mutex_unlock(&majority_vote_lock);
  }

  INFO_STAGE_PRINT("Starting block production for block %s", current_block_height);
  int block_creation_result = block_verifiers_create_block();

  if (block_creation_result == ROUND_OK) {
    INFO_PRINT_STATUS_OK("Block %s created successfully", current_block_height);
  } else {
    INFO_PRINT_STATUS_FAIL("Block %s was not created", current_block_height);
  }

  // Set to blank so we start fresh at top of round
  current_block_height[0] = '\0';

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
  xcash_round_result_t round_result = ROUND_OK;
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

  // set up delegates for round
  if (!fill_delegates_from_db()) {
    ERROR_PRINT("Failed to load and organize delegates from DB");
    // maybe sync the delegates collection and try again... Then fatal
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

    round_result = process_round();

    if (round_result != ROUND_OK) {
      for (size_t i = 0; i < BLOCK_VERIFIERS_TOTAL_AMOUNT; i++) {
        if (strcmp(delegates_all[i].public_address, xcash_wallet_public_address) == 0) {
          // Found current delegate
          if (strcmp(delegates_all[i].online_status, delegates_all[i].online_status_ck) != 0) {
            DEBUG_PRINT("Updating Online status...");
            strncpy(delegates_all[i].online_status, delegates_all[i].online_status_ck,
                    sizeof(delegates_all[i].online_status));
            delegates_all[i].online_status[sizeof(delegates_all[i].online_status) - 1] = '\0';

            // update online status in collection later

            if (round_result == ROUND_ERROR_RD) {
              // need to add code to sync the delegates collection
              //        init_db_from_top();  // --------------------------------------------------------------------?????
            }
          }
          break;
        }
      }
    }

    // set up delegates for next round
    if (!fill_delegates_from_db()) {
      ERROR_PRINT("Failed to load and organize delegates for next round");
      // need to add code to sync the delegates collection amd maybe retry???
    }
  }
}