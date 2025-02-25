#include "db_sync.h"

void show_majority_statistics(const xcash_node_sync_info_t* majority_list, size_t items_count) {
    if (!majority_list || items_count == 0) {
        WARNING_PRINT("No valid nodes in majority list. The network might be offline.");
        return;
    }
    INFO_STAGE_PRINT("Nodes majority status (%ld nodes found):", items_count);

    for (size_t i = 0; i < items_count; i++) {
        const char* node_name = address_to_node_name(majority_list[i].public_address);
        INFO_PRINT_STATUS_OK("Node: %-40s", node_name ? node_name : "Unknown");
    }
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

/// @brief Check data integrity and return majority_list and count of majority_list nodes
/// @param majority_count Pointer to store the count of nodes in the majority list.
/// @param majority_list_result Optional. If NULL, the list will be freed internally.
/// @return true if a valid majority is reached, false otherwise.
bool initial_db_sync_check(size_t* majority_count, xcash_node_sync_info_t** majority_list_result) {
    if (!majority_count) {
        ERROR_PRINT("Null pointer passed for majority_count");
        return false;
    }

    *majority_count = 0;
    xcash_node_sync_info_t* nodes_majority_list = NULL;
    size_t nodes_majority_count = 0;
    bool result = false;

    INFO_STAGE_PRINT("Checking the network data majority...");

    // Retrieve the majority list of synced nodes
//    if (!get_sync_nodes_majority_list_top(&nodes_majority_list, &nodes_majority_count)) {
//        WARNING_PRINT("Could not retrieve data majority nodes sync list.");
//    }

    show_majority_statistics(nodes_majority_list, nodes_majority_count);

    if (nodes_majority_count < BLOCK_VERIFIERS_VALID_AMOUNT) {
        INFO_PRINT_STATUS_FAIL("Not enough data majority. Nodes: [%ld/%d]", nodes_majority_count, BLOCK_VERIFIERS_VALID_AMOUNT);
    } else {
        INFO_PRINT_STATUS_OK("Data majority reached. Nodes: [%ld/%d]", nodes_majority_count, BLOCK_VERIFIERS_VALID_AMOUNT);
        
        // Pick a random sync source and attempt initial sync
        int sync_source_index = get_random_majority(nodes_majority_list, nodes_majority_count);
        result = initial_sync_node(&nodes_majority_list[sync_source_index]);

        *majority_count = nodes_majority_count;
    }

    // Handle memory allocation for output parameter
    if (majority_list_result) {
        *majority_list_result = nodes_majority_list;
    } else {
        free(nodes_majority_list);
    }

    return result;
}
