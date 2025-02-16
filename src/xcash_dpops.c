#include <argp.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/sysinfo.h>

#include "arg_config.h"
#include "xcash_dpops.h"
#include "dpops_config.h"
#include "common_utils.h"

const char *argp_program_bug_address = "https://github.com/Xcash-Labs/xcash-labs-dpops/issues";

char *block_verifiers_secret_key;
bool generate_key;
bool debug_mode;
int total_threads;


static char doc[] =
"\n"
BRIGHT_WHITE_TEXT("General Options:\n")
"  -h, --help                              List all valid parameters.\n"
"  -k, --block-verifiers-secret-key <KEY>  Set the block verifier's secret key\n"
"\n"
BRIGHT_WHITE_TEXT("Debug Options:\n")
"  -d, --debug                             Display all server messages.\n"
"\n"
BRIGHT_WHITE_TEXT("Advanced Options:\n")
"  --total-threads THREADS                 Set total threads (Default: CPU total threads).\n"
"  --generate-key                       Generate public/private key for block verifiers.\n"
"\n"
"For more details on each option, refer to the documentation or use the --help option.";

static struct argp_option options[] = {
  {"help", 'h', 0, 0, "List all valid parameters.", 0},
  {"block-verifiers-secret-key", 'k', "SECRET_KEY", 0, "Set the block verifier's secret key", 0},
  {"debug", OPTION_DEBUG, 0, 0, "Display debug and informational messages.", 0},
  {"total-threads", OPTION_TOTAL_THREADS, "THREADS", 0, "Set total threads (Default: CPU total threads).", 0},
  {"generate-key", OPTION_GENERATE_KEY, 0, 0, "Generate public/private key for block verifiers.", 0},
  {0}
};

bool show_help = false;

// set global variables defined in define_macros.h
bool debug_enabled = false;
bool is_seed_node = false;
char xcash_wallet_public_address[XCASH_WALLET_LENGTH + 1];
char XCASH_daemon_IP_address[IP_LENGTH + 1];
char XCASH_DPOPS_delegates_IP_address[IP_LENGTH + 1];
char XCASH_wallet_IP_address[IP_LENGTH + 1];

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
    arguments->debug_mode = true;
    break;
  case OPTION_GENERATE_KEY:
    arguments->generate_key = true;
    break;
  case OPTION_TOTAL_THREADS:
    arguments->total_threads = atoi(arg);
    break;
  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static struct argp argp = {options, parse_opt, 0, doc, NULL, NULL, NULL};

/*---------------------------------------------------------------------------------------------------------
Name: init_processing
Description: Initialize globals and print program start header.
---------------------------------------------------------------------------------------------------------*/
bool init_processing(const arg_config_t *arg_config)
{
  debug_enabled = arg_config->debug_mode;
  snprintf(xcash_wallet_public_address, sizeof(xcash_wallet_public_address), "%s", arg_config->block_verifiers_secret_key);
  snprintf(XCASH_daemon_IP_address, sizeof(XCASH_daemon_IP_address), "%s", "127.0.0.1");
  snprintf(XCASH_DPOPS_delegates_IP_address, sizeof(XCASH_DPOPS_delegates_IP_address), "%s", "127.0.0.1");
  snprintf(XCASH_wallet_IP_address, sizeof(XCASH_wallet_IP_address), "%s", "127.0.0.1");
  total_threads = (arg_config->total_threads == 0) ? get_nprocs() : arg_config->total_threads;

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
  "MongoDB:\t%s\n"
  
  fprintf(stderr, xcash_tech_status_fmt,
          XCASH_DPOPS_CURRENT_VERSION, "~Lazarus",
          xcash_wallet_public_address,
          is_seed_node ? "SEED NODE" : "DELEGATE NODE",
          XCASH_daemon_IP_address, XCASH_DAEMON_PORT,
          XCASH_DPOPS_delegates_IP_address, XCASH_DPOPS_PORT,
          XCASH_wallet_IP_address, XCASH_WALLET_PORT,
          DATABASE_CONNECTION);
  return 0;
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
    HANDLE_ERROR("No arguments entered. Try `xcash-dpops --help'");
  }
  if (argp_parse(&argp, argc, argv, ARGP_NO_EXIT, 0, &arg_config) != 0)
  {
    HANDLE_ERROR("Invalid option entered. Try `xcash-dpops --help'");
  }
  if (show_help)
  {
    argp_help(&argp, stdout, ARGP_NO_HELP, argv[0]);
    return 0;
  }
  //  if (arg_config.generate_key) {
  //      generate_key();                    add later
  //      return 0;
  //  }
  if (!arg_config.block_verifiers_secret_key || strlen(arg_config.block_verifiers_secret_key) == 0)
  {
    HANDLE_ERROR("--block-verifiers-secret-key is mandatory!");
  }

  if (init_processing(&arg_config))
  {
    //    start_block_production();
  }

  // uvlib can cause assertion errors if some of STD PIPES closed
  //  fix_std_pipes();

  //  if (!initialize_database(arg_config.mongodb_uri)){
  //      ERROR_PRINT("Can't initialize mongo database");
  //      return 1;
  //  }

  //  signal(SIGINT, sigint_handler);

  //  if (processing(&arg_config)) {
  //      start_block_production();
  //  }

  //  shutdown_database();
  //  if (server_log_fp)
  //      fclose(server_log_fp);

  return 0;
}