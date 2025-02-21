#include "string_functions.h"

/*---------------------------------------------------------------------------------------------------------
Name: parse_json_data
Description: Parses JSON data safely using cJSON, supporting nested fields.
Parameters:
  - data: The JSON-formatted string.
  - field_name: The field to extract (supports "parent.child" for nested fields).
  - result: Output buffer to store extracted value.
  - result_size: The size of the output buffer.
Return:
  - XCASH_OK (1) if successful.
  - XCASH_ERROR (0) if an error occurred.
---------------------------------------------------------------------------------------------------------*/
int parse_json_data(const char *data, const char *field_name, char *result, size_t result_size) {
    if (!data || !field_name || !result) {
        DEBUG_PRINT("Invalid parameters");
        return XCASH_ERROR;
    }

    // Debug raw JSON
    DEBUG_PRINT("Raw JSON: %s", data);

    // Attempt to parse JSON
    cJSON *json = cJSON_Parse(data);
    if (!json) {
        const char *error_ptr = cJSON_GetErrorPtr();
        DEBUG_PRINT("JSON parsing error near: %s", error_ptr ? error_ptr : "unknown location");
        return XCASH_ERROR;
    }

    cJSON *field = NULL;

    // Handle nested fields like "result.count"
    char field_copy[256];
    strncpy(field_copy, field_name, sizeof(field_copy) - 1);
    field_copy[sizeof(field_copy) - 1] = '\0';  // Ensure null termination

    char *token = strtok(field_copy, ".");
    cJSON *current = json;

    while (token) {
        field = cJSON_GetObjectItemCaseSensitive(current, token);
        if (!field) {
            DEBUG_PRINT("Field '%s' not found in JSON", token);
            cJSON_Delete(json);
            return XCASH_ERROR;
        }
        current = field;
        token = strtok(NULL, ".");
    }

    // Extract and store the field value
    if (cJSON_IsString(field) && field->valuestring) {
        strncpy(result, field->valuestring, result_size - 1);
        result[result_size - 1] = '\0';  // Ensure null termination
    } else if (cJSON_IsNumber(field)) {
        snprintf(result, result_size, "%d", field->valueint);
    } else {
        DEBUG_PRINT("Field '%s' has unsupported data type", field_name);
        cJSON_Delete(json);
        return XCASH_ERROR;
    }

    cJSON_Delete(json);
    return XCASH_OK;
}