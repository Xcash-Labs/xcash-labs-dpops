#ifndef DEFINE_MACRO_FUNCTIONS_H_ /* Include guard */
#define DEFINE_MACRO_FUNCTIONS_H_

#include <signal.h>

#define RED_TEXT(text) "\033[31m" text "\033[0m"
#define ORANGE_TEXT(text) "\033[38;5;208m" text "\033[0m"
#define YELLOW_TEXT(text) "\033[1;33m" text "\033[0m"
#define GREEN_TEXT(text) "\x1b[32m" text "\x1b[0m"
#define BLUE_TEXT(text) "\033[34m" text "\033[0m"
#define LIGHT_BLUE_TEXT(text) "\033[94m" text "\033[0m"
#define PURPLE_TEXT(text) "\033[35m" text "\033[0m"
#define LIGHT_PURPLE_TEXT(text) "\033[95m" text "\033[0m"
#define WHITE_TEXT(text) "\033[97m" text "\033[0m"
#define BRIGHT_WHITE_TEXT(text) "\033[1;97m" text "\033[0m"
#define INFO_STATUS_OK "\t[" GREEN_TEXT("OK") "]"
#define INFO_STATUS_FAIL "\t[" RED_TEXT("X") "]"

// Define log levels

#define LOG_LEVEL_DEBUG 4
#define LOG_LEVEL_INFO 3
#define LOG_LEVEL_WARNING 2
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_CRITICAL 0

#define DEBUG_PRINT(fmt, ...) do { \
    if (log_level >= LOG_LEVEL_DEBUG) { \
        time_t raw_time = time(NULL); \
        struct tm *tm_info = localtime(&raw_time); \
        char time_buf[20]; \
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info); \
        fprintf(stderr, "\033[1;35m[%s] DEBUG: ", time_buf); \
        fprintf(stderr, fmt, ##__VA_ARGS__); \
        fprintf(stderr, "\033[0m\n  --> TRACE: %s:%d, %s()\n\n", __FILE__, __LINE__, __func__); \
    } \
} while (0)

#define INFO_PRINT(fmt, ...) do { \
    if (log_level >= LOG_LEVEL_INFO) { \
        time_t raw_time = time(NULL); \
        struct tm *tm_info = localtime(&raw_time); \
        char time_buf[20]; \
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info); \
        fprintf(stderr, "\033[1;37m[%s] INFO: ", time_buf); \
        fprintf(stderr, fmt, ##__VA_ARGS__); \
        fprintf(stderr, "\n\n"); \
    } \
} while (0)

#define WARNING_PRINT(fmt, ...) do { \
    if (log_level >= LOG_LEVEL_WARNING) { \
        time_t raw_time = time(NULL); \
        struct tm *tm_info = localtime(&raw_time); \
        char time_buf[20]; \
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info); \
        fprintf(stderr, "\033[1;33m[%s] WARNING: ", time_buf); \
        fprintf(stderr, fmt, ##__VA_ARGS__); \
        fprintf(stderr, "\033[0m\n  --> TRACE: %s:%d, %s()\n\n", __FILE__, __LINE__, __func__); \
    } \
} while (0)


#define ERROR_PRINT(fmt, ...) do { \
    if (log_level >= LOG_LEVEL_ERROR) { \
        time_t raw_time = time(NULL); \
        struct tm *tm_info = localtime(&raw_time); \
        char time_buf[20]; \
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info); \
        fprintf(stderr, "\033[1;31m[%s] ERROR: ", time_buf); \
        fprintf(stderr, fmt, ##__VA_ARGS__); \
        fprintf(stderr, "\033[0m\n  --> TRACE: %s:%d, %s()\n\n", __FILE__, __LINE__, __func__); \
    } \
} while (0)

#define FATAL_ERROR_EXIT(fmt, ...) do { \
    if (log_level >= LOG_LEVEL_CRITICAL) { \
        time_t raw_time = time(NULL); \
        struct tm *tm_info = localtime(&raw_time); \
        char time_buf[20]; \
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info); \
        fprintf(stderr, "\033[1;31m[%s] FATAL: " fmt "\033[0m\n", time_buf, ##__VA_ARGS__); \
        fflush(NULL); \
        signal(SIGTERM, SIG_DFL); \
        sigset_t _set; sigemptyset(&_set); sigaddset(&_set, SIGTERM); \
        sigprocmask(SIG_UNBLOCK, &_set, NULL); \
        if (raise(SIGTERM) != 0) { \
            kill(getpid(), SIGTERM); \
        } \
        _exit(128 + SIGTERM); \
    } \
} while (0)

#define INFO_PRINT_STATUS_OK(fmt, ...) do { \
    if (log_level >= LOG_LEVEL_INFO) { \
        time_t raw_time = time(NULL); \
        struct tm *tm_info = localtime(&raw_time); \
        char time_buf[20]; \
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info); \
        fprintf(stderr, BRIGHT_WHITE_TEXT("[%s] INFO: " fmt INFO_STATUS_OK "\n"), time_buf, ##__VA_ARGS__); \
    } \
} while (0)

#define INFO_PRINT_STATUS_FAIL(fmt, ...) do { \
    if (log_level >= LOG_LEVEL_INFO) { \
        time_t raw_time = time(NULL); \
        struct tm *tm_info = localtime(&raw_time); \
        char time_buf[20]; \
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info); \
        fprintf(stderr, BRIGHT_WHITE_TEXT("[%s] INFO: " fmt INFO_STATUS_FAIL "\n"), time_buf, ##__VA_ARGS__); \
    } \
} while (0)

#define INFO_STAGE_PRINT(fmt, ...) do { \
    if (log_level >= LOG_LEVEL_INFO) { \
        time_t raw_time = time(NULL); \
        struct tm *tm_info = localtime(&raw_time); \
        char time_buf[20]; \
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info); \
        fprintf(stderr, BRIGHT_WHITE_TEXT("\n\n[%s] INFO: ") LIGHT_BLUE_TEXT(fmt) "\n", time_buf, ##__VA_ARGS__); \
    } \
} while (0)

#define HOST_OK_STATUS(host, fmt, ...) BLUE_TEXT(host)" "fmt"\t["GREEN_TEXT("OK")"]"
#define HOST_FALSE_STATUS(host, fmt, ...) BLUE_TEXT(host)" "fmt"\t["RED_TEXT("X")"]"

#define COLOR_PRINT(string, color)                            \
    do                                                        \
    {                                                         \
        const char *color_code = "";                          \
        if (strcmp(color, "red") == 0)                        \
            color_code = "\033[1;31m";                        \
        else if (strcmp(color, "green") == 0)                 \
            color_code = "\033[1;32m";                        \
        else if (strcmp(color, "yellow") == 0)                \
            color_code = "\033[1;33m";                        \
        else if (strcmp(color, "blue") == 0)                  \
            color_code = "\033[1;34m";                        \
        else if (strcmp(color, "purple") == 0)                \
            color_code = "\033[1;35m";                        \
        else if (strcmp(color, "lightblue") == 0)             \
            color_code = "\033[1;36m";                        \
        fprintf(stderr, "%s%s\033[0m\n", color_code, string); \
    } while (0)

#define SERVER_ERROR(rmess)                                     \
  do {                                                          \
    send_data(client, (unsigned char *)(rmess), strlen(rmess)); \
    return;                                                     \
  } while (0)

#endif