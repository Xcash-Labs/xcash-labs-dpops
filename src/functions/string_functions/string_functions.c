#include "string_functions.h"

/*---------------------------------------------------------------------------------------------------------
/// @brief Converts a hexadecimal string to a byte array.
/// @param hex_string The input hex string.
/// @param byte_array The output byte array.
/// @param byte_array_size The size of the output array.
/// @return 1 if successful, 0 if an error occurred.
---------------------------------------------------------------------------------------------------------*/
bool hex_to_byte_array(const char *hex_string, unsigned char *byte_array, size_t byte_array_size) {
    if (!hex_string || !byte_array) {
        ERROR_PRINT("Hex string can not be null.");
        return XCASH_ERROR;
    }

    size_t hex_length = strlen(hex_string);
    
    if (hex_length % 2 != 0) {
        ERROR_PRINT("Hex string must be even length.");
        return XCASH_ERROR;
    }

    size_t expected_bytes = hex_length / 2;
    if (expected_bytes > byte_array_size) {
        ERROR_PRINT("Buffer is too small for new string.");
        return XCASH_ERROR;
    }

    for (size_t i = 0; i < expected_bytes; i++) {
        char byte_chars[3] = {hex_string[i * 2], hex_string[i * 2 + 1], '\0'};
        
        if (!isxdigit(byte_chars[0]) || !isxdigit(byte_chars[1])) {
            ERROR_PRINT("Invalid hex character.");
            return XCASH_ERROR; // Invalid hex character
        }

        byte_array[i] = (unsigned char)strtol(byte_chars, NULL, 16);
    }

    return XCASH_OK; // Success
}

/*---------------------------------------------------------------------------------------------------------
Name: parse_json_data
Description: Parses JSON data safely using cJSON, supporting both root-level and "result" fields.
Parameters:
  - data: The JSON-formatted string.
  - field_name: The field to extract (can be at root level or inside "result").
  - result: Output buffer to store extracted value.
  - result_size: The size of the output buffer.
Return:
  - XCASH_OK (1) if successful.
  - XCASH_ERROR (0) if an error occurred.
---------------------------------------------------------------------------------------------------------*/
int parse_json_dataxxx(const char *data, const char *field_name, char *result, size_t result_size) {
    if (!data || !field_name || !result) {
        ERROR_PRINT("Invalid parameters");
        return XCASH_ERROR;
    }

    // Attempt to parse JSON
    cJSON *json = cJSON_Parse(data);
    if (!json) {
        const char *error_ptr = cJSON_GetErrorPtr();
        ERROR_PRINT("JSON parsing error near: %s", error_ptr ? error_ptr : "unknown location");
        return XCASH_ERROR;
    }

    cJSON *field = cJSON_GetObjectItemCaseSensitive(json, field_name);

    // If field is not found at root level, search inside "result"
    if (!field) {
        cJSON *result_obj = cJSON_GetObjectItemCaseSensitive(json, "result");
        if (!result_obj || !cJSON_IsObject(result_obj)) {
            ERROR_PRINT("Field 'result' not found in JSON or is not an object");
            DEBUG_PRINT("Raw JSON: %s", data);
            cJSON_Delete(json);
            return XCASH_ERROR;
        }

        field = cJSON_GetObjectItemCaseSensitive(result_obj, field_name);
    }

    if (!field) {
        ERROR_PRINT("Field '%s' not found in JSON", field_name);
        DEBUG_PRINT("Parsed JSON structure: %s", cJSON_PrintUnformatted(json)); // Debug parsed JSON
        cJSON_Delete(json);
        return XCASH_ERROR;
    }

    // Extract and store the field value
    if (cJSON_IsString(field) && field->valuestring) {
        strncpy(result, field->valuestring, result_size - 1);
        result[result_size - 1] = '\0';  // Ensure null termination
    } else if (cJSON_IsNumber(field)) {
        snprintf(result, result_size, "%.6f", field->valuedouble);  // Supports both ints & floats
    } else {
        ERROR_PRINT("Field '%s' has unsupported data type", field_name);
        cJSON_Delete(json);
        return XCASH_ERROR;
    }

    cJSON_Delete(json);
    return XCASH_OK;
}



int parse_json_data(const char *data, const char *field_name, char *result, size_t result_size) {
    if (!data || !field_name || !result) {
        ERROR_PRINT("Invalid parameters");
        return XCASH_ERROR;
    }

    // Attempt to parse JSON
    cJSON *json = cJSON_Parse(data);
    if (!json) {
        const char *error_ptr = cJSON_GetErrorPtr();
        ERROR_PRINT("JSON parsing error near: %s", error_ptr ? error_ptr : "unknown location");
        return XCASH_ERROR;
    }

    // Handle nested JSON paths like "result.block_header.hash"
    char path_copy[256];
    strncpy(path_copy, field_name, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';  // Ensure null termination

    cJSON *current_obj = json;
    char *token = strtok(path_copy, ".");
    while (token != NULL) {
        current_obj = cJSON_GetObjectItemCaseSensitive(current_obj, token);
        if (!current_obj) {
            ERROR_PRINT("Field '%s' not found in JSON", field_name);
            DEBUG_PRINT("Parsed JSON structure: %s", cJSON_PrintUnformatted(json));
            cJSON_Delete(json);
            return XCASH_ERROR;
        }
        token = strtok(NULL, ".");
    }

    // Extract and store the field value
    if (cJSON_IsString(current_obj) && current_obj->valuestring) {
        strncpy(result, current_obj->valuestring, result_size - 1);
        result[result_size - 1] = '\0';  // Ensure null termination
    } else if (cJSON_IsNumber(current_obj)) {
        snprintf(result, result_size, "%.6f", current_obj->valuedouble);  // Supports both ints & floats
    } else {
        ERROR_PRINT("Field '%s' has unsupported data type", field_name);
        cJSON_Delete(json);
        return XCASH_ERROR;
    }

    cJSON_Delete(json);
    return XCASH_OK;
}

/*---------------------------------------------------------------------------------------------------------
Name: string_replace
Description: String replace
Parameters:
  data - The string to replace the data
  DATA_TOTAL_LENGTH - The maximum size of data
  STR1 - The string to be replaced
  STR2 - The string to replace the other string
Return: The result string
---------------------------------------------------------------------------------------------------------*/
void string_replace(char *data, const size_t DATA_TOTAL_LENGTH, const char* STR1, const char* STR2) { 
    if (!data || !STR1 || !STR2) {
        ERROR_PRINT("Invalid input to string_replace");
        return;
    }

    size_t slen = strlen(STR1);
    size_t rlen = strlen(STR2);

    if (slen == 0) {
        ERROR_PRINT("Empty search string in string_replace()");
        return;
    }

    size_t data_len = strlen(data);
    size_t occurrence_count = 0;
    char* temp = data;
    while ((temp = strstr(temp, STR1)) != NULL) {
        occurrence_count++;
        temp += slen;
    }

    size_t max_possible_size = data_len + occurrence_count * (rlen > slen ? (rlen - slen) : 0);
    if (max_possible_size >= DATA_TOTAL_LENGTH) {
        ERROR_PRINT("Buffer too small for replacements");
        return;
    }

    char *buf = calloc(max_possible_size + 1, sizeof(char)); // +1 for null-terminator
    if (!buf) {
        ERROR_PRINT("Memory allocation failed in string_replace()");
        return;
    }

    char *b = data;
    char *find;
    size_t buf_len = 0;
    while ((find = strstr(b, STR1)) != NULL) {   
        size_t segment_len = find - b;
        memcpy(buf + buf_len, b, segment_len);
        buf_len += segment_len;

        memcpy(buf + buf_len, STR2, rlen);
        buf_len += rlen;

        b = find + slen;
    }

    strcpy(buf + buf_len, b);
    snprintf(data, DATA_TOTAL_LENGTH - 1, "%s", buf);
    data[DATA_TOTAL_LENGTH - 1] = '\0';
    free(buf);
    buf = NULL;
}

/*---------------------------------------------------------------------------------------------------------
Name: random_string
Description: Creates a random string of specified length
Parameters:
  result - The string where you want the random string to be saved to
  LENGTH - The length of the random string
Return: 0 if an error has occured, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int random_string(char *result, const size_t LENGTH) {

    const size_t string_len = sizeof(ALPHANUM_STRING) - 1;   // Length of ALPHANUM_STRING

    if (!result)
    {
        ERROR_PRINT("ERROR: random_string() received NULL pointer for 'result'\n");
        return XCASH_ERROR;
    }
    if (LENGTH == 0)
    {
        ERROR_PRINT("ERROR: random_string() received LENGTH = 0\n");
        return XCASH_ERROR;
    }

    memset(result, 0, LENGTH + 1);
    unsigned char random_bytes[LENGTH];
    size_t generated = 0;
    ssize_t bytes_read = getrandom(random_bytes, LENGTH, 0);
    if (bytes_read < 0) {
        ERROR_PRINT("getrandom() failed");
        return XCASH_ERROR;
    }

    // Convert random bytes into allowed characters from `ALPHANUM_STRING`
    for (generated = 0; generated < LENGTH; generated++) {
        result[generated] = ALPHANUM_STRING[random_bytes[generated] % string_len];
    }

    result[LENGTH] = '\0'; 
    return (generated == LENGTH) ? XCASH_OK : XCASH_ERROR;
}

/*---------------------------------------------------------------------------------------------------------
Name: string_count
Description: Counts the occurrences of a substring in a string.
Parameters:
  DATA - The string to count the occurrence in.
  STRING - The substring to count the occurrences of.
Return: The number of occurrences of the substring in the string, otherwise 0 if an error has occurred.
---------------------------------------------------------------------------------------------------------*/
size_t string_count(const char* DATA, const char* STRING)
{
    // Validate inputs
    if (DATA == NULL || STRING == NULL || *STRING == '\0') {
        return 0;  // Return 0 if either string is NULL or STRING is empty
    }
    const size_t DATA_LENGTH = strlen(DATA);
    const size_t STRING_LENGTH = strlen(STRING);
    if (STRING_LENGTH > DATA_LENGTH) {
        return 0;
    }
    size_t count = 0;
    const char* pos = DATA;
    while ((pos = strstr(pos, STRING)) != NULL) {
        count++;
        pos += STRING_LENGTH;  // Move past the last found substring
    }
    return count;
}


void bin_to_hex(const unsigned char *bin_data, int data_size, char *buf)
{
    static const char hex[] = "0123456789abcdef";  // `static` to avoid reallocation on each call

    if (!bin_data || !buf || data_size <= 0) {  // Validate inputs
        if (buf) *buf = '\0';  // Ensure buf is null-terminated if it's not NULL
        return;
    }

    for (int i = 0; i < data_size; ++i) {
        buf[i * 2]     = hex[(bin_data[i] >> 4) & 0xF];
        buf[i * 2 + 1] = hex[bin_data[i] & 0xF];
    }
    buf[data_size * 2] = '\0';  // Null-terminate the string
}

void md5_hex(const char *src, char *dest)
{
    if (!src || !dest) {  // Validate inputs
        if (dest) *dest = '\0';  // Ensure dest is null-terminated if it's not NULL
        return;
    }

    unsigned char md5_bin[MD5_DIGEST_LENGTH];  // Use MD5_DIGEST_LENGTH for clarity and safety

    MD5((const unsigned char *)src, strlen(src), md5_bin);  // Combines Init, Update, and Final

    bin_to_hex(md5_bin, MD5_DIGEST_LENGTH, dest);  // Convert binary MD5 to hex string
}