#include "xcash_block.h"

bool sync_block_producers(void) {
    response_t** replies = NULL;

    if (!send_message(XNET_DELEGATES_ALL, XMSG_XCASH_GET_BLOCK_PRODUCERS, &replies)) {
        WARNING_PRINT("Could not send XMSG_XCASH_GET_BLOCK_PRODUCERS message");
        cleanup_responses(replies);
        return false;
    }

    size_t producers_majority[BLOCK_VERIFIERS_AMOUNT];
    memset(producers_majority, 0, sizeof(producers_majority));

    for(size_t i = 0; replies && replies[i]; i++) {
        if (replies[i]->status == STATUS_OK) {
            json_error_t error;
            json_t *msg_json = json_loads(replies[i]->data, 0, &error);
            if (!msg_json) {
                WARNING_PRINT("Can't parse msg json from %s, %s", replies[i]->host, replies[i]->data);
                continue;
            }

            json_t *producers_array = json_object_get(msg_json, "producers");
            if (!producers_array) {
                WARNING_PRINT("Can't get producers value from %s, %s", replies[i]->host, replies[i]->data);
                json_decref(msg_json);
                continue;
            }

            size_t index;
            json_t *value;
            json_array_foreach(producers_array, index, value) {
                const char *producer_public_address = json_string_value(value);
                // fprintf(stderr, "%s - %s\n", producer_public_address, address_to_node_name(producer_public_address));

                for (size_t j = 0; j < BLOCK_VERIFIERS_AMOUNT; j++)
                {
                    if (strcmp(delegates_all[j].public_address, producer_public_address) == 0) {
                        producers_majority[j]++;
                        break;
                    }
                }
            }
            json_decref(msg_json);
        }
    }

    cleanup_responses(replies);



    // set online status only for nodes in majority list
    INFO_STAGE_PRINT("Round Block Producers");
    size_t majority_nodes_count = 0;
    for (size_t i = 0; i < BLOCK_VERIFIERS_AMOUNT; i++)
    {
        if (producers_majority[i]>=BLOCK_VERIFIERS_VALID_AMOUNT) {
            strcpy(delegates_all[i].online_status, "true");
            majority_nodes_count++;
            INFO_PRINT_STATUS_OK("[%02ld] %-40s",producers_majority[i], delegates_all[i].delegate_name);
        } else if (strlen(delegates_all[i].public_address) !=0){
            strcpy(delegates_all[i].online_status, "false");
            if (producers_majority[i]>0) {
                INFO_PRINT_STATUS_FAIL("[%02ld] %-40s",producers_majority[i], delegates_all[i].delegate_name);
            }
        }
    }


    if (majority_nodes_count < BLOCK_VERIFIERS_VALID_AMOUNT) {
        INFO_PRINT_STATUS_FAIL("[%02ld/%02ld] Not enough valid producers to continue", majority_nodes_count, BLOCK_VERIFIERS_VALID_AMOUNT);
        return false;
    }

    // FIXME until we don't have stable network exchange we should consider to check all possible delegates

    if (!select_block_producers(0)) {
      INFO_PRINT_STATUS_FAIL("Block producer NOT selected");
      return false;
    }

    INFO_PRINT_STATUS_OK("Block producer selected");
    show_block_producer(0);

    return true;
}