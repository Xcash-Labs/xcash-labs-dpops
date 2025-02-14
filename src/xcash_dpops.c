#include <argp.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "arg_config.h"
#include "xcash_dpops.h"
#include "dpops_config.h"
#include "common_utils.h"

const char *argp_program_version = "Xcash Labs DPoPS v. 2.0.0";
const char *argp_program_bug_address = "https://github.com/Xcash-Labs/xcash-labs-dpops/issues";

static char doc[] = "Usage: xcash-dpops [OPTIONS]\n"
"\n"
BRIGHT_WHITE_TEXT("General Options:\n")
"  -h, --help                              List all valid parameters.\n"
"  -k, --block-verifiers-secret-key <KEY>  Set the block verifier's secret key\n"
"\n"
BRIGHT_WHITE_TEXT("Debug Options:\n")
"  -d, --debug                             Display all server messages.\n"
"\n"
"For more details on each option, refer to the documentation or use the --help option."
;

static struct argp_option options[] = {
  {"help", 'h', 0, 0, "List all valid parameters.", 0},
  {"block-verifiers-secret-key", 'k', "SECRET_KEY", 0, "Set the block verifier's secret key", 0},
  {"debug", OPTION_DEBUG, 0, 0, "Display debug and informational messages.", 0},
  {0}};

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
arg_config_t *arguments = state->input;
switch (key)
{
case 'k':
  arguments->block_verifiers_secret_key = arg;
  break;
case OPTION_DEBUG:
  arguments->debug_mode = true;
  break;
default:
  return ARGP_ERR_UNKNOWN;
}
return 0;
}



// set global variables defined in define_macros.h
bool debug_enabled = false;
bool is_seed_node = false;

char xcash_wallet_public_address[XCASH_WALLET_LENGTH + 1];
char XCASH_daemon_IP_address[IP_LENGTH + 1];
char XCASH_DPOPS_delegates_IP_address[IP_LENGTH + 1];
char XCASH_wallet_IP_address[IP_LENGTH + 1];



static struct argp argp = {options, parse_opt, 0, doc, NULL, NULL, NULL};

/*---------------------------------------------------------------------------------------------------------
Name: init_parameters
Description: Initialize globals and print program start header.
-----------------------------------------------------------------------------------------------------------
*/
void init_parameters(void)
{
  strcpy(XCASH_daemon_IP_address, "127.0.0.1");
  strcpy(XCASH_DPOPS_delegates_IP_address, "127.0.0.1");
  strcpy(XCASH_wallet_IP_address, "127.0.0.1");
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
}

/*
-----------------------------------------------------------------------------------------------------------
Name: set_parameters
Description: Sets the parameters
Parameters:
  parameters_count - The parameter count
  parameters - The parameters
Return: 0 if an error has occured, 1 if successfull, 2 to disable the timers
-----------------------------------------------------------------------------------------------------------
*/
int set_parameters(int parameters_count, char *parameters[])
{
  // check if they want to display the parameters
  if (parameters_count == 2 &&
      (strncmp(parameters[1], "--help", strlen("--help")) == 0 ||
       strncmp(parameters[1], "--h", strlen("--h")) == 0))
  {
    printf(INVALID_PARAMETERS_ERROR_MESSAGE);
    exit(0);
  }

  if (parameters_count < 3)
  {
    HANDLE_ERROR("Missing block verifier secret key. Please include the --block-verifiers-secret-key parameter.");
  }

  bool found_secret_key = false;

  for (int i = 0; i < parameters_count; i++)
  {
    if (strcmp(parameters[i], "--block-verifiers-secret-key"), strlen("--block-verifiers-secret-key") == 0)
    {
      if (i + 1 >= parameters_count || strlen(parameters[i + 1]) != VRF_SECRET_KEY_LENGTH)
      {
        HANDLE_ERROR("Invalid block verifier secret key length.");
      }
      found_secret_key = true;
    }
    else if (strcmp(parameters[i], "--debug", strlen("--debug")) == 0)
    {
      debug_enabled = true;
      HANDLE_DEBUG("Debug mode enabled.");
    }
  }

  if (!found_secret_key)
  {
    HANDLE_ERROR("Invalid --block-verifiers-secret-key parameter.");
  }

  return 1;
}

/*
-----------------------------------------------------------------------------------------------------------
Name: print_settings
Description: Prints the delegates settings
-----------------------------------------------------------------------------------------------------------
*/
void print_settings(void)
{
#define xcash_tech_status_fmt "%s (%s)\n\n"        \
                              "Address:\t%s\n"     \
                              "\n"                 \
                              "Node Type:\t%s\n"   \
                              "\n"                 \
                              "Services:\n"        \
                              "Daemon:\t\t%s:%d\n" \
                              "DPoPS:\t\t%s:%d\n"  \
                              "Wallet:\t\t%s:%d\n" \
                              "MongoDB:\t%s\n"

  fprintf(stderr, xcash_tech_status_fmt,
          XCASH_DPOPS_CURRENT_VERSION, "~Lazarus",
          xcash_wallet_public_address,
          is_seed_node ? "SEED NODE" : "DELEGATE NODE",
          XCASH_daemon_IP_address, XCASH_DAEMON_PORT,
          XCASH_DPOPS_delegates_IP_address, XCASH_DPOPS_PORT,
          XCASH_wallet_IP_address, XCASH_WALLET_PORT,
          DATABASE_CONNECTION);
}
/*
-----------------------------------------------------------------------------------------------------------
Name: main
Description: The start point of the program
Parameters:
  parameters_count - The parameter count
  parameters - The parameters
Return: 0 if an error has occured, 1 if successfull
-----------------------------------------------------------------------------------------------------------
*/
int main(int argc, char *argv[])
{
  arg_config_t arg_config = {0};
   
  setenv("ARGP_HELP_FMT", "rmargin=120", 1);

  if (argc == 1) {
      argp_help(&argp, stdout, ARGP_HELP_STD_HELP, argv[0]);
      return 0;
  }

  if (argp_parse(&argp, argc, argv, 0, 0, &arg_config) != 0) {
      argp_help(&argp, stdout, ARGP_HELP_STD_HELP, argv[0]);
      return 1;
  }

//  init_parameters();
//  int result = set_parameters(argc, argv);
//  print_settings();
  return 1;
}