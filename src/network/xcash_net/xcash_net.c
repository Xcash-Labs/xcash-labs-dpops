#include "xcash_net.h"

void check_responses(response_t **responses) {
  if (!responses) {
      return;
  }

  int i = 0;
  while (responses[i]) {
      if (!responses[i]->data) {
          WARNING_PRINT("[WARNING] Response data is NULL for host '%s'. Skipping...", responses[i]->host);
          responses[i]->status = STATUS_INCOMPLETE;
          i++;
          continue;
      }

      if (responses[i]->status == STATUS_OK) {
          if (responses[i]->size == 0) {
              responses[i]->status = STATUS_INCOMPLETE;
              DEBUG_PRINT("[DEBUG] Returned data from host '%s' is empty. Marked as STATUS_INCOMPLETE", responses[i]->host);
          }
      }
      i++;
  }
}

// Sends a message to designated hosts
bool xnet_send_data_multi(xcash_dest_t dest, const char *message, response_t ***reply) {
  bool result = false;
  if (!reply) {
    DEBUG_PRINT("reply parameter can't be NULL");
    return false;
  }
  *reply = NULL;

  // Host array placeholders
  const char **hosts = NULL;
  response_t **responses = NULL;

  switch (dest) {
    case XNET_SEEDS_ALL: {
      const char **all_hosts = malloc((network_data_nodes_amount + 1) * sizeof(char *));
      if (!all_hosts) {
        ERROR_PRINT("Failed to allocate memory for all_hosts");
        return false;  // Handle memory allocation failure
      }

      int i = 0, di = 0;  // `di` is for destination index to compact the array
      while (i < network_data_nodes_amount) {
        bool not_self = (strcmp(network_nodes[i].seed_public_address, xcash_wallet_public_address) != 0);
        bool has_ip = (network_nodes[i].ip_address != NULL && strlen(network_nodes[i].ip_address) != 0);

        if (not_self && has_ip) {
          all_hosts[di++] = network_nodes[i].ip_address;  // Only add if not self and has valid IP
        } else if (!not_self) {
          DEBUG_PRINT("Skipping self: %s", network_nodes[i].ip_address);
        } else if (!has_ip) {
          ERROR_PRINT("Invalid IP address for node with public address %s", network_nodes[i].seed_public_address);
        }
        i++;  // Move to the nmessage_enderext node regardless of conditions
      }

      all_hosts[di] = NULL;  // Null-terminate the array
      hosts = all_hosts;     // Assign heap-allocated array to hosts
    } break;

    case XNET_SEEDS_ALL_ONLINE: {
      const char **online_hosts = malloc((network_data_nodes_amount + 1) * sizeof(char *));
      if (!online_hosts) {
        ERROR_PRINT("Failed to allocate memory for online_hosts");
        return false;  // Handle memory allocation failure
      }

      int si = 0, di = 0;
      while (si < network_data_nodes_amount) {
        if (network_nodes[si].online_status == 1) {
          if (!network_nodes[si].ip_address) {  // Check for NULL IP address
            ERROR_PRINT("IP address is NULL for node %d", si);
            continue;  // Skip to next node
          }
          online_hosts[di++] = network_nodes[si].ip_address;  // Assign IP if online
        }
        si++;
      }
      online_hosts[di] = NULL;  // Null-terminate the array
      hosts = online_hosts;     // Assign heap-allocated array to hosts
    } break;

    case XNET_DELEGATES_ALL: {
      const char **delegates_hosts = malloc((BLOCK_VERIFIERS_TOTAL_AMOUNT + 1) * sizeof(char *));
      if (!delegates_hosts) {
        ERROR_PRINT("Failed to allocate memory for delegates_hosts");
        return false;  // Handle memory allocation failure
      }
      
      size_t host_index = 0;
      for (size_t i = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
        const char *ip = delegates_all[i].IP_address;
        const char *delegate_name = delegates_all[i].delegate_name;
    
        // Null check first for safety
        if (!ip) {
            ERROR_PRINT("IP address is NULL for delegate %s", delegate_name);
            continue;
        }
    
        // Skip empty IP strings
        if (strlen(ip) == 0) {
            continue;
        }
    
        // Skip self by public address (assuming index `i` aligns correctly)
        if (strcmp(network_nodes[i].seed_public_address, xcash_wallet_public_address) == 0) {
            DEBUG_PRINT("Skipping self delegate: %s (%s)", delegate_name, ip);
            continue;
        }
    
        DEBUG_PRINT("REQ to %s : %s", delegate_name, ip);
        delegates_hosts[host_index++] = ip;
      }
      delegates_hosts[host_index] = NULL; // Null-terminate the array
      hosts = delegates_hosts;            // Assign heap-allocated array to hosts
    } break;

    case XNET_DELEGATES_ALL_ONLINE: {
      const char **delegates_online_hosts = malloc((BLOCK_VERIFIERS_TOTAL_AMOUNT + 1) * sizeof(char *));
      if (!delegates_online_hosts) {
        ERROR_PRINT("Failed to allocate memory for delegates_online_hosts");
        return false;  // Handle memory allocation failure
      }

      size_t host_index = 0;
      for (size_t i = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
        bool is_online = (strcmp(delegates_all[i].online_status, "true") == 0);
        bool has_ip = (strlen(delegates_all[i].IP_address) != 0);
        bool not_self = (strcmp(delegates_all[i].public_address, xcash_wallet_public_address) != 0);

        if (is_online && has_ip && not_self) {
          if (!delegates_all[i].IP_address) {  // Check for NULL IP address
            ERROR_PRINT("IP address is NULL for delegate %s", delegates_all[i].delegate_name);
            continue;  // Skip to next delegate
          }

          DEBUG_PRINT("Online delegate: %s (%s)", delegates_all[i].delegate_name, delegates_all[i].IP_address);
          delegates_online_hosts[host_index++] = delegates_all[i].IP_address;  // Direct assignment
        }
      }
      delegates_online_hosts[host_index] = NULL;  // Null-terminate the array
      hosts = delegates_online_hosts;             // Assign heap-allocated array to hosts
    } break;

    default: {
      ERROR_PRINT("Invalid xcash_dest_t: %d", dest);
      return false;
    }
  }

  if (!hosts) {
    ERROR_PRINT("Host array is NULL or not initialized properly.");
    return false;
  }

  responses = send_multi_request(hosts, XCASH_DPOPS_PORT, message);
  free(hosts);
  if (responses) {
    check_responses(responses);
    result = true;
  }
  *reply = responses;
  return result;
}

// Wrappers for sending messages with parameter lists or variadic arguments
bool send_message_param_list(xcash_dest_t dest, xcash_msg_t msg, response_t ***reply, const char **pair_params) {
  bool result = false;
  *reply = NULL;

  char *message_data = create_message_param_list(msg, pair_params);
  if (!message_data) {
    return false;
  }

  result = xnet_send_data_multi(dest, message_data, reply);
  free(message_data);

  return result;
}

bool send_message_param(xcash_dest_t dest, xcash_msg_t msg, response_t ***reply, ...) {
  bool result = false;
  char *message_data = NULL;
  *reply = NULL;

  va_list args;
  va_start(args, reply);
  message_data = create_message_args(msg, args);
  va_end(args);

  if (!message_data) {
    return false;
  }

  result = xnet_send_data_multi(dest, message_data, reply);
  free(message_data);

  return result;
}

bool send_message(xcash_dest_t dest, xcash_msg_t msg, response_t ***reply) {
  bool result = false;
  *reply = NULL;

  char *message_data = create_message(msg);
  if (!message_data) {
    return false;
  }

  result = xnet_send_data_multi(dest, message_data, reply);
  free(message_data);

  return result;
}

// Direct message sends
bool send_direct_message_param_list(const char *host, xcash_msg_t msg, response_t ***reply, const char **pair_params) {
  bool result = false;
  *reply = NULL;

  const char *hosts[] = {host, NULL};
  char *message_data = create_message_param_list(msg, pair_params);
  if (!message_data) {
    return false;
  }

  response_t **responses = send_multi_request(hosts, XCASH_DPOPS_PORT, message_data);
  free(message_data);

  if (responses) {
    check_responses(responses);
    result = true;
  }
  *reply = responses;

  return result;
}

bool send_direct_message_param(const char *host, xcash_msg_t msg, response_t ***reply, ...) {
  bool result = false;
  char *message_data = NULL;
  *reply = NULL;

  const char *hosts[] = {host, NULL};

  va_list args;
  va_start(args, reply);
  message_data = create_message_args(msg, args);
  va_end(args);

  if (!message_data) {
    return false;
  }

  response_t **responses = send_multi_request(hosts, XCASH_DPOPS_PORT, message_data);
  free(message_data);

  if (responses) {
    check_responses(responses);
    result = true;
  }

  *reply = responses;
  return result;
}

bool send_direct_message(const char *host, xcash_msg_t msg, response_t ***reply) {
  *reply = NULL;
  return send_direct_message_param(host, msg, reply, NULL);
}