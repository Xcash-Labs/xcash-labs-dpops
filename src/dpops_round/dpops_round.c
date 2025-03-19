#include "dpops_round.h"

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


return ROUND_ERROR;

    // Fill block verifiers list with proven online nodes
//    block_verifiers_list_t* bf = &current_block_verifiers_list;
//    memset(bf, 0, sizeof(block_verifiers_list_t));

//    for (size_t i = 0, j = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
//        strcpy(bf->block_verifiers_name[j], delegates_all[i].delegate_name);
//        strcpy(bf->block_verifiers_public_address[j], delegates_all[i].public_address);
//        strcpy(bf->block_verifiers_public_key[j], delegates_all[i].public_key);
//        strcpy(bf->block_verifiers_IP_address[j], delegates_all[i].IP_address);
//        j++;
//    }

    // STEP 3: Select block producer using deterministic algorithm
//   select_block_producers(round_number);

//    is_block_creation_stage = true;
//    INFO_STAGE_PRINT("Starting block production for block %s", current_block_height);

//    int block_creation_result = block_verifiers_create_block(round_number);
//    is_block_creation_stage = false;

//    return (xcash_round_result_t)block_creation_result;
}

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

        // Retry loop for round processing with a maximum of 2 retries
        for (retries = 0; retries < 2 && round_number < 1; retries++) {
            gettimeofday(&round_start_time, NULL);
            round_result = process_round(round_number);

            if (round_result == ROUND_RETRY) {
                sleep(5);  // Wait before retrying
                continue;   // Retry the same round
            }

            if (round_result == ROUND_ERROR || round_result == ROUND_SKIP) {
                round_created = false;
            } else if (round_result == ROUND_OK) {
                round_created = true;
            }

            if (round_created) {
                INFO_PRINT_STATUS_OK("Block %s created successfully", current_block_height);
            } else {
                INFO_PRINT_STATUS_FAIL("Block %s not created within %ld rounds", current_block_height, round_number);
            }
            break;
        }
    }
}