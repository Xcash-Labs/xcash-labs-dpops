#include "string_functions.h"

/*---------------------------------------------------------------------------------------------------------
Name: parse_json_data
Description: Parses JSON data safely using cJSON, supporting nested field extraction.
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
        DEBUG_PRINT("Invalid parameters");
        return XCASH_ERROR;
    }

    DEBUG_PRINT("Raw JSON: %s", data);

    // Attempt to parse JSON
    cJSON *json = cJSON_Parse(data);
    if (!json) {
        const char *error_ptr = cJSON_GetErrorPtr();
        DEBUG_PRINT("JSON parsing error near: %s", error_ptr ? error_ptr : "unknown location");
        return XCASH_ERROR;
    }

    // First, look for "result" object
    cJSON *result_obj = cJSON_GetObjectItemCaseSensitive(json, "result");
    if (!result_obj || !cJSON_IsObject(result_obj)) {
        DEBUG_PRINT("Field 'result' not found in JSON or is not an object");
        cJSON_Delete(json);
        return XCASH_ERROR;
    }

    // Now search inside "result" for the requested field
    cJSON *field = cJSON_GetObjectItemCaseSensitive(result_obj, field_name);
    if (!field) {
        DEBUG_PRINT("Field '%s' not found in 'result' JSON object", field_name);
        cJSON_Delete(json);
        return XCASH_ERROR;
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