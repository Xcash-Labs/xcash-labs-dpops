#include "block_verifiers_functions.h"

int block_verifiers_create_block_signature(char* message)
{
  char data[BUFFER_SIZE];
  size_t count, count2, counter;
  int block_producer_backup_settings[5] = {0};  // 5 backups

  const char* backup_public_addresses[] = {
    main_nodes_list.block_producer_public_address,
    main_nodes_list.block_producer_backup_block_verifier_1_public_address,
    main_nodes_list.block_producer_backup_block_verifier_2_public_address,
    main_nodes_list.block_producer_backup_block_verifier_3_public_address,
    main_nodes_list.block_producer_backup_block_verifier_4_public_address,
    main_nodes_list.block_producer_backup_block_verifier_5_public_address
  };

  memset(data, 0, sizeof(data));

  // Convert network block string to blockchain data
  if (network_block_string_to_blockchain_data(VRF_data.block_blob, "0", BLOCK_VERIFIERS_AMOUNT) == 0) {
    ERROR_PRINT("Could not convert the network block string to a blockchain data");
    return XCASH_ERROR;
  }

  // Set block producer network block nonce
  memcpy(blockchain_data.nonce_data, BLOCK_PRODUCER_NETWORK_BLOCK_NONCE, sizeof(BLOCK_PRODUCER_NETWORK_BLOCK_NONCE) - 1);

  // Determine current block producer or backup node
  int backup_index = current_round_part_backup_node[0] - '0'; // Converts "0"-"5" to integer

  if (backup_index >= 0 && backup_index <= 5) {
    for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
      if (strncmp(current_block_verifiers_list.block_verifiers_public_address[count],
                  backup_public_addresses[backup_index], XCASH_WALLET_LENGTH) == 0) {
        memcpy(blockchain_data.blockchain_reserve_bytes.block_producer_delegates_name,
               current_block_verifiers_list.block_verifiers_name[count],
               strnlen(current_block_verifiers_list.block_verifiers_name[count],
                       sizeof(current_block_verifiers_list.block_verifiers_name[count])));
        memcpy(blockchain_data.blockchain_reserve_bytes.block_producer_public_address,
               current_block_verifiers_list.block_verifiers_public_address[count],
               XCASH_WALLET_LENGTH);
        break;
      }
    }
  }

  // Map backup node indexes
  for (size_t backup_idx = 0; backup_idx < 5; backup_idx++) {
    for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
      if (strncmp(backup_public_addresses[backup_idx + 1], current_block_verifiers_list.block_verifiers_public_address[count], XCASH_WALLET_LENGTH) == 0) {
        block_producer_backup_settings[backup_idx] = (int)count;
        break;
      }
    }
  }

  // Backup node index in blockchain data
  memcpy(blockchain_data.blockchain_reserve_bytes.block_producer_node_backup_count,
         current_round_part_backup_node, sizeof(char));

  // Add backup node names, comma-separated
  blockchain_data.blockchain_reserve_bytes.block_producer_backup_nodes_names[0] = '\0';  // Init empty string
  for (size_t i = 0; i < 5; i++) {
      strcat(blockchain_data.blockchain_reserve_bytes.block_producer_backup_nodes_names,
             current_block_verifiers_list.block_verifiers_name[block_producer_backup_settings[i]]);
      if (i != 4) {
          strcat(blockchain_data.blockchain_reserve_bytes.block_producer_backup_nodes_names, ",");
      }
  }
  
  // Add VRF data
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_secret_key, VRF_data.vrf_secret_key, crypto_vrf_SECRETKEYBYTES);
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_secret_key_data, VRF_data.vrf_secret_key_data, VRF_SECRET_KEY_LENGTH);
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_public_key, VRF_data.vrf_public_key, crypto_vrf_PUBLICKEYBYTES);
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_public_key_data, VRF_data.vrf_public_key_data, VRF_PUBLIC_KEY_LENGTH);
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_alpha_string, VRF_data.vrf_alpha_string, strnlen((const char*)VRF_data.vrf_alpha_string, BUFFER_SIZE));
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_alpha_string_data, VRF_data.vrf_alpha_string_data, strnlen(VRF_data.vrf_alpha_string_data, BUFFER_SIZE));
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_proof, VRF_data.vrf_proof, crypto_vrf_PROOFBYTES);
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_proof_data, VRF_data.vrf_proof_data, VRF_PROOF_LENGTH);
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_beta_string, VRF_data.vrf_beta_string, crypto_vrf_OUTPUTBYTES);
  memcpy(blockchain_data.blockchain_reserve_bytes.vrf_beta_string_data, VRF_data.vrf_beta_string_data, VRF_BETA_LENGTH);

  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
    memcpy(blockchain_data.blockchain_reserve_bytes.block_verifiers_vrf_secret_key[count], VRF_data.block_verifiers_vrf_secret_key[count], crypto_vrf_SECRETKEYBYTES);
    memcpy(blockchain_data.blockchain_reserve_bytes.block_verifiers_vrf_public_key[count], VRF_data.block_verifiers_vrf_public_key[count], crypto_vrf_PUBLICKEYBYTES);
    memcpy(blockchain_data.blockchain_reserve_bytes.block_verifiers_vrf_secret_key_data[count], VRF_data.block_verifiers_vrf_secret_key_data[count], VRF_SECRET_KEY_LENGTH);
    memcpy(blockchain_data.blockchain_reserve_bytes.block_verifiers_vrf_public_key_data[count], VRF_data.block_verifiers_vrf_public_key_data[count], VRF_PUBLIC_KEY_LENGTH);
    memcpy(blockchain_data.blockchain_reserve_bytes.block_verifiers_random_data_text[count], VRF_data.block_verifiers_random_data[count], RANDOM_STRING_LENGTH);

    memcpy(blockchain_data.blockchain_reserve_bytes.next_block_verifiers_public_address[count], next_block_verifiers_list.block_verifiers_public_key[count], VRF_PUBLIC_KEY_LENGTH);
    memcpy(blockchain_data.blockchain_reserve_bytes.block_validation_node_signature_data[count], GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_SIGNATURE_DATA, sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_SIGNATURE_DATA) - 1);
    memcpy(blockchain_data.blockchain_reserve_bytes.block_validation_node_signature[count], GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_SIGNATURE, sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_SIGNATURE) - 1);

    for (counter = 0, count2 = 0; counter < RANDOM_STRING_LENGTH; counter++, count2 += 2) {
      snprintf(blockchain_data.blockchain_reserve_bytes.block_verifiers_random_data[count] + count2, RANDOM_STRING_LENGTH, "%02x", VRF_data.block_verifiers_random_data[count][counter] & 0xFF);
    }
  }

  memcpy(blockchain_data.blockchain_reserve_bytes.previous_block_hash_data, blockchain_data.previous_block_hash_data, BLOCK_HASH_LENGTH);

  // Convert blockchain_data to network block string
  if (blockchain_data_to_network_block_string(VRF_data.block_blob, BLOCK_VERIFIERS_AMOUNT) == 0) {
    ERROR_PRINT("Could not convert the blockchain_data to a network_block_string");
    return XCASH_ERROR;
  }

  // Sign network block string
  memset(data, 0, sizeof(data));
  if (sign_network_block_string(data, VRF_data.block_blob) == 0) {
    ERROR_PRINT("Could not sign the network block string");
    return XCASH_ERROR;
  }

  // Add signature to VRF data
  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
    if (strncmp(current_block_verifiers_list.block_verifiers_public_address[count], xcash_wallet_public_address, XCASH_WALLET_LENGTH) == 0) {
      memcpy(VRF_data.block_blob_signature[count], data, strnlen(data, BUFFER_SIZE));
      break;
    }
  }

  // Construct message
  // create the message
  memset(message,0,strlen(message));
  memcpy(message,"{\r\n \"message_settings\": \"BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_BLOCK_BLOB_SIGNATURE\",\r\n \"block_blob_signature\": \"",110);
  memcpy(message+110,data,strnlen(data,BUFFER_SIZE));
  memcpy(message+strlen(message),"\",\r\n}",5);
  return XCASH_OK;
}

// Helper function: Sync fallback logic
int sync_with_backup_fallback(int backup_index, int primary_min, int primary_sec, int backup_min, int backup_sec) {
  if (backup_index == 0) {
      return sync_block_verifiers_minutes_and_seconds(primary_min, primary_sec);
  } else {
      return sync_block_verifiers_minutes_and_seconds(backup_min, backup_sec);
  }
}

// Helper function: Check majority and log
int check_majority(size_t valid_count, const char* stage_description) {
  if (valid_count >= BLOCK_VERIFIERS_VALID_AMOUNT) {
      INFO_PRINT_STATUS_OK("[%zu / %d] block verifiers have a majority for %s", valid_count, BLOCK_VERIFIERS_VALID_AMOUNT, stage_description);
      return XCASH_OK;
  } else {
      INFO_PRINT_STATUS_FAIL("[%zu / %d] block verifiers lack majority for %s", valid_count, BLOCK_VERIFIERS_VALID_AMOUNT, stage_description);
      WARNING_PRINT("Insufficient majority for %s", stage_description);
      return XCASH_ERROR;
  }
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

// Helper function: Send data & cleanup
int send_and_cleanup(const char* data) {
  response_t** replies = NULL;
  if (!xnet_send_data_multi(XNET_DELEGATES_ALL_ONLINE, data, &replies)) {
      cleanup_responses(replies);
      return XCASH_ERROR;
  }
  cleanup_responses(replies);
  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: block_verifiers_create_block
Description: Runs the round where the block verifiers will create the block
Return: 0 if an error has occured, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int block_verifiers_create_block(size_t round_number) {
  char data[BUFFER_SIZE] = {0};
  size_t count, count2;
  int backup_node_index = current_round_part_backup_node[0] = (char)('0' + round_number) - '0';

  // Clear all VRF data
  pthread_mutex_lock(&majority_vote_lock);
  memset(&VRF_data, 0, sizeof(VRF_data));
  memset(&current_block_verifiers_majority_vote, 0, sizeof(current_block_verifiers_majority_vote));
  pthread_mutex_unlock(&majority_vote_lock);

  // Initial sync
  INFO_STAGE_PRINT("Waiting for block synchronization start time...");
  if (sync_with_backup_fallback(backup_node_index, 0, 30, 3, 5) == XCASH_ERROR)
      return ROUND_SKIP;

  // Check block height 
  if (get_current_block_height(data) == 1 && strncmp(current_block_height, data, BUFFER_SIZE) != 0) {
      WARNING_PRINT("Your block height is not synced correctly, waiting for next round");
      replayed_round_settings = 1;
      return ROUND_NEXT;
  }

  // Get previous block hash
  if (get_previous_block_hash(previous_block_hash) == 0) {
      WARNING_PRINT("Could not get previous block hash");
      return ROUND_NEXT;
  }

  
  //******************************************************************************** 
  // Part 0 - Sync block producers
  INFO_STAGE_PRINT("Part 0 - Exchanging block producers list");
  if (!sync_block_producers()) {
      WARNING_PRINT("Can't select block producer");
      return ROUND_NEXT;
  }

  // Part 1 - Create VRF data
  INFO_STAGE_PRINT("Part 1 - Create VRF data");
  if (block_verifiers_create_VRF_secret_key_and_VRF_public_key(data) == 0 || sign_data(data) == 0) {
      WARNING_PRINT("Could not create VRF data");
      return ROUND_NEXT;
  }
  INFO_PRINT_STATUS_OK("The VRF data has been created");

  // Part 2 - Send VRF data
  INFO_STAGE_PRINT("Part 2 - Send VRF data to all block verifiers");
  if (!send_and_cleanup(data)) return ROUND_NEXT;
  INFO_PRINT_STATUS_OK("The VRF data has been sent");

  // Part 3 - Wait for VRF sync
  if (sync_with_backup_fallback(backup_node_index, 1, 10, 3, 15) == XCASH_ERROR)
      return ROUND_SKIP;

  // Part 4 - Create individual majority VRF data
  INFO_STAGE_PRINT("Part 4 - Create each individual majority VRF data");
  memset(data, 0, sizeof(data));
  block_verifiers_create_vote_majority_results(data, 0);
  if (sign_data(data) == 0) return ROUND_NEXT;
  INFO_PRINT_STATUS_OK("Each individual majority VRF data created");

  // Part 5 - Send individual VRF majority
  INFO_STAGE_PRINT("Part 5 - Send individual majority VRF data");
  if (!send_and_cleanup(data)) return ROUND_NEXT;
  INFO_PRINT_STATUS_OK("Individual majority VRF data sent");

  // Part 6 - Wait
  if (sync_with_backup_fallback(backup_node_index, 1, 25, 3, 25) == XCASH_ERROR)
      return ROUND_SKIP;

  // Part 7 - Check VRF majority (temp fix applied)
  INFO_STAGE_PRINT("Part 7 - Check VRF majority");
  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
      if (strlen(VRF_data.block_verifiers_vrf_secret_key_data[count]) == 0) {
          memcpy(VRF_data.block_verifiers_vrf_secret_key_data[count], GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_SECRET_KEY_DATA, sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_SECRET_KEY_DATA) - 1);
          memcpy(VRF_data.block_verifiers_vrf_secret_key[count], GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_SECRET_KEY, sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_SECRET_KEY) - 1);
          memcpy(VRF_data.block_verifiers_vrf_public_key_data[count], GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_PUBLIC_KEY_DATA, sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_PUBLIC_KEY_DATA) - 1);
          memcpy(VRF_data.block_verifiers_vrf_public_key[count], GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_PUBLIC_KEY, sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_PUBLIC_KEY) - 1);
          memcpy(VRF_data.block_verifiers_random_data[count], GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_RANDOM_STRING, sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_RANDOM_STRING) - 1);
      }
  }
  count = BLOCK_VERIFIERS_AMOUNT;
  INFO_PRINT_STATUS_OK("Checked VRF data majority");

  // Part 8 - Majority check
  if (!check_majority(count, "VRF data")) return ROUND_NEXT;
  if (check_restart_if_alone(count)) return ROUND_SKIP;

  // Part 9 - Check overall majority
  INFO_STAGE_PRINT("Part 9 - Check overall majority for VRF data");
  memset(delegates_error_list,0,sizeof(delegates_error_list)); \
  memcpy(delegates_error_list,"The following delegates are reported as not working for this part:",66);
  count2 = 0;
  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
      if (strncmp(VRF_data.block_verifiers_vrf_secret_key_data[count], GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_SECRET_KEY_DATA, BUFFER_SIZE) != 0)
          count2++;
      else {
          strcat(delegates_error_list, current_block_verifiers_list.block_verifiers_name[count]);
          strcat(delegates_error_list, "|");
      }
  }
  if (!check_majority(count2, "overall VRF data")) return ROUND_NEXT;
  if (check_restart_if_alone(count2)) return ROUND_SKIP;

  // Part 10 - Select VRF data
  INFO_STAGE_PRINT("Part 10 - Select VRF data to use");
  if (block_verifiers_create_VRF_data() == 0) return ROUND_NEXT;
  INFO_PRINT_STATUS_OK("VRF data selected");

  // Part 11 - Create or wait for block template
  if (strcmp(producer_refs[round_number].public_address, xcash_wallet_public_address) == 0) {
      INFO_STAGE_PRINT("Part 11 - Create and send block template");
      if (get_block_template(VRF_data.block_blob) == 0) return ROUND_NEXT;
      memset(data, 0, sizeof(data));
      snprintf(data, sizeof(data), "{\r\n \"message_settings\": \"MAIN_NODES_TO_NODES_PART_4_OF_ROUND_CREATE_NEW_BLOCK\",\r\n \"block_blob\": \"%s\",\r\n}", VRF_data.block_blob);
      if (sign_data(data) == 0) return ROUND_NEXT;
      if (!send_and_cleanup(data)) return ROUND_NEXT;
  } else {
      INFO_STAGE_PRINT("Part 11 - Wait for block template");
  }

  if (sync_with_backup_fallback(backup_node_index, 2, 20, 3, 35) == XCASH_ERROR)
      return ROUND_SKIP;

  if (strncmp(VRF_data.block_blob, "", 1) == 0) {
      WARNING_PRINT("Did not receive block template");
      return ROUND_NEXT;
  }
  INFO_PRINT_STATUS_OK("Received block template");

  // Part 12 - Add VRF data and sign block
  INFO_STAGE_PRINT("Part 12 - Add VRF data and sign block template");
  if (block_verifiers_create_block_signature(data) == 0 || sign_data(data) == 0)
      return ROUND_NEXT;
  INFO_PRINT_STATUS_OK("Block template signed");

  // Part 13 - Send block signature
  INFO_STAGE_PRINT("Part 13 - Send block template signature");
  if (!send_and_cleanup(data)) return ROUND_NEXT;
  INFO_PRINT_STATUS_OK("Sent block template signature");

  if (sync_with_backup_fallback(backup_node_index, 2, 30, 3, 45) == XCASH_ERROR)
      return ROUND_SKIP;

  // Part 15 - Create individual majority block signatures
  INFO_STAGE_PRINT("Part 15 - Create majority block template signature");
  memset(data, 0, sizeof(data));
  block_verifiers_create_vote_majority_results(data, 1);
  if (sign_data(data) == 0) return ROUND_NEXT;
  INFO_PRINT_STATUS_OK("Created majority block template signature");

  // Part 16 - Send majority block signatures
  INFO_STAGE_PRINT("Part 16 - Send majority block template signature");
  if (!send_and_cleanup(data)) return ROUND_NEXT;
  INFO_PRINT_STATUS_OK("Sent majority block template signature");

  if (sync_with_backup_fallback(backup_node_index, 2, 40, 3, 55) == XCASH_ERROR)
      return ROUND_SKIP;

  // Part 18 - Check individual majority signatures
  INFO_STAGE_PRINT("Part 18 - Check block template signature majority");
  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
      if (strlen(VRF_data.block_blob_signature[count]) == 0)
          memcpy(VRF_data.block_blob_signature[count], GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_SIGNATURE, sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_SIGNATURE) - 1);
  }
  count = BLOCK_VERIFIERS_AMOUNT;
  INFO_PRINT_STATUS_OK("Checked block template signature majority");

  if (!check_majority(count, "block template signature")) return ROUND_NEXT;
  if (check_restart_if_alone(count)) return ROUND_SKIP;

  // Part 20 - Overall majority signature
  INFO_STAGE_PRINT("Part 20 - Check overall block template signature majority");
  memset(delegates_error_list,0,sizeof(delegates_error_list)); \
  memcpy(delegates_error_list,"The following delegates are reported as not working for this part:",66);
  count2 = 0;
  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
      if (strncmp(VRF_data.block_blob_signature[count], GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_SIGNATURE, BUFFER_SIZE) != 0)
          count2++;
      else {
          strcat(delegates_error_list, current_block_verifiers_list.block_verifiers_name[count]);
          strcat(delegates_error_list, "|");
      }
  }
  if (!check_majority(count2, "overall block template signature")) return ROUND_NEXT;
  if (check_restart_if_alone(count2)) return ROUND_SKIP;

  // Part 22 - Create vote results for reserve bytes
  if (block_verifiers_create_vote_results(data) == 0 || sign_data(data) == 0) return ROUND_NEXT;
  INFO_PRINT_STATUS_OK("Created reserve bytes majority data");

  if (sync_with_backup_fallback(backup_node_index, 2, 45, BLOCK_TIME - 1, 0) == XCASH_ERROR)
      return ROUND_SKIP;

  // Part 23 - Send reserve bytes data
  INFO_STAGE_PRINT("Part 23 - Send reserve bytes data");
  if (!send_and_cleanup(data)) return ROUND_NEXT;
  INFO_PRINT_STATUS_OK("Sent reserve bytes data");

  if (sync_with_backup_fallback(backup_node_index, 2, 55, BLOCK_TIME - 1, 10) == XCASH_ERROR)
      return ROUND_SKIP;

  // Part 25 - Final reserve bytes majority check
  INFO_STAGE_PRINT("Part 25 - Check reserve bytes majority");
  if (current_round_part_vote_data.vote_results_valid >= BLOCK_VERIFIERS_VALID_AMOUNT) {
      INFO_PRINT_STATUS_OK("[%d / %d] reserve bytes majority", current_round_part_vote_data.vote_results_valid, BLOCK_VERIFIERS_VALID_AMOUNT);
  } else {
      WARNING_PRINT("Invalid reserve bytes majority");
      return ROUND_NEXT;
  }

  // Final step - Update database
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
  struct timeval current_time;

  // Get current time
  if (gettimeofday(&current_time, NULL) != 0)
  {
    ERROR_PRINT("Failed to get current time");
    return XCASH_ERROR;
  }

  size_t seconds_per_block = BLOCK_TIME * 60;
  size_t seconds_within_block = current_time.tv_sec % seconds_per_block;
  size_t target_seconds = MINUTES * 60 + SECONDS;

  if (seconds_within_block >= target_seconds)
  {
    WARNING_PRINT("Sleep time exceeded current time by %zu seconds", seconds_within_block - target_seconds);
    return XCASH_ERROR;
  }

  size_t sleep_seconds = target_seconds - seconds_within_block;
  DEBUG_PRINT("Sleeping for %zu seconds to sync to target time...", sleep_seconds);

  sleep(sleep_seconds);

  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: block_verifiers_create_VRF_secret_key_and_VRF_public_key
Description: The block verifiers will create a VRF secret key and a VRF public key
Parameters:
  message - The message to send to the block verifiers
Return: 0 if an error has occured, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int block_verifiers_create_VRF_secret_key_and_VRF_public_key(char* message)
{
  // Variables
  char data[SMALL_BUFFER_SIZE];
  size_t count;
  size_t counter;

   memset(data,0,sizeof(data));
  
  // create a random VRF public key and secret key
  if (create_random_VRF_keys(VRF_data.vrf_public_key,VRF_data.vrf_secret_key) != 1 || crypto_vrf_is_valid_key((const unsigned char*)VRF_data.vrf_public_key) != 1)
  {
    ERROR_PRINT("Could not create the VRF secret key or VRF public key for the VRF data");
    return XCASH_ERROR;
  }  

  // convert the VRF secret key to hexadecimal
  for (count = 0, counter = 0; count < crypto_vrf_SECRETKEYBYTES; count++, counter += 2)
  {
    snprintf(VRF_data.vrf_secret_key_data+counter,BUFFER_SIZE_NETWORK_BLOCK_DATA-1,"%02x",VRF_data.vrf_secret_key[count] & 0xFF);
  }

  // convert the VRF public key to hexadecimal
  for (count = 0, counter = 0; count < crypto_vrf_PUBLICKEYBYTES; count++, counter += 2)
  {
    snprintf(VRF_data.vrf_public_key_data+counter,BUFFER_SIZE_NETWORK_BLOCK_DATA-1,"%02x",VRF_data.vrf_public_key[count] & 0xFF);
  } 

  // create the message
  memset(message,0,strlen(message));
  memcpy(message,"{\r\n \"message_settings\": \"BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_VRF_DATA\",\r\n \"vrf_secret_key\": \"",92);
  memcpy(message+92,VRF_data.vrf_secret_key_data,VRF_SECRET_KEY_LENGTH);
  memcpy(message+220,"\",\r\n \"vrf_public_key\": \"",24);
  memcpy(message+244,VRF_data.vrf_public_key_data,VRF_PUBLIC_KEY_LENGTH);
  memcpy(message+308,"\",\r\n \"random_data\": \"",21);
  
  // create random data to use in the alpha string of the VRF data
  if (random_string(data,RANDOM_STRING_LENGTH) == 0)
  {
    ERROR_PRINT("Could not create random data for the VRF data");
    return XCASH_ERROR;
  }

  memcpy(message+329,data,RANDOM_STRING_LENGTH);
  memcpy(message+429,"\",\r\n}",5);

  // add the VRF data to the block verifiers VRF data copy
  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++)
  {
    if (strncmp(current_block_verifiers_list.block_verifiers_public_address[count],xcash_wallet_public_address,XCASH_WALLET_LENGTH) == 0)
    {        
      memcpy(VRF_data.block_verifiers_vrf_secret_key[count],VRF_data.vrf_secret_key,crypto_vrf_SECRETKEYBYTES);
      memcpy(VRF_data.block_verifiers_vrf_secret_key_data[count],VRF_data.vrf_secret_key_data,VRF_SECRET_KEY_LENGTH);
      memcpy(VRF_data.block_verifiers_vrf_public_key[count],VRF_data.vrf_public_key,crypto_vrf_PUBLICKEYBYTES);
      memcpy(VRF_data.block_verifiers_vrf_public_key_data[count],VRF_data.vrf_public_key_data,VRF_PUBLIC_KEY_LENGTH);
      memcpy(VRF_data.block_verifiers_random_data[count],data,RANDOM_STRING_LENGTH);
    }
  } 

  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: block_verifiers_create_vote_majority_results
Description: The block verifiers will create the vote majority results
Parameters:
  result - The result
  SETTINGS - The data settings
---------------------------------------------------------------------------------------------------------*/
void block_verifiers_create_vote_majority_results(char *result, const int SETTINGS)
{
  const char *MESSAGE_HEADER = "{\r\n \"message_settings\": \"NODES_TO_NODES_VOTE_MAJORITY_RESULTS\",\r\n ";
  const char *VOTE_KEY_PREFIX = "\"vote_data_";
  const char *VOTE_KEY_SUFFIX = "\": \"";
  const char *VOTE_ENTRY_SUFFIX = "\",\r\n ";
  
  size_t offset = 0;
  int count, count2;

  // Clear result buffer
  memset(result, 0, BUFFER_SIZE);

  // reset the current_block_verifiers_majority_vote
  pthread_mutex_lock(&majority_vote_lock);
  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
    for (count2 = 0; count2 < BLOCK_VERIFIERS_AMOUNT; count2++) {
      memset(current_block_verifiers_majority_vote.data[count][count2], 0, sizeof(current_block_verifiers_majority_vote.data[count][count2]));
    }
  }
  pthread_mutex_unlock(&majority_vote_lock);

  // Add message header
  memcpy(result, MESSAGE_HEADER, strlen(MESSAGE_HEADER));
  offset = strlen(MESSAGE_HEADER);

  // Loop through block verifiers
  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++)
  {
    offset += snprintf(result + offset, BUFFER_SIZE - offset, "%s%d%s", VOTE_KEY_PREFIX, count + 1, VOTE_KEY_SUFFIX);

    if (SETTINGS == 0)
    {
      if (strlen(VRF_data.block_verifiers_vrf_secret_key_data[count]) == VRF_SECRET_KEY_LENGTH &&
          strlen(VRF_data.block_verifiers_vrf_public_key_data[count]) == VRF_PUBLIC_KEY_LENGTH &&
          strlen(VRF_data.block_verifiers_random_data[count]) == RANDOM_STRING_LENGTH)
      {
        memcpy(result + offset, VRF_data.block_verifiers_vrf_secret_key_data[count], VRF_SECRET_KEY_LENGTH);
        offset += VRF_SECRET_KEY_LENGTH;
        memcpy(result + offset, VRF_data.block_verifiers_vrf_public_key_data[count], VRF_PUBLIC_KEY_LENGTH);
        offset += VRF_PUBLIC_KEY_LENGTH;
        memcpy(result + offset, VRF_data.block_verifiers_random_data[count], RANDOM_STRING_LENGTH);
        offset += RANDOM_STRING_LENGTH;
      }
      else
      {
        memcpy(result + offset, BLOCK_VERIFIER_MAJORITY_VRF_DATA_TEMPLATE, strlen(BLOCK_VERIFIER_MAJORITY_VRF_DATA_TEMPLATE));
        offset += strlen(BLOCK_VERIFIER_MAJORITY_VRF_DATA_TEMPLATE);
      }
    }
    else
    {
      if (strlen(VRF_data.block_blob_signature[count]) == VRF_PROOF_LENGTH + VRF_BETA_LENGTH)
      {
        memcpy(result + offset, VRF_data.block_blob_signature[count], VRF_PROOF_LENGTH + VRF_BETA_LENGTH);
        offset += VRF_PROOF_LENGTH + VRF_BETA_LENGTH;
      }
      else
      {
        memcpy(result + offset, BLOCK_VERIFIER_MAJORITY_BLOCK_VERIFIERS_SIGNATURE_TEMPLATE, strlen(BLOCK_VERIFIER_MAJORITY_BLOCK_VERIFIERS_SIGNATURE_TEMPLATE));
        offset += strlen(BLOCK_VERIFIER_MAJORITY_BLOCK_VERIFIERS_SIGNATURE_TEMPLATE);
      }
    }

    memcpy(result + offset, VOTE_ENTRY_SUFFIX, strlen(VOTE_ENTRY_SUFFIX));
    offset += strlen(VOTE_ENTRY_SUFFIX);
  }

  // Replace the last comma with closing bracket
  result[offset - 3] = '}';
  result[offset - 2] = '\0';

  // Add to current_block_verifiers_majority_vote
  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++)
  {
    if (strncmp(current_block_verifiers_list.block_verifiers_public_address[count], xcash_wallet_public_address, XCASH_WALLET_LENGTH) == 0)
    {
      break;
    }
  }
  pthread_mutex_lock(&majority_vote_lock);
  for (count2 = 0; count2 < BLOCK_VERIFIERS_AMOUNT; count2++)
  {
    memcpy(current_block_verifiers_majority_vote.data[count][count2], VRF_data.block_verifiers_vrf_secret_key_data[count2], VRF_SECRET_KEY_LENGTH);
    memcpy(current_block_verifiers_majority_vote.data[count][count2] + VRF_SECRET_KEY_LENGTH, VRF_data.block_verifiers_vrf_public_key_data[count2], VRF_PUBLIC_KEY_LENGTH);
    memcpy(current_block_verifiers_majority_vote.data[count][count2] + VRF_SECRET_KEY_LENGTH + VRF_PUBLIC_KEY_LENGTH, VRF_data.block_verifiers_random_data[count2], RANDOM_STRING_LENGTH);
  }
  pthread_mutex_unlock(&majority_vote_lock);
  return;
}

/*---------------------------------------------------------------------------------------------------------
Name: block_verifiers_create_VRF_data
Description: The block verifiers will create all of the VRF data
Return: 0 if an error has occured, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int block_verifiers_create_VRF_data(void)
{
  // Variables
  char data[SMALL_BUFFER_SIZE] = {0};
  char data2[SMALL_BUFFER_SIZE] = {0};
  size_t count, count2, counter;

  // Initialize vrf_alpha_string
  memset(VRF_data.vrf_alpha_string, 0, strlen((const char*)VRF_data.vrf_alpha_string));
  memcpy(VRF_data.vrf_alpha_string, previous_block_hash, BLOCK_HASH_LENGTH);

  // Append block verifiers random data or placeholder
  for (count = 0; count < BLOCK_VERIFIERS_AMOUNT; count++) {
      if (strlen((const char*)VRF_data.block_verifiers_vrf_secret_key[count]) == crypto_vrf_SECRETKEYBYTES &&
          strlen((const char*)VRF_data.block_verifiers_vrf_public_key[count]) == crypto_vrf_PUBLICKEYBYTES &&
          strlen(VRF_data.block_verifiers_random_data[count]) == RANDOM_STRING_LENGTH) {
          memcpy(VRF_data.vrf_alpha_string + strlen((const char*)VRF_data.vrf_alpha_string),
                 VRF_data.block_verifiers_random_data[count], RANDOM_STRING_LENGTH);
      } else {
          memcpy(VRF_data.vrf_alpha_string + strlen((const char*)VRF_data.vrf_alpha_string),
                 GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_RANDOM_STRING,
                 sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_RANDOM_STRING) - 1);
      }
  }

  // Convert vrf_alpha_string to hex string
  size_t alpha_len = strlen((const char*)VRF_data.vrf_alpha_string);
  for (count2 = 0, count = 0; count2 < alpha_len; count2++, count += 2) {
    snprintf(VRF_data.vrf_alpha_string_data + count, BUFFER_SIZE - count, "%02x", VRF_data.vrf_alpha_string[count2] & 0xFF);
  }

  // Hash vrf_alpha_string_data
  crypto_hash_sha512((unsigned char*)data, (const unsigned char*)VRF_data.vrf_alpha_string_data, strlen(VRF_data.vrf_alpha_string_data));

  // Convert hash to hex string
  for (count2 = 0, count = 0; count2 < DATA_HASH_LENGTH / 2; count2++, count += 2) {
    snprintf(data2 + count, sizeof(data2) - count, "%02x", data[count2] & 0xFF);
  }

  // Determine which verifier's keys to use
  for (count = 0; count < DATA_HASH_LENGTH; count += 2) {
    char byte_str[3] = {0};
    memcpy(byte_str, &data2[count], 2);
    counter = (int)strtol(byte_str, NULL, 16);

    if (counter >= MINIMUM_BYTE_RANGE && counter <= MAXIMUM_BYTE_RANGE) {
      counter %= BLOCK_VERIFIERS_AMOUNT;

      if (strncmp(VRF_data.block_verifiers_vrf_secret_key_data[counter],
                  GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_SECRET_KEY_DATA,
                  sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_SECRET_KEY_DATA) - 1) != 0 &&
          strncmp(VRF_data.block_verifiers_vrf_public_key_data[counter],
                  GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_PUBLIC_KEY_DATA,
                  sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_VRF_PUBLIC_KEY_DATA) - 1) != 0 &&
          strncmp(VRF_data.block_verifiers_random_data[counter], GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_RANDOM_STRING,
                  sizeof(GET_BLOCK_TEMPLATE_BLOCK_VERIFIERS_RANDOM_STRING) - 1) != 0) {
          break;
      }
    }
  }

  // Set selected verifier's keys
  memcpy(VRF_data.vrf_secret_key_data, VRF_data.block_verifiers_vrf_secret_key_data[counter], VRF_SECRET_KEY_LENGTH);
  memcpy(VRF_data.vrf_secret_key, VRF_data.block_verifiers_vrf_secret_key[counter], crypto_vrf_SECRETKEYBYTES);
  memcpy(VRF_data.vrf_public_key_data, VRF_data.block_verifiers_vrf_public_key_data[counter], VRF_PUBLIC_KEY_LENGTH);
  memcpy(VRF_data.vrf_public_key, VRF_data.block_verifiers_vrf_public_key[counter], crypto_vrf_PUBLICKEYBYTES);

  // Create VRF proof and beta string
  if (crypto_vrf_prove(VRF_data.vrf_proof, (const unsigned char*)VRF_data.vrf_secret_key,
                       (const unsigned char*)VRF_data.vrf_alpha_string_data, strlen((const char*)VRF_data.vrf_alpha_string_data)) != 0) {
    ERROR_PRINT("Could not create the vrf proof");
    return XCASH_ERROR;
  }
  if (crypto_vrf_proof_to_hash(VRF_data.vrf_beta_string, (const unsigned char*)VRF_data.vrf_proof) != 0) {
    ERROR_PRINT("Could not create the vrf beta string");
    return XCASH_ERROR;
  }
  if (crypto_vrf_verify(VRF_data.vrf_beta_string, (const unsigned char*)VRF_data.vrf_public_key,
                        (const unsigned char*)VRF_data.vrf_proof, (const unsigned char*)VRF_data.vrf_alpha_string_data,
                        strlen((const char*)VRF_data.vrf_alpha_string_data)) != 0) {
    ERROR_PRINT("Could not verify the VRF data");
    return XCASH_ERROR;
  }

  // Convert proof and beta string to hex strings
  for (counter = 0, count = 0; counter < crypto_vrf_PROOFBYTES; counter++, count += 2) {
    snprintf(VRF_data.vrf_proof_data + count, BUFFER_SIZE_NETWORK_BLOCK_DATA - count, "%02x", VRF_data.vrf_proof[counter] & 0xFF);
  }
  for (counter = 0, count = 0; counter < crypto_vrf_OUTPUTBYTES; counter++, count += 2) {
    snprintf(VRF_data.vrf_beta_string_data + count, BUFFER_SIZE_NETWORK_BLOCK_DATA - count, "%02x", VRF_data.vrf_beta_string[counter] & 0xFF);
  }

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
  // Variables
  char data[BUFFER_SIZE] = {0};
  char hash_raw[SMALL_BUFFER_SIZE] = {0};
  char hash_hex[SMALL_BUFFER_SIZE] = {0};

  INFO_STAGE_PRINT("Part 21 - Verify the block verifiers from the previous block signatures are valid");

  // Verify block signatures validity
  if (verify_network_block_data(1, 1, "0", BLOCK_VERIFIERS_AMOUNT) == 0)
  {
    ERROR_PRINT("The MAIN_NODES_TO_NODES_PART_4_OF_ROUND message is invalid");
    return XCASH_ERROR;
  }

  INFO_STAGE_PRINT("Part 22 - Create the overall majority data for the reserve bytes (block template with VRF data)");

  // Convert blockchain_data to network block string
  if (blockchain_data_to_network_block_string(data, BLOCK_VERIFIERS_AMOUNT) == 0)
  {
    ERROR_PRINT("Could not convert the blockchain_data to a network_block_string");
    return XCASH_ERROR;
  }

  // Copy network block string to VRF block blob
  memset(VRF_data.block_blob, 0, strlen(VRF_data.block_blob));
  memcpy(VRF_data.block_blob, data, strnlen(data, BUFFER_SIZE));

  // Hash the network block string using SHA512
  crypto_hash_sha512((unsigned char*)hash_raw, (const unsigned char*)data, strnlen(data, BUFFER_SIZE));

  // Convert SHA512 hash to hex string
  for (size_t i = 0; i < DATA_HASH_LENGTH / 2; i++)
  {
    snprintf(hash_hex + (i * 2), 3, "%02x", hash_raw[i] & 0xFF);
  }

  // Reset vote data structure
  memset(current_round_part_vote_data.current_vote_results, 0, sizeof(current_round_part_vote_data.current_vote_results));
  current_round_part_vote_data.vote_results_valid = 1;
  current_round_part_vote_data.vote_results_invalid = 0;
  memcpy(current_round_part_vote_data.current_vote_results, hash_hex, DATA_HASH_LENGTH);

  // Construct the JSON message
  snprintf(message, BUFFER_SIZE,
           "{\r\n \"message_settings\": \"NODES_TO_NODES_VOTE_RESULTS\",\r\n "
           "\"vote_settings\": \"valid\",\r\n \"vote_data\": \"%s\",\r\n}", 
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
int block_verifiers_create_block_and_update_database(void)
{
  // Variables
  char data[BUFFER_SIZE] = {0};
  char data2[BUFFER_SIZE] = {0};
  char data3[BUFFER_SIZE] = {0};
  time_t current_date_and_time;
  struct tm current_UTC_date_and_time;
  size_t count;
  size_t block_height;

  // Add data hash to the network block string
  INFO_STAGE_PRINT("Part 26 - Add the data hash of the reserve bytes to the block");
  if (add_data_hash_to_network_block_string(VRF_data.block_blob, data) == 0)
  {
    ERROR_PRINT("Could not add the data hash of the reserve bytes to the block");
    return XCASH_ERROR;
  }
  INFO_PRINT_STATUS_OK("Added the data hash of the reserve bytes to the block");

  // Update reserve bytes database
  INFO_STAGE_PRINT("Part 27 - Add the reserve bytes to the database");
  get_reserve_bytes_database(&count);

  snprintf(data2, sizeof(data2),
           "{\"block_height\":\"%s\",\"reserve_bytes_data_hash\":\"%s\",\"reserve_bytes\":\"%s\"}",
           current_block_height, VRF_data.reserve_bytes_data_hash, VRF_data.block_blob);

  snprintf(data3, sizeof(data3), "reserve_bytes_%zu", count);

  if (upsert_json_to_db(DATABASE_NAME, XCASH_DB_RESERVE_BYTES, count, data2, false) == XCASH_ERROR)
  {
    ERROR_PRINT("Could not add the reserve bytes to the database");
    return XCASH_ERROR;
  }
  INFO_PRINT_STATUS_OK("Added the reserve bytes to the database");

  // Handle reserve proofs check
  if (strncmp(current_round_part_backup_node, "0", 1) == 0)
  {
    sscanf(current_block_height, "%zu", &block_height);
    time(&current_date_and_time);
    gmtime_r(&current_date_and_time,&current_UTC_date_and_time);
    INFO_STAGE_PRINT("Part 28 - Starting the reserve proofs delegate check");
    reserve_proofs_delegate_check();
    INFO_PRINT_STATUS_OK("The reserve proofs delegate check is finished");
  }

  if (sync_block_verifiers_minutes_and_seconds((BLOCK_TIME - 1), 0) == XCASH_ERROR)
  {
    return XCASH_ERROR;
  }

  // Submit block template if this node is the block producer or backup producer
  if ((strncmp(current_round_part_backup_node, "0", 1) == 0 &&
       strncmp(main_nodes_list.block_producer_public_address, xcash_wallet_public_address, XCASH_WALLET_LENGTH) == 0) ||
      (strncmp(current_round_part_backup_node, "1", 1) == 0 &&
       strncmp(main_nodes_list.block_producer_backup_block_verifier_1_public_address, xcash_wallet_public_address, XCASH_WALLET_LENGTH) == 0) ||
      (strncmp(current_round_part_backup_node, "2", 1) == 0 &&
       strncmp(main_nodes_list.block_producer_backup_block_verifier_2_public_address, xcash_wallet_public_address, XCASH_WALLET_LENGTH) == 0) ||
      (strncmp(current_round_part_backup_node, "3", 1) == 0 &&
       strncmp(main_nodes_list.block_producer_backup_block_verifier_3_public_address, xcash_wallet_public_address, XCASH_WALLET_LENGTH) == 0) ||
      (strncmp(current_round_part_backup_node, "4", 1) == 0 &&
       strncmp(main_nodes_list.block_producer_backup_block_verifier_4_public_address, xcash_wallet_public_address, XCASH_WALLET_LENGTH) == 0) ||
      (strncmp(current_round_part_backup_node, "5", 1) == 0 &&
       strncmp(main_nodes_list.block_producer_backup_block_verifier_5_public_address, xcash_wallet_public_address, XCASH_WALLET_LENGTH) == 0))
  {
    INFO_STAGE_PRINT("Sending the new block to blockchain");
    if (submit_block_template(data) != XCASH_OK)
    {
      WARNING_PRINT("Sending the new block to blockchain returned error");
    }
    else
    {
      INFO_PRINT_STATUS_OK("New block sent to blockchain successfully");
    }
  }

  sleep(BLOCK_VERIFIERS_SETTINGS);

  // Ensure seed nodes also submit the block
  for (count = 0; network_nodes[count].seed_public_address != NULL; count++)
  {
    if (strncmp(network_nodes[count].seed_public_address, xcash_wallet_public_address, XCASH_WALLET_LENGTH) == 0)
    {
      INFO_STAGE_PRINT("Sending the new block to blockchain");
      if (submit_block_template(data) != XCASH_OK)
      {
        WARNING_PRINT("Sending the new block to blockchain returned error");
      }
      else
      {
        INFO_PRINT_STATUS_OK("New block sent to blockchain successfully");
      }
    }
  }

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
  if (get_block_template(data) == 0)
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
  if (start_blocks_create_vrf_data() == 0)
  {
    ERROR_PRINT("Could not add the VRF data");
    return XCASH_ERROR;
  }
  
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
  memset(VRF_data.block_blob,0,strlen(VRF_data.block_blob));
  if (blockchain_data_to_network_block_string(VRF_data.block_blob,BLOCK_VERIFIERS_AMOUNT) == 0)
  {
    ERROR_PRINT("Could not convert the blockchain_data to a network_block_string");
    return XCASH_ERROR;
  }

  // add the data hash to the network block string
  memset(network_block_string,0,strlen(network_block_string));
  if (add_data_hash_to_network_block_string(VRF_data.block_blob,network_block_string) == 0)
  {
    ERROR_PRINT("Could not add the network block string data hash");
    return XCASH_ERROR;
  }

  // update the reserve bytes database
  memset(data2,0,sizeof(data2));
  memcpy(data2,"{\"block_height\":\"",17);
  memcpy(data2+17,current_block_height,strnlen(current_block_height,sizeof(current_block_height)));
  memcpy(data2+strlen(data2),"\",\"reserve_bytes_data_hash\":\"",29);
  memcpy(data2+strlen(data2),VRF_data.reserve_bytes_data_hash,DATA_HASH_LENGTH);
  memcpy(data2+strlen(data2),"\",\"reserve_bytes\":\"",19);
  memcpy(data2+strlen(data2),VRF_data.block_blob,strnlen(VRF_data.block_blob,sizeof(data2)));
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
  memcpy(message+strlen(message),VRF_data.reserve_bytes_data_hash,DATA_HASH_LENGTH);
  memcpy(message+strlen(message),"\",\r\n}",5);
  
  // sign_data
  if (sign_data(message) == 0)
  { 
    ERROR_PRINT("Could not sign_data");
    return XCASH_ERROR;
  }

  // clear the VRF_data.block_blob so at the start of the next round, the main network data node does not try to update the databases
  memset(VRF_data.block_blob,0,strlen(VRF_data.block_blob));

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