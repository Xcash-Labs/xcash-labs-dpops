#include "init_processing.h"

static int total_threads = 0;

/*---------------------------------------------------------------------------------------------------------
Name: configure_uv_threadpool
Description: Sets the UV_THREADPOOL_SIZE environment variable. Default is the system default of 4 and can
not be greater that the number of cpus for the server.
---------------------------------------------------------------------------------------------------------*/
bool configure_uv_threadpool(const arg_config_t *arg_config) {
  total_threads = arg_config->total_threads;

  if (total_threads == 0) {
    total_threads = 4;
  }

  int wsthreads = get_nprocs();
  if (wsthreads < 1) {
    ERROR_PRINT("Failed to get CPU core count. Defaulting to 4 threads.");
    total_threads = 4;
  } else if (total_threads > wsthreads) {
    total_threads = wsthreads;
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

  if (!configure_uv_threadpool(&arg_config)) {
    return XCASH_ERROR;
  }

  network_data_nodes_amount = get_seed_node_count();
  pthread_rwlock_init(&rwlock,NULL);
  
  current_round_part[0] = '1';
  current_round_part[1] = '\0';
  current_round_part_backup_node[0] = '0';
  current_round_part_backup_node[1] = '\0';
  memset(&previous_block_verifiers_list, 0, sizeof(previous_block_verifiers_list));
  memset(&current_block_verifiers_list, 0, sizeof(current_block_verifiers_list));
  memset(&next_block_verifiers_list, 0, sizeof(next_block_verifiers_list));

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
    DATABASE_CONNECTION, arg_config->total_threads, log_level
  );

  retrun XCASH_OK;
}