#include "xcash_dpops.h"

// set globals defined in globals.h
int log_level = 0;
bool is_seed_node = false;  
int network_data_nodes_amount = 0;
delegates_t delegates_all[BLOCK_VERIFIERS_TOTAL_AMOUNT] = {0};
char xcash_wallet_public_address[XCASH_PUBLIC_ADDR_LENGTH+1] = {0};
char current_block_height[BUFFER_SIZE_NETWORK_BLOCK_DATA] = {0};
char previous_block_hash[BLOCK_HASH_LENGTH+1] = {0};
unsigned char secret_key_data[crypto_vrf_SECRETKEYBYTES+1] = {0};
char secret_key[VRF_SECRET_KEY_LENGTH+1] = {0};
const NetworkNode network_nodes[] = {
  {"XCA1dd7JaWhiuBavUM2ZTJG3GdgPkT1Yd5Q6VvNvnxbEfb6JhUhziTF6w5mMPVeoSv3aa1zGyhedpaa2QQtGEjBo7N6av9nhaU", "xcashseeds.us", 1},
  {"XCA1b6Sg5QVBX4jrctQ9SVUcHFqpaGST6bqtFpyoQadTX8SaDs92xR8iec3VfaXKzhYijFiMfwoM4TuYRgy6NXzn5titJnWbra", "xcashseeds.uk", 1},
  // Sentinel value (empty entry to mark the end)
  {NULL, NULL, NULL}};
mongoc_client_pool_t* database_client_thread_pool = NULL;

// local
static bool show_help = false;
static bool create_key = false;
static int total_threads = 4;
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
"  -l, --log-level                         The log-level displays log messages based on the level passed:\n"
"                                          Critial - 0, Info - 3, Warming - 2, Error - 1, Debug - 4\n"
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
  {"log-level", 'l', "LOG_LEVEL", 0, "Displays log messages based on the level passed.", 0},
  {"total-threads", OPTION_TOTAL_THREADS, "THREADS", 0, "Set total threads (Default: 4).", 0},
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
  case 'l':
    if (atoi(arg) >= 0 && atoi(arg) <= 4)
    {
      log_level = atoi(arg);
    }
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
void init_processing(const arg_config_t *arg_config)
{

  network_data_nodes_amount = get_seed_node_count();
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

/*---------------------------------------------------------------------------------------------------------
Name: sigint_handler
Description: Shuts program down on signal
---------------------------------------------------------------------------------------------------------*/
void sigint_handler(int sig_num) {
  sig_requests++;
  DEBUG_PRINT("Termination signal %d received [%d] times. Shutting down...", sig_num, sig_requests);
  stop_tcp_server();
  INFO_PRINT("Shutting down database engine");
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

  if (create_key)
  {
    generate_key();
    return 0;
  }

  if (!(get_node_data()))
  {
    FATAL_ERROR_EXIT("Failed to get the nodes public wallet address");
  }

  if (!arg_config.block_verifiers_secret_key || strlen(arg_config.block_verifiers_secret_key) != VRF_SECRET_KEY_LENGTH)
  {
    FATAL_ERROR_EXIT("The --block-verifiers-secret-key is mandatory and should be %d characters long!", VRF_SECRET_KEY_LENGTH);
  }

  strncpy(secret_key, arg_config.block_verifiers_secret_key, sizeof(secret_key) - 1);
  secret_key[sizeof(secret_key) - 1] = '\0';
  if (!(hex_to_byte_array(secret_key, secret_key_data, sizeof(secret_key_data))))
  {
    FATAL_ERROR_EXIT("Failed to convert the block-verifiers-secret-key to a byte array: %s", arg_config.block_verifiers_secret_key);
  }

  if (!(configure_uv_threadpool()))
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
  INFO_PRINT("Database closed");
  return 0;
}