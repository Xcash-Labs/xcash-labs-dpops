#include "xcash_round.h"

producer_ref_t producer_refs[] = {
    {main_nodes_list.block_producer_public_address, main_nodes_list.block_producer_IP_address},
};

bool select_block_producers(void) {
  producer_node_t producers_list[BLOCK_VERIFIERS_AMOUNT] = {0};
  size_t num_producers = 0;

  // Collect eligible delegates
  for (size_t i = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
    if (strlen(delegates_all[i].public_address) == 0) break;
    if (is_seed_address(delegates_all[i].public_address)) continue;
    if (strcmp(delegates_all[i].online_status, "false") == 0) continue;

    strcpy(producers_list[num_producers].public_address, delegates_all[i].public_address);
    strcpy(producers_list[num_producers].IP_address, delegates_all[i].IP_address);
    producers_list[num_producers].is_online = true;
    num_producers++;
  }

  if (num_producers < 1) {
    WARNING_PRINT("No valid producers generated during producer selection.");
    return false;
  }









  // For now there is only one block producer and no backups
  memset(&main_nodes_list, 0, sizeof(main_nodes_list));
  for (size_t i = 0; i < PRODUCER_REF_COUNT && i < num_producers; i++) {
    strcpy(producer_refs[i].public_address, producers_list[i].public_address);
    strcpy(producer_refs[i].IP_address, producers_list[i].IP_address);
  }

  return true;
}



xcash_round_result_t process_round(void) {
  char vrf_message[SMALL_BUFFER_SIZE] = {0};

  // Get the current block height Then Sync the databases and build the majority list
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

  size_t network_majority_count = 0;
  xcash_node_sync_info_t* nodes_majority_list = NULL;

  if (!initial_db_sync_check(&network_majority_count, &nodes_majority_list) || !nodes_majority_list) {
    WARNING_PRINT("Can't sync databases with network majority");
    free(nodes_majority_list);
    return ROUND_ERROR;
  }

  // Update with fresh delegates list
  if (!fill_delegates_from_db()) {
    DEBUG_PRINT("Can't read delegates list from DB");
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
  block_verifiers_list_t* bf = &current_block_verifiers_list;
  memset(bf, 0, sizeof(block_verifiers_list_t));
  memset(&VRF_data, 0, sizeof(VRF_data));

  for (size_t i = 0, j = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
    strcpy(bf->block_verifiers_name[j], delegates_all[i].delegate_name);
    strcpy(bf->block_verifiers_public_address[j], delegates_all[i].public_address);
    strcpy(bf->block_verifiers_public_key[j], delegates_all[i].public_key);
    strcpy(bf->block_verifiers_IP_address[j], delegates_all[i].IP_address);
    j++;
  }



  // Generate VRF keys and VRF message request
  if (block_verifiers_create_VRF_secret_key_and_VRF_public_key(vrf_message) == XCASH_OK) {
    DEBUG_PRINT("Generated VRF message: %s", vrf_message);
  } else {
    ERROR_PRINT("Failed to generate VRF keys and message");
    return ROUND_ERROR;
  }



  unsigned char vrf_output[32] = {0};
  if (hex_to_byte_array(previous_block_hash, vrf_output, sizeof(vrf_output)) != XCASH_OK) {
    ERROR_PRINT("Failed to convert previous_block_hash to VRF output");
    return ROUND_ERROR;
  }








  

  is_block_creation_stage = true;
  INFO_STAGE_PRINT("Starting block production for block %s", current_block_height);

  int block_creation_result = block_verifiers_create_block();
  is_block_creation_stage = false;

  return (xcash_round_result_t)block_creation_result;
}

void start_block_production(void) {
  struct timeval current_time;
  xcash_round_result_t round_result = ROUND_OK;
  bool current_block_healthy = false;

  // Wait for node to be fully synced
  while (!current_block_healthy) {
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
        INFO_PRINT("Missed block window. Block %s — Next round starts in [%ld:%02ld]",
                   current_block_height,
                   BLOCK_TIME - 1 - minute_within_block,
                   59 - (current_time.tv_sec % 60));
      }
      sleep(5);
      continue;
    }

    // Step 3: Recheck block height before proceeding
    if (get_current_block_height(current_block_height) != XCASH_OK) {
      WARNING_PRINT("Failed to fetch current block height. Retrying...");
      sleep(5);
      continue;
    }

    bool round_created = false;

    // Special PoS bootstrapping block
    if (strtoull(current_block_height, NULL, 10) == XCASH_PROOF_OF_STAKE_BLOCK_HEIGHT) {
      if (strncmp(network_nodes[0].seed_public_address, xcash_wallet_public_address, XCASH_WALLET_LENGTH) == 0) {
        round_created = (start_current_round_start_blocks() != XCASH_ERROR);
        if (!round_created) {
          ERROR_PRINT("start_current_round_start_blocks() failed");
        }
      } else {
        INFO_PRINT("This node is not the PoS boot node. Skipping.");
        sleep(SUBMIT_NETWORK_BLOCK_TIME_SECONDS);
        continue;
      }
    } else {
      // Standard block production
      round_result = process_round();
      if (round_result == ROUND_OK) {
        round_created = true;
      } else if (round_result == ROUND_RETRY) {
        INFO_PRINT("Round retry. Waiting before trying ...");
        ;
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
    }

    break;  // TEMP: exit after one round (for testing)
  }
}