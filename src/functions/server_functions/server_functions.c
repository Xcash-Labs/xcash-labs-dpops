#include "server_functions.h"

/*---------------------------------------------------------------------------------------------------------
Name: server_limit_IP_addresses
Description: Limits or removes connections based on IP addresses to the server.

Parameters:
  LIMIT_ACTION - LIMIT_CHECK (1) to enforce the limit and add the IP address if below threshold,
                 LIMIT_REMOVE (0) to remove the IP address from the limit list.
  IP_ADDRESS    - The client IP address to check or remove.

Return:
  1 if the operation was successful (limit passed or address removed),
  0 if the limit is exceeded, input is invalid, or an error occurred.
---------------------------------------------------------------------------------------------------------*/
int server_limit_IP_addresses(limit_action_t action, const char* IP_ADDRESS) {
  if (!IP_ADDRESS || *IP_ADDRESS == '\0') return 0;
  if (strlen(IP_ADDRESS) >= 64 || strchr(IP_ADDRESS, '|')) return 0;

  char data[SMALL_BUFFER_SIZE];
  snprintf(data, sizeof(data), "|%s", IP_ADDRESS);

  int result = XCASH_ERROR;

  pthread_mutex_lock(&database_data_IP_address_lock);

  if (action == LIMIT_CHECK) {
    // Check limit before accepting connection
    if (string_count(server_limit_IP_address_list, data) < MAXIMUM_CONNECTIONS_IP_ADDRESS_OR_PUBLIC_ADDRESS) {
      snprintf(server_limit_IP_address_list + strlen(server_limit_IP_address_list),
         sizeof(server_limit_IP_address_list) - strlen(server_limit_IP_address_list),
         "%s", data);
      result = XCASH_OK;
    }
  } else if (action == LIMIT_REMOVE) {
    // Remove one occurrence
    string_replace_limit(server_limit_IP_address_list, sizeof(server_limit_IP_address_list), data, "", 1);
    result = XCASH_OK;
  }

  pthread_mutex_unlock(&database_data_IP_address_lock);
  return result;
}

/*---------------------------------------------------------------------------------------------------------
Name: server_limit_public_addresses
Description: Limits or removes connections based on public addresses.

Parameters:
  LIMIT_ACTION - LIMIT_CHECK (1) to enforce limit and add address if below threshold,
                 LIMIT_REMOVE (0) to remove the address from the limit list.
  MESSAGE - JSON string containing the "public_address" field.

Return:
  1 if the operation was successful (limit passed or address removed),
  0 if the limit is exceeded, input is invalid, or an error occurred.
---------------------------------------------------------------------------------------------------------*/
int server_limit_public_addresses(limit_action_t action, const char* MESSAGE) {
  if (!MESSAGE || *MESSAGE == '\0') return 0;

  char public_address[XCASH_WALLET_LENGTH + 1] = {0};
  char data[SMALL_BUFFER_SIZE] = {0};

  if (parse_json_data(MESSAGE, "public_address", public_address, sizeof(public_address)) != 1)
    return 0;

  if (strlen(public_address) != XCASH_WALLET_LENGTH ||
      strncmp(public_address, XCASH_WALLET_PREFIX, strlen(XCASH_WALLET_PREFIX)) != 0)
    return 0;

  snprintf(data, sizeof(data), "|%s", public_address);

  int result = XCASH_ERROR;
  pthread_mutex_lock(&database_data_IP_address_lock);

  if (action == LIMIT_CHECK) {
    if (string_count(server_limit_public_address_list, data) < MAXIMUM_CONNECTIONS_IP_ADDRESS_OR_PUBLIC_ADDRESS) {
      strncat(server_limit_public_address_list, data,
              sizeof(server_limit_public_address_list) - strlen(server_limit_public_address_list) - 1);
      result = XCASH_OK;
    }
  } else if (action == LIMIT_REMOVE) {
    string_replace_limit(server_limit_public_address_list,
                         sizeof(server_limit_public_address_list), data, "", 1);
    result = XCASH_OK;
  }

  pthread_mutex_unlock(&database_data_IP_address_lock);
  return result;
}