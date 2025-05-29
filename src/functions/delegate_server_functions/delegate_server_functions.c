#include "delegate_server_functions.h"

/*---------------------------------------------------------------------------------------------------------
Name: check_for_valid_delegate_name
Description: Checks for a valid delegate name
Parameters:
  DELEGATE_NAME - The delegate name
Return: 0 if the delegate name is not valid, 1 if the delegate name is valid
---------------------------------------------------------------------------------------------------------*/
int check_for_valid_delegate_name(const char* DELEGATE_NAME)
{
  #define VALID_DATA "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-"

  size_t length = strlen(DELEGATE_NAME);

  // Check name length bounds
  if (length > MAXIMUM_BUFFER_SIZE_DELEGATES_NAME ||
      length < MINIMUM_BUFFER_SIZE_DELEGATES_NAME)
  {
    WARNING_PRINT("Attempt to register a delegate whose name is either too short or too long");
    return XCASH_ERROR;
  }

  // Validate all characters
  for (size_t i = 0; i < length; i++)
  {
    if (strchr(VALID_DATA, DELEGATE_NAME[i]) == NULL)
    {
      return XCASH_ERROR;
    }
  }

  return XCASH_OK;
  #undef VALID_DATA
}

/*---------------------------------------------------------------------------------------------------------
Name: check_for_valid_ip_address
Description: Checks for a valid IP address
Parameters:
  HOST - The IP address or the domain name
Return: XCASH_ERROR if the IP address is not valid, 1 if the IP address is valid
---------------------------------------------------------------------------------------------------------*/
int check_for_valid_ip_address(const char *host) {
  struct addrinfo hints = {0}, *res = NULL;
  struct sockaddr_in *ipv4;
  char ip_str[INET_ADDRSTRLEN];

  if (!host || strlen(host) >= INET_ADDRSTRLEN) return 0;

  // Setup hints
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if (getaddrinfo(host, NULL, &hints, &res) != 0) {
    return XCASH_ERROR; // Not resolvable to IPv4
  }

  ipv4 = (struct sockaddr_in *)res->ai_addr;
  inet_ntop(AF_INET, &(ipv4->sin_addr), ip_str, sizeof(ip_str));
  freeaddrinfo(res);

  uint32_t ip = ntohl(ipv4->sin_addr.s_addr);

  // Reject 0.0.0.0/8, 10.0.0.0/8, 127.0.0.0/8, 169.254.0.0/16, 172.16.0.0/12, 192.168.0.0/16
  if ((ip >> 24) == 0 ||                   // 0.0.0.0/8
      (ip >> 24) == 10 ||                  // 10.0.0.0/8
      (ip >> 24) == 127 ||                 // 127.0.0.0/8
      (ip >> 16) == 0xA9FE ||              // 169.254.0.0/16
      (ip >> 20) == 0xAC1 ||               // 172.16.0.0/12
      (ip >> 16) == 0xC0A8 ||              // 192.168.0.0/16
      (ip >> 24) >= 224) {                 // Multicast/reserved
    return XCASH_ERROR;
  }

  return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: server_receive_data_socket_nodes_to_block_verifiers_register_delegates
Description: Runs the code when the server receives the NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE message
Parameters:
  CLIENT_SOCKET - The socket to send data to
  MESSAGE - The message
---------------------------------------------------------------------------------------------------------*/
void server_receive_data_socket_nodes_to_block_verifiers_register_delegates(server_client_t* client, const char* MESSAGE)
{
  char data[SMALL_BUFFER_SIZE] = {0};
  char delegate_name[MAXIMUM_BUFFER_SIZE_DELEGATES_NAME] = {0};
  char delegate_public_address[XCASH_WALLET_LENGTH + 1] = {0};
  char delegate_public_key[VRF_PUBLIC_KEY_LENGTH + 1] = {0};
  unsigned char delegate_public_key_data[crypto_vrf_PUBLICKEYBYTES + 1] = {0};
  char delegates_IP_address[BLOCK_VERIFIERS_IP_ADDRESS_TOTAL_LENGTH + 1] = {0};
  struct tm current_UTC_date_and_time;
  int count = 0, count2 = 0;
  size_t data_size = 0;

  #define SERVER_RECEIVE_DATA_SOCKET_NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE_ERROR(settings) \
  do { \
    send_data(client, (unsigned char*)settings, strlen(settings)); \
    return; \
  } while (0)

  if (count_all_documents_in_collection(DATABASE_NAME, DB_COLLECTION_DELEGATES) >= (MAXIMUM_AMOUNT_OF_DELEGATES - 1))
  {
    SERVER_RECEIVE_DATA_SOCKET_NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE_ERROR("The maximum amount of delegates has been registered}");
  }

  if (string_count(MESSAGE,"|") != REGISTER_PARAMETER_AMOUNT || check_for_invalid_strings(MESSAGE) == 0)
  {
    SERVER_RECEIVE_DATA_SOCKET_NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE_ERROR("Could not verify the message}");
  }

  for (count = 0, count2 = 0; count < REGISTER_PARAMETER_AMOUNT; count++)
  {
    data_size = strlen(MESSAGE) - strlen(strstr(MESSAGE + count2, "|")) - count2;
    if (count == 1 && data_size < sizeof(delegate_name))
      memcpy(delegate_name, &MESSAGE[count2], data_size);
    else if (count == 2 && data_size < sizeof(delegates_IP_address))
      memcpy(delegates_IP_address, &MESSAGE[count2], data_size);
    else if (count == 3 && data_size == VRF_PUBLIC_KEY_LENGTH)
      memcpy(delegate_public_key, &MESSAGE[count2], data_size);
    else if (count == 4 && data_size == XCASH_WALLET_LENGTH)
      memcpy(delegate_public_address, &MESSAGE[count2], data_size);
    else if (count != 0)
      SERVER_RECEIVE_DATA_SOCKET_NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE_ERROR("Invalid message data}");

    count2 = strlen(MESSAGE) - strlen(strstr(MESSAGE + count2, "|")) + 1;
  }

  for (count2 = 0, count = 0; count2 < VRF_PUBLIC_KEY_LENGTH; count++, count2 += 2)
  {
    memcpy(data, &delegate_public_key[count2], 2);
    delegate_public_key_data[count] = (unsigned char)strtol(data, NULL, 16);
  }

  if (check_for_valid_delegate_name(delegate_name) == 0 || strlen(delegate_public_address) != XCASH_WALLET_LENGTH || 
      strncmp(delegate_public_address, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0 || 
      check_for_valid_ip_address(delegates_IP_address) == 0 || 
      strlen(delegate_public_key) != VRF_PUBLIC_KEY_LENGTH || 
      crypto_vrf_is_valid_key((const unsigned char*)delegate_public_key_data) != 1)
  {
    SERVER_RECEIVE_DATA_SOCKET_NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE_ERROR("Invalid data}");
  }

  snprintf(data, sizeof(data), "{\"public_address\":\"%s\"}", delegate_public_address);
  if (count_documents_in_collection(DATABASE_NAME, DB_COLLECTION_DELEGATES, data) != 0)
  {
    SERVER_RECEIVE_DATA_SOCKET_NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE_ERROR("The delegates public address is already registered}");
  }

  snprintf(data, sizeof(data), "{\"IP_address\":\"%s\"}", delegates_IP_address);
  if (count_documents_in_collection(DATABASE_NAME, DB_COLLECTION_DELEGATES, data) != 0)
  {
    SERVER_RECEIVE_DATA_SOCKET_NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE_ERROR("The delegates IP address is already registered}");
  }

  snprintf(data, sizeof(data), "{\"public_key\":\"%s\"}", delegate_public_key);
  if (count_documents_in_collection(DATABASE_NAME, DB_COLLECTION_DELEGATES, data) != 0)
  {
    SERVER_RECEIVE_DATA_SOCKET_NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE_ERROR("The delegates public key is already registered}");
  }

  snprintf(data, sizeof(data), "{\"delegate_name\":\"%s\"}", delegate_name);
  if (count_documents_in_collection(DATABASE_NAME, DB_COLLECTION_DELEGATES, data) != 0)
  {
    SERVER_RECEIVE_DATA_SOCKET_NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE_ERROR("The delegates name is already registered}");
  }

time_t registration_time = time(NULL);

snprintf(data, sizeof(data),
  "{\"public_address\":\"%s\",\"total_vote_count\":\"0\",\"IP_address\":\"%s\",\"delegate_name\":\"%s\","
  "\"about\":\"\",\"website\":\"\",\"team\":\"\",\"shared_delegate_status\":\"solo\",\"delegate_fee\":\"\","
  "\"server_specs\":\"\",\"block_verifier_score\":\"0\",\"online_status\":\"true\",\"block_verifier_total_rounds\":\"0\","
  "\"block_verifier_online_total_rounds\":\"0\",\"block_verifier_online_percentage\":\"0\","
  "\"block_producer_total_rounds\":\"0\",\"block_producer_block_heights\":\"\",\"public_key\":\"%s\","
  "\"registration_timestamp\":\"%ld\"}",
  delegate_public_address, delegates_IP_address, delegate_name, delegate_public_key, registration_time);

  if (insert_document_into_collection_json(DATABASE_NAME, DB_COLLECTION_DELEGATES, data) == 0)
  {
    SERVER_RECEIVE_DATA_SOCKET_NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE_ERROR("The delegate could not be added to the database}");
  }

  send_data(client, (unsigned char*)"Registered the delegate}", strlen("Registered the delegate}"));

  return;

  #undef SERVER_RECEIVE_DATA_SOCKET_NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE_ERROR
}