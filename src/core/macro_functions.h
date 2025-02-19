#ifndef DEFINE_MACRO_FUNCTIONS_H_   /* Include guard */
#define DEFINE_MACRO_FUNCTIONS_H_

#define RED_TEXT(text) "\033[31m"text"\033[0m"
#define YELLOW_TEXT(text) "\033[1;33m"text"\033[0m"
#define GREEN_TEXT(text) "\x1b[32m"text"\x1b[0m"
#define BRIGHT_WHITE_TEXT(text) "\033[1;97m"text"\033[0m"

#define __DEBUG_PRINT_FUNC_CALLER if (debug_enabled)fprintf(stderr, "  --> TRACE: %s:%d, %s()\n\n", __FILE__, __LINE__, __func__);
#define FATAL_ERROR_EXIT(fmt, ...) do { fprintf(stderr, "\033[1;31mFATAL: " fmt "\033[0m\n", ##__VA_ARGS__); __DEBUG_PRINT_FUNC_CALLER; exit(1); } while (0)
#define DEBUG_PRINT(fmt, ...) do { if (debug_enabled)fprintf(stderr, PURPLE_TEXT("DEBUG: ")fmt"\n", ##__VA_ARGS__); __DEBUG_PRINT_FUNC_CALLER; } while (0)

// Not sure which ones I will keep
#define INFO_PRINT(fmt, ...) do { fprintf(stderr, BRIGHT_WHITE_TEXT("INFO: ") fmt "\n", ##__VA_ARGS__); __DEBUG_PRINT_FUNC_CALLER; } while (0)
#define INFO_STAGE_PRINT(fmt, ...) do { fprintf(stderr, BRIGHT_WHITE_TEXT("\n\nINFO: ") LIGHT_BLUE_TEXT(fmt) "\n\n", ##__VA_ARGS__); __DEBUG_PRINT_FUNC_CALLER; } while (0)
#define WARNING_PRINT(fmt, ...) do { fprintf(stderr, ORANGE_TEXT("WARNING: ") fmt "\n", ##__VA_ARGS__); __DEBUG_PRINT_FUNC_CALLER; } while (0)
#define ERROR_PRINT(fmt, ...) do { fprintf(stderr, RED_TEXT("ERROR: ") fmt "\n", ##__VA_ARGS__); __DEBUG_PRINT_FUNC_CALLER; } while (0)
#define INFO_PRINT_STATUS_OK(fmt, ...) do { fprintf(stderr, BRIGHT_WHITE_TEXT("INFO: ") fmt INFO_STATUS_OK "\n", ##__VA_ARGS__); __DEBUG_PRINT_FUNC_CALLER; } while (0)
#define INFO_PRINT_STATUS_FAIL(fmt, ...) do { fprintf(stderr, BRIGHT_WHITE_TEXT("INFO: ") fmt INFO_STATUS_FAIL "\n", ##__VA_ARGS__); __DEBUG_PRINT_FUNC_CALLER; } while (0)


#define COLOR_PRINT(string, color) do { \
    const char *color_code = ""; \
    if (strcmp(color, "red") == 0) color_code = "\033[1;31m"; \
    else if (strcmp(color, "green") == 0) color_code = "\033[1;32m"; \
    else if (strcmp(color, "yellow") == 0) color_code = "\033[1;33m"; \
    else if (strcmp(color, "blue") == 0) color_code = "\033[1;34m"; \
    else if (strcmp(color, "purple") == 0) color_code = "\033[1;35m"; \
    else if (strcmp(color, "lightblue") == 0) color_code = "\033[1;36m"; \
    fprintf(stderr, "%s%s\033[0m\n", color_code, string); \
} while (0)

#endif