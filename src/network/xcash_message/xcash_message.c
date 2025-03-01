#include "xcash_message.h"

const char* xcash_net_messages[] = {
    "NODE_TO_BLOCK_VERIFIERS_ADD_RESERVE_PROOF",
    "NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE",
    "NODE_TO_NETWORK_DATA_NODES_CHECK_VOTE_STATUS",
    "NODES_TO_BLOCK_VERIFIERS_UPDATE_DELEGATE",
    "NODES_TO_BLOCK_VERIFIERS_RECOVER_DELEGATE",
    "NODE_TO_BLOCK_VERIFIERS_GET_RESERVE_BYTES_DATABASE_HASH",
    "BLOCK_VERIFIERS_TO_NODES_RESERVE_BYTES_DATABASE_SYNC_CHECK_ALL_DOWNLOAD",
    "GET_CURRENT_BLOCK_HEIGHT",
    "SEND_CURRENT_BLOCK_HEIGHT",
    "MAIN_NODES_TO_NODES_PART_4_OF_ROUND_CREATE_NEW_BLOCK",
    "MAIN_NETWORK_DATA_NODE_TO_BLOCK_VERIFIERS_START_BLOCK",
    "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_VRF_DATA",
    "NODES_TO_NODES_VOTE_MAJORITY_RESULTS",
    "NODES_TO_NODES_VOTE_RESULTS",
    "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_BLOCK_BLOB_SIGNATURE",
    "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_INVALID_RESERVE_PROOFS",
    "NODE_TO_NETWORK_DATA_NODES_GET_PREVIOUS_CURRENT_NEXT_BLOCK_VERIFIERS_LIST",
    "NODE_TO_NETWORK_DATA_NODES_GET_CURRENT_BLOCK_VERIFIERS_LIST",
    "NETWORK_DATA_NODE_TO_NODE_SEND_PREVIOUS_CURRENT_NEXT_BLOCK_VERIFIERS_LIST",
    "NETWORK_DATA_NODE_TO_NODE_SEND_CURRENT_BLOCK_VERIFIERS_LIST",
    "BLOCK_VERIFIERS_TO_NETWORK_DATA_NODE_BLOCK_VERIFIERS_CURRENT_TIME",
    "NETWORK_DATA_NODE_TO_BLOCK_VERIFIERS_BLOCK_VERIFIERS_CURRENT_TIME",
    "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_ONLINE_STATUS",
    "NODE_TO_BLOCK_VERIFIERS_CHECK_IF_CURRENT_BLOCK_VERIFIER",
    "BLOCK_VERIFIERS_TO_NODE_SEND_RESERVE_BYTES",
    "NETWORK_DATA_NODES_TO_NETWORK_DATA_NODES_DATABASE_SYNC_CHECK",
    "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_SYNC_CHECK_ALL_UPDATE",
    "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_SYNC_CHECK_ALL_DOWNLOAD",
    "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_SYNC_CHECK_DOWNLOAD",
    "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_DOWNLOAD_FILE_UPDATE",
    "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_DOWNLOAD_FILE_DOWNLOAD",
    "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_SYNC_CHECK_ALL_UPDATE",
    "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_SYNC_CHECK_ALL_DOWNLOAD",
    "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_DOWNLOAD_FILE_UPDATE",
    "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_DOWNLOAD_FILE_DOWNLOAD",
    "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_SYNC_CHECK_UPDATE",
    "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_SYNC_CHECK_DOWNLOAD",
    "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_DOWNLOAD_FILE_UPDATE",
    "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_DOWNLOAD_FILE_DOWNLOAD",
    "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_SYNC_CHECK_UPDATE",
    "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_SYNC_CHECK_DOWNLOAD",
    "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_DOWNLOAD_FILE_UPDATE",
    "BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_DOWNLOAD_FILE_DOWNLOAD",
    "XCASH_GET_SYNC_INFO",
    "XCASH_GET_BLOCK_PRODUCERS",
    "XCASH_GET_BLOCK_HASH",
};

const xcash_msg_t WALLET_SIGN_MESSAGES[] = {
    XMSG_NODE_TO_NETWORK_DATA_NODES_GET_PREVIOUS_CURRENT_NEXT_BLOCK_VERIFIERS_LIST,
    XMSG_NODE_TO_BLOCK_VERIFIERS_ADD_RESERVE_PROOF,
    XMSG_NODES_TO_BLOCK_VERIFIERS_REGISTER_DELEGATE,
    XMSG_NODES_TO_BLOCK_VERIFIERS_UPDATE_DELEGATE,
    XMSG_NONE
};
const size_t WALLET_SIGN_MESSAGES_COUNT = ARRAY_SIZE(WALLET_SIGN_MESSAGES) - 1;

const xcash_msg_t UNSIGNED_MESSAGES[] = {
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_ONLINE_STATUS,
    XMSG_GET_CURRENT_BLOCK_HEIGHT,
    XMSG_XCASH_GET_SYNC_INFO,
    XMSG_NONE
};
const size_t UNSIGNED_MESSAGES_COUNT = ARRAY_SIZE(UNSIGNED_MESSAGES) - 1;

const xcash_msg_t NONRETURN_MESSAGES[] = {
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_ONLINE_STATUS,
    XMSG_NETWORK_DATA_NODES_TO_NETWORK_DATA_NODES_DATABASE_SYNC_CHECK,
    XMSG_SEND_CURRENT_BLOCK_HEIGHT,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_VRF_DATA,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_BLOCK_BLOB_SIGNATURE,
    XMSG_NODES_TO_NODES_VOTE_RESULTS,
    XMSG_NODES_TO_NODES_VOTE_MAJORITY_RESULTS,
    XMSG_MAIN_NODES_TO_NODES_PART_4_OF_ROUND_CREATE_NEW_BLOCK,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_INVALID_RESERVE_PROOFS,
    XMSG_MAIN_NETWORK_DATA_NODE_TO_BLOCK_VERIFIERS_START_BLOCK,
    XMSG_NONE
};
const size_t NONRETURN_MESSAGES_COUNT = ARRAY_SIZE(NONRETURN_MESSAGES) - 1;

const xcash_msg_t xcash_db_sync_messages[] = {
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_SYNC_CHECK_UPDATE,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_SYNC_CHECK_UPDATE,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_SYNC_CHECK_ALL_UPDATE,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_SYNC_CHECK_ALL_UPDATE
};

const xcash_msg_t xcash_db_download_messages[] = {
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_DELEGATES_DATABASE_DOWNLOAD_FILE_UPDATE,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_STATISTICS_DATABASE_DOWNLOAD_FILE_UPDATE,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_PROOFS_DATABASE_DOWNLOAD_FILE_UPDATE,
    XMSG_BLOCK_VERIFIERS_TO_BLOCK_VERIFIERS_RESERVE_BYTES_DATABASE_DOWNLOAD_FILE_UPDATE
};

// Checks if a message is unsigned
bool is_unsigned_type(xcash_msg_t msg) {
    for (size_t i = 0; i < UNSIGNED_MESSAGES_COUNT; i++) {
        if (msg == UNSIGNED_MESSAGES[i]) {
            return true;
        }
    }
    return false;
}

// Checks if a message requires wallet signature
bool is_walletsign_type(xcash_msg_t msg) {
    for (size_t i = 0; i < WALLET_SIGN_MESSAGES_COUNT; i++) {
        if (msg == WALLET_SIGN_MESSAGES[i]) {
            return true;
        }
    }
    return false;
}

// Checks if a message does not require a return (nonblocking)
bool is_nonreturn_type(xcash_msg_t msg) {
    for (size_t i = 0; i < NONRETURN_MESSAGES_COUNT; i++) {
        if (msg == NONRETURN_MESSAGES[i]) {
            return true;
        }
    }
    return false;
}

// Sign a message in message_buf using local node's private key
bool sign_message(char* message_buf, size_t message_buf_size) {
    (void)message_buf_size;  // Currently unused
    int result = sign_data(message_buf); // sign_data presumably modifies message_buf
    return (result == 1);
}

// Sign a message in message_buf using wallet's private key
bool sign_message_by_wallet(char* message_buf, size_t message_buf_size) {
    (void)message_buf_size;  // Currently unused
    int result = sign_data(message_buf);
    return (result == 1);
}

// Create a message with key-value parameters
char* create_message_param_list(xcash_msg_t msg, const char** pair_params) {
    char message_buf[BUFFER_SIZE];
    memset(message_buf, 0, sizeof(message_buf));

    snprintf(message_buf, sizeof(message_buf), "{\r\n \"message_settings\": \"%s\"", xcash_net_messages[msg]);
    int message_offset = (int)strlen(message_buf);

    size_t current_pair_index = 0;
    while (pair_params[current_pair_index]) {
        const char* param_key = pair_params[current_pair_index++];
        const char* param_value = pair_params[current_pair_index++];
        if (!param_value) {
            DEBUG_PRINT("Mismatched param key/value for message %s", xcash_net_messages[msg]);
            break;
        }
        snprintf(message_buf + message_offset, sizeof(message_buf) - message_offset,
                 ",\r\n \"%s\": \"%s\"", param_key, param_value);
        message_offset = (int)strlen(message_buf);
    }

    strncat(message_buf + message_offset, ",\r\n}", sizeof(message_buf) - strlen(message_buf) - 1);

    // If message is signed
    if (!is_unsigned_type(msg)) {
        if (!sign_message(message_buf, sizeof(message_buf))) {
            ERROR_PRINT("Failed to sign message: %s", xcash_net_messages[msg]);
            return NULL;
        }
    }

    return strdup(message_buf);
}

// Create a message from variadic arguments
char* create_message_args(xcash_msg_t msg, va_list args) {
    // Calculate how many key/value pairs
    va_list args_copy;
    va_copy(args_copy, args);

    size_t param_count = 0;
    while (va_arg(args_copy, char*)) {
        param_count++;
        // skip the next param in pair
        va_arg(args_copy, char*);
    }
    va_end(args_copy);

    // param_count x 2 but we store them in sequence. +1 for sentinel
    param_count = param_count * 2 + 1;

    const char** param_list = calloc(param_count, sizeof(char*));
    if (!param_list) {
        return NULL;
    }

    size_t index = 0;
    while (true) {
        char *key = va_arg(args, char*);
        if (!key) {
            break;
        }
        char *value = va_arg(args, char*);
        param_list[index++] = key;
        param_list[index++] = value;
    }
    param_list[index] = NULL;

    char* message = create_message_param_list(msg, param_list);
    free(param_list);
    return message;
}

// Create message with variadic parameters (exposed)
char* create_message_param(xcash_msg_t msg, ...) {
    va_list args;
    va_start(args, msg);
    char* message = create_message_args(msg, args);
    va_end(args);
    return message;
}

// Create a basic message with no parameters
char* create_message(xcash_msg_t msg) {
    return create_message_param(msg, NULL);
}

// Splits a string by delimiter, returning array of elements
int split(const char* str, char delimiter, char*** result_elements) {
    if (!str || !result_elements) return -1;

    int elemCount = 1;
    for (int i = 0; str[i]; i++) {
        if (str[i] == delimiter) {
            elemCount++;
        }
    }

    char** result = calloc(elemCount + 1, sizeof(char*)); // +1 for sentinel
    if (!result) return -1;

    int startIdx = 0, endIdx = 0, rIndex = 0;
    for (int i = 0; ; i++) {
        if (str[i] == delimiter || str[i] == '\0') {
            int length = i - startIdx;
            result[rIndex] = malloc(length + 1);
            if (!result[rIndex]) {
                // free previously allocated
                for (int k = 0; k < rIndex; k++) {
                    free(result[k]);
                }
                free(result);
                return -1;
            }
            strncpy(result[rIndex], &str[startIdx], length);
            result[rIndex][length] = '\0';
            rIndex++;
            startIdx = i + 1;

            if (str[i] == '\0') {
                break;
            }
        }
    }
    *result_elements = result;
    return rIndex; // Number of elements
}

// Frees array created by split()
void cleanup_char_list(char **element_list) {
    if (!element_list) return;
    for (int i = 0; element_list[i]; i++) {
        free(element_list[i]);
    }
    free(element_list);
}