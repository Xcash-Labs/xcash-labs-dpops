#include "xcash_delegates.h"

delegates_t temp_instance;

size_t delegate_field_sizes[NUM_FIELDS] = {sizeof(temp_instance.public_address),
                                           sizeof(temp_instance.total_vote_count),
                                           sizeof(temp_instance.IP_address),
                                           sizeof(temp_instance.delegate_name),
                                           sizeof(temp_instance.about),
                                           sizeof(temp_instance.website),
                                           sizeof(temp_instance.team),
                                           sizeof(temp_instance.shared_delegate_status),
                                           sizeof(temp_instance.delegate_fee),
                                           sizeof(temp_instance.server_specs),
                                           sizeof(temp_instance.block_verifier_score),
                                           sizeof(temp_instance.online_status),
                                           sizeof(temp_instance.block_verifier_total_rounds),
                                           sizeof(temp_instance.block_verifier_online_total_rounds),
                                           sizeof(temp_instance.block_verifier_online_percentage),
                                           sizeof(temp_instance.block_producer_total_rounds),
                                           sizeof(temp_instance.block_producer_block_heights),
                                           sizeof(temp_instance.public_key)};

const char* delegate_keys[NUM_FIELDS] = {
    "public_address",
    "total_vote_count",
    "IP_address",
    "delegate_name",
    "about",
    "website",
    "team",
    "shared_delegate_status",
    "delegate_fee",
    "server_specs",
    "block_verifier_score",
    "online_status",
    "block_verifier_total_rounds",
    "block_verifier_online_total_rounds",
    "block_verifier_online_percentage",
    "block_producer_total_rounds", 
    "block_producer_block_heights",
    "public_key",
};

// Helper function to get the position of a delegate in the network_data_nodes_list
int get_network_data_node_position(const char* public_address) {
    int i = 0;
    while (network_nodes[i].seed_public_address != NULL) {
        if (strcmp(public_address, network_nodes[i].seed_public_address) == 0) {
            return i;  // Return the index if a match is found
        }
        i++;
    }
    return network_data_nodes_amount + 1;  // Return -1 if not found
}

// Comparison function for qsort
int compare_delegates(const void* a, const void* b) {
    const delegates_t* delegate1 = (const delegates_t*)a;
    const delegates_t* delegate2 = (const delegates_t*)b;

    // 1. Sort by the position of the delegate in the network data nodes list
    int position1 = get_network_data_node_position(delegate1->public_address);
    int position2 = get_network_data_node_position(delegate2->public_address);
    if (position1 != position2) {
        return position1 - position2;
    }

    // 2. Sort by if the delegate is online or offline

    // to be compatible to original

    // int settings;
    // if ((settings = strcmp(delegate2->online_status, delegate1->online_status)) != 0) {
    //     return settings < 0 ? -1 : 1;
    // }

    // int online_status1 = strcmp(delegate1->online_status, "true") == 0 ? 1 : 0;
    // int online_status2 = strcmp(delegate2->online_status, "true") == 0 ? 1 : 0;
    // if (online_status1 != online_status2) {
    //     return online_status2 - online_status1;
    // }

    // 3. Sort by how many total votes the delegate has

    // remain code from original to be compatible
    long long int count;
    long long int count2;
    sscanf(delegate1->total_vote_count, "%lld", &count);
    sscanf(delegate2->total_vote_count, "%lld", &count2);

    if (count != count2) {
        return count2 - count < 0 ? -1 : 1;
    }

    // int total_votes1 = atoi(delegate1->total_vote_count);
    // int total_votes2 = atoi(delegate2->total_vote_count);
    // if (total_votes1 != total_votes2) {
    //     return total_votes2 - total_votes1;
    // }

    // 4. Sort by the public address
    return strcmp(delegate1->public_address, delegate2->public_address);
}

int read_organize_delegates(delegates_t* delegates, size_t* delegates_count_result) {
    bson_error_t error;
    int delegates_count;

    bson_t* delegates_db_data = bson_new();
    if (!db_find_all_doc(DATABASE_NAME, collection_names[XCASH_DB_DELEGATES], delegates_db_data, &error)) {
        DEBUG_PRINT("Failed to read delegates from db. %s", error.message);
        bson_destroy(delegates_db_data);
        return XCASH_ERROR;
    }

    // TODO probably, if the db is brand new, we should fill at least the nodes information

    delegates_count = count_recs(delegates_db_data);
    if (delegates_count == 0 || delegates_count < 20) {
        WARNING_PRINT("delegates db has only %d delegates", delegates_count);
    }

    bson_iter_t iter;
    int delegate_index = 0;
    if (bson_iter_init(&iter, delegates_db_data)) {
        while (delegate_index < MAXIMUM_AMOUNT_OF_DELEGATES && bson_iter_next(&iter)) {
            bson_t record;
            const uint8_t* data;
            uint32_t len;

            bson_iter_document(&iter, &len, &data);
            bson_init_static(&record, data, len);

            bson_iter_t record_iter;
            if (bson_iter_init(&record_iter, &record)) {
                while (bson_iter_next(&record_iter)) {
                    // get db record key and assign it to delegate structure accordingly
                    const char* db_key = bson_iter_key(&record_iter);
                    char* current_delegate = (char*)&delegates[delegate_index];
                    bool field_set = false;
                    for (int field_index = 0; field_index < NUM_FIELDS; field_index++) {
                        if (strcmp(db_key, delegate_keys[field_index]) == 0) {
                            strncpy(current_delegate, bson_iter_utf8(&record_iter, NULL),
                                    delegate_field_sizes[field_index]);
                            // make sure it's \0 terminated string in case the field is bigger than we expect
                            current_delegate[delegate_field_sizes[field_index] - 1] = '\0';
                            field_set = true;
                            break;
                        }
                        current_delegate += delegate_field_sizes[field_index];  // step to next field
                    }

                    if (!field_set) {
                        DEBUG_PRINT("The db key '%s' doesn't belong to delegate structure", db_key);
                        bson_destroy(delegates_db_data);
                        return XCASH_ERROR;
                    }
                }
            }
            delegate_index++;
        }
    }

    bson_destroy(delegates_db_data);

    qsort(delegates, delegates_count, sizeof(delegates_t), compare_delegates);

    // for (int i = 0; i < delegates_count; i++) {
    //     fprintf(stderr, "public_address: %s\n", delegates[i].public_address);
    //     fprintf(stderr, "\tdelegate_name: %s\n", delegates[i].delegate_name);
    //     fprintf(stderr, "\tonline_status: %s\n", delegates[i].online_status);
    //     fprintf(stderr, "\ttotal_vote_count: %s\n", delegates[i].total_vote_count);
    // }

    *delegates_count_result = delegates_count;

    return XCASH_OK;
}