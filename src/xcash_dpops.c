#include "xcash_dpops.h"

static bool show_help = false;
static bool create_key = false;
static volatile sig_atomic_t sig_requests = 0;

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
"                                          Critial - 0, Error - 1, Warning - 2, Info - 3, Debug - 4\n"
"\n"
BRIGHT_WHITE_TEXT("Website Options: (deprecated)\n")
"  --delegates-website                    Run the delegate's website.\n"
"  --shared-delegates-website             Run shared delegate's website with specified minimum amount.\n"
"\n"
BRIGHT_WHITE_TEXT("Delegate Options:\n")
"  --minimum-amount <minimum-amount>      The minimum amount of payouts to voters.\n"
"\n"
BRIGHT_WHITE_TEXT("Advanced Options:\n")
"  --generate-key                         Generate public/private key for block verifiers.\n"
"  --quorum-bootstrap                     Ensures quorum before checking sync status, only used to start things rolling when first starting chain.\n"
"\n"
"For more details on each option, refer to the documentation or use the --help option.\n";

static struct argp_option options[] = {
  {"help", 'h', 0, 0, "List all valid parameters.", 0},
  {"block-verifiers-secret-key", 'k', "SECRET_KEY", 0, "Set the block verifier's secret key", 0},
  {"log-level", OPTION_LOG_LEVEL, "LOG_LEVEL", 0, "Displays log messages based on the level passed.", 0},
  {"delegates-website", OPTION_DELEGATES_WEBSITE, 0, 0, "Run the delegate's website.", 0},
  {"shared-delegates-website", OPTION_SHARED_DELEGATES_WEBSITE, 0, 0, "Run shared delegate's website with specified minimum amount.", 0},
  {"minimum-amount", OPTION_MINIMUM_AMOUNT, "MINIMUM_PAYOUT", 0, "The minimum amount of payouts to voters.", 0},
  {"generate-key", OPTION_GENERATE_KEY, 0, 0, "Generate public/private key for block verifiers.", 0},
  {"quorum-bootstrap", OPTION_QUORUM_BOOTSTRAP, 0, 0, "Ensures quorum before checking sync status, only used to start things rolling when first starting chain.", 0},
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
  case OPTION_DELEGATES_WEBSITE:
    arguments->delegates_website = true;
    break;
  case OPTION_SHARED_DELEGATES_WEBSITE:
    arguments->shared_delegates_website = true;
    break;
  case OPTION_MINIMUM_AMOUNT:
    arguments->minimum_amount = strtoull(arg, NULL, 10);
    break;
  case OPTION_GENERATE_KEY:
    create_key = true;
    break;
  case OPTION_QUORUM_BOOTSTRAP:
    quorum_bootstrap = true;
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

  // Free heap buffers allocated in init_globals().
  if (server_limit_IP_address_list) {
    free(server_limit_IP_address_list);
    server_limit_IP_address_list = NULL;
  }
  if (server_limit_public_address_list) {
    free(server_limit_public_address_list);
    server_limit_public_address_list = NULL;
  }

  // Wipe sensitive material (best-effort).
  memset(secret_key_data, 0, sizeof(secret_key_data));
  memset(secret_key, 0, sizeof(secret_key));
  // Public, but clearing is fine:
  memset(vrf_public_key, 0, sizeof(vrf_public_key));
  memset(sync_token, 0, sizeof(sync_token));

  // Clear large globals (optional hygiene to reduce RSS before exit).
  memset(delegates_all, 0, sizeof(delegates_all));
  memset(delegates_timer_all, 0, sizeof(delegates_timer_all));
  memset(&current_block_verifiers_list, 0, sizeof(current_block_verifiers_list));
  memset(xcash_wallet_public_address, 0, sizeof(xcash_wallet_public_address));
  memset(current_block_height, 0, sizeof(current_block_height));
  memset(previous_block_hash, 0, sizeof(previous_block_hash));
  memset(delegates_hash, 0, sizeof(delegates_hash));

  // Destroy mutexes
  pthread_mutex_destroy(&delegates_all_lock);
  pthread_mutex_destroy(&current_block_verifiers_lock);
  pthread_mutex_destroy(&producer_refs_lock);
  pthread_mutex_destroy(&database_data_IP_address_lock);

  return;
}

/*---------------------------------------------------------------------------------------------------------
Name: sigint_handler
Description: Shuts program down on signal
---------------------------------------------------------------------------------------------------------*/
void sigint_handler(int sig_num) {
 (void)sig_num;
  sig_requests++;
  if (sig_requests == 1) {
    static const char msg[] = "\nShutdown request received. Finishing current round, please wait...\n";
    if (write(STDERR_FILENO, msg, sizeof(msg) - 1) < 0) { /* ignore */ }
  }
  atomic_store(&shutdown_requested, true);
  if (sig_requests >= 2) {
    _exit(0);
  }
}

void install_signal_handlers(void) {
  struct sigaction sa = {0};
  sa.sa_handler = sigint_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  sigaction(SIGINT,  &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
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
  install_signal_handlers();
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

  if (!arg_config.block_verifiers_secret_key || strlen(arg_config.block_verifiers_secret_key) != VRF_SECRET_KEY_LENGTH) {
    FATAL_ERROR_EXIT("The --block-verifiers-secret-key is mandatory and should be %d characters long!", VRF_SECRET_KEY_LENGTH);
  }

  strncpy(secret_key, arg_config.block_verifiers_secret_key, sizeof(secret_key) - 1);
  secret_key[sizeof(secret_key) - 1] = '\0';
  if (!(hex_to_byte_array(secret_key, secret_key_data, sizeof(secret_key_data)))) {
    FATAL_ERROR_EXIT("Failed to convert the block-verifiers-secret-key to a byte array: %s", arg_config.block_verifiers_secret_key);
  }

  if (!start_tcp_server(XCASH_DPOPS_PORT)) {
    FATAL_ERROR_EXIT("Failed to start TCP server");
  }

  if (!initialize_database()) {
    stop_tcp_server();
    FATAL_ERROR_EXIT("Can't open mongo database");
  }

  g_ctx = dnssec_init();

  if (!(init_processing(&arg_config))) {
    FATAL_ERROR_EXIT("Failed server initialization");
  }

// start the daily scheduler on seeds (ONE thread)
  pthread_t timer_tid = 0;
  bool sched_started = false;
  // scheduler needs the pool; initialize_database() has created database_client_thread_pool
  sched_ctx_t* sched_ctx = NULL;
  sched_ctx = malloc(sizeof *sched_ctx);
  if (is_seed_node) {
    {
      if (!sched_ctx) {
        FATAL_ERROR_EXIT("Scheduler: malloc failed; can not continue without scheduled jobs");
      } else {
        sched_ctx->pool = database_client_thread_pool;
        if (pthread_create(&timer_tid, NULL, timer_thread, sched_ctx) != 0) {
          FATAL_ERROR_EXIT("Scheduler: pthread_create failed; can not continue without scheduled jobs");
          free(sched_ctx);
          sched_ctx = NULL;
        } else {
          sched_started = true;
          INFO_PRINT("Scheduler thread started");
        }
      }
    }
  } else {
    if (arg_config.minimum_amount == 0) {
      WARNING_PRINT("Unable to read minimum payout parameter so using default");
    } else {
      minimum_payout = arg_config.minimum_amount;
    }
  }

  if (get_node_data()) {
    if (!is_seed_node) {
      if (get_delegate_fee(&delegate_fee_percent) == XCASH_ERROR) {
        WARNING_PRINT("Unable to read fee from database so using default");
      }
    }
    if (print_starter_state(&arg_config)) {
      start_block_production();
    }
    fprintf(stderr, "Daemon is shutting down...\n");
  } else {
    ERROR_PRINT("Failed to get the nodes public wallet address, shutting down...");
  }

  // Signal scheduler to stop and join it
  if (sched_started) {
    pthread_join(timer_tid, NULL);
    free(sched_ctx);
  }

  if (g_ctx) {
    dnssec_destroy(g_ctx);
    g_ctx = NULL;
  }

  shutdown_db();
  INFO_PRINT("Database shutdown successfully");
  stop_tcp_server();
  cleanup_data_structures();
  return 0;
}