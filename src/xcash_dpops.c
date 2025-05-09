#include "xcash_dpops.h"

static bool show_help = false;
static bool create_key = false;
static int sig_requests = 0;

static char doc[] =
"\n"
BRIGHT_WHITE_TEXT("General Options:\n")
"Program Bug Address: https://github.com/Xcash-Labs/xcash-labs-dpops/issues\n"
"\n"
"  -h, --help                              List all valid parameters.\n"
"  -k, --block-verifiers-secret-key <KEY>  Set the block verifier's secret key\n"
"\n"
BRIGHT_WHITE_TEXT("Debug Options:\n")
"  --log-level                             The log-level displays log messages based on the level passed:\n"
"                                          Critial - 0, Info - 1, Warning - 2, Error - 3, Debug - 4\n"
"\n"
BRIGHT_WHITE_TEXT("Advanced Options:\n")
"  --total-threads THREADS                 Sets the UV_THREADPOOL_SIZE environment variable that controls the.\n"
"                                          number of worker threads in the libuv thread pool (default is 4).\n"
"\n"
"  --generate-key                          Generate public/private key for block verifiers.\n"
"\n"
"  --init-db-from-seeds                    Sync current node data from seeds. Needed only during installation\n"
"                                          process.\n"
"\n"
"  --init-db-from-top                      Sync current node data from top block_height nodes.\n"
"\n"
"For more details on each option, refer to the documentation or use the --help option.\n";

static struct argp_option options[] = {
  {"help", 'h', 0, 0, "List all valid parameters.", 0},
  {"block-verifiers-secret-key", 'k', "SECRET_KEY", 0, "Set the block verifier's secret key", 0},
  {"log-level", OPTION_LOG_LEVEL, "LOG_LEVEL", 0, "Displays log messages based on the level passed.", 0},
  {"total-threads", OPTION_TOTAL_THREADS, "THREADS", 0, "Set total threads (Default: based on server threads).", 0},
  {"generate-key", OPTION_GENERATE_KEY, 0, 0, "Generate public/private key for block verifiers.", 0},
  {"init-db-from-seeds", OPTION_INIT_DB_FROM_SEEDS, 0, 0, "Sync current node data from seeds. Needed only during installation process", 0},
  {"init-db-from-top", OPTION_INIT_DB_FROM_TOP, 0, 0, "Sync current node data from top block_height nodes.", 0},
  {0}
};

/*---------------------------------------------------------------------------------------------------------
Name: error_t parse_opt
Description: Load program options.  Using the argp system calls.
---------------------------------------------------------------------------------------------------------*/
static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
  arg_config_t *arguments = state->input;
  switch (key)
  {
  case 'h':
    show_help = true;
    break;
  case 'k':
    arguments->block_verifiers_secret_key = arg;
    break;
  case OPTION_LOG_LEVEL:
    if (atoi(arg) >= 0 && atoi(arg) <= 4)
    {
      log_level = atoi(arg);
    }
    break;
  case OPTION_GENERATE_KEY:
    create_key = true;
    break;
  case OPTION_TOTAL_THREADS:
    arguments->total_threads = atoi(arg);
    break;
  case OPTION_INIT_DB_FROM_SEEDS:
    arguments->init_db_from_seeds = true;
    break;
  case OPTION_INIT_DB_FROM_TOP:
    arguments->init_db_from_top = true;
    break;
  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static struct argp argp = {options, parse_opt, 0, doc, NULL, NULL, NULL};

/*---------------------------------------------------------------------------------------------------------
Name: cleanup_data_structure
Description: Clean up before ending
---------------------------------------------------------------------------------------------------------*/
void cleanup_data_structures(void) {
  pthread_rwlock_destroy(&rwlock);
  pthread_rwlock_destroy(&rwlock_reserve_proofs);
  pthread_mutex_destroy(&lock);
  pthread_mutex_destroy(&database_lock);
  pthread_mutex_destroy(&verify_network_block_lock);
  pthread_mutex_destroy(&vote_lock);
  pthread_mutex_destroy(&add_reserve_proof_lock);
  pthread_mutex_destroy(&invalid_reserve_proof_lock);
  pthread_mutex_destroy(&database_data_IP_address_lock);
  pthread_mutex_destroy(&update_current_block_height_lock);
  pthread_mutex_destroy(&hash_mutex);

  pthread_mutex_destroy(&lock);
  pthread_mutex_destroy(&database_lock);
  pthread_mutex_destroy(&verify_network_block_lock);
  pthread_mutex_destroy(&vote_lock);
  pthread_mutex_destroy(&add_reserve_proof_lock);
  pthread_mutex_destroy(&invalid_reserve_proof_lock);
  pthread_mutex_destroy(&database_data_IP_address_lock);
  pthread_mutex_destroy(&update_current_block_height_lock);
  pthread_rwlock_destroy(&rwlock_reserve_proofs);
  pthread_rwlock_destroy(&rwlock);

  pthread_mutex_destroy(&hash_mutex);

  //free(server_limit_IP_address_list);
  //free(server_limit_public_address_list);

  // free the blockchain_data struct
  // free(blockchain_data.network_version_data);
  // free anything that needs freeing...
  return;
}

/*---------------------------------------------------------------------------------------------------------
Name: sigint_handler
Description: Shuts program down on signal
---------------------------------------------------------------------------------------------------------*/
void sigint_handler(int sig_num) {
  sig_requests++;
  DEBUG_PRINT("Termination signal %d received [%d] times. Shutting down...", sig_num, sig_requests);
  stop_tcp_server();
  INFO_PRINT("Shutting down database engine");
  cleanup_data_structures();
  shutdown_database();
  exit(0);
}

/*---------------------------------------------------------------------------------------------------------
Name: is_ntp_enabled
Description: Checks if ntp is enabled for the server
---------------------------------------------------------------------------------------------------------*/
bool is_ntp_enabled(void) {
  FILE *fp;
  char buffer[256];
  bool ntp_active = false;

  fp = popen("timedatectl status", "r");
  if (fp == NULL) {
      perror("popen failed");
      return false;
  }

  while (fgets(buffer, sizeof(buffer), fp)) {
      if (strstr(buffer, "System clock synchronized: yes") ||
          strstr(buffer, "NTP service: active")) {
          ntp_active = true;
          break;
      }
  }

  pclose(fp);
  return ntp_active;
}

void fix_pipe(int fd) {
  if (fcntl(fd, F_GETFD) != -1 || errno != EBADF) {
    return;
  }

  int f = open("/dev/null", fd == STDIN_FILENO ? O_RDONLY : O_WRONLY);
  if (f == -1) {
    FATAL_ERROR_EXIT("failed to open /dev/null for missing stdio pipe");
    // abort();
  }
  if (f != fd) {
    dup2(f, fd);
    close(f);
  }
}

void fix_std_pipes(void) {
  fix_pipe(STDIN_FILENO);
  fix_pipe(STDOUT_FILENO);
  fix_pipe(STDERR_FILENO);
}

/*---------------------------------------------------------------------------------------------------------
Name: main
Description: The start point of the program
Parameters:
  parameters_count - The parameter count
  parameters - The parameters
Return: 0 if an error has occured, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int main(int argc, char *argv[]) {
  arg_config_t arg_config = {0};
  init_globals();
  setenv("ARGP_HELP_FMT", "rmargin=120", 1);
  if (argc == 1) {
    FATAL_ERROR_EXIT("No arguments entered. Try `xcash-dpops --help'");
  }
  if (argp_parse(&argp, argc, argv, ARGP_NO_EXIT | ARGP_NO_ERRS, 0, &arg_config) != 0) {
    FATAL_ERROR_EXIT("Invalid option entered. Try `xcash-dpops --help'");
  }
  if (show_help) {
    argp_help(&argp, stdout, ARGP_NO_HELP, argv[0]);
    return 0;
  }

  if (create_key) {
    generate_key();
    return 0;
  }

  if (is_ntp_enabled()) {
    INFO_PRINT("NTP Service is Active");
  } else {
    FATAL_ERROR_EXIT("Please enable ntp for your server");
  }

  // uvlib can cause assertion errors if some of STD PIPES closed
  fix_std_pipes();

  if (!(get_node_data())) {
    FATAL_ERROR_EXIT("Failed to get the nodes public wallet address");
  }

  if (!arg_config.block_verifiers_secret_key || strlen(arg_config.block_verifiers_secret_key) != VRF_SECRET_KEY_LENGTH) {
    FATAL_ERROR_EXIT("The --block-verifiers-secret-key is mandatory and should be %d characters long!", VRF_SECRET_KEY_LENGTH);
  }

  strncpy(secret_key, arg_config.block_verifiers_secret_key, sizeof(secret_key) - 1);
  secret_key[sizeof(secret_key) - 1] = '\0';
  if (!(hex_to_byte_array(secret_key, secret_key_data, sizeof(secret_key_data)))) {
    FATAL_ERROR_EXIT("Failed to convert the block-verifiers-secret-key to a byte array: %s", arg_config.block_verifiers_secret_key);
  }

  if (!configure_uv_threadpool(&arg_config)) {
      FATAL_ERROR_EXIT("Failed to set UV_THREADPOOL_SIZE.");
  }

  if (!initialize_database()) {
    FATAL_ERROR_EXIT("Can't open mongo database");
  }

  signal(SIGINT, sigint_handler);

  if (!start_tcp_server(XCASH_DPOPS_PORT)) {
    shutdown_database();
    FATAL_ERROR_EXIT("Failed to start TCP server.");
  }

  if (init_processing(&arg_config)) {;
    print_starter_state(&arg_config);
    start_block_production();
  } else {
    FATAL_ERROR("Failed server initialization."); 
  }

  stop_tcp_server();
  INFO_PRINT("Database........");
  usleep(5000000);  // 5 second
  if (database_client_thread_pool != NULL) {
    INFO_PRINT("Database........");
    shutdown_database();
  }
  INFO_PRINT("Database closed");
  cleanup_data_structures();
  return 0;
}