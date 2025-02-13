#include <string.h>
#include <stdbool.h>

#include "xcash_dpops.h"
#include "define_macros.h"
#include "common_utils.h"

// set global variables defined in define_macros.h
bool debug_enabled = false;

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

  // write the message
  color_print(XCASH_DPOPS_CURRENT_VERSION, "green");

  // check if they want to display the parameters
  if (parameters_count == 2 &&
      (strncmp(parameters[1], "--help", BUFFER_SIZE) == 0 ||
       strncmp(parameters[1], "--h", BUFFER_SIZE) == 0))
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
    if (strcmp(parameters[i], "--block-verifiers-secret-key") == 0)
    {
      if (i + 1 >= parameters_count || strlen(parameters[i + 1]) != VRF_SECRET_KEY_LENGTH)
      {
        HANDLE_ERROR("Invalid block verifier secret key length.");
      }
      found_secret_key = true;
    }
    else if (strcmp(parameters[i], "--debug") == 0)
    {
      debug_enabled = true;
      HANDLE_DEBUG("Debug mode enabled.");
    }
  }

  if (!found_secret_key)
  {
    HANDLE_ERROR("Iinvalid --block-verifiers-secret-key parameter.");
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
  static const char xcash_tech_header[] =
      "\n"
      " /$$   /$$                           /$$        / $$              / $$                   \n"
      "| $$  / $$                          | $$        | $$              | $$                    \n"
      "|  $$/ $$/ /$$$$$$$ /$$$$$$  /$$$$$$| $$$$$$$   | $$      /$$$$$$ | $$       /$$$$$$      \n"
      " \\  $$$$/ /$$_____/|____  $$/$$_____| $$__  $$  | $$     |____  $$| $$      /$$_____     \n"
      "  /$$  $$| $$       /$$$$$$|  $$$$$$| $$  \\ $$  | $$      /$$$$$$ | $$$$$$$ | $$$$$$     \n"
      " /$$/\\  $| $$      /$$__  $$\\____  $| $$  | $$  | $$     /$$__  $$| $$   $$ \\____  $$   \n"
      "| $$  \\ $|  $$$$$$|  $$$$$$$/$$$$$$$| $$  | $$/ | $$$$$$$| $$$$$$$| $$$$$$$ |$$$$$$$      \n"
      "|__/  |__/\\_______/\\_______|_______/|__/  |__|__|________/\\_______/\\________/\\______/ \n"
      "\n";
  fputs(xcash_tech_header, stderr);
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

  int result = set_parameters(argc, argv);
  print_settings;

  return result;
}