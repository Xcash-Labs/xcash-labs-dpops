#include "xcash_round.h"

unsigned char* generate_deterministic_entropy(const unsigned char* vrf_output, size_t vrf_output_len, size_t total_bytes_needed) {
    size_t iterations = (total_bytes_needed / SHA512_DIGEST_LENGTH) + 1;

    unsigned char* hash_buf = calloc(iterations, SHA512_DIGEST_LENGTH);
    if (!hash_buf) return NULL;

    for (size_t i = 0; i < iterations; i++) {
        // Create new EVP digest context
        EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
        if (!mdctx) {
            free(hash_buf);
            return NULL;
        }

        if (EVP_DigestInit_ex(mdctx, EVP_sha512(), NULL) != 1) {
            EVP_MD_CTX_free(mdctx);
            free(hash_buf);
            return NULL;
        }

        // Add VRF output
        if (EVP_DigestUpdate(mdctx, vrf_output, vrf_output_len) != 1) {
            EVP_MD_CTX_free(mdctx);
            free(hash_buf);
            return NULL;
        }

        // Add counter as 8-byte little-endian integer
        unsigned char counter[8];
        for (int j = 0; j < 8; j++)
            counter[j] = (i >> (8 * j)) & 0xff;

        if (EVP_DigestUpdate(mdctx, counter, sizeof(counter)) != 1) {
            EVP_MD_CTX_free(mdctx);
            free(hash_buf);
            return NULL;
        }

        // Finalize digest into proper slice of hash_buf
        if (EVP_DigestFinal_ex(mdctx, hash_buf + (i * SHA512_DIGEST_LENGTH), NULL) != 1) {
            EVP_MD_CTX_free(mdctx);
            free(hash_buf);
            return NULL;
        }

        EVP_MD_CTX_free(mdctx);
    }

    return hash_buf;
}

bool select_block_producers(const unsigned char* vrf_output, size_t vrf_output_len) {
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

  // Generate deterministic entropy
  size_t entropy_bytes = num_producers * 2;
  unsigned char* entropy = generate_deterministic_entropy(vrf_output, vrf_output_len, entropy_bytes);
  if (!entropy) {
    ERROR_PRINT("Failed to generate VRF-based entropy.");
    return false;
  }

  // Shuffle the producers list directly
  for (size_t i = num_producers - 1; i > 0; i--) {
    size_t index = ((entropy[i * 2] << 8) | entropy[i * 2 + 1]) % (i + 1);
    producer_node_t temp = producers_list[i];
    producers_list[i] = producers_list[index];
    producers_list[index] = temp;
  }
  free(entropy);

  // Fill the global producer_refs[]
  memset(producer_refs, 0, sizeof(producer_refs));  // Zero out all producer slots
  for (size_t i = 0; i < PRODUCER_REF_COUNT && i < num_producers; i++) {
    strcpy(producer_refs[i].public_address, producers_list[i].public_address);
    strcpy(producer_refs[i].IP_address, producers_list[i].IP_address);
  }

  return true;
}

xcash_round_result_t process_round(void) {
  // Sync the databases and build the majority list

  // Get the current block height
  if (get_current_block_height(current_block_height) != XCASH_OK) {
    ERROR_PRINT("Can't get current block height");
    return ROUND_ERROR;
  }

  // Get the previous block hash
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

  // Check if we have enough nodes for block production
  if (network_majority_count < BLOCK_VERIFIERS_VALID_AMOUNT) {
    INFO_PRINT_STATUS_FAIL("Nodes majority: [%ld/%d]", network_majority_count, BLOCK_VERIFIERS_VALID_AMOUNT);
    return ROUND_RETRY;
  }

  INFO_PRINT_STATUS_OK("Nodes majority: [%ld/%d]", network_majority_count, BLOCK_VERIFIERS_VALID_AMOUNT);

  // Update block verifiers list

  if (update_block_verifiers_list() == 0) {
    DEBUG_PRINT("Could not update the previous, current, and next block verifiers list from database");
    return ROUND_ERROR;
  }

  // Fill block verifiers list with proven online nodes
  block_verifiers_list_t* bf = &current_block_verifiers_list;
  memset(bf, 0, sizeof(block_verifiers_list_t));

  for (size_t i = 0, j = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
    strcpy(bf->block_verifiers_name[j], delegates_all[i].delegate_name);
    strcpy(bf->block_verifiers_public_address[j], delegates_all[i].public_address);
    strcpy(bf->block_verifiers_public_key[j], delegates_all[i].public_key);
    strcpy(bf->block_verifiers_IP_address[j], delegates_all[i].IP_address);
    j++;
  }

  // Select block producer using deterministic algorithm
  INFO_STAGE_PRINT("Part 1 - Selecting block producers");
  if (select_block_producers()) {
    DEBUG_PRINT("Failed to select a block producer")
    return ROUND_ERROR;
  }
  INFO_PRINT_STATUS_OK("Block producers selected");

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

  // Step 1: Wait for node to be fully synced
  while (!current_block_healthy) {
      if (get_current_block_height(current_block_height) == XCASH_OK) {
          current_block_healthy = true;
      } else {
          WARNING_PRINT("Node is still syncing. Waiting for recovery...");
          sleep(5);
      }
  }

  // Step 2: Start production loop
  while (true) {
      gettimeofday(&current_time, NULL);
      size_t seconds_within_block = current_time.tv_sec % (BLOCK_TIME * 60);
      size_t minute_within_block  = (current_time.tv_sec / 60) % BLOCK_TIME;

      // Skip production if outside initial window
      if (seconds_within_block > 25) {
          if (round_result != ROUND_OK && seconds_within_block > 280) {
              WARNING_PRINT("Last round failed. Refreshing DB from top...");
              init_db_from_top();
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

      // Step 4: Special PoS bootstrapping block
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
          // Step 5: Standard block production
          round_result = process_round();
          if (round_result == ROUND_OK) {
              round_created = true;
          } else if (round_result == ROUND_RETRY) {
              INFO_PRINT("Round retry. Waiting before trying ...");;
              sleep(10); // Allow 2 retries max within 25s window
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

      break; // TEMP: exit after one round (for testing)
  }
}

void show_block_producer(void) {
  INFO_STAGE_PRINT("Block producers for block: [%s]", current_block_height);
  INFO_PRINT("Main Block Producer: " GREEN_TEXT("%s"), address_to_node_name(producer_refs[0].public_address));
}