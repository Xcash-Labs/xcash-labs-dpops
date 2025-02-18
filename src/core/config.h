#ifndef DEFINE_CONFIG_H_   /* Include guard */
#define CONFIG_H_

#include <stdbool.h>

#define XCASH_DPOPS_CURRENT_VERSION "xCash Labs DPoPs V. 2.0.0"
#define XCASH_DAEMON_PORT 18281 // The X-CASH Daemon RPC port
#define XCASH_WALLET_PORT 18285 // The X-CASH Wallet RPC port
#define XCASH_DPOPS_PORT 18283 // The X-CASH Dpops service
#define DATABASE_CONNECTION "mongodb://127.0.0.1:27017" // The database connection string

#define BLOCK_VERIFIERS_IP_ADDRESS_TOTAL_LENGTH 100 // The maximum length of the block verifiers IP address
#define XCASH_WALLET_LENGTH 98 // The length of a XCA address
#define VRF_SECRET_KEY_LENGTH 128 // Length of VRF Secret Key

#define IP_LENGTH 39 // Length of ip address for IPv4 and IPv6
#define LOG_BUFFER_LEN 1024 //Length of log buffer

#define RED_TEXT(text) "\033[31m"text"\033[0m"
#define YELLOW_TEXT(text) "\033[1;33m"text"\033[0m"
#define GREEN_TEXT(text) "\x1b[32m"text"\x1b[0m"
#define BRIGHT_WHITE_TEXT(text) "\033[1;97m"text"\033[0m"


#define __DEBUG_PRINT_FUNC_CALLER if (debug_settings)fprintf(stderr, "  --> TRACE: %s:%d, %s()\n", __FILE__, __LINE__, __func__);
#define INFO_STAGE_PRINT(fmt, ...) fprintf(stderr, BRIGHT_WHITE_TEXT("\n\nINFO: ")LIGHT_BLUE_TEXT(fmt)"\n\n", ##__VA_ARGS__); __DEBUG_PRINT_FUNC_CALLER
#define INFO_PRINT(fmt, ...) fprintf(stderr, BRIGHT_WHITE_TEXT("INFO: ")fmt"\n", ##__VA_ARGS__); __DEBUG_PRINT_FUNC_CALLER
#define WARNING_PRINT(fmt, ...) fprintf(stderr, ORANGE_TEXT("WARNING: ")fmt"\n", ##__VA_ARGS__); __DEBUG_PRINT_FUNC_CALLER
#define ERROR_PRINT(fmt, ...) fprintf(stderr, RED_TEXT("ERROR: ")fmt"\n", ##__VA_ARGS__); __DEBUG_PRINT_FUNC_CALLER
#define DEBUG_PRINT(fmt, ...) if (debug_settings)fprintf(stderr, PURPLE_TEXT("DEBUG: ")fmt"\n", ##__VA_ARGS__); __DEBUG_PRINT_FUNC_CALLER
#define FATAL_ERROR_EXIT(fmt, ...) fprintf(stderr, RED_TEXT("FATAL: ")fmt"\n", ##__VA_ARGS__); __DEBUG_PRINT_FUNC_CALLER; exit(1)
#define INFO_PRINT_STATUS_OK(fmt, ...) fprintf(stderr, BRIGHT_WHITE_TEXT("INFO: ")fmt INFO_STATUS_OK"\n", ##__VA_ARGS__); __DEBUG_PRINT_FUNC_CALLER
#define INFO_PRINT_STATUS_FAIL(fmt, ...) fprintf(stderr, BRIGHT_WHITE_TEXT("INFO: ")fmt INFO_STATUS_FAIL"\n", ##__VA_ARGS__); __DEBUG_PRINT_FUNC_CALLER
#endif