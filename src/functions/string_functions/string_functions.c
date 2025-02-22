#include "string_functions.h"

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
    if (slen == 0) return; // Prevent infinite loops

    size_t data_len = strlen(data);
    size_t max_possible_size = data_len + (rlen - slen) * 10; // Assume max 10 replacements
    if (max_possible_size > DATA_TOTAL_LENGTH) {
        ERROR_PRINT("Buffer too small for replacements");
        return;
    }

    char *buf = calloc(max_possible_size + 1, sizeof(char)); // +1 for null-terminator
    if (!buf) {
        ERROR_PRINT("Memory allocation failed in string_replace");
        return;
    }

    char *b = data;
    char *find;
    size_t buf_len = 0;

    while ((find = strstr(b, STR1)) != NULL) {   
        // Copy everything up to occurrence
        size_t segment_len = find - b;
        memcpy(buf + buf_len, b, segment_len);
        buf_len += segment_len;

        // Copy the replacement string
        memcpy(buf + buf_len, STR2, rlen);
        buf_len += rlen;

        // Move past the found substring
        b = find + slen;
    }

    strcpy(buf + buf_len, b);
    snprintf(data, DATA_TOTAL_LENGTH, "%s", buf);
    free(buf);
}