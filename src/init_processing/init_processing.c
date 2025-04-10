#include "init_processing.h"

static int total_threads = 0;

/*---------------------------------------------------------------------------------------------------------
Name: configure_uv_threadpool
Description: Sets the UV_THREADPOOL_SIZE environment variable. Default is the system default of 4 and 
setting it 2–4 times the number of CPU cores can improve performance.
---------------------------------------------------------------------------------------------------------*/
bool configure_uv_threadpool(const arg_config_t *arg_config) {
  int wsthreads = get_nprocs();
  if (wsthreads < 1) {
    WARNING_PRINT("Failed to get CPU core count. Defaulting to system default of 4 threads.");
    total_threads = 4;
    return XCASH_OK;
  }

  if (arg_config->total_threads == 0) {
    total_threads = wsthreads * 2;
  } else if (arg_config->total_threads >= (wsthreads*2)) {
    total_threads = wsthreads * 2;
    WARNING_PRINT("Limiting UV_THREADPOOL_SIZE to %d", total_threads);
  } else {
    total_threads = arg_config->total_threads;
  }

  if (total_threads > MAX_THREADS) {
    total_threads = MAX_THREADS;
    WARNING_PRINT("Capping UV_THREADPOOL_SIZE to %d", MAX_THREADS);
  }

  char threadpool_size[10];
  snprintf(threadpool_size, sizeof(threadpool_size), "%d", total_threads);

  if (setenv("UV_THREADPOOL_SIZE", threadpool_size, 1) != 0) {
    ERROR_PRINT("Failed to set UV_THREADPOOL_SIZE");
    return XCASH_ERROR;
  } else {
    DEBUG_PRINT("UV_THREADPOOL_SIZE set to %s", threadpool_size);
  }

  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: init_processing
Description: Initialize globals and print program start header.
---------------------------------------------------------------------------------------------------------*/
bool init_processing(const arg_config_t *arg_config) {
  network_data_nodes_amount = get_seed_node_count();

  if (arg_config->init_db_from_seeds) {
    INFO_STAGE_PRINT("Initializing database from seeds");
    if (!init_db_from_seeds()) {
      ERROR_PRINT("Can't initialize database from seeds");
      return XCASH_ERROR;
    };
  }

  if (arg_config->init_db_from_top) {
    if (!fill_delegates_from_db()) {
      ERROR_PRINT("Can't read delegates list from DB");
      return XCASH_ERROR;
    }
    INFO_STAGE_PRINT("Initializing database from top height nodes");
    if (!init_db_from_top()) {
      ERROR_PRINT("Can't initialize database from top height nodes");
      return XCASH_ERROR;
    };
  }

  // brief check if database is empty
  if (count_db_delegates() <= 0 || count_db_statistics() <= 0) {
    // Check if it should create the default database data
    char json_buffer[TRANSFER_BUFFER_SIZE];

    for (int i = 0; network_nodes[i].seed_public_address != NULL; i++) {
      char delegate_name[256];
      strncpy(delegate_name, network_nodes[i].ip_address, sizeof(delegate_name));
      delegate_name[sizeof(delegate_name) - 1] = '\0';  // Null-terminate
      // Replace '.' with '_'
      for (char *p = delegate_name; *p; p++) {
        if (*p == '.') *p = '_';
      }

      snprintf(json_buffer, sizeof(json_buffer),
               "{"
               "\"public_address\":\"%s\","
               "\"total_vote_count\":\"0\","
               "\"IP_address\":\"%s\","
               "\"delegate_name\":\"%s_xcash_foundation\","
               "\"about\":\"Official xCash-Labs Node\","
               "\"website\":\"%s\","
               "\"team\":\"xCash-Labs Team\","
               "\"shared_delegate_status\":\"solo\","
               "\"delegate_fee\":\"\","
               "\"server_specs\":\"Operating System = Ubuntu 22.04\","
               "\"block_verifier_score\":\"0\","
               "\"online_status\":\"false\","
               "\"block_verifier_total_rounds\":\"0\","
               "\"block_verifier_online_total_rounds\":\"0\","
               "\"block_verifier_online_percentage\":\"0\","
               "\"block_producer_total_rounds\":\"0\","
               "\"block_producer_block_heights\":\"|%d\","
               "\"public_key\":\"%s\""
               "}",
               network_nodes[i].seed_public_address,
               network_nodes[i].ip_address,
               delegate_name,
               network_nodes[i].ip_address,
               XCASH_PROOF_OF_STAKE_BLOCK_HEIGHT,
               network_nodes[i].seed_public_key);

      if (insert_document_into_collection_json(DATABASE_NAME, "delegates", json_buffer) != XCASH_OK) {
        ERROR_PRINT("Failed to insert delegate document during initialization. IP: %s", network_nodes[i].ip_address);
        return XCASH_ERROR;
      }
    }
  }

  const char* statistics_default_data =
    "{\"username\":\"XCASH\","
    "\"most_total_rounds_delegate_name\":\"xcashseeds_us\","
    "\"most_total_rounds\":\"0\","
    "\"best_block_verifier_online_percentage_delegate_name\":\"xcashseeds_us\","
    "\"best_block_verifier_online_percentage\":\"0\","
      "\"most_block_producer_total_rounds_delegate_name\":\"xcashseeds_us\","
    "\"most_block_producer_total_rounds\":\"0\"}";

  if (insert_document_into_collection_json(DATABASE_NAME, "statistics", statistics_default_data) != XCASH_OK) {
    ERROR_PRINT("Failed to insert statistics document during initialization.");
    return XCASH_ERROR;
  }

  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: print_starter_state
Description: Print program start header.
---------------------------------------------------------------------------------------------------------*/
void print_starter_state(const arg_config_t *arg_config)
{
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
  fprintf(stderr, "Daemon startup successful...\n");

  #define xcash_tech_status_fmt \
  "%s (%s)\n\n"\
  "Address:\t%s\n"\
  "\n"\
  "Node Type:\t%s\n"\
  "\n"\
  "Services:\n"\
  "Daemon:\t\t%s:%d\n"\
  "DPoPS:\t\t%s:%d\n"\
  "Wallet:\t\t%s:%d\n"\
  "MongoDB:\t%s\n"\
  "Total threads:\t\%d\n"\
  "Log level:\t\%d\n"
  
  INFO_PRINT(xcash_tech_status_fmt,
    XCASH_DPOPS_CURRENT_VERSION, "~Lazarus",
    arg_config->block_verifiers_secret_key,
    is_seed_node ? "SEED NODE" : "DELEGATE NODE",
    XCASH_DAEMON_IP, XCASH_DAEMON_PORT,
    XCASH_DPOPS_IP, XCASH_DPOPS_PORT,
    XCASH_WALLET_IP, XCASH_WALLET_PORT,
    DATABASE_CONNECTION, total_threads, log_level
  );
}