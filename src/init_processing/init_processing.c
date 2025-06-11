#include "init_processing.h"

/*---------------------------------------------------------------------------------------------------------
Name: init_processing
Description: Initialize globals and print program start header.
---------------------------------------------------------------------------------------------------------*/
bool init_processing(const arg_config_t *arg_config) {
  network_data_nodes_amount = get_seed_node_count();

  if (arg_config->init_db_from_seeds) {
    INFO_STAGE_PRINT("Initializing database from seeds");
//    if (!init_db_from_seeds()) {
//      ERROR_PRINT("Can't initialize database from seeds");
//      return XCASH_ERROR;
//    };
  }

  if (arg_config->init_db_from_top) {
    if (!fill_delegates_from_db()) {
      ERROR_PRINT("Can't read delegates list from DB");
      return XCASH_ERROR;
    }
    INFO_STAGE_PRINT("Initializing database from top height nodes");
//    if (!init_db_from_top()) {
//      ERROR_PRINT("Can't initialize database from top height nodes");
//      return XCASH_ERROR;
//    };
  }

  // Check if database is empty and create the default database data
  if (count_db_delegates() <= 0) {
    INFO_PRINT("Delegates collection does not exist so creating it.");
    for (int i = 0; network_nodes[i].seed_public_address != NULL; i++) {
      char delegate_name[256];
      strncpy(delegate_name, network_nodes[i].ip_address, sizeof(delegate_name));
      delegate_name[sizeof(delegate_name) - 1] = '\0';  // Null-terminate
      // Replace '.' with '_'
      for (char *p = delegate_name; *p; p++) {
        if (*p == '.') *p = '_';
      }

      uint64_t registration_time = (uint64_t)time(NULL);
      double set_delegate_fee = 0.00;
      uint64_t set_counts = 0;

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

    // Numbers (need to be in this order for mongodb to assign types correctly)
      bson_append_int64(&bson, "total_vote_count", -1, set_counts);
      bson_append_int64(&bson, "block_verifier_total_rounds", -1, set_counts);
      bson_append_int64(&bson, "block_verifier_online_total_rounds", -1, set_counts);
      bson_append_int64(&bson, "block_producer_total_rounds", -1, set_counts);
      bson_append_int64(&bson, "registration_timestamp", -1, registration_time);
      bson_append_double(&bson, "delegate_fee", -1, set_delegate_fee);

      if (insert_document_into_collection_bson(DATABASE_NAME, "delegates", &bson) != XCASH_OK) {
        ERROR_PRINT("Failed to insert delegate document.");
        bson_destroy(&bson);
        return XCASH_ERROR;
      }

      bson_destroy(&bson);
    }
  }

  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: print_starter_state
Description: Print program start header.
---------------------------------------------------------------------------------------------------------*/
void print_starter_state(const arg_config_t *arg_config)
{
  (void) arg_config;
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
          "\n"
          "Services:\n"
          "Daemon:\t\t%s:%d\n"
          "DPoPS:\t\t%s:%d\n"
          "Wallet:\t\t%s:%d\n"
          "MongoDB:\t%s\n"
          "Log level:\t%d\n",
          XCASH_DPOPS_CURRENT_VERSION, "~Lazarus",
          xcash_wallet_public_address,
          is_seed_node ? "SEED NODE" : "DELEGATE NODE",
          XCASH_DAEMON_IP, XCASH_DAEMON_PORT,
          XCASH_DPOPS_IP, XCASH_DPOPS_PORT,
          XCASH_WALLET_IP, XCASH_WALLET_PORT,
          DATABASE_CONNECTION, log_level);

  fprintf(stderr, "[%s] Daemon startup successful and is busy processing requests...\n\n", time_str);
}