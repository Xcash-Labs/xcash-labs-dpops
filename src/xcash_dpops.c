#include <string.h>
#include <stdbool.h>

#include "xcash_dpops.h"
#include "define_macros.h"
#include "common_utils.h"

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
int set_parameters(int parameters_count, char *parameters[]) {
  if (parameters_count < 3) {
      HANDLE_ERROR("Missing block verifier secret key. Please include the --block-verifiers-secret-key parameter.");
  }

  bool found_secret_key = false;

  for (int i = 0; i < parameters_count; i++) {
      if (strcmp(parameters[i], "--block-verifiers-secret-key") == 0) {
          if (i + 1 >= parameters_count || strlen(parameters[i + 1]) != VRF_SECRET_KEY_LENGTH) {
              HANDLE_ERROR("Invalid block verifier secret key length.");
          }
          found_secret_key = true;
      } 
      else if (strcmp(parameters[i], "--debug") == 0) {
          debug_settings = true;
          log_message(LOG_INFO, __func__, "Debug mode enabled.");
      }
  }

  if (!found_secret_key) {
      HANDLE_ERROR("Missing or invalid --block-verifiers-secret-key parameter.");
  }

  return 1;
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
int main(int argc, char *argv[]) {
  
// set global variables defined in define_macros.h
  bool debug_settings = false;

  int result = set_parameters(argc, argv);

  return result;
}