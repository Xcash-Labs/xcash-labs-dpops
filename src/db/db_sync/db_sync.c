#include "db_sync.h"

void show_majority_statistics(const xcash_node_sync_info_t* majority_list, size_t items_count) {
    if (!majority_list || items_count == 0) {
        WARNING_PRINT("No valid nodes in majority list. The network might be offline.");
        return;
    }
    INFO_PRINT("Nodes majority status (%ld nodes found):", items_count);
}

bool get_sync_nodes_majority_list_top(xcash_node_sync_info_t** majority_list_result, size_t* majority_count_result) {
    if (!majority_list_result || !majority_count_result) {
        ERROR_PRINT("Invalid argument: NULL pointer passed to get_sync_nodes_majority_list_top");
        return XCASH_ERROR;
    }

    *majority_list_result = NULL;
    *majority_count_result = 0;

    response_t** replies = NULL;

    // Send message to get sync info from all nodes
    if (!send_message(XNET_DELEGATES_ALL, XMSG_XCASH_GET_SYNC_INFO, &replies)) {
        ERROR_PRINT("Failed to get sync info from all nodes");
        cleanup_responses(replies);
        return XCASH_ERROR;
    }

    xcash_node_sync_info_t* majority_list = NULL;
    size_t majority_count = 0;

    // Process the responses to determine the majority list
    if (!check_sync_nodes_majority_list(replies, &majority_list, &majority_count, true)) {
        ERROR_PRINT("Failed to process majority list from sync nodes");
        cleanup_responses(replies);
        return XCASH_ERROR;
    }

    *majority_count_result = majority_count;
    *majority_list_result = majority_list;

    cleanup_responses(replies);
    return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: initial_db_sync_check
Description: Check data integrity and return majority_list and count of majority_list nodes
Parameters:
  param majority_count Pointer to store the count of nodes in the majority list.
  param majority_list_result Optional. If NULL, the list will be freed internally.
return true if a valid majority is reached, false otherwise.
Return: 0 if an error has occured, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
bool initial_db_sync_check(size_t* majority_count, xcash_node_sync_info_t** majority_list_result) {
    if (!majority_count) {
        ERROR_PRINT("Invalid argument: majority_count is NULL");
        return XCASH_ERROR;
    }

    *majority_count = 0;
    xcash_node_sync_info_t* nodes_majority_list = NULL;
    size_t nodes_majority_count = 0;

    INFO_STAGE_PRINT("Checking the network data majority");

    // Attempt to get the majority list
    if (!get_sync_nodes_majority_list_top(&nodes_majority_list, &nodes_majority_count)) {
        WARNING_PRINT("Could not get data majority nodes sync list");
        return XCASH_ERROR;
    }

    // Calculate majority dynamically
    size_t required_majority = (nodes_majority_count / 2) + 1;

    // Validate if we have enough majority nodes
    if (nodes_majority_count < required_majority) {
        INFO_PRINT_STATUS_FAIL("Not enough data majority. Nodes available: [%ld], Required majority: [%ld]", 
                               nodes_majority_count, required_majority);
        free(nodes_majority_list);
        return XCASH_ERROR;
    }

    INFO_PRINT_STATUS_OK("Data majority reached. Nodes available: [%ld], Required majority: [%ld]", 
                         nodes_majority_count, required_majority);

    // Select a sync source randomly
    int sync_source_index = get_random_majority(nodes_majority_list, nodes_majority_count);
    bool sync_result = initial_sync_node(&nodes_majority_list[sync_source_index]);

    *majority_count = nodes_majority_count;

    // Handle majority list memory
    if (majority_list_result) {
        *majority_list_result = nodes_majority_list;
    } else {
        free(nodes_majority_list);
    }

    return sync_result;
}

bool check_sync_nodes_majority_list(response_t** replies, xcash_node_sync_info_t** majority_list_result, size_t* majority_count_result, bool by_top_block_height) {
    // Validate input pointers early
    if (!replies || !majority_list_result || !majority_count_result) {
        ERROR_PRINT("Invalid arguments passed to check_sync_nodes_majority_list");
        return false;
    }

    *majority_list_result = NULL;
    *majority_count_result = 0;

    // Count valid replies
    size_t num_replies = 0;
    for (size_t i = 0; replies[i]; i++) {
        if (replies[i]->status == STATUS_OK) {
            num_replies++;
        }
    }

    if (num_replies == 0) {
        WARNING_PRINT("No valid replies received. Can't make majority sync list");
        return true;  // Return true to indicate no error but no valid data
    }

    // Allocate memory for sync states
    xcash_node_sync_info_t* sync_states_list = calloc(num_replies, sizeof(xcash_node_sync_info_t));
    if (!sync_states_list) {
        ERROR_PRINT("Memory allocation failed for sync_states_list");
        return false;
    }

    char parse_buffer[DATA_HASH_LENGTH + 1] = {0};
    char record_name[DB_COLLECTION_NAME_SIZE] = {0};
    size_t sync_state_index = 0;

    // Parse responses
    for (size_t i = 0; replies[i] && sync_state_index < num_replies; i++) {
        if (replies[i]->status != STATUS_OK) continue;

        xcash_node_sync_info_t* current_sync_state = &sync_states_list[sync_state_index];
        memset(current_sync_state, 0, sizeof(xcash_node_sync_info_t));

        if (parse_json_data(replies[i]->data, "public_address", current_sync_state->public_address, sizeof(current_sync_state->public_address)) == 0) {
            ERROR_PRINT("Can't parse 'public_address' reply from %s", replies[i]->host);
            continue;
        }

        if (parse_json_data(replies[i]->data, "block_height", parse_buffer, sizeof(parse_buffer)) == 0 ||
            sscanf(parse_buffer, "%zu", &current_sync_state->block_height) != 1) {
            ERROR_PRINT("Can't parse 'block_height' reply from %s", replies[i]->host);
            continue;
        }

        // Parse database hashes
        bool parse_error = false;
        for (size_t db_i = 0; db_i < DATABASE_TOTAL; db_i++) {
            sprintf(record_name, "data_hash_%s", collection_names[db_i]);
            if (parse_json_data(replies[i]->data, record_name, current_sync_state->db_hashes[db_i], sizeof(current_sync_state->db_hashes[db_i])) == 0) {
                ERROR_PRINT("Can't parse '%s' reply from %s", record_name, replies[i]->host);
                parse_error = true;
                break;
            }
        }
        if (parse_error) continue;

        sync_state_index++;
    }

    if (sync_state_index == 0) {
        WARNING_PRINT("All valid replies failed to parse correctly");
        free(sync_states_list);
        return true;  // No valid data but not an error
    }

    // Create a majority list based on parsed data
    xcash_node_sync_info_t** sync_majority_list = make_nodes_majority_list(sync_states_list, sync_state_index, by_top_block_height);
    if (!sync_majority_list) {
        ERROR_PRINT("Failed to create majority list");
        free(sync_states_list);
        return false;
    }

    // Calculate majority count
    size_t majority_count = 0;
    while (sync_majority_list[majority_count]) {
        majority_count++;
    }

    if (majority_count == 0) {
        WARNING_PRINT("No majority nodes found");
        free(sync_majority_list);
        free(sync_states_list);
        return true;
    }

    // Allocate memory for the majority states list
    xcash_node_sync_info_t* majority_states_list = calloc(majority_count, sizeof(xcash_node_sync_info_t));
    if (!majority_states_list) {
        ERROR_PRINT("Memory allocation failed for majority_states_list");
        free(sync_majority_list);
        free(sync_states_list);
        return false;
    }

    // Copy the majority sync statuses
    for (size_t i = 0; i < majority_count; i++) {
        memcpy(&majority_states_list[i], sync_majority_list[i], sizeof(xcash_node_sync_info_t));
    }

    // Set the results
    *majority_count_result = majority_count;
    *majority_list_result = majority_states_list;

    // Cleanup
    free(sync_majority_list);
    free(sync_states_list);
    return true;
}