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
    char data[SMALL_BUFFER_SIZE]                     = {0};
    char delegate_name[MAXIMUM_BUFFER_SIZE_DELEGATES_NAME]     = {0};
    char delegate_public_address[XCASH_WALLET_LENGTH + 1]      = {0};
    char delegate_public_key[VRF_PUBLIC_KEY_LENGTH + 1]        = {0};
    unsigned char delegate_public_key_data[crypto_vrf_PUBLICKEYBYTES + 1] = {0};
    char delegates_IP_address[BLOCK_VERIFIERS_IP_ADDRESS_TOTAL_LENGTH + 1] = {0};
    uint64_t registration_time = 0;

    #define SERVER_ERROR(rmess) \
      do { \
        send_data(client, (unsigned char*)(rmess), strlen(rmess)); \
        return; \
      } while (0)

    // 1) Parse incoming MESSAGE as JSON
    cJSON *root = cJSON_Parse(MESSAGE);
    if (!root) {
        SERVER_ERROR("Could not verify the message}");
    }

    // 2) Extract and validate each required field
    cJSON *msg_settings = cJSON_GetObjectItemCaseSensitive(root, "message_settings");
    cJSON *js_name      = cJSON_GetObjectItemCaseSensitive(root, "delegate_name");
    cJSON *js_ip        = cJSON_GetObjectItemCaseSensitive(root, "delegate_IP");
    cJSON *js_pubkey    = cJSON_GetObjectItemCaseSensitive(root, "delegate_public_key");
    cJSON *js_address   = cJSON_GetObjectItemCaseSensitive(root, "public_address");
    cJSON *js_reg_time  = cJSON_GetObjectItemCaseSensitive(root, "registration_timestamp");

    if (!cJSON_IsString(msg_settings)     || (msg_settings->valuestring == NULL) ||
        !cJSON_IsString(js_name)          || (js_name->valuestring == NULL)      ||
        !cJSON_IsString(js_ip)            || (js_ip->valuestring == NULL)        ||
        !cJSON_IsString(js_pubkey)        || (js_pubkey->valuestring == NULL)    ||
        !cJSON_IsString(js_address)       || (js_address->valuestring == NULL)  ||
        !cJSON_IsNumber(js_reg_time))
    {
        cJSON_Delete(root);
        SERVER_ERROR("Could not verify the message}");
    }

    // 2a) Ensure message_settings matches exactly
    if (strcmp(msg_settings->valuestring, "NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE") != 0) {
        cJSON_Delete(root);
        SERVER_ERROR("Invalid message_settings}");
    }

    // 2b) Copy them into our local buffers (including null terminators)
    size_t name_len    = strlen(js_name->valuestring);
    size_t ip_len      = strlen(js_ip->valuestring);
    size_t pubkey_len  = strlen(js_pubkey->valuestring);
    size_t address_len = strlen(js_address->valuestring);

    if (name_len == 0 || name_len >= sizeof(delegate_name) ||
        ip_len == 0   || ip_len >= sizeof(delegates_IP_address) ||
        pubkey_len != VRF_PUBLIC_KEY_LENGTH ||
        address_len != XCASH_WALLET_LENGTH)
    {
        cJSON_Delete(root);
        SERVER_ERROR("Invalid message data}");
    }

    memcpy(delegate_name,        js_name->valuestring,    name_len);
    memcpy(delegates_IP_address, js_ip->valuestring,      ip_len);
    memcpy(delegate_public_key,  js_pubkey->valuestring,  pubkey_len);
    memcpy(delegate_public_address, js_address->valuestring, address_len);
    registration_time = (uint64_t)js_reg_time->valuedouble;

    // 3) Convert hex string → raw bytes for VRF public key
    //    (each two hex chars → one byte)
    for (int i = 0, j = 0; i < (int)pubkey_len; i += 2, j++) {
        char byte_hex[3] = { delegate_public_key[i], delegate_public_key[i+1], 0 };
        delegate_public_key_data[j] = (unsigned char)strtol(byte_hex, NULL, 16);
    }
    delegate_public_key_data[crypto_vrf_PUBLICKEYBYTES] = 0; // just in case

    // 4) Validate ranges and formats
    if (check_for_valid_delegate_name(delegate_name) == 0 ||
        strlen(delegate_public_address) != XCASH_WALLET_LENGTH ||
        strncmp(delegate_public_address, XCASH_WALLET_PREFIX, sizeof(XCASH_WALLET_PREFIX) - 1) != 0 ||
        check_for_valid_ip_address(delegates_IP_address) == 0 ||
        crypto_vrf_is_valid_key(delegate_public_key_data) != 1)
    {
        cJSON_Delete(root);
        SERVER_ERROR("Invalid data}");
    }

    cJSON_Delete(root); // we no longer need the JSON tree

    // 5) Check uniqueness in database
    // 5a) public_address
    snprintf(data, sizeof(data), "{\"public_address\":\"%s\"}", delegate_public_address);
    if (count_documents_in_collection(DATABASE_NAME, DB_COLLECTION_DELEGATES, data) != 0)
    {
        SERVER_ERROR("The delegates public address is already registered}");
    }

    // 5b) IP_address
    snprintf(data, sizeof(data), "{\"IP_address\":\"%s\"}", delegates_IP_address);
    if (count_documents_in_collection(DATABASE_NAME, DB_COLLECTION_DELEGATES, data) != 0)
    {
        SERVER_ERROR("The delegates IP address is already registered}");
    }

    // 5c) public_key
    snprintf(data, sizeof(data), "{\"public_key\":\"%s\"}", delegate_public_key);
    if (count_documents_in_collection(DATABASE_NAME, DB_COLLECTION_DELEGATES, data) != 0)
    {
        SERVER_ERROR("The delegates public key is already registered}");
    }

    // 5d) delegate_name
    snprintf(data, sizeof(data), "{\"delegate_name\":\"%s\"}", delegate_name);
    if (count_documents_in_collection(DATABASE_NAME, DB_COLLECTION_DELEGATES, data) != 0)
    {
        SERVER_ERROR("The delegates name is already registered}");
    }

    // 6) Check overall delegate count
    int delegate_count = count_documents_in_collection(DATABASE_NAME, DB_COLLECTION_DELEGATES, "{}");
    if (delegate_count >= BLOCK_VERIFIERS_TOTAL_AMOUNT) {
      SERVER_ERROR("The maximum amount of delegates has been reached}");
    }

    // 7) Finally insert a new document
    double set_delegate_fee = 0.00;
    uint64_t set_counts = 0;

    bool is_primary = false;

#ifdef SEED_NODE_ON
    if (is_primary_node()) {
      is_primary = true;
    }
#endif

    if (!is_seed_node || is_primary) {
      bson_t bson;
      bson_init(&bson);

      // Strings
      bson_append_utf8(&bson, "public_address", -1, delegate_public_address, -1);
      bson_append_utf8(&bson, "IP_address", -1, delegates_IP_address, -1);
      bson_append_utf8(&bson, "delegate_name", -1, delegate_name, -1);
      bson_append_utf8(&bson, "about", -1, "", -1);
      bson_append_utf8(&bson, "website", -1, "", -1);
      bson_append_utf8(&bson, "team", -1, "", -1);
      bson_append_utf8(&bson, "delegate_type", -1, "shared", -1);
      bson_append_utf8(&bson, "server_specs", -1, "", -1);
      bson_append_utf8(&bson, "online_status", -1, "false", -1);
      bson_append_utf8(&bson, "public_key", -1, delegate_public_key, -1);

      // Numbers
      bson_append_int64(&bson, "total_vote_count", -1, set_counts);
      bson_append_double(&bson, "delegate_fee", -1, set_delegate_fee);
      bson_append_int64(&bson, "registration_timestamp", -1, registration_time);

      if (insert_document_into_collection_bson(DATABASE_NAME, DB_COLLECTION_DELEGATES, &bson) != XCASH_OK) {
        bson_destroy(&bson);
        SERVER_ERROR("Failed to insert the delegate document}");
      }

      bson_destroy(&bson);
    }

// Only update statics on seed nodes
#ifdef SEED_NODE_ON

      bson_t bson_statistics;
      bson_init(&bson_statistics);

      // Strings
      bson_append_utf8(&bson_statistics, "public_key", -1, delegate_public_address, -1);

      // Numbers
      bson_append_int64(&bson_statistics, "block_verifier_total_rounds", -1, set_counts);
      bson_append_int64(&bson_statistics, "block_verifier_online_total_rounds", -1, set_counts);
      bson_append_int64(&bson_statistics, "block_producer_total_rounds", -1, set_counts);

      // Insert into "statistics" collection
      if (insert_document_into_collection_bson(DATABASE_NAME, DB_COLLECTION_STATISTICS, &bson_statistics) != XCASH_OK) {
        bson_destroy(&bson_statistics);
        SERVER_ERROR("Failed to insert the statistics document}");
      }

      bson_destroy(&bson_statistics);

#endif

    // 8) Success: reply back to the client
    send_data(client, (unsigned char *)"Registered the delegate}", strlen("Registered the delegate}"));
    return;

#undef SERVER_ERROR
}

/*---------------------------------------------------------------------------------------------------------
Name: server_receive_data_socket_nodes_to_block_verifiers_validate_block
Description: Runs the code when the server receives the XCASHD_TO_DPOPS_VERIFY message.
             This function verifies the VRF proof and beta from a proposed block by reconstructing
             the alpha input (prev_block_hash || height || pubkey) and validating the cryptographic proof.
Parameters:
  CLIENT_SOCKET - The socket to send data to
  MESSAGE - The JSON message containing the VRF proof, beta, public key, block height, and previous hash
---------------------------------------------------------------------------------------------------------*/
void server_receive_data_socket_nodes_to_block_verifiers_validate_block(server_client_t *client, const char *MESSAGE) {
  char response[VSMALL_BUFFER_SIZE] = {0};

  // Parse the incoming JSON message
  cJSON *root = cJSON_Parse(MESSAGE);
  if (!root) {
    send_data(client, (unsigned char *)"Invalid JSON format}", strlen("Invalid JSON format}"));
    return;
  }

  // Extract fields
  cJSON *msg_settings = cJSON_GetObjectItemCaseSensitive(root, "message_settings");
  cJSON *js_vrf_proof = cJSON_GetObjectItemCaseSensitive(root, "vrf_proof");
  cJSON *js_vrf_beta = cJSON_GetObjectItemCaseSensitive(root, "vrf_beta");
  cJSON *js_vrf_pubkey = cJSON_GetObjectItemCaseSensitive(root, "vrf_pubkey");
  cJSON *js_vote_hash = cJSON_GetObjectItemCaseSensitive(root, "vote_hash");
  cJSON *js_height = cJSON_GetObjectItemCaseSensitive(root, "height");
  cJSON *js_prev_hash = cJSON_GetObjectItemCaseSensitive(root, "prev_block_hash");

  if (!cJSON_IsString(msg_settings) || strcmp(msg_settings->valuestring, "XCASHD_TO_DPOPS_VERIFY") != 0 ||
      !cJSON_IsString(js_vrf_proof) || !cJSON_IsString(js_vrf_beta) || !cJSON_IsString(js_vrf_pubkey) ||
      !cJSON_IsString(js_vote_hash) || !cJSON_IsNumber(js_height) || !cJSON_IsString(js_prev_hash)) {
    cJSON_Delete(root);
    send_data(client, (unsigned char *)"Missing or invalid fields}", strlen("Missing or invalid fields}"));
    return;
  }

  // Extract strings and height
  const char *vrf_proof_str = js_vrf_proof->valuestring;
  const char *vrf_beta_str = js_vrf_beta->valuestring;
  const char *vrf_pubkey_str = js_vrf_pubkey->valuestring;
  const char *vote_hash_str   = js_vote_hash->valuestring;
  const char *prev_hash_str = js_prev_hash->valuestring;
  uint64_t height = (uint64_t)js_height->valuedouble;

  // Buffers for binary data
  unsigned char pk_bin[crypto_vrf_PUBLICKEYBYTES] = {0};
  unsigned char proof_bin[crypto_vrf_PROOFBYTES] = {0};
  unsigned char beta_bin[crypto_vrf_OUTPUTBYTES] = {0};
  unsigned char prev_hash_bin[32] = {0};
  unsigned char alpha_input[72] = {0};
  unsigned char computed_beta[crypto_vrf_OUTPUTBYTES] = {0};

  // Convert hex → binary
  if (!hex_to_byte_array(vrf_pubkey_str, pk_bin, sizeof(pk_bin)) ||
      !hex_to_byte_array(vrf_proof_str, proof_bin, sizeof(proof_bin)) ||
      !hex_to_byte_array(vrf_beta_str, beta_bin, sizeof(beta_bin)) ||
      !hex_to_byte_array(prev_hash_str, prev_hash_bin, sizeof(prev_hash_bin))) {
    cJSON_Delete(root);
    send_data(client, (unsigned char *)"Hex decoding failed}", strlen("Hex decoding failed}"));
    return;
  }

  // Create alpha = prev_block_hash || height || pubkey
  memcpy(alpha_input, prev_hash_bin, 32);
  uint64_t height_le = htole64(height);
  memcpy(alpha_input + 32, &height_le, sizeof(height_le));
  memcpy(alpha_input + 40, pk_bin, 32);

  // Verify VRF
  bool valid_block = true;
  if (crypto_vrf_verify(computed_beta, pk_bin, proof_bin, alpha_input, sizeof(alpha_input)) != 0) {
    valid_block = false;
  } else if (memcmp(computed_beta, beta_bin, sizeof(beta_bin)) != 0) {
    valid_block = false;
  }

  cJSON_Delete(root);

  if (valid_block) {
    INFO_PRINT("VALID BLOCK.........................................................");
    snprintf(response, sizeof(response),
             "1|Block verification passed|%s}",
             vote_hash_str);
    send_data(client, (unsigned char *)response, strlen(response));
  } else {
    snprintf(response, sizeof(response),
             "0|Block verification failed|%s}",
             vote_hash_str);
    send_data(client, (unsigned char *)response, strlen(response));
  }





//  Need to add some more checks above --- 





  return;
}