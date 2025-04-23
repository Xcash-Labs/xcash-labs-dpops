#include "server_functions.h"

/*---------------------------------------------------------------------------------------------------------
Name: server_limit_public_addresses
Description: limits connections based on registered public addresses to the server
Parameters:
  SETTINGS - 1 to run the check before running the server code, 0 to remove the IP address so it can make another connection,
    2 to not verify any of the previous block hash or current round part
  MESSAGE - The message
  Return: 0 if there is multiple connections, 1 if there is a single connection
---------------------------------------------------------------------------------------------------------*/
int server_limit_public_addresses(const int SETTINGS, const char* MESSAGE) {
  if (!MESSAGE || *MESSAGE == '\0') return 0;

  char data[SMALL_BUFFER_SIZE] = {0};
  char public_address[SMALL_BUFFER_SIZE] = {0};
  long long int vote_count = 0;
  size_t address_len = 0;

  // Extract public address
  if (SETTINGS == 1 || SETTINGS == 3) {
      if (parse_json_data(MESSAGE, "public_address", public_address, sizeof(public_address)) == 0) {
          return 0;
      }
  } else if (SETTINGS == 2 || SETTINGS == 4) {
      int pipe_offset = 0;
      for (int i = 0; i < GET_RESERVE_BYTES_DATABASE_HASH_PARAMETER_AMOUNT; i++) {
          if (i == 2) {
              address_len = strlen(MESSAGE) - strlen(strstr(MESSAGE + pipe_offset, "|")) - pipe_offset;
              if (address_len != XCASH_WALLET_LENGTH) return 0;
              memcpy(public_address, &MESSAGE[pipe_offset], address_len);
              if (strncmp(public_address, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0) return 0;
              break;
          }
          pipe_offset = (int)(strlen(MESSAGE) - strlen(strstr(MESSAGE + pipe_offset, "|")) + 1);
      }
  } else {
      return 0;
  }

  // Format limiter key
  snprintf(data, sizeof(data), "|%.*s", (int)(sizeof(data) - 2), public_address);

  // SETTINGS 3 or 4: REMOVE the address
  if (SETTINGS == 3 || SETTINGS == 4) {
      pthread_mutex_lock(&database_data_IP_address_lock);
      string_replace_limit(server_limit_public_address_list, sizeof(server_limit_public_address_list), data, "", 1);
      pthread_mutex_unlock(&database_data_IP_address_lock);
      return 1;
  }

  // SETTINGS 1 or 2: VERIFY and APPLY limit
  if (verify_data(MESSAGE, 0) == 0) return 0;

  if (!is_seed_address(public_address)) {
      char query[SMALL_BUFFER_SIZE];
      snprintf(query, sizeof(query), "{\"public_address\":\"%.*s\"}", (int)(sizeof(query) - 23), public_address);
      if (read_document_field_from_collection(DATABASE_NAME, DB_COLLECTION_DELEGATES, query, "total_vote_count", public_address) == 1) {
          sscanf(public_address, "%lld", &vote_count);
          if (vote_count < 0) return 0;
      } else {
          return 0;
      }
  }

  // LIMIT check
  pthread_mutex_lock(&database_data_IP_address_lock);
  if (string_count(server_limit_public_address_list, data) > MAXIMUM_CONNECTIONS_IP_ADDRESS_OR_PUBLIC_ADDRESS) {
      pthread_mutex_unlock(&database_data_IP_address_lock);
      return 0;
  }
  strncat(server_limit_public_address_list, data, sizeof(server_limit_public_address_list) - strlen(server_limit_public_address_list) - 1);
  pthread_mutex_unlock(&database_data_IP_address_lock);
  return 1;
}

/*---------------------------------------------------------------------------------------------------------
Name: server_limit_IP_addresses
Description: limits connections based on IP addresses to the server
Parameters:
  SETTINGS - 1 to run the check before running the server code, 0 to remove the IP address so it can make another connection
  IP_ADDRESS - The IP address
  Return: 0 if there is multiple connections, 1 if there is a single connection
---------------------------------------------------------------------------------------------------------*/
int server_limit_IP_addresses(const int SETTINGS, const char* IP_ADDRESS) {
  if (!IP_ADDRESS || *IP_ADDRESS == '\0') {
      return 0;  // Invalid input
  }

  char data[SMALL_BUFFER_SIZE];
  snprintf(data, sizeof(data), "|%s", IP_ADDRESS);  // Prefix with "|"

  int result = 0;

  pthread_mutex_lock(&database_data_IP_address_lock);

  if (SETTINGS == 1) {
      // Check if the IP has exceeded the limit
      if (string_count(server_limit_IP_address_list, data) < MAXIMUM_CONNECTIONS_IP_ADDRESS_OR_PUBLIC_ADDRESS) {
          strncat(server_limit_IP_address_list, data, sizeof(server_limit_IP_address_list) - strlen(server_limit_IP_address_list) - 1);
          result = 1;
      }
  } else if (SETTINGS == 0) {
      // Remove the IP entry (limit 1 occurrence)
      string_replace_limit(server_limit_IP_address_list, sizeof(server_limit_IP_address_list), data, "", 1);
      result = 1;
  }

  pthread_mutex_unlock(&database_data_IP_address_lock);
  return result;
}