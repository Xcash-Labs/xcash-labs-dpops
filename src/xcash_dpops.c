#include "xcash_dpops.h"

// set globals defined in globals.h
int sig_requests = 0;
bool is_seed_node = false;
bool debug_enabled = false;

// Globals
char XCASH_daemon_IP_address[IP_LENGTH + 1] = {0};
char XCASH_wallet_IP_address[IP_LENGTH+1] = {0};
char xcash_wallet_public_address[XCASH_PUBLIC_ADDR_LENGTH + 1] = {0};
char current_block_height[BUFFER_SIZE_NETWORK_BLOCK_DATA] = {0};
mongoc_client_pool_t* database_client_thread_pool = NULL;

// local
char XCASH_DPOPS_delegates_IP_address[IP_LENGTH+1] = {0};
static bool show_help = false;
static bool create_key = false;
static int total_threads = 4;

static char doc[] =
"\n"
BRIGHT_WHITE_TEXT("General Options:\n")
"Program Bug Address: https://github.com/Xcash-Labs/xcash-labs-dpops/issues\n"
"\n"
"  -h, --help                              List all valid parameters.\n"
"  -k, --block-verifiers-secret-key <KEY>  Set the block verifier's secret key\n"
"\n"
BRIGHT_WHITE_TEXT("Debug Options:\n")
"  --debug                                 Display verbose log messages.\n"
"\n"
BRIGHT_WHITE_TEXT("Advanced Options:\n")
"  --total-threads THREADS                 Sets the UV_THREADPOOL_SIZE environment variable that controls the.\n"
"                                          number of worker threads in the libuv thread pool (default is 4).\n"
"\n"
"  --generate-key                          Generate public/private key for block verifiers.\n"
"\n"
"For more details on each option, refer to the documentation or use the --help option.\n";

static struct argp_option options[] = {
  {"help", 'h', 0, 0, "List all valid parameters.", 0},
  {"block-verifiers-secret-key", 'k', "SECRET_KEY", 0, "Set the block verifier's secret key", 0},
  {"debug", OPTION_DEBUG, 0, 0, "Display debug and informational messages.", 0},
  {"total-threads", OPTION_TOTAL_THREADS, "THREADS", 0, "Set total threads (Default: 4).", 0},
  {"generate-key", OPTION_GENERATE_KEY, 0, 0, "Generate public/private key for block verifiers.", 0},
  {0}
};

const NetworkNode network_nodes[] = {
  {"XCA1dd7JaWhiuBavUM2ZTJG3GdgPkT1Yd5Q6VvNvnxbEfb6JhUhziTF6w5mMPVeoSv3aa1zGyhedpaa2QQtGEjBo7N6av9nhaU",
   "xcashseeds.us",
   "xcashseeds_us",
   "f681a933620c8e9e029d9ac0977d3a2f1d6a64cc49304e079458e3b5d2d4a66f"},
  {"XCA1b6Sg5QVBX4jrctQ9SVUcHFqpaGST6bqtFpyoQadTX8SaDs92xR8iec3VfaXKzhYijFiMfwoM4TuYRgy6NXzn5titJnWbra",
   "xcashseeds.uk",
   "xcashseeds_uk",
   "63232aa1b020a772945bf50ce96db9a04242583118b5a43952f0aaf9ecf7cfbb"},
  // Sentinel value (empty entry to mark the end)
  {NULL, NULL, NULL, NULL}};

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
  case OPTION_DEBUG:
    debug_enabled = true;
    break;
  case OPTION_GENERATE_KEY:
    create_key = true;
    break;
  case OPTION_TOTAL_THREADS:
    total_threads = atoi(arg);
    break;
  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static struct argp argp = {options, parse_opt, 0, doc, NULL, NULL, NULL};

/*---------------------------------------------------------------------------------------------------------
Name: configure_uv_threadpool
Description: Sets the UV_THREADPOOL_SIZE environment variable. Default is the system default of 4 and can
not be greater that the number of cpus for the server.
---------------------------------------------------------------------------------------------------------*/
bool configure_uv_threadpool(void)
{
  if (total_threads != 4)
  {
    int wsthreads = get_nprocs();
    if (wsthreads < 1)
    {
      ERROR_PRINT("Failed to get CPU core count. Defaulting to 4 threads.");
      return XCASH_OK;
    }
    if (total_threads > wsthreads)
    {
      total_threads = wsthreads;
    }

    char threadpool_size[10];
    snprintf(threadpool_size, sizeof(threadpool_size), "%d", total_threads);
    if (setenv("UV_THREADPOOL_SIZE", threadpool_size, 1) != 0)
    {
      ERROR_PRINT("Failed to set UV_THREADPOOL_SIZE");
      return XCASH_ERROR;
    }
    else
    {
      DEBUG_PRINT("UV_THREADPOOL_SIZE set to %s", threadpool_size);
    }
  }
  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: init_processing
Description: Initialize globals and print program start header.
---------------------------------------------------------------------------------------------------------*/
bool init_processing(const arg_config_t *arg_config)
{
  snprintf(XCASH_daemon_IP_address, sizeof(XCASH_daemon_IP_address), "%s", "127.0.0.1");
  snprintf(XCASH_DPOPS_delegates_IP_address, sizeof(XCASH_DPOPS_delegates_IP_address), "%s", "127.0.0.1");
  snprintf(XCASH_wallet_IP_address, sizeof(XCASH_wallet_IP_address), "%s", "127.0.0.1");

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
  "Total threads:\t\%d\n"
  
  INFO_PRINT(xcash_tech_status_fmt,
    XCASH_DPOPS_CURRENT_VERSION, "~Lazarus",
    arg_config->block_verifiers_secret_key,
    is_seed_node ? "SEED NODE" : "DELEGATE NODE",
    XCASH_daemon_IP_address, XCASH_DAEMON_PORT,
    XCASH_DPOPS_delegates_IP_address, XCASH_DPOPS_PORT,
    XCASH_wallet_IP_address, XCASH_WALLET_PORT,
    DATABASE_CONNECTION, total_threads);
  if (debug_enabled)
  {
    fprintf(stderr, "\n");
    DEBUG_PRINT("Debug is enabled.");
  }
  return 0;
}

/*---------------------------------------------------------------------------------------------------------
Name: sigint_handler
Description: Shuts program down on signal
---------------------------------------------------------------------------------------------------------*/
void sigint_handler(int sig_num) {
  sig_requests++;
  DEBUG_PRINT("Termination signal %d received [%d] times. Shutting down...", sig_num, sig_requests);
  stop_tcp_server();
  DEBUG_PRINT("Shutting down database engine");
  // cleanup_data_structures();                     add this later......
  shutdown_database();
  exit(0);
}

/*---------------------------------------------------------------------------------------------------------
Name: main
Description: The start point of the program
Parameters:
  parameters_count - The parameter count
  parameters - The parameters
Return: 0 if an error has occured, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
  arg_config_t arg_config = {0};
  setenv("ARGP_HELP_FMT", "rmargin=120", 1);
  if (argc == 1)
  {
    FATAL_ERROR_EXIT("No arguments entered. Try `xcash-dpops --help'");
  }
  if (argp_parse(&argp, argc, argv, ARGP_NO_EXIT | ARGP_NO_ERRS, 0, &arg_config) != 0)
  {
    FATAL_ERROR_EXIT("Invalid option entered. Try `xcash-dpops --help'");
  }
  if (show_help)
  {
    argp_help(&argp, stdout, ARGP_NO_HELP, argv[0]);
    return 0;
  }

  if (create_key) {
    generate_key();
    return 0;
  }

  if (!arg_config.block_verifiers_secret_key || (strlen(arg_config.block_verifiers_secret_key) != VRF_SECRET_KEY_LENGTH))
  {
    FATAL_ERROR_EXIT("The --block-verifiers-secret-key is mandatory and should be %d characters long!", VRF_SECRET_KEY_LENGTH);
  }
  
  if(!(configure_uv_threadpool()))
  {
    FATAL_ERROR_EXIT("Can't configure uv_threadpool");
  }

  init_processing(&arg_config);

  if (initialize_database())
  {
    INFO_PRINT("Database opened successfully");
  } else {
    FATAL_ERROR_EXIT("Can't open mongo database");
  }

  // not sure about this - relook at
  if(!(initialize_network_nodes()))
  {
    shutdown_database();
    FATAL_ERROR_EXIT("Can't add seed nodes to mongo database");
  }

  signal(SIGINT, sigint_handler);

  if (start_tcp_server(XCASH_DPOPS_PORT))
  {
    start_block_production();
  }
  else
  {
    FATAL_ERROR_EXIT("Failed to start TCP server.");
  }

  stop_tcp_server();
  shutdown_database();
  INFO_PRINT("Database closed...");
  return 0;
}