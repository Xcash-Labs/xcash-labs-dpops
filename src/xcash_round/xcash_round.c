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
 *                                ROUND_SKIP if round was skipped due to node majority failure or not selected as producer,
 *                                ROUND_ERROR on critical errors.
 */
xcash_round_result_t process_round(void) {
  // Get the current block height Then Sync the databases and build the majority list
  INFO_STAGE_PRINT("Part 1 - Get Current Block Height and Previous Block Hash");
  snprintf(current_round_part, sizeof(current_round_part), "%d", 1);
  if (get_current_block_height(current_block_height) != XCASH_OK) {
    ERROR_PRINT("Can't get current block height");
    return ROUND_ERROR;
  }

  // Get the previous block hash
  memset(previous_block_hash, 0, BLOCK_HASH_LENGTH);
  if (get_previous_block_hash(previous_block_hash) != XCASH_OK) {
    ERROR_PRINT("Can't get previous block hash");
    return ROUND_ERROR;
  }

  INFO_STAGE_PRINT("Part 2 - Initial Network Block Verifiers Sync");
  snprintf(current_round_part, sizeof(current_round_part), "%d", 2);
  // Update with fresh delegates list
  if (!fill_delegates_from_db()) {
    DEBUG_PRINT("Can't read delegates list from DB");
    free(nodes_majority_list);
    return ROUND_ERROR;
  }

  size_t network_majority_count = 0;
  xcash_node_sync_info_t* nodes_majority_list = NULL;
  if (!initial_db_sync_check(&network_majority_count, &nodes_majority_list) || !nodes_majority_list) {
    WARNING_PRINT("Can't sync databases with network majority");
    free(nodes_majority_list);
    return ROUND_ERROR;
  }

  // Update online status from majority list
  INFO_STAGE_PRINT("Nodes online for block %s", current_block_height);

  // do I need to update the db status of a delegate here?????????? wait until after round

  for (size_t i = 0; i < BLOCK_VERIFIERS_TOTAL_AMOUNT && strlen(delegates_all[i].public_address) > 0; i++) {
    strcpy(delegates_all[i].online_status, "false");

    for (size_t j = 0; j < network_majority_count; j++) {
      if (strcmp(delegates_all[i].public_address, nodes_majority_list[j].public_address) == 0) {
        strcpy(delegates_all[i].online_status, "true");
        INFO_PRINT_STATUS_OK("Node: " BLUE_TEXT("%-30s"), delegates_all[i].delegate_name);
        break;
      }
    }
  }

  free(nodes_majority_list);  // Clean up the majority list after use

  // need to update this for prodction needs to be 75% response ????

  // Check if we have enough nodes for block production
  if (network_majority_count < BLOCK_VERIFIERS_VALID_AMOUNT) {
    INFO_PRINT_STATUS_FAIL("Nodes majority: [%ld/%d]", network_majority_count, BLOCK_VERIFIERS_VALID_AMOUNT);
    return ROUND_SKIP;
  }

  INFO_PRINT_STATUS_OK("Nodes majority: [%ld/%d]", network_majority_count, BLOCK_VERIFIERS_VALID_AMOUNT);

  // Fill block verifiers list with proven online nodes
  pthread_mutex_lock(&majority_vote_lock);
  memset(&current_block_verifiers_list, 0, sizeof(current_block_verifiers_list));
  for (size_t i = 0, j = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
    strcpy(current_block_verifiers_list.block_verifiers_name[j], delegates_all[i].delegate_name);
    strcpy(current_block_verifiers_list.block_verifiers_public_address[j], delegates_all[i].public_address);
    strcpy(current_block_verifiers_list.block_verifiers_public_key[j], delegates_all[i].public_key);
    strcpy(current_block_verifiers_list.block_verifiers_IP_address[j], delegates_all[i].IP_address);
    j++;
  }
  pthread_mutex_unlock(&majority_vote_lock);

  // Sync start
  if (sync_block_verifiers_minutes_and_seconds(0, 30) == XCASH_ERROR)
      return ROUND_SKIP;


  INFO_STAGE_PRINT("Part 3 - Create VRF Data and Send To All Block Verifiers");
  snprintf(current_round_part, sizeof(current_round_part), "%d", 3);
  response_t** responses = NULL;
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

  // Sync start
  if (sync_block_verifiers_minutes_and_seconds(1, 0) == XCASH_ERROR)
      return ROUND_SKIP;

  INFO_STAGE_PRINT("Part 4 - Select Block Creator From VRF Data");
  snprintf(current_round_part, sizeof(current_round_part), "%d", 4);

  // PoS bootstrapping block
  int producer_indx = -1;
  if (strtoull(current_block_height, NULL, 10) == XCASH_PROOF_OF_STAKE_BLOCK_HEIGHT) {
    INFO_PRINT("Creating first DPOPS block.");
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
//    if (is_blockchain_synced()) {
    if (get_current_block_height(current_block_height) == XCASH_OK) {
      current_block_healthy = true;
    } else {
      WARNING_PRINT("Node is still syncing. Waiting for recovery...");
      sleep(5);
    }
  }

  // Start production loop
  while (true) {
    gettimeofday(&current_time, NULL);
    size_t seconds_within_block = current_time.tv_sec % (BLOCK_TIME * 60);
    size_t minute_within_block = (current_time.tv_sec / 60) % BLOCK_TIME;

    // Skip production if outside initial window
    if (seconds_within_block > 25) {
      if (round_result != ROUND_OK && seconds_within_block > 280) {
        WARNING_PRINT("Last round failed. Refreshing DB from top...");
        init_db_from_top();  // --------------------------------------------------------------------?????
        round_result = ROUND_OK;
      } else {
        INFO_PRINT("Block %s — Next round starts in [%ld:%02ld]",
                   current_block_height,
                   BLOCK_TIME - 1 - minute_within_block,
                   59 - (current_time.tv_sec % 60));
      }
      sleep(5);
      continue;
    }

    bool round_created = false;

    // Standard block production
    round_result = process_round();
    if (round_result == ROUND_OK) {
      round_created = true;
    } else if (round_result == ROUND_RETRY) {
      INFO_PRINT("Round retry. Waiting before trying ...");
      sleep(10);  // Allow 2 retries max within 25s window
      continue;
    } else {
      round_created = false;
    }

    if (round_created) {
      INFO_PRINT_STATUS_OK("Block %s created successfully", current_block_height);
    } else {
      INFO_PRINT_STATUS_FAIL("Block %s was not created", current_block_height);
    }

    break;  // TEMP: exit after one round (for testing)
  }
}