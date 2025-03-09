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
bool init_processing(const arg_config_t *arg_config)
{
  network_data_nodes_amount = get_seed_node_count();
  pthread_rwlock_init(&rwlock,NULL);
  memset(&previous_block_verifiers_list, 0, sizeof(previous_block_verifiers_list));
  memset(&current_block_verifiers_list, 0, sizeof(current_block_verifiers_list));
  memset(&next_block_verifiers_list, 0, sizeof(next_block_verifiers_list));

  if (arg_config->init_db_from_seeds) {
    INFO_STAGE_PRINT("Initializing database from seeds")
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
    INFO_STAGE_PRINT("Initializing database from top height nodes")
    if (!init_db_from_top()) {
      ERROR_PRINT("Can't initialize database from top height nodes");
      return XCASH_ERROR;
    };
  }

  // brief check if database is empty
  if (count_db_delegates() <= 0 || count_db_statistics() <= 0) {
    ERROR_PRINT("'delegates' or 'statistics' DB not initialized. Do it manually with --init-db-from-seeds");
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