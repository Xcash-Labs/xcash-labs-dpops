#include "xcash_delegates.h"

// delegates_t temp_instance;

// Helper function to get the position of a delegate in the network_data_nodes_list
int get_network_data_node_position(const char* public_address) {
  for (int i = 0; i < network_data_nodes_amount; i++) {
    if (network_nodes[i].seed_public_address == NULL) break;
    if (strcmp(public_address, network_nodes[i].seed_public_address) == 0) {
      return i;
    }
  }
  // Return a value larger than any valid index to push non-seed nodes after seeds
  return network_data_nodes_amount + 1;
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

  // 3. Sort by how many total votes the delegate has
  uint64_t count = delegate1->total_vote_count;
  uint64_t count2 = delegate2->total_vote_count;
  if (count2 < count)
    return -1;
  else if (count2 > count)
    return 1;
  else
    return 0;

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

  delegates_count = count_recs(delegates_db_data);
  if (delegates_count == 0 || delegates_count < 20) {
    WARNING_PRINT("delegates db has only %d delegates", delegates_count);
  }

  memset(delegates, 0, sizeof(delegates_t) * BLOCK_VERIFIERS_TOTAL_AMOUNT);

  bson_iter_t iter;
  int delegate_index = 0;
  time_t now = time(NULL);

  if (bson_iter_init(&iter, delegates_db_data)) {
    while (delegate_index < BLOCK_VERIFIERS_TOTAL_AMOUNT && bson_iter_next(&iter)) {
      bson_t record;
      const uint8_t* data;
      uint32_t len;

      bson_iter_document(&iter, &len, &data);
      bson_init_static(&record, data, len);

      bson_iter_t record_iter;
      bool skip_delegate = false;

/*     
      if (bson_iter_init(&record_iter, &record)) {
        while (bson_iter_next(&record_iter)) {
          const char* db_key = bson_iter_key(&record_iter);

          if (strcmp(db_key, "public_address") == 0 && BSON_ITER_HOLDS_UTF8(&record_iter)) {
            strncpy(delegates[delegate_index].public_address, bson_iter_utf8(&record_iter, NULL), XCASH_WALLET_LENGTH);
          } else if (strcmp(db_key, "total_vote_count") == 0) {
            if (BSON_ITER_HOLDS_INT64(&record_iter)) {
              delegates[delegate_index].total_vote_count = bson_iter_int64(&record_iter);
            } else if (BSON_ITER_HOLDS_INT32(&record_iter)) {
              delegates[delegate_index].total_vote_count = bson_iter_int32(&record_iter);
            } else {
              WARNING_PRINT("Unexpected type for total_vote_count: %d", bson_iter_type(&record_iter));
            }
          } else if (strcmp(db_key, "IP_address") == 0 && BSON_ITER_HOLDS_UTF8(&record_iter)) {
            strncpy(delegates[delegate_index].IP_address, bson_iter_utf8(&record_iter, NULL), IP_LENGTH);
          } else if (strcmp(db_key, "delegate_name") == 0 && BSON_ITER_HOLDS_UTF8(&record_iter)) {
            strncpy(delegates[delegate_index].delegate_name, bson_iter_utf8(&record_iter, NULL), MAXIMUM_BUFFER_SIZE_DELEGATES_NAME);
          } else if (strcmp(db_key, "about") == 0 && BSON_ITER_HOLDS_UTF8(&record_iter)) {
            strncpy(delegates[delegate_index].about, bson_iter_utf8(&record_iter, NULL), 1024);
          } else if (strcmp(db_key, "website") == 0 && BSON_ITER_HOLDS_UTF8(&record_iter)) {
            strncpy(delegates[delegate_index].website, bson_iter_utf8(&record_iter, NULL), 255);
          } else if (strcmp(db_key, "team") == 0 && BSON_ITER_HOLDS_UTF8(&record_iter)) {
            strncpy(delegates[delegate_index].team, bson_iter_utf8(&record_iter, NULL), 255);
          } else if (strcmp(db_key, "delegate_type") == 0 && BSON_ITER_HOLDS_UTF8(&record_iter)) {
            strncpy(delegates[delegate_index].delegate_type, bson_iter_utf8(&record_iter, NULL), 10);
          } else if (strcmp(db_key, "delegate_fee") == 0) {
            if (BSON_ITER_HOLDS_DOUBLE(&record_iter)) {
              delegates[delegate_index].delegate_fee = bson_iter_double(&record_iter);
            } else {
              WARNING_PRINT("Unexpected type for delegate_fee: %d", bson_iter_type(&record_iter));
            }
          } else if (strcmp(db_key, "server_specs") == 0 && BSON_ITER_HOLDS_UTF8(&record_iter)) {
            strncpy(delegates[delegate_index].server_specs, bson_iter_utf8(&record_iter, NULL), 1024);
          } else if (strcmp(db_key, "online_status") == 0 && BSON_ITER_HOLDS_UTF8(&record_iter)) {
            strncpy(delegates[delegate_index].online_status, bson_iter_utf8(&record_iter, NULL), 10);
          } else if (strcmp(db_key, "block_verifier_total_rounds") == 0) {
            if (BSON_ITER_HOLDS_INT64(&record_iter)) {
              delegates[delegate_index].block_verifier_total_rounds = bson_iter_int64(&record_iter);
            } else if (BSON_ITER_HOLDS_INT32(&record_iter)) {
              delegates[delegate_index].block_verifier_total_rounds = bson_iter_int32(&record_iter);
            } else {
              WARNING_PRINT("Unexpected type for block_verifier_total_rounds: %d", bson_iter_type(&record_iter));
            }
          } else if (strcmp(db_key, "block_verifier_online_total_rounds") == 0) {
            if (BSON_ITER_HOLDS_INT64(&record_iter)) {
              delegates[delegate_index].block_verifier_online_total_rounds = bson_iter_int64(&record_iter);
            } else if (BSON_ITER_HOLDS_INT32(&record_iter)) {
              delegates[delegate_index].block_verifier_online_total_rounds = bson_iter_int32(&record_iter);
            } else {
              WARNING_PRINT("Unexpected type for block_verifier_online_total_rounds: %d", bson_iter_type(&record_iter));
            }
          } else if (strcmp(db_key, "block_producer_total_rounds") == 0) {
            if (BSON_ITER_HOLDS_INT64(&record_iter)) {
              delegates[delegate_index].block_producer_total_rounds = bson_iter_int64(&record_iter);
            } else if (BSON_ITER_HOLDS_INT32(&record_iter)) {
              delegates[delegate_index].block_producer_total_rounds = bson_iter_int32(&record_iter);
            } else {
              WARNING_PRINT("Unexpected type for block_producer_total_rounds: %d", bson_iter_type(&record_iter));
            }
          } else if (strcmp(db_key, "public_key") == 0 && BSON_ITER_HOLDS_UTF8(&record_iter)) {
            strncpy(delegates[delegate_index].public_key, bson_iter_utf8(&record_iter, NULL), VRF_PUBLIC_KEY_LENGTH);
          } else if (strcmp(db_key, "registration_timestamp") == 0) {
            if (BSON_ITER_HOLDS_INT64(&record_iter)) {
              time_t reg_time = bson_iter_int64(&record_iter);
              if (now - reg_time < 300) {
                skip_delegate = true;
              }
              delegates[delegate_index].registration_timestamp = reg_time;
            } else if (BSON_ITER_HOLDS_INT32(&record_iter)) {
              time_t reg_time = bson_iter_int32(&record_iter);
              if (now - reg_time < 300) {
                skip_delegate = true;
              }
              delegates[delegate_index].registration_timestamp = reg_time;
            } else {
              WARNING_PRINT("Unexpected type for registration_timestamp: %d", bson_iter_type(&record_iter));
            }
          }
        }
      }
*/






if (bson_iter_init(&record_iter, &record)) {
  while (bson_iter_next(&record_iter)) {
    const char* db_key = bson_iter_key(&record_iter);

    if (strcmp(db_key, "public_address") == 0 && BSON_ITER_HOLDS_UTF8(&record_iter)) {
      strncpy(delegates[delegate_index].public_address, bson_iter_utf8(&record_iter, NULL), XCASH_WALLET_LENGTH);
    } else if (strcmp(db_key, "total_vote_count") == 0) {
      if (BSON_ITER_HOLDS_INT64(&record_iter) || BSON_ITER_HOLDS_INT32(&record_iter)) {
        delegates[delegate_index].total_vote_count = (uint64_t)bson_iter_as_int64(&record_iter);
      } else {
        WARNING_PRINT("Unexpected type for total_vote_count: %d", bson_iter_type(&record_iter));
      }
    } else if (strcmp(db_key, "IP_address") == 0 && BSON_ITER_HOLDS_UTF8(&record_iter)) {
      strncpy(delegates[delegate_index].IP_address, bson_iter_utf8(&record_iter, NULL), IP_LENGTH);
    } else if (strcmp(db_key, "delegate_name") == 0 && BSON_ITER_HOLDS_UTF8(&record_iter)) {
      strncpy(delegates[delegate_index].delegate_name, bson_iter_utf8(&record_iter, NULL), MAXIMUM_BUFFER_SIZE_DELEGATES_NAME);
    } else if (strcmp(db_key, "about") == 0 && BSON_ITER_HOLDS_UTF8(&record_iter)) {
      strncpy(delegates[delegate_index].about, bson_iter_utf8(&record_iter, NULL), 1024);
    } else if (strcmp(db_key, "website") == 0 && BSON_ITER_HOLDS_UTF8(&record_iter)) {
      strncpy(delegates[delegate_index].website, bson_iter_utf8(&record_iter, NULL), 255);
    } else if (strcmp(db_key, "team") == 0 && BSON_ITER_HOLDS_UTF8(&record_iter)) {
      strncpy(delegates[delegate_index].team, bson_iter_utf8(&record_iter, NULL), 255);
    } else if (strcmp(db_key, "delegate_type") == 0 && BSON_ITER_HOLDS_UTF8(&record_iter)) {
      strncpy(delegates[delegate_index].delegate_type, bson_iter_utf8(&record_iter, NULL), 10);
    } else if (strcmp(db_key, "delegate_fee") == 0) {
      if (BSON_ITER_HOLDS_DOUBLE(&record_iter)) {
        delegates[delegate_index].delegate_fee = bson_iter_double(&record_iter);
      } else {
        WARNING_PRINT("Unexpected type for delegate_fee: %d", bson_iter_type(&record_iter));
      }
    } else if (strcmp(db_key, "server_specs") == 0 && BSON_ITER_HOLDS_UTF8(&record_iter)) {
      strncpy(delegates[delegate_index].server_specs, bson_iter_utf8(&record_iter, NULL), 1024);
    } else if (strcmp(db_key, "online_status") == 0 && BSON_ITER_HOLDS_UTF8(&record_iter)) {
      strncpy(delegates[delegate_index].online_status, bson_iter_utf8(&record_iter, NULL), 10);
    } else if (strcmp(db_key, "block_verifier_total_rounds") == 0) {
      if (BSON_ITER_HOLDS_INT64(&record_iter) || BSON_ITER_HOLDS_INT32(&record_iter)) {
        delegates[delegate_index].block_verifier_total_rounds = (uint64_t)bson_iter_as_int64(&record_iter);
      } else {
        WARNING_PRINT("Unexpected type for block_verifier_total_rounds: %d", bson_iter_type(&record_iter));
      }
    } else if (strcmp(db_key, "block_verifier_online_total_rounds") == 0) {
      if (BSON_ITER_HOLDS_INT64(&record_iter) || BSON_ITER_HOLDS_INT32(&record_iter)) {
        delegates[delegate_index].block_verifier_online_total_rounds = (uint64_t)bson_iter_as_int64(&record_iter);
      } else {
        WARNING_PRINT("Unexpected type for block_verifier_online_total_rounds: %d", bson_iter_type(&record_iter));
      }
    } else if (strcmp(db_key, "block_producer_total_rounds") == 0) {
      if (BSON_ITER_HOLDS_INT64(&record_iter) || BSON_ITER_HOLDS_INT32(&record_iter)) {
        delegates[delegate_index].block_producer_total_rounds = (uint64_t)bson_iter_as_int64(&record_iter);
      } else {
        WARNING_PRINT("Unexpected type for block_producer_total_rounds: %d", bson_iter_type(&record_iter));
      }
    } else if (strcmp(db_key, "public_key") == 0 && BSON_ITER_HOLDS_UTF8(&record_iter)) {
      strncpy(delegates[delegate_index].public_key, bson_iter_utf8(&record_iter, NULL), VRF_PUBLIC_KEY_LENGTH);
    } else if (strcmp(db_key, "registration_timestamp") == 0) {
      if (BSON_ITER_HOLDS_INT64(&record_iter) || BSON_ITER_HOLDS_INT32(&record_iter)) {
        time_t reg_time = bson_iter_as_int64(&record_iter);
        if (now - reg_time < 300) {
          skip_delegate = true;
        }
        delegates[delegate_index].registration_timestamp = reg_time;
      } else {
        WARNING_PRINT("Unexpected type for registration_timestamp: %d", bson_iter_type(&record_iter));
      }
    }
  }
}






      if (!skip_delegate) {
        strncpy(delegates[delegate_index].online_status, "false", sizeof(delegates[delegate_index].online_status));
        delegates[delegate_index].online_status[sizeof(delegates[delegate_index].online_status) - 1] = '\0';
        delegate_index++;
      } else {
        INFO_PRINT("Skipping newly added delegate...");
      }
    }
  }

  bson_destroy(delegates_db_data);
  qsort(delegates, delegate_index, sizeof(delegates_t), compare_delegates);
  *delegates_count_result = delegate_index;

  return XCASH_OK;
}