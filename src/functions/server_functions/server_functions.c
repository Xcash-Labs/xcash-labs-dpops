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

  char data[VVSMALL_BUFFER_SIZE];
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

  if (result == XCASH_ERROR) {
    ERROR_PRINT("Rate limit hit ip_address: %s", IP_ADDRESS);
  }

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
  char data[VVSMALL_BUFFER_SIZE] = {0};

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
      size_t len = strlen(server_limit_public_address_list);
      snprintf(server_limit_public_address_list + len,
               sizeof(server_limit_public_address_list) - len,
               "%s", data);
      result = XCASH_OK;
    }
  } else if (action == LIMIT_REMOVE) {
    string_replace_limit(server_limit_public_address_list,
                         sizeof(server_limit_public_address_list), data, "", 1);
    result = XCASH_OK;
  }

  pthread_mutex_unlock(&database_data_IP_address_lock);

  if (result == XCASH_ERROR) {
    ERROR_PRINT("Rate limit hit for public_address: %s", public_address);
  }

  return result;
}

/*---------------------------------------------------------------------------------------------------------
Name: server_limit_public_addresses_vrf_lookup
Description: Limits or removes connections based on public addresses.  Public address is retrieved 
  using the vrf public key.

Parameters:
  LIMIT_ACTION - LIMIT_CHECK (1) to enforce limit and add address if below threshold,
                 LIMIT_REMOVE (0) to remove the address from the limit list.
  MESSAGE - JSON string containing the "vrf_pubkey" field.

Return:
  1 if the operation was successful (limit passed or address removed),
  0 if the limit is exceeded, input is invalid, or an error occurred.
---------------------------------------------------------------------------------------------------------*/
int server_limit_public_addresses_vrf_lookup(limit_action_t action, const char* MESSAGE) {
  if (!MESSAGE || *MESSAGE == '\0') return 0;

  char vrf_pubkey[VRF_PUBLIC_KEY_LENGTH + 1] = {0};
  char public_address[XCASH_WALLET_LENGTH + 1] = {0};
  char data[VVSMALL_BUFFER_SIZE] = {0};

  if (parse_json_data(MESSAGE, "vrf_pubkey", vrf_pubkey, sizeof(vrf_pubkey)) != 1)
    return XCASH_ERROR;

  if (strlen(vrf_pubkey) != VRF_PUBLIC_KEY_LENGTH)
    return XCASH_ERROR;

  char filter_json[VVSMALL_BUFFER_SIZE] = {0};
  snprintf(filter_json, sizeof(filter_json), "{ \"public_key\": \"%s\" }", vrf_pubkey);
  if (read_document_field_from_collection(
      DATABASE_NAME,
      DB_COLLECTION_DELEGATES,
      filter_json,
      "public_address",
      public_address,
      sizeof(public_address)) != XCASH_OK) {
        ERROR_PRINT("Failed to map vrf_pubkey to public_address: %s", vrf_pubkey);
        return XCASH_ERROR;
  }

  snprintf(data, sizeof(data), "|%s", public_address);

  int result = XCASH_ERROR;
  pthread_mutex_lock(&database_data_IP_address_lock);

  if (action == LIMIT_CHECK) {
    if (string_count(server_limit_public_address_list, data) < MAXIMUM_CONNECTIONS_IP_ADDRESS_OR_PUBLIC_ADDRESS) {
      size_t len = strlen(server_limit_public_address_list);
      snprintf(server_limit_public_address_list + len,
               sizeof(server_limit_public_address_list) - len,
               "%s", data);
      result = XCASH_OK;
    }
  } else if (action == LIMIT_REMOVE) {
    string_replace_limit(server_limit_public_address_list,
                         sizeof(server_limit_public_address_list), data, "", 1);
    result = XCASH_OK;
  }

  pthread_mutex_unlock(&database_data_IP_address_lock);

  if (result == XCASH_ERROR) {
    ERROR_PRINT("Rate limit hit for public_address: %s", public_address);
  }

  return result;
}