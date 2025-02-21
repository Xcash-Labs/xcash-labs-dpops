#include "string_functions.h"

/*---------------------------------------------------------------------------------------------------------
Name: parse_json_data
Description: Parses JSON data safely using cJSON
Parameters:
  - data: The JSON-formatted string.
  - field_name: The field to extract.
  - result: Output buffer to store extracted value.
  - result_size: The size of the output buffer.
Return:
  - XCASH_OK (1) if successful.
  - XCASH_ERROR (0) if an error occurred.
---------------------------------------------------------------------------------------------------------*/
int parse_json_data(const char *data, const char *field_name, char *result, size_t result_size) {
    if (!data || !field_name || !result) {
        DEBUG_PRINT("Invalid Parameters");
        return XCASH_ERROR; // Invalid parameters
    }

    cJSON *json = cJSON_Parse(data);
    if (!json) {
        DEBUG_PRINT("JSON parsing failed: %s", data);
        return XCASH_ERROR;
    }

    cJSON *field = cJSON_GetObjectItemCaseSensitive(json, field_name);
    if (!field) {
        DEBUG_PRINT("Field '%s' not found in JSON", field_name);
        cJSON_Delete(json);
        return XCASH_ERROR;
    }

    // Copy the field value to the result buffer
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
