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