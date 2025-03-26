#include "xcash_round.h"

producer_ref_t producer_refs[] = {
    {main_nodes_list.block_producer_public_address, main_nodes_list.block_producer_IP_address},
    {main_nodes_list.block_producer_backup_block_verifier_1_public_address, main_nodes_list.block_producer_backup_block_verifier_1_IP_address},
    {main_nodes_list.block_producer_backup_block_verifier_2_public_address, main_nodes_list.block_producer_backup_block_verifier_2_IP_address},
    {main_nodes_list.block_producer_backup_block_verifier_3_public_address, main_nodes_list.block_producer_backup_block_verifier_3_IP_address},
    {main_nodes_list.block_producer_backup_block_verifier_4_public_address, main_nodes_list.block_producer_backup_block_verifier_4_IP_address},
    {main_nodes_list.block_producer_backup_block_verifier_5_public_address, main_nodes_list.block_producer_backup_block_verifier_5_IP_address},
};

unsigned char* get_pseudo_random_hash(size_t seed, size_t feed_size) {
    char salt_data[512];
    SHA512_CTX sha512;

    // we need 2 bytes for each step
    size_t iterations = (feed_size*2 / SHA512_DIGEST_LENGTH) +1;

    unsigned char* hash_buf = calloc(iterations, SHA512_DIGEST_LENGTH);

    for (size_t i = 0; i < iterations; i++)
    {
        snprintf(salt_data, sizeof(salt_data), "%020ld%020ld", seed, i);
        SHA512_Init(&sha512);
        SHA512_Update(&sha512, salt_data, (size_t)strlen((const char*)salt_data));
        SHA512_Update(&sha512, hash_buf, (size_t)strlen((const char*)hash_buf));
        SHA512_Final(hash_buf + i*SHA512_DIGEST_LENGTH, &sha512);
    }

    return hash_buf;
}

bool select_block_producers(size_t round_number) {
    (void)round_number;
    producer_node_t producers_list[BLOCK_VERIFIERS_AMOUNT] = {0};
    size_t num_producers = 0;

    // Count valid delegates
    for (size_t i = 0, j = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
        if (strlen(delegates_all[i].public_address) == 0) {
            break; // End of delegate list
        }

        // skip seed nodes from block production
        if (is_seed_address(delegates_all[i].public_address))
            continue;

        // Skip offline nodes
        if (strcmp(delegates_all[i].online_status, "false") == 0) {
            continue;
        }

        // Copy to producers list
        strcpy(producers_list[j].public_address, delegates_all[i].public_address);
        strcpy(producers_list[j].IP_address, delegates_all[i].IP_address);
        producers_list[j].is_online = true;

        j++;
        num_producers++;
    }

    if (num_producers == 0) {
        WARNING_PRINT("No valid producers generated during producer selection.");
        return false;
    }

    // Get block height
    size_t block_height, seed_block;
    sscanf(current_block_height, "%zu", &block_height);

    // Seed block ensures same shuffle list for the day
    seed_block = block_height / BLOCKS_PER_DAY_FIVE_MINUTE_BLOCK_TIME;

    unsigned char* pseudo_random_hash = get_pseudo_random_hash(seed_block, BLOCKS_PER_DAY_FIVE_MINUTE_BLOCK_TIME);

    // Initialize shuffled list
    producer_node_t* producers_shuffle_list[BLOCKS_PER_DAY_FIVE_MINUTE_BLOCK_TIME];
    for (size_t i = 0; i < BLOCKS_PER_DAY_FIVE_MINUTE_BLOCK_TIME; i++) {
        size_t producer_index = i % num_producers;
        producers_shuffle_list[i] = &producers_list[producer_index];
    }

    // Fisher-Yates Shuffle using pseudo_random_hash
    for (size_t i = BLOCKS_PER_DAY_FIVE_MINUTE_BLOCK_TIME - 1; i > 0; i--) {
        unsigned int j = (pseudo_random_hash[i * 2] << 8 | pseudo_random_hash[i * 2 + 1]) % (i + 1);
        producer_node_t* temp = producers_shuffle_list[i];
        producers_shuffle_list[i] = producers_shuffle_list[j];
        producers_shuffle_list[j] = temp;
    }

    free(pseudo_random_hash);

    // Clear current main_nodes_list
    memset(&main_nodes_list, 0, sizeof(main_nodes_list));

    // Determine producing position based on block height and current time
    size_t producing_position = block_height % BLOCKS_PER_DAY_FIVE_MINUTE_BLOCK_TIME;

    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    size_t shift_position = (current_time.tv_sec / (BLOCK_TIME * 60)) % BLOCKS_PER_DAY_FIVE_MINUTE_BLOCK_TIME;

    DEBUG_PRINT("Positions: %ld (block: %ld, shift: %ld)", producing_position + shift_position, producing_position, shift_position);

    producing_position += shift_position;

    // Assign producers to producer_refs
    size_t producer_refs_size = sizeof(producer_refs) / sizeof(producer_ref_t);
    for (size_t i = 0; i < producer_refs_size; i++) {
        producing_position = producing_position % BLOCKS_PER_DAY_FIVE_MINUTE_BLOCK_TIME;

        strcpy(producer_refs[i].public_address, producers_shuffle_list[producing_position]->public_address);
        strcpy(producer_refs[i].IP_address, producers_shuffle_list[producing_position]->IP_address);

        producing_position++;
    }

    return true;
}

xcash_round_result_t process_round(size_t round_number) {
  // STEP 1: Sync the databases and build the majority list

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
  INFO_STAGE_PRINT("Nodes online in block %s, round %ld", current_block_height, round_number);

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
    WARNING_PRINT("Nodes majority is NOT enough for block production. Waiting for network recovery...");
    return ROUND_RETRY;
  }

  INFO_PRINT_STATUS_OK("Nodes majority: [%ld/%d]", network_majority_count, BLOCK_VERIFIERS_VALID_AMOUNT);

  // STEP 2: Update block verifiers list

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

  // STEP 3: Select block producer using deterministic algorithm
  select_block_producers(round_number);

  is_block_creation_stage = true;
  INFO_STAGE_PRINT("Starting block production for block %s", current_block_height);

  int block_creation_result = block_verifiers_create_block(round_number);
  is_block_creation_stage = false;

  return (xcash_round_result_t)block_creation_result;
}

/*
***************************************************************

  #define RESET_VARIABLES \
  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) \
  { \
    memset(VRF_data.block_verifiers_vrf_secret_key_data[count],0,strlen(VRF_data.block_verifiers_vrf_secret_key_data[count])); \
    memset(VRF_data.block_verifiers_vrf_secret_key[count],0,strlen((const char*)VRF_data.block_verifiers_vrf_secret_key[count])); \
    memset(VRF_data.block_verifiers_vrf_public_key_data[count],0,strlen(VRF_data.block_verifiers_vrf_public_key_data[count])); \
    memset(VRF_data.block_verifiers_vrf_public_key[count],0,strlen((const char*)VRF_data.block_verifiers_vrf_public_key[count])); \
    memset(VRF_data.block_verifiers_random_data[count],0,strlen(VRF_data.block_verifiers_random_data[count])); \
    memset(VRF_data.block_blob_signature[count],0,strlen(VRF_data.block_blob_signature[count])); \
  } \
  memset(VRF_data.vrf_secret_key_data,0,strlen(VRF_data.vrf_secret_key_data)); \
  memset(VRF_data.vrf_secret_key,0,strlen((const char*)VRF_data.vrf_secret_key)); \
  memset(VRF_data.vrf_public_key_data,0,strlen(VRF_data.vrf_public_key_data)); \
  memset(VRF_data.vrf_public_key,0,strlen((const char*)VRF_data.vrf_public_key)); \
  memset(VRF_data.vrf_alpha_string_data,0,strlen(VRF_data.vrf_alpha_string_data)); \
  memset(VRF_data.vrf_alpha_string,0,strlen((const char*)VRF_data.vrf_alpha_string)); \
  memset(VRF_data.vrf_proof_data,0,strlen(VRF_data.vrf_proof_data)); \
  memset(VRF_data.vrf_proof,0,strlen((const char*)VRF_data.vrf_proof)); \
  memset(VRF_data.vrf_beta_string_data,0,strlen(VRF_data.vrf_beta_string_data)); \
  memset(VRF_data.vrf_beta_string,0,strlen((const char*)VRF_data.vrf_beta_string)); \
  memset(VRF_data.reserve_bytes_data_hash,0,strlen(VRF_data.reserve_bytes_data_hash)); \
  memset(VRF_data.block_blob,0,strlen(VRF_data.block_blob));

if (count == XCASH_PROOF_OF_STAKE_BLOCK_HEIGHT)
{
  // this is the first block of the network
  color_print("The current block is the first block on the network, meaning that the main network node will create this block","yellow");

  RESET_VARIABLES;
  
  // set the main_network_data_node_create_block so the main network data node can create the block
  main_network_data_node_create_block = 1;
  if (start_current_round_start_blocks() == 0)
  {      
    START_NEW_ROUND_ERROR("start_current_round_start_blocks error");
  } 
}

int start_current_round_start_blocks(void)
{
  // Variables
  char data[BUFFER_SIZE];
  char data2[BUFFER_SIZE];
  
  // define macros
  #define DATABASE_COLLECTION "reserve_bytes_1"

  #define START_CURRENT_ROUND_START_BLOCKS_ERROR(settings) \
  memcpy(error_message.function[error_message.total],"start_current_round_start_blocks",32); \
  memcpy(error_message.data[error_message.total],settings,sizeof(settings)-1); \
  error_message.total++; \
  return 0;
  
  memset(data,0,sizeof(data));
  memset(data2,0,sizeof(data2));

  // set the main_network_data_node_create_block so the main network data node can create the block
  main_network_data_node_create_block = 1;

  // check if the block verifier is the main network data node
  if (strncmp(network_data_nodes_list.network_data_nodes_public_address[0],xcash_wallet_public_address,XCASH_WALLET_LENGTH) != 0)
  {
    color_print("Your block verifier is not the main data network node so your block verifier will sit out for the remainder of the round\n","yellow");
    sync_block_verifiers_minutes_and_seconds((BLOCK_TIME-1),SUBMIT_NETWORK_BLOCK_TIME_SECONDS);
    return 1;
  } 

  color_print("Your block verifier is the main data network node so your block verifier will create the block\n","yellow");

  // wait until the non network data nodes have synced the previous current and next block verifiers list
  sleep(30);
  
  // create the data
  if (start_blocks_create_data(data,data2) == 0)
  {
    START_CURRENT_ROUND_START_BLOCKS_ERROR("Could not create the start blocks data");
  }

  // send the database data to all block verifiers
  sleep(BLOCK_VERIFIERS_SETTINGS);
  block_verifiers_send_data_socket((const char*)data);

  color_print("Waiting for the block producer to submit the block to the network\n","blue");
  sync_block_verifiers_minutes_and_seconds((BLOCK_TIME-1),SUBMIT_NETWORK_BLOCK_TIME_SECONDS);

  // have the main network data node submit the block to the network  
  submit_block_template(data2);
  
  return 1;

  #undef DATABASE_COLLECTION
  #undef START_CURRENT_ROUND_START_BLOCKS_ERROR
}




****************************************************************
*/

void start_block_production(void) {
  struct timeval current_time, round_start_time, block_start_time;
  xcash_round_result_t round_result = ROUND_OK;
  size_t retries = 0;
  bool current_block_healthy = false;
  while (!current_block_healthy) {
    if (get_current_block_height(current_block_height) == XCASH_OK) {
      current_block_healthy = true;
    } else {
      WARNING_PRINT("Can't get current block height. Possible node is still syncing blocks. Waiting for recovery...");
      sleep(5);  // Sleep to prevent high CPU usage
    }
  }

  /*
  while (true) {
    gettimeofday(&current_time, NULL);
    size_t seconds_within_block = current_time.tv_sec % (BLOCK_TIME * 60);
    size_t minute_within_block = (current_time.tv_sec / 60) % BLOCK_TIME;

    // Skip block production if the block time is past 25 seconds or if blockchain is not synced
    if (seconds_within_block > 25) {
      retries = 0;

      // Refresh DB if last round error occurred and enough time has passed
      if (round_result != ROUND_OK && seconds_within_block > 280) {
        init_db_from_top();
        round_result = ROUND_OK;
      } else {
        INFO_STAGE_PRINT("Starting production of block %d in ... [%ld:%02ld]",
                         (int)atof(current_block_height),  // Convert to float first if it's a string, then cast to int
                         BLOCK_TIME - 1 - minute_within_block,
                         59 - (current_time.tv_sec % 60));
        sleep(5);
      }
      continue;  // Skip to next loop iteration
    }

    // Check if the current block height is healthy
    current_block_healthy = (get_current_block_height(current_block_height) == XCASH_OK);
    if (!current_block_healthy) {
      WARNING_PRINT("Can't get current block height. Possible node is still syncing blocks. Waiting for recovery...");
      sleep(5);
      continue;  // Skip to next loop iteration if the block height is not healthy
    }

    // Proceed with block production if within the first 25 seconds
    gettimeofday(&block_start_time, NULL);
    size_t round_number = 0;
    bool round_created = false;

    // check for first POS block
    if (current_block_height == XCASH_PROOF_OF_STAKE_BLOCK_HEIGHT) {
      if (strncmp(network_nodes[0].seed_public_address, xcash_wallet_public_address, XCASH_WALLET_LENGTH) == 0) {
        if (start_current_round_start_blocks() == 0) {
          ERROR_PRINT("Start_current_round_start_blocks error");
          round_created = false;
        } else {
          round_created = true;
        }
      } else {
        INFO_PRINT("This block verifier is not the main data network node so it will sit out for the remainder of this round",);
        sleep(SUBMIT_NETWORK_BLOCK_TIME_SECONDS);
        continue;  // Skip to next loop iteration
      }
    } else {
      // Retry loop for round processing with a maximum of 2 retries
      for (retries = 0; retries < 2 && round_number < 1; retries++) {
        gettimeofday(&round_start_time, NULL);
        round_result = process_round(round_number);

        if (round_result == ROUND_RETRY) {
          sleep(5);  // Wait before retrying
          continue;  // Retry the same round
        }

        if (round_result == ROUND_ERROR || round_result == ROUND_SKIP) {
          round_created = false;
        } else if (round_result == ROUND_OK) {
          round_created = true;
        }
      }
    }
    if (round_created) {
      INFO_PRINT_STATUS_OK("Block %s created successfully", current_block_height);
    } else {
      INFO_PRINT_STATUS_FAIL("Block %s not created within %ld rounds", current_block_height, round_number);
    }
    break;
  }
*/



  while (true) {
    gettimeofday(&current_time, NULL);
    size_t seconds_within_block = current_time.tv_sec % (BLOCK_TIME * 60);
    size_t minute_within_block = (current_time.tv_sec / 60) % BLOCK_TIME;

    // Skip block production if outside the first 25 seconds of the block interval
    if (seconds_within_block > 25) {
      retries = 0;

      // Refresh DB if previous round failed and we're late in the interval
      if (round_result != ROUND_OK && seconds_within_block > 280) {
        init_db_from_top();
        round_result = ROUND_OK;
      } else {
        INFO_STAGE_PRINT("Waiting for next round... Block %d in [%ld:%02ld]",
                         (int)atof(current_block_height),
                         BLOCK_TIME - 1 - minute_within_block,
                         59 - (current_time.tv_sec % 60));
        sleep(5);
      }
      continue;
    }

    // Check if current block height is healthy
    current_block_healthy = (get_current_block_height(current_block_height) == XCASH_OK);
    if (!current_block_healthy) {
      WARNING_PRINT("Block height unavailable. Node might be syncing. Retrying...");
      sleep(5);
      continue;
    }

    gettimeofday(&block_start_time, NULL);
    size_t round_number = 0;
    bool round_created = false;
    round_result = ROUND_OK;

    // Check for first PoS block
    if (strtoull(current_block_height, NULL, 10) == XCASH_PROOF_OF_STAKE_BLOCK_HEIGHT) {
      if (strncmp(network_nodes[0].seed_public_address, xcash_wallet_public_address, XCASH_WALLET_LENGTH) == 0) {
        if (start_current_round_start_blocks() != 0) {
          round_created = true;
        } else {
          ERROR_PRINT("start_current_round_start_blocks failed");
        }
      } else {
        INFO_PRINT("Node is not the primary data network node. Sitting out this round.");
        sleep(SUBMIT_NETWORK_BLOCK_TIME_SECONDS);
        continue;
      }
    } else {
      // Standard round processing logic (up to 2 retries)
      for (retries = 0; retries < 2; retries++) {
        gettimeofday(&round_start_time, NULL);
        round_result = process_round(round_number);

        if (round_result == ROUND_RETRY) {
          sleep(5);
          continue;
        }

        if (round_result == ROUND_ERROR || round_result == ROUND_SKIP) {
          round_created = false;
          break;
        }

        if (round_result == ROUND_OK) {
          round_created = true;
          break;
        }

        round_number++;
      }
    }

    // Final round result handling
    if (round_created) {
      INFO_PRINT_STATUS_OK("Block %s created successfully", current_block_height);
    } else {
      INFO_PRINT_STATUS_FAIL("Block %s not created after %zu attempt(s)", current_block_height, round_number + 1);
    }

    break;  // Exit main production loop
  }


}

void show_block_producer(size_t round_number) {
    INFO_STAGE_PRINT("Block producers for block: [%s]", current_block_height);
    INFO_PRINT("Main Block Producer: "GREEN_TEXT("%s"), address_to_node_name(producer_refs[round_number].public_address));
};