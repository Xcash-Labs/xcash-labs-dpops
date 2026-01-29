#include "init_processing.h"

/*---------------------------------------------------------------------------------------------------------
Name: init_processing
Description: Initialize globals and print program start header.
---------------------------------------------------------------------------------------------------------*/
bool init_processing(const arg_config_t *arg_config) {
  (void) arg_config;
  size_t i = 0;

#ifdef SEED_NODE_ON

  while (!is_replica_set_ready()) {
    INFO_PRINT("Mongodb Replica set not ready, waiting...");
    sleep(5);
  }

#endif

  network_data_nodes_amount = get_seed_node_count();

  // Check if database is empty and create the default database data if true
  if (count_db_delegates() <= 0) {
    INFO_PRINT("Delegates collection does not exist so creating it.");
    uint64_t set_counts = 0;

    for (i = 0; network_nodes[i].seed_public_address != NULL; i++) {
      char delegate_name[256];
      strncpy(delegate_name, network_nodes[i].ip_address, sizeof(delegate_name));
      delegate_name[sizeof(delegate_name) - 1] = '\0';  // Null-terminate
      // Replace '.' with '_'
      for (char *p = delegate_name; *p; p++) {
        if (*p == '.') *p = '_';
      }

      uint64_t registration_time = SEED_REGISTRATION_TIME_UTC;
      double set_delegate_fee = 0.00;
      int32_t set_delegate_minimum_payout = 5000;  // default value

      bson_t bson;
      bson_init(&bson);

      // Strings
      bson_append_utf8(&bson, "public_address", -1, network_nodes[i].seed_public_address, -1);
      bson_append_utf8(&bson, "IP_address", -1, network_nodes[i].ip_address, -1);
      bson_append_utf8(&bson, "delegate_name", -1, delegate_name, -1);
      bson_append_utf8(&bson, "about", -1, "Official xCash-Labs Node", -1);
      bson_append_utf8(&bson, "website", -1, "xcashlabs.org", -1);
      bson_append_utf8(&bson, "team", -1, "xCash-Labs Team", -1);
      bson_append_utf8(&bson, "delegate_type", -1, "seed", -1);
      bson_append_utf8(&bson, "server_specs", -1, "Operating System = Ubuntu 22.04", -1);
      bson_append_utf8(&bson, "online_status", -1, "false", -1);
      bson_append_utf8(&bson, "public_key", -1, network_nodes[i].seed_public_key, -1);

      // Numbers
      bson_append_int64(&bson, "total_vote_count", -1, set_counts);
      bson_append_double(&bson, "delegate_fee", -1, set_delegate_fee);
      bson_append_int32(&bson, "minimum_payout", -1, set_delegate_minimum_payout);
      int64_t ms = (int64_t)registration_time * 1000;
      bson_append_date_time(&bson, "registration_timestamp", -1, ms);

      if (insert_document_into_collection_bson(DATABASE_NAME, DB_COLLECTION_DELEGATES, &bson) != XCASH_OK) {
        ERROR_PRINT("Failed to insert delegate document.");
        bson_destroy(&bson);
        return false;
      }

      bson_destroy(&bson);

// Only update statistics on seed nodes
#ifdef SEED_NODE_ON

      bson_t bson_statistics;
      bson_init(&bson_statistics);

      // Strings
      BSON_APPEND_UTF8(&bson_statistics, "_id", network_nodes[i].seed_public_key);

      // Numbers
      bson_append_int64(&bson_statistics, "block_verifier_total_rounds", -1, set_counts);
      bson_append_int64(&bson_statistics, "block_verifier_online_total_rounds", -1, set_counts);
      bson_append_int64(&bson_statistics, "block_producer_total_rounds", -1, set_counts);

      // Guard watermark for exactly-once counting:
      bson_append_int64(&bson_statistics, "last_counted_block", -1, (int64_t)-1);

      // Insert into "statistics" collection
      if (insert_document_into_collection_bson(DATABASE_NAME, DB_COLLECTION_STATISTICS, &bson_statistics) != XCASH_OK) {
        ERROR_PRINT("Failed to insert statistics document during initialization.");
        bson_destroy(&bson_statistics);
        return false;
      }

// count other stuff... see cloud flare

      bson_destroy(&bson_statistics);

      if (i == 0) {
        if (!add_seed_indexes()) {
          ERROR_PRINT("Failed to add seed indexes to database!");
          return false;
        }
      }

#endif

    }
    if (!add_indexes_delegates()) {
      ERROR_PRINT("Failed to add indexes to delegates collection!");
      return false;
    }
    if (!is_seed_node) {
      if (!add_indexes_blocks_found()) {
        ERROR_PRINT("Failed to add indexes to blocks_found collection!");
        return false;
      }
    }

  }

  return true;
}

/*---------------------------------------------------------------------------------------------------------
Name: print_starter_state
Description: Print program start header.
---------------------------------------------------------------------------------------------------------*/
bool print_starter_state(const arg_config_t *arg_config) {
  (void) arg_config;
  size_t i = 0;
  int count_seeds = 0;
  static const char xcash_tech_header[] =
      "\n"
      " /$$   /$$                           /$$        / $$              / $$                    \n"
      "| $$  / $$                          | $$        | $$              | $$                    \n"
      "|  $$/ $$/ /$$$$$$$ /$$$$$$  /$$$$$$| $$$$$$$   | $$      /$$$$$$ | $$       /$$$$$$      \n"
      " \\  $$$$/ /$$_____/|____  $$/$$_____| $$__  $$  | $$     |____  $$| $$      /$$_____     \n"
      "  /$$  $$| $$       /$$$$$$|  $$$$$$| $$  \\ $$  | $$      /$$$$$$ | $$$$$$$ | $$$$$$     \n"
      " /$$/\\  $| $$      /$$__  $$\\____  $| $$  | $$  | $$     /$$__  $$| $$   $$ \\____  $$  \n"
      "| $$  \\ $|  $$$$$$|  $$$$$$$/$$$$$$$| $$  | $$/ | $$$$$$$| $$$$$$$| $$$$$$$ |$$$$$$$     \n"
      "|__/  |__/\\_______/\\_______|_______/|__/  |__|__|________/\\_______/\\________/\\______/\n"
      "\n";
  fputs(xcash_tech_header, stderr);
  time_t now = time(NULL);
  char time_str[64];
  strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
  fprintf(stderr,
          "%s (%s)\n\n"
          "Wallet Public Address:\t%s\n"
          "\n"
          "Node Type:\t%s\n"
          "Services:\n"
          "Daemon:\t\t%s:%d\n"
          "DPoPS:\t\t%s:%d\n"
          "Wallet:\t\t%s:%d\n"
          "MongoDB:\t%s\n"
          "Log level:\t%d\n"
          "Image Hash:\t%s\n",
          XCASH_DPOPS_CURRENT_VERSION, "~Lazarus",
          xcash_wallet_public_address,
          is_seed_node ? "SEED NODE" : "DELEGATE NODE",
          XCASH_DAEMON_IP, XCASH_DAEMON_PORT,
          XCASH_DPOPS_IP, XCASH_DPOPS_PORT,
          XCASH_WALLET_IP, XCASH_WALLET_PORT,
          DATABASE_CONNECTION, log_level, self_sha);
  if (!is_seed_node) {
    fprintf(stderr, "Delegate fee: %.2f%%, minimum payout: %" PRIu64 "\n", delegate_fee_percent, (uint64_t)minimum_payout);
  }

//
// Checking DNSSEC records for seeds and image version using xcashpulse dns entries
//
  INFO_PRINT("Validating DNSSEC entries...");
  for (i = 0; network_nodes[i].ip_address != NULL; i++) {
    bool have = false;
    dnssec_status_t st = dnssec_query(g_ctx, network_nodes[i].ip_address, RR_IN, &have);
    if (st == DNSSEC_SECURE && have) {
      count_seeds++;
    }
  }

  if (!(count_seeds == network_data_nodes_amount)) {
    ERROR_PRINT("Counld not validate DNSSEC records for seed nodes, unable to start");
    return false;
  }

  // Compute our running binary digest
  if (!get_self_sha256(self_sha)) {
    ERROR_PRINT("Unable to compute self SHA-256");
    return false;
  }

  char min_version[XCASH_VERSION_LENGTH + 1] = {0};
  char allowed_sha[SHA256_DIGEST_SIZE + 1] = {0};
  if (check_software_version(min_version, allowed_sha)) {
    if (semver_cmp(XCASH_DPOPS_CURRENT_VERSION_NUM, min_version) < 0) {
      ERROR_PRINT("This version is less than the minimum required version (local=%s, minimum=%s), unable to start",
       XCASH_DPOPS_CURRENT_VERSION_NUM, min_version);
      return false;
    }
  }

  get_banned_delegates();

  fprintf(stderr, "[%s] Daemon startup successful and is busy processing requests...\n\n", time_str);\
  return true;
}