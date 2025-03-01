#include "xcash_net.h"

// Helper function to append SOCKET_END_STRING to a message
static char* _build_message_ender(const char* message) {
    int message_buf_size = strlen(message) + strlen(SOCKET_END_STRING) + 1;
    char *message_ender = calloc(message_buf_size, 1);
    if (!message_ender) {
        ERROR_PRINT("Memory allocation failed in _build_message_ender()");
        return NULL;
    }
    snprintf(message_ender, message_buf_size, "%s%s", message, SOCKET_END_STRING);
    return message_ender;
}

// Removes trailing |END| from each valid response
void remove_enders(response_t **responses) {
    if (!responses) return;

    for (int i = 0; responses[i]; i++) {
        // Only process OK status
        if (responses[i]->status == STATUS_OK) {
            if (responses[i]->size == 0) {
                responses[i]->status = STATUS_INCOMPLETE;
                DEBUG_PRINT("Returned data from host '%s' is empty; marked STATUS_INCOMPLETE",
                            responses[i]->host);
            } else {
                bool ender_found = false;
                char *tmp = calloc(responses[i]->size + 1, 1);
                if (tmp) {
                    memcpy(tmp, responses[i]->data, responses[i]->size);
                    tmp[responses[i]->size] = '\0';

                    if (responses[i]->size >= (sizeof(SOCKET_END_STRING) - 1)) {
                        char *ender_position = strstr(tmp, SOCKET_END_STRING);
                        if (ender_position) {
                            ender_found = true;
                            int ender_pos = (int)(ender_position - tmp);
                            responses[i]->data[ender_pos] = '\0';
                            responses[i]->size = strlen(responses[i]->data);
                        }
                    }
                    if (!ender_found) {
                        WARNING_PRINT("Returned data has no |END|; size %ld, message: %s",
                                      responses[i]->size, tmp);
                    }
                    free(tmp);
                } else {
                    ERROR_PRINT("Memory allocation failed in remove_enders()");
                }
            }
        }
    }
}

// Sends a message (with appended |END|) to a group of hosts via send_multi_request()
bool xnet_send_data_multi(xcash_dest_t dest, const char* message, response_t ***reply) {
    bool result = false;
    if (!reply) {
        DEBUG_PRINT("reply parameter can't be NULL");
        return false;
    }
    *reply = NULL;

    // Host array placeholders
    const char **hosts = NULL;
    response_t **responses = NULL;
    char *message_ender = NULL;

    switch (dest) {
        case XNET_SEEDS_ALL: {
            const char *all_hosts[network_data_nodes_amount + 1];
            int i = 0;
            while (i < network_data_nodes_amount) {
                all_hosts[i] = network_data_nodes_list.network_data_nodes_IP_address[i];
                i++;
            }
            all_hosts[i] = NULL;
            hosts = all_hosts;
        } break;

        case XNET_SEEDS_ALL_ONLINE: {
            const char *online_hosts[network_data_nodes_amount + 1];
            int si = 0, di = 0;
            while (si < network_data_nodes_amount) {
                if (network_data_nodes_list.online_status[si] == 1) {
                    online_hosts[di++] = network_data_nodes_list.network_data_nodes_IP_address[si];
                }
                si++;
            }
            online_hosts[di] = NULL;
            hosts = online_hosts;
        } break;

        case XNET_DELEGATES_ALL: {
            const char *delegates_hosts[BLOCK_VERIFIERS_TOTAL_AMOUNT + 1];
            size_t host_index = 0;
            for (size_t i = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
                if (strlen(delegates_all[i].IP_address) != 0) {
                    DEBUG_PRINT("REQ to %s : %s", delegates_all[i].delegate_name, delegates_all[i].IP_address);
                    delegates_hosts[host_index++] = delegates_all[i].IP_address;
                }
            }
            delegates_hosts[host_index] = NULL;
            hosts = delegates_hosts;
        } break;

        case XNET_DELEGATES_ALL_ONLINE: {
            const char *delegates_online_hosts[BLOCK_VERIFIERS_TOTAL_AMOUNT + 1];
            size_t host_index = 0;
            for (size_t i = 0; i < BLOCK_VERIFIERS_AMOUNT; i++) {
                bool is_online = (strcmp(delegates_all[i].online_status, "true") == 0);
                bool has_ip    = (strlen(delegates_all[i].IP_address) != 0);
                bool not_self  = (strcmp(delegates_all[i].public_address, xcash_wallet_public_address) != 0);

                if (is_online && has_ip && not_self) {
                    delegates_online_hosts[host_index++] = delegates_all[i].IP_address;
                }
            }
            delegates_online_hosts[host_index] = NULL;
            hosts = delegates_online_hosts;
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

    // Build message_ender
    message_ender = _build_message_ender(message);
    if (!message_ender) {
        return false;  // Already logged error
    }

    responses = send_multi_request(hosts, XCASH_DPOPS_PORT, message_ender);
    free(message_ender);

    if (responses) {
        remove_enders(responses);
        result = true;
    }
    *reply = responses;
    return result;
}

// Wrappers for sending messages with parameter lists or variadic arguments
bool send_message_param_list(xcash_dest_t dest, xcash_msg_t msg, response_t ***reply, const char** pair_params) {
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

    char* message_data = create_message(msg);
    if (!message_data) {
        return false;
    }

    result = xnet_send_data_multi(dest, message_data, reply);
    free(message_data);

    return result;
}

// Direct message sends
bool send_direct_message_param_list(const char* host, xcash_msg_t msg, response_t ***reply, const char** pair_params) {
    bool result = false;
    *reply = NULL;

    const char *hosts[] = {host, NULL};
    char *message_data = create_message_param_list(msg, pair_params);
    if (!message_data) {
        return false;
    }

    char *message_ender = _build_message_ender(message_data);
    free(message_data);

    if (!message_ender) {
        return false;
    }

    response_t **responses = send_multi_request(hosts, XCASH_DPOPS_PORT, message_ender);
    free(message_ender);

    if (responses) {
        remove_enders(responses);
        result = true;
    }
    *reply = responses;

    return result;
}

bool send_direct_message_param(const char* host, xcash_msg_t msg, response_t ***reply, ...) {
    bool result = false;
    char* message_data = NULL;
    *reply = NULL;

    const char *hosts[] = {host, NULL};

    va_list args;
    va_start(args, reply);
    message_data = create_message_args(msg, args);
    va_end(args);

    if (!message_data) {
        return false;
    }

    char *message_ender = _build_message_ender(message_data);
    free(message_data);

    if (!message_ender) {
        return false;
    }

    response_t **responses = send_multi_request(hosts, XCASH_DPOPS_PORT, message_ender);
    free(message_ender);

    if (responses) {
        remove_enders(responses);
        result = true;
    }

    *reply = responses;
    return result;
}

bool send_direct_message(const char* host, xcash_msg_t msg, response_t ***reply) {
    *reply = NULL;
    return send_direct_message_param(host, msg, reply, NULL);
}