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
  if (test_settings == 1) {
    return 1;
  }

  char data[SMALL_BUFFER_SIZE] = {0};
  char data2[SMALL_BUFFER_SIZE] = {0};
  char data3[SMALL_BUFFER_SIZE] = {0};
  size_t data_size = 0;
  long long int number = 0;

  // Extract public address based on SETTINGS
  if ((SETTINGS == 1 || SETTINGS == 3) && parse_json_data(MESSAGE, "public_address", data2, sizeof(data2)) == 0) {
    return 0;
  }

  if ((SETTINGS == 2 || SETTINGS == 4)) {
    int count2 = 0;
    for (int count = 0; count < GET_RESERVE_BYTES_DATABASE_HASH_PARAMETER_AMOUNT; count++) {
      if (count == 2) {
        data_size = strlen(MESSAGE) - strlen(strstr(MESSAGE + count2, "|")) - count2;
        if (data_size != XCASH_WALLET_LENGTH) return 0;
        memcpy(data2, &MESSAGE[count2], data_size);
        if (strncmp(data2, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0) return 0;
        break;
      }
      count2 = (int)(strlen(MESSAGE) - strlen(strstr(MESSAGE + count2, "|")) + 1);
    }
  }

  snprintf(data, sizeof(data), "|%s", data2);

  // Start processing based on SETTINGS
  if (SETTINGS == 1 || SETTINGS == 2) {
    if (verify_data(MESSAGE, 0) == 0) return 0;

    snprintf(data3, sizeof(data3), "{\"public_address\":\"%s\"}", data2);

    if (is_seed_address(data2)) {
      goto check_limit;
    }

    // Check delegate registration and vote count
    if (read_document_field_from_collection(database_name, DB_COLLECTION_DELEGATES, data3, "total_vote_count", data2) == 1) {
      sscanf(data2, "%lld", &number);
      if ((production_settings == 1 && number < DATABASE_DATA_SYNC_DELEGATE_MINIMUM_AMOUNT) ||
          (production_settings == 0 && number < 0)) {
        return 0;
      }
    } else {
      return 0;
    }
  }

check_limit:
  pthread_mutex_lock(&database_data_IP_address_lock);
  if (string_count(server_limit_public_address_list, data) > MAXIMUM_CONNECTIONS_IP_ADDRESS_OR_PUBLIC_ADDRESS) {
    pthread_mutex_unlock(&database_data_IP_address_lock);
    return 0;
  }
  strcat(server_limit_public_address_list, data);
  pthread_mutex_unlock(&database_data_IP_address_lock);
  return 1;

  // Remove public address from limit list
  if (SETTINGS == 3 || SETTINGS == 4) {
    pthread_mutex_lock(&database_data_IP_address_lock);
    string_replace_limit(server_limit_public_address_list, 15728640, data, "", 1);
    pthread_mutex_unlock(&database_data_IP_address_lock);
    return 1;
  }

  return 0;
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
      return 0;  // Return early if IP_ADDRESS is NULL or empty
  }

  char data[SMALL_BUFFER_SIZE] = {0};

  // Construct the data string with a leading "|"
  snprintf(data, sizeof(data), "|%s", IP_ADDRESS);

  pthread_mutex_lock(&database_data_IP_address_lock);

  if (SETTINGS == 1) {
      // Check if the IP has exceeded max allowed connections
      if (string_count(server_limit_IP_address_list, data) >= MAXIMUM_CONNECTIONS_IP_ADDRESS_OR_PUBLIC_ADDRESS) {
          pthread_mutex_unlock(&database_data_IP_address_lock);
          return 0;
      }

      // Append the new IP entry to the list
      strncat(server_limit_IP_address_list, data, sizeof(server_limit_IP_address_list) - strlen(server_limit_IP_address_list) - 1);
      pthread_mutex_unlock(&database_data_IP_address_lock);
      return 1;
  }

  if (SETTINGS == 0) {
      // Remove IP address from the tracking list
      string_replace_limit(server_limit_IP_address_list, 15728640, data, "", 1);
      pthread_mutex_unlock(&database_data_IP_address_lock);
      return 1;
  }

  pthread_mutex_unlock(&database_data_IP_address_lock);
  return 0;
}