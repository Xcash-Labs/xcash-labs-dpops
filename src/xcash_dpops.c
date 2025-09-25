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
"  --delegates-website                  Run the delegate's website.\n"
"  --shared-delegates-website           Run shared delegate's website with specified fee and minimum amount.\n"
"\n"
BRIGHT_WHITE_TEXT("Delegate Options:\n")
"  --fee  <reward>                         The fee reward to running delegate (0..100).\n"
"  --minimum-amount <minimum-amount>       The minimum amount of payouts to voters.\n"
"\n"
BRIGHT_WHITE_TEXT("Advanced Options:\n")
"  --generate-key                          Generate public/private key for block verifiers.\n"
"\n"
"For more details on each option, refer to the documentation or use the --help option.\n";

static struct argp_option options[] = {
  {"help", 'h', 0, 0, "List all valid parameters.", 0},
  {"block-verifiers-secret-key", 'k', "SECRET_KEY", 0, "Set the block verifier's secret key", 0},
  {"log-level", OPTION_LOG_LEVEL, "LOG_LEVEL", 0, "Displays log messages based on the level passed.", 0},
  {"delegates-website", OPTION_DELEGATES_WEBSITE, 0, 0, "Run the delegate's website.", 0},
  {"shared-delegates-website", OPTION_SHARED_DELEGATES_WEBSITE, 0, 0, "Run shared delegate's website with specified fee and minimum amount.", 0},
  {"fee", OPTION_FEE, "FEE", 0, "The fee reward to running delegate (in percents 0..100).", 0},
  {"minimum-amount", OPTION_MINIMUM_AMOUNT, "MINIMUM_PAYOUT", 0, "The minimum amount of payouts to voters.", 0},
  {"generate-key", OPTION_GENERATE_KEY, 0, 0, "Generate public/private key for block verifiers.", 0},
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
  case OPTION_FEE:
    arguments->fee = atof(arg);
    break;
  case OPTION_MINIMUM_AMOUNT:
    arguments->minimum_amount = strtoull(arg, NULL, 10);
    break;
  case OPTION_GENERATE_KEY:
    create_key = true;
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
//  signal(SIGINT, sigint_handler);
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
    FATAL_ERROR_EXIT("Failed to start TCP server.");
  }

  if (!initialize_database()) {
    stop_tcp_server();
    FATAL_ERROR_EXIT("Can't open mongo database");
  }

  if (!(init_processing(&arg_config))) {
    FATAL_ERROR_EXIT("Failed server initialization.");
  }

  if (get_node_data()) {
    print_starter_state(&arg_config);
    start_block_production();
    fprintf(stderr, "Daemon is shutting down...\n");
  } else {
    FATAL_ERROR_EXIT("Failed to get the nodes public wallet address"); 
  }

  shutdown_db();
  INFO_PRINT("Database shutdown successfully");
  stop_tcp_server();
  cleanup_data_structures();
  return 0;
}