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

/*--------------------------------------------------------------------------
* get_self_sha256
* 
* Compute the SHA-256 hash of the currently running executable image and
* return it as a lowercase hexadecimal string.
*
* Source of bytes:
*   Reads from /proc/self/exe, which points to the exact ELF image the
 *   process was started with (works even if the on-disk path was later
 *   moved or unlinked).
 *
 * Parameters:
 *   out_hex
 *     Buffer provided by the caller to receive the hex digest string.
 *     Must be at least 65 bytes long (64 hex chars + NUL).
 *
 * Returns:
 *   true  on success (out_hex is filled with a 64-char hex string + NUL).
 *   false on failure (out_hex is left unspecified).
 *
 -------------------------------------------------------------------------------*/
bool get_self_sha256(char out_hex[SHA256_DIGEST_SIZE + 1]) {
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int  dlen = 0;

  int fd = open("/proc/self/exe", O_RDONLY | O_CLOEXEC);
  if (fd < 0) return false;

  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  if (!ctx) { close(fd); return false; }

  if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
    EVP_MD_CTX_free(ctx); close(fd); return false;
  }

  unsigned char buf[1 << 16]; // 64 KiB
  for (;;) {
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n > 0) {
      if (EVP_DigestUpdate(ctx, buf, (size_t)n) != 1) {
        EVP_MD_CTX_free(ctx); close(fd); return false;
      }
    } else if (n == 0) {
      break; // EOF
    } else {
      if (errno == EINTR) continue;
      EVP_MD_CTX_free(ctx); close(fd); return false;
    }
  }

  if (EVP_DigestFinal_ex(ctx, digest, &dlen) != 1) {
    EVP_MD_CTX_free(ctx); close(fd); return false;
  }

  EVP_MD_CTX_free(ctx);
  close(fd);

  static const char hexdig[] = "0123456789abcdef";
  for (unsigned int i = 0; i < dlen; ++i) {
    out_hex[2*i    ] = hexdig[(digest[i] >> 4) & 0xF];
    out_hex[2*i + 1] = hexdig[(digest[i]     ) & 0xF];
  }
  out_hex[2*dlen] = '\0';
  return true;
}