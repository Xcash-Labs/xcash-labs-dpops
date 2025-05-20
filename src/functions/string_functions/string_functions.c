#include "string_functions.h"

static const char* BASE58_ALPHABET = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

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
            ERROR_PRINT("Invalid hex character in input: %c%c", byte_chars[0], byte_chars[1]);
            return XCASH_ERROR; // Invalid hex character
        }

        byte_array[i] = (unsigned char)strtol(byte_chars, NULL, 16);
    }

    return XCASH_OK; // Success
}

/*---------------------------------------------------------------------------------------------------------
Name: bytes_to_hex
Description:
  Converts a binary byte array into a lowercase null-terminated hexadecimal string.
  Each byte is represented by two hex characters (e.g., 0xAB becomes "ab").

Parameters:
  bytes        - Pointer to the input byte array.
  byte_len     - Length of the input byte array in bytes.
  hex_out      - Pointer to the output buffer that will receive the hex string.
  hex_out_len  - Total size of the output buffer. Must be at least (byte_len * 2 + 1).

Return:
  None. The function writes the hex string to hex_out. If inputs are invalid or the buffer is too small,
  the function writes an empty string to hex_out.
---------------------------------------------------------------------------------------------------------*/
void bytes_to_hex(const unsigned char* bytes, size_t byte_len, char* hex_out, size_t hex_out_len) {
    if (!bytes || !hex_out || hex_out_len < (byte_len * 2 + 1)) {
      if (hex_out && hex_out_len > 0) {
        hex_out[0] = '\0';
      }
      return;
    }
  
    for (size_t i = 0; i < byte_len; i++) {
      snprintf(hex_out + (i * 2), hex_out_len - (i * 2), "%02x", bytes[i]);
    }
    hex_out[byte_len * 2] = '\0';
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
int parse_json_data(const char *data, const char *field_name, char *result, size_t result_size) {
    if (!data || !field_name || !result) {
        ERROR_PRINT("Invalid parameters");
        return XCASH_ERROR;
    }

    // Parse JSON
    cJSON *json = cJSON_Parse(data);
    if (!json) {
        const char *error_ptr = cJSON_GetErrorPtr();
        ERROR_PRINT("JSON parsing error near: %s", error_ptr ? error_ptr : "unknown location");
        return XCASH_ERROR;
    }

    // Handle nested JSON paths with array support (e.g., "result.addresses[0].address")
    char path_copy[256];
    strncpy(path_copy, field_name, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    cJSON *current_obj = json;
    char *token = strtok(path_copy, ".");
    while (token != NULL) {
        // Check for array access syntax (e.g., addresses[0])
        char *bracket_pos = strchr(token, '[');
        if (bracket_pos) {
            *bracket_pos = '\0';  // Split field name and index part

            current_obj = cJSON_GetObjectItemCaseSensitive(current_obj, token);
            if (!current_obj || !cJSON_IsArray(current_obj)) {
                ERROR_PRINT("Field '%s' not found or is not an array", token);
                cJSON_Delete(json);
                return XCASH_ERROR;
            }

            // Extract the index
            int index = atoi(bracket_pos + 1);
            current_obj = cJSON_GetArrayItem(current_obj, index);
            if (!current_obj) {
                ERROR_PRINT("Index %d out of range for field '%s'", index, token);
                cJSON_Delete(json);
                return XCASH_ERROR;
            }
        } else {
            current_obj = cJSON_GetObjectItemCaseSensitive(current_obj, token);
            if (!current_obj) {
                ERROR_PRINT("Field '%s' not found in JSON", field_name);
                DEBUG_PRINT("Parsed JSON structure: %s", cJSON_PrintUnformatted(json));
                cJSON_Delete(json);
                return XCASH_ERROR;
            }
        }
        token = strtok(NULL, ".");
    }

    // Extract and store the field value
    if (cJSON_IsString(current_obj) && current_obj->valuestring) {
        strncpy(result, current_obj->valuestring, result_size - 1);
        result[result_size - 1] = '\0';
    } else if (cJSON_IsNumber(current_obj)) {
        if (strcmp(field_name, "result.count") == 0) {
            snprintf(result, result_size, "%d", current_obj->valueint);
        } else {
            snprintf(result, result_size, "%.6f", current_obj->valuedouble);
        }
    } else if (cJSON_IsBool(current_obj)) {
        snprintf(result, result_size, "%s", cJSON_IsTrue(current_obj) ? "true" : "false");
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
    if (!src || !dest) {
        if (dest) *dest = '\0';
        return;
    }

    unsigned char md5_bin[MD5_DIGEST_LENGTH] = {0};

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        *dest = '\0';
        return;
    }

    if (EVP_DigestInit_ex(mdctx, EVP_md5(), NULL) != 1 ||
        EVP_DigestUpdate(mdctx, src, strlen(src)) != 1 ||
        EVP_DigestFinal_ex(mdctx, md5_bin, NULL) != 1) {
        EVP_MD_CTX_free(mdctx);
        *dest = '\0';
        return;
    }

    EVP_MD_CTX_free(mdctx);

    bin_to_hex(md5_bin, MD5_DIGEST_LENGTH, dest);
}

/*---------------------------------------------------------------------------------------------------------
Name: string_replace
Description: String replace only a specific amount of string occurences
Parameters:
  data - The string to replace the data
  DATA_TOTAL_LENGTH - The maximum size of data
  STR1 - The string to be replaced
  STR2 - The string to replace the other string
  COUNT - The number of string occurences to replace
Return: The result string
---------------------------------------------------------------------------------------------------------*/
void string_replace_limit(char *data, const size_t DATA_TOTAL_LENGTH, const char* STR1, const char* STR2, const int COUNT)
{
  // Early exit if invalid inputs
  if (data == NULL || STR1 == NULL || STR2 == NULL || COUNT <= 0) return;
  
  size_t slen = strlen(STR1);
  size_t rlen = strlen(STR2);
  if (slen == 0) return;  // avoid infinite loop

  size_t buf_size = DATA_TOTAL_LENGTH;
  char* buf = calloc(buf_size, sizeof(char));
  if (buf == NULL) return;

  const char* current = data;
  char* dest = buf;
  int replaced = 0;

  while (*current != '\0') {
    const char* found = strstr(current, STR1);

    if (found == NULL || replaced >= COUNT) {
      // Copy remaining part
      size_t remaining = strlen(current);
      if ((dest - buf) + remaining >= buf_size) break;  // prevent overflow
      memcpy(dest, current, remaining);
      dest += remaining;
      break;
    }

    // Copy up to found STR1
    size_t prefix_len = found - current;
    if ((dest - buf) + prefix_len + rlen >= buf_size) break;  // prevent overflow
    memcpy(dest, current, prefix_len);
    dest += prefix_len;

    // Copy replacement STR2
    memcpy(dest, STR2, rlen);
    dest += rlen;
    current = found + slen;
    replaced++;
  }

  // Null-terminate
  *dest = '\0';

  // Copy back safely
  strncpy(data, buf, DATA_TOTAL_LENGTH - 1);
  data[DATA_TOTAL_LENGTH - 1] = '\0'; // Ensure null termination

  free(buf);
}

bool compress_gzip_with_prefix(const unsigned char* input, size_t input_len,
                              unsigned char** output, size_t* output_len) {
    if (!input || !output || !output_len) return XCASH_ERROR;

    uLongf bound = compressBound(input_len);
    *output = malloc(bound + 1);  // +1 for the prefix
    if (!*output) return XCASH_ERROR;

    (*output)[0] = 0x01;  // Prefix to signal gzip

    int result = compress2(*output + 1, &bound, input, input_len, Z_BEST_COMPRESSION);
    if (result != Z_OK) {
        free(*output);
        *output = NULL;
        *output_len = 0;
        return XCASH_ERROR;
    }

    *output_len = bound + 1;
    return XCASH_OK;
}

bool decompress_gzip_with_prefix(const unsigned char* input, size_t input_len,
                                unsigned char** output, size_t* output_len) {
    if (!input || !output || !output_len || input_len < 2) return XCASH_ERROR;

    if (input[0] != 0x01) {
        // No compression: treat as plain text
        *output = malloc(input_len);
        if (!*output) return XCASH_ERROR;

        memcpy(*output, input, input_len);
        *output_len = input_len;
        return XCASH_OK;
    }

    // Strip prefix and decompress
    size_t alloc_size = input_len * 2;
    *output = NULL;

    for (int i = 0; i < 5; ++i) {
        free(*output);
        *output = malloc(alloc_size);
        if (!*output) return XCASH_ERROR;

        uLongf actual_len = alloc_size;
        int result = uncompress(*output, &actual_len, input + 1, input_len - 1);

        if (result == Z_OK) {
            *output_len = actual_len;
            return XCASH_OK;
        } else if (result == Z_BUF_ERROR) {
            alloc_size *= 2;
        } else {
            free(*output);
            *output = NULL;
            *output_len = 0;
            return XCASH_ERROR;
        }
    }

    free(*output);
    *output = NULL;
    *output_len = 0;
    return XCASH_ERROR;
}

/*---------------------------------------------------------------------------------------------------------
Generate random binary string
---------------------------------------------------------------------------------------------------------*/
int get_random_bytes(unsigned char *buf, size_t len) {
    ssize_t ret = getrandom(buf, len, 0);
    if (ret < 0 || (size_t)ret != len) {
        ERROR_PRINT("getrandom() failed: %s", strerror(errno));
        return XCASH_ERROR;
    }
    return XCASH_OK;
}


/*---------------------------------------------------------------------------------------------------------
base58 helper function
---------------------------------------------------------------------------------------------------------*/
int base58_char_to_value(char c) {
  const char* p = strchr(BASE58_ALPHABET, c);
  return p ? (int)(p - BASE58_ALPHABET) : -1;
}

/*---------------------------------------------------------------------------------------------------------
 * @brief Decodes a Base58-encoded string into binary.
 *
 * @param input            Null-terminated Base58 string.
 * @param output           Output buffer to receive binary result.
 * @param max_output_len   Maximum size of output buffer.
 * @param decoded_len      Pointer to store number of bytes written.
 * @return true on success, false on failure.
 ---------------------------------------------------------------------------------------------------------*/
/*
 bool base58_decode__OLD__(const char* input, uint8_t* output, size_t max_output_len, size_t* decoded_len) {
  size_t input_len = strlen(input);
  size_t i, j;

  if (!input || !output || !decoded_len || max_output_len == 0)
    return false;

  uint8_t tmp[BASE58_TMP_SIZE] = {0};

  for (i = 0; i < input_len; i++) {
    int val = base58_char_to_value(input[i]);
    if (val < 0) {
      ERROR_PRINT("Invalid Base58 char: '%c'", input[i]);
      return false;
    }

    int carry = val;
    for (j = BASE58_TMP_SIZE; j-- > 0;) {
      carry += 58 * tmp[j];
      tmp[j] = carry % 256;
      carry /= 256;
    }

    if (carry != 0) {
      ERROR_PRINT("Base58 overflow");
      return false;
    }
  }

  // Handle leading '1's (Base58 zero bytes)
  size_t leading_zeros = 0;
  for (i = 0; i < input_len && input[i] == '1'; i++) {
    output[leading_zeros++] = 0;
  }

  // Find start of non-zero data in tmp
  size_t start = 0;
  while (start < BASE58_TMP_SIZE && tmp[start] == 0) {
    start++;
  }

  size_t decoded_size = BASE58_TMP_SIZE - start;
  if (leading_zeros + decoded_size > max_output_len) {
    ERROR_PRINT("Output buffer too small (required %zu, available %zu)", leading_zeros + decoded_size, max_output_len);
    return false;
  }

  memcpy(output + leading_zeros, tmp + start, decoded_size);
  *decoded_len = leading_zeros + decoded_size;

  // Warn if decoded to 65 bytes with leading zero
  if (*decoded_len == 65 && output[0] == 0x00) {
    WARNING_PRINT("Decoded Base58 result is 65 bytes with leading 0x00 — likely a signature. Caller must normalize if needed.");
  }

  return true;
}

bool base58_decode(const char* input, uint8_t* output, size_t max_output_len, size_t* decoded_len) {
  if (!input || !output || !decoded_len || max_output_len == 0) {
    ERROR_PRINT("Invalid arguments to base58_decode");
    return false;
  }

  size_t input_len = strlen(input);
  size_t i, j;

  DEBUG_PRINT("Starting Base58 decode: input='%s', length=%zu", input, input_len);

  uint8_t tmp[BASE58_TMP_SIZE] = {0};

  for (i = 0; i < input_len; i++) {
    int val = base58_char_to_value(input[i]);
    if (val < 0) {
      ERROR_PRINT("Invalid Base58 char: '%c' at position %zu", input[i], i);
      return false;
    }

    int carry = val;
    for (j = BASE58_TMP_SIZE; j-- > 0;) {
      carry += 58 * tmp[j];
      tmp[j] = carry % 256;
      carry /= 256;
    }

    if (carry != 0) {
      ERROR_PRINT("Base58 overflow at input index %zu (char='%c')", i, input[i]);
      return false;
    }
  }

  // Handle leading '1's (Base58 zero bytes)
  size_t leading_zeros = 0;
  for (i = 0; i < input_len && input[i] == '1'; i++) {
    output[leading_zeros++] = 0;
  }

  DEBUG_PRINT("Leading Base58 zeros: %zu", leading_zeros);

  // Find start of non-zero data in tmp
  size_t start = 0;
  while (start < BASE58_TMP_SIZE && tmp[start] == 0) {
    start++;
  }

  size_t decoded_size = BASE58_TMP_SIZE - start;
  size_t total_size = leading_zeros + decoded_size;

  DEBUG_PRINT("Decoded non-zero data starts at tmp[%zu]", start);
  DEBUG_PRINT("Decoded size (excluding leading zeros): %zu", decoded_size);

  if (total_size > max_output_len) {
    ERROR_PRINT("Output buffer too small (required %zu, available %zu)", total_size, max_output_len);
    return false;
  }

  for (size_t k = 0; k < decoded_size; k++) {
   output[leading_zeros + k] = tmp[start + decoded_size - 1 - k];
  }
  *decoded_len = total_size;

  DEBUG_PRINT("Final decoded size: %zu", *decoded_len);
  DEBUG_PRINT("Decoded data (hex):");
  for (i = 0; i < *decoded_len; i++) {
    DEBUG_PRINT("  [%02zu]: 0x%02X", i, output[i]);
  }

  if (*decoded_len == 65 && output[0] == 0x00) {
    WARNING_PRINT("Decoded Base58 result is 65 bytes with leading 0x00 — likely a signature. Caller must normalize if needed.");
  }

  return true;
}
*/



int base58_decode(const char *base58, uint8_t *decoded, size_t *decoded_len) {
    size_t input_len = strlen(base58); // Length of the input Base58 string
    size_t output_len = 0;            // Length of the decoded output
    memset(decoded, 0, *decoded_len); // Initialize the output buffer with zeros

    for (size_t i = 0; i < input_len; i++) {
        const char *p = strchr(BASE58_ALPHABET, base58[i]); // Find character in Base58 alphabet
        if (!p) return XCASH_ERROR; // Return 0 if the character is invalid
        int carry = p - BASE58_ALPHABET; // Get the numeric value of the character

        for (size_t j = 0; j < output_len; j++) {
            carry += decoded[j] * 58;  // Multiply by 58 and add the carry
            decoded[j] = carry & 0xFF; // Store the least significant byte
            carry >>= 8;               // Carry the overflow to the next byte
        }

        while (carry) { // Append carry bytes if necessary
            decoded[output_len++] = carry & 0xFF;
            carry >>= 8;
        }
    }

    for (size_t i = 0; i < output_len / 2; i++) { // Reverse byte order
        uint8_t temp = decoded[i];
        decoded[i] = decoded[output_len - 1 - i];
        decoded[output_len - 1 - i] = temp;
    }

    *decoded_len = output_len; // Set the output length
    return XCASH_OK;                  // Return success
}