#include "string_functions.h"

// Helper
static inline bool is_hex_char(char c) {
  return (c >= '0' && c <= '9') ||
         (c >= 'a' && c <= 'f') ||
         (c >= 'A' && c <= 'F');
}

// Returns true if `s` is non-null, exactly `expected_len` characters long,
// and every character is a valid hex digit (0-9, a-f, A-F).
bool is_hex_len(const char *s, size_t expected_len) {
  if (!s) return false;
  const size_t n = strlen(s);
  if (n != expected_len) return false;
  for (size_t i = 0; i < n; ++i) {
    if (!is_hex_char(s[i])) return false;
  }
  return true;
}

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

/* Helper to find the "result": { ... } bounds in raw JSON */
static int json_find_result_object(const char* json, const char** out_start, const char** out_end) {
  const char* p = strstr(json, "\"result\"");
  if (!p) return 0;
  p = strchr(p, '{'); if (!p) return 0;
  int depth = 1, in_str = 0;
  const char* q = p + 1;
  while (*q && depth > 0) {
    unsigned char c = (unsigned char)*q;
    if (in_str) { if (c == '"' && q[-1] != '\\') in_str = 0; }
    else { if (c == '"') in_str = 1; else if (c == '{') depth++; else if (c == '}') depth--; }
    q++;
  }
  if (depth != 0) return 0;
  *out_start = p; *out_end = q - 1;
  return 1;
}

/* Helper to find key "<key>": <value> inside [obj_start, obj_end]; return ptr to value start */
static const char* json_find_key_value_in_object(const char* obj_start, const char* obj_end, const char* key) {
  char needle[128];
  int n = snprintf(needle, sizeof needle, "\"%s\"", key);
  if (n <= 0 || (size_t)n >= sizeof needle) return NULL;

  const char* pos = obj_start;
  while ((pos = strstr(pos, needle)) && pos < obj_end) {
    const char* colon = strchr(pos + n, ':');
    if (!colon || colon >= obj_end) return NULL;
    const char* v = colon + 1;
    while (v < obj_end && isspace((unsigned char)*v)) v++;
    return (v < obj_end) ? v : NULL;
  }
  return NULL;
}

/* Helper to given value starting at '{', get its matching '}' */
static int json_object_bounds_from_value(const char* v, const char* limit, const char** out_start, const char** out_end) {
  if (!v || v >= limit || *v != '{') return 0;
  int depth = 1, in_str = 0;
  const char* p = v + 1;
  while (p < limit && depth > 0) {
    unsigned char c = (unsigned char)*p;
    if (in_str) { if (c == '"' && p[-1] != '\\') in_str = 0; }
    else { if (c == '"') in_str = 1; else if (c == '{') depth++; else if (c == '}') depth--; }
    p++;
  }
  if (depth != 0) return 0;
  *out_start = v; *out_end = p - 1;
  return 1;
}

/* Extract decimal digits (optionally quoted) for a dot-path under "result": e.g. "count", "block_header.timestamp" */
static int json_result_path_digits(const char* json, const char* path, char* out, size_t outsz) {
  if (!json || !path || !out || outsz == 0) return 0;
  out[0] = '\0';

  const char *rs, *re;
  if (!json_find_result_object(json, &rs, &re)) return 0;

  char tmp[160];
  strncpy(tmp, path, sizeof tmp - 1);
  tmp[sizeof tmp - 1] = '\0';

  const char* cur_s = rs;
  const char* cur_e = re;
  char* save = NULL;
  for (char* tok = strtok_r(tmp, ".", &save); tok; tok = strtok_r(NULL, ".", &save)) {
    const char* v = json_find_key_value_in_object(cur_s, cur_e, tok);
    if (!v) return 0;

    char* next = strtok_r(NULL, ".", &save);
    if (next) {
      /* Descend into object value */
      const char *os, *oe;
      if (!json_object_bounds_from_value(v, cur_e, &os, &oe)) return 0;
      cur_s = os; cur_e = oe;
      /* restore next for next loop iteration */
      size_t nextlen = strlen(next);
      memmove(next - 1, next, nextlen + 1); // put back strtok's consumed token boundary
      continue;
    }

    /* Last token: extract digits (optionally quoted) */
    while (v < cur_e && isspace((unsigned char)*v)) v++;
    int quoted = 0;
    if (v < cur_e && *v == '"') { quoted = 1; v++; }
    if (v >= cur_e || !isdigit((unsigned char)*v)) return 0;

    size_t w = 0;
    while (v < cur_e && isdigit((unsigned char)*v)) {
      if (w + 1 >= outsz) return 0;
      out[w++] = *v++;
    }
    out[w] = '\0';

    if (quoted) {
      while (v < cur_e && isspace((unsigned char)*v)) v++;
      if (v >= cur_e || *v != '"') return 0;
    }
    return (w > 0);
  }
  return 0;
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
    char *saveptr = NULL;
    char *token = strtok_r(path_copy, ".", &saveptr);
    while (token != NULL) {
      char *bracket_pos = strchr(token, '[');
      if (bracket_pos) {
        *bracket_pos = '\0';

        current_obj = cJSON_GetObjectItemCaseSensitive(current_obj, token);
        if (!current_obj || !cJSON_IsArray(current_obj)) {
          ERROR_PRINT("Field '%s' not found or is not an array", token);
          cJSON_Delete(json);
          return XCASH_ERROR;
        }

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
          cJSON_Delete(json);
          return XCASH_ERROR;
        }
      }
      token = strtok_r(NULL, ".", &saveptr);
    }

    // Extract and store the field value
    if (cJSON_IsString(current_obj) && current_obj->valuestring) {
        strncpy(result, current_obj->valuestring, result_size - 1);
        result[result_size - 1] = '\0';
    } else if (cJSON_IsNumber(current_obj)) {
      const char* path_in_result = (strncmp(field_name, "result.", 7) == 0) ? field_name + 7 : field_name;
      if (!json_result_path_digits(data, path_in_result, result, result_size)) {
        snprintf(result, result_size, "%.0f", current_obj->valuedouble);
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

      // Find the last valid '}'
      size_t json_length = 0;
      for (size_t i = 0; i < input_len; ++i) {
        if (input[i] == '}') {
          json_length = i + 1;
        }
      }

      if (json_length == 0) {
        return XCASH_ERROR;  // No closing brace = invalid
      }

      *output = malloc(json_length + 1);
      if (!*output) return XCASH_ERROR;

      memcpy(*output, input, json_length);
      (*output)[json_length] = '\0';  // null-terminate for safety
      *output_len = json_length;

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
Generate a sync token
---------------------------------------------------------------------------------------------------------*/
int create_sync_token(void) {
    size_t bin_len = SYNC_TOKEN_LEN / 2;
    unsigned char bin[bin_len];


    if (get_random_bytes(bin, sizeof(bin)) != XCASH_OK) {
        ERROR_PRINT("Failed to generate random sync token");
        return XCASH_ERROR;
    }

    for (size_t i = 0; i < bin_len; ++i) {
        sprintf(&sync_token[i * 2], "%02x", bin[i]);
    }

    sync_token[SYNC_TOKEN_LEN] = '\0'; // Null-terminate

    DEBUG_PRINT("Generated sync token: %s", sync_token); 
    return XCASH_OK;
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
 * @brief Decode a Base64-encoded string using OpenSSL.
 * 
 * @param input         Null-terminated Base64 input string.
 * @param output        Output buffer for binary result.
 * @param max_output    Maximum size of output buffer.
 * @param decoded_len   Pointer to receive number of decoded bytes.
 * @return true on success, false on error.
---------------------------------------------------------------------------------------------------------*/
bool base64_decode(const char* input, uint8_t* output, size_t max_output, size_t* decoded_len) {
    if (!input || !output || !decoded_len || max_output == 0) return false;

    BIO *b64 = BIO_new(BIO_f_base64());
    BIO *bmem = BIO_new_mem_buf(input, -1);
    if (!b64 || !bmem) return false;

    BIO_push(b64, bmem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

    int len = BIO_read(b64, output, max_output);
    BIO_free_all(b64);

    if (len <= 0 || (size_t)len > max_output) return false;

    *decoded_len = (size_t)len;
    return true;
}

// Helper, check for invalid strings
int check_for_invalid_strings(const char* MESSAGE)
{
  if (!MESSAGE) return XCASH_ERROR;  // Defensive check for null pointer

  return !(strchr(MESSAGE, '"') || strchr(MESSAGE, ',') || strchr(MESSAGE, ':'));
}

// Helper, check for valid base58 string
bool str_is_base58(const char* s) {
  static const char* B58 = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
  for (const unsigned char *p = (const unsigned char*)s; *p; ++p) {
    if (*p < 0x20 || *p == 0x7F) return false;
    if (!strchr(B58, (int)*p)) return false;
  }
  return true;
}

/* 
   Compute SHA-256 over the canonical encoding of an outputs array.
   Canonical bytes per entry:
     - uint16 LE length of address (len16)
     - address bytes (ASCII, exactly 'len16' bytes; no NUL included)
     - uint64 LE amount
   The order of 'outs' MUST be deterministic across nodes.
   out32: receives 32 bytes of the SHA-256 digest.
*/
void outputs_digest_sha256(const payout_output_t *outs, size_t n, uint8_t out32[32]) {
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  if (!ctx) { memset(out32, 0, 32); return; }

  if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
    EVP_MD_CTX_free(ctx);
    memset(out32, 0, 32);
    return;
  }

  for (size_t i = 0; i < n; ++i) {
    /* address length (uint16 LE) */
    uint16_t alen = (uint16_t)strnlen(outs[i].a, XCASH_WALLET_LENGTH);
    uint8_t le16[2] = { (uint8_t)(alen & 0xFF), (uint8_t)((alen >> 8) & 0xFF) };
    EVP_DigestUpdate(ctx, le16, sizeof le16);

    /* address bytes (no NUL) */
    if (alen > 0) {
      EVP_DigestUpdate(ctx, (const unsigned char*)outs[i].a, (size_t)alen);
    }

    /* amount (uint64 LE) */
    uint8_t le64[8];
    for (int b = 0; b < 8; ++b) le64[b] = (uint8_t)((outs[i].v >> (8 * b)) & 0xFF);
    EVP_DigestUpdate(ctx, le64, sizeof le64);
  }

  unsigned int md_len = 0;
  unsigned char md[EVP_MAX_MD_SIZE];
  if (EVP_DigestFinal_ex(ctx, md, &md_len) != 1 || md_len != 32) {
    memset(out32, 0, 32);
  } else {
    memcpy(out32, md, 32);
  }

  EVP_MD_CTX_free(ctx);
}

bool parse_updpops_entry(const char* s, updpops_entry_t* out) {
  const char* pfx = "xcashdpops:source:";
  size_t plen = strlen(pfx);
  if (strncmp(s, pfx, plen) != 0) return false;

  const char* p = s + plen;               // version start
  const char* c = strchr(p, ':');         // colon before digest
  if (!c) return false;

  size_t vlen = (size_t)(c - p);
  if (vlen == 0 || vlen >= sizeof(out->version)) return false;
  memcpy(out->version, p, vlen);
  out->version[vlen] = '\0';

  const char* d = c + 1;                  // digest
  if (strlen(d) != 64) return false;
  for (int i = 0; i < 64; ++i) {
    char ch = d[i];
    if (!((ch >= '0' && ch <= '9') ||
          (ch >= 'a' && ch <= 'f') ||
          (ch >= 'A' && ch <= 'F'))) return false;
    out->digest[i] = (ch >= 'A' && ch <= 'F') ? (char)(ch + 32) : ch; // lower
  }
  out->digest[64] = '\0';
  return true;
}

int parse_semver(const char *s, int *maj, int *min, int *pat) {
  if (!s || !maj || !min || !pat) return 0;
  // allow optional leading 'v' or 'V'
  if (s[0] == 'v' || s[0] == 'V') s++;
  // tolerate missing minor/patch as 0 (e.g., "1" or "1.2")
  int m = 0, n = 0, p = 0;
  int count = sscanf(s, "%d.%d.%d", &m, &n, &p);
  if (count <= 0) return 0;
  if (count == 1) { n = 0; p = 0; }
  if (count == 2) { p = 0; }
  *maj = m; *min = n; *pat = p;
  return 1;
}

int semver_cmp(const char *a, const char *b) {
  int A=0,B=0,C=0, D=0,E=0,F=0;
  int ok_a = parse_semver(a, &A,&B,&C);
  int ok_b = parse_semver(b, &D,&E,&F);
  if (!ok_a || !ok_b) {
    // Fallback: lexical compare if not parseable
    return strcmp(a ? a : "", b ? b : "");
  }
  if (A != D) return (A < D) ? -1 : 1;
  if (B != E) return (B < E) ? -1 : 1;
  if (C != F) return (C < F) ? -1 : 1;
  return 0;
}

static int cmp_entry_digest(const void *a, const void *b) {
  const updpops_entry_t *A = (const updpops_entry_t*)a;
  const updpops_entry_t *B = (const updpops_entry_t*)b;
  return strcmp(A->digest, B->digest);
}

// Order-insensitive set compare by digest only.
int same_set_by_digest(const updpops_entry_t *A, size_t na, const updpops_entry_t *B, size_t nb) {
  if (na != nb) return 0;
  updpops_entry_t *AA = (updpops_entry_t*)malloc(na * sizeof(*AA));
  updpops_entry_t *BB = (updpops_entry_t*)malloc(nb * sizeof(*BB));
  if (!AA || !BB) { free(AA); free(BB); return 0; }
  memcpy(AA, A, na * sizeof(*AA));
  memcpy(BB, B, nb * sizeof(*BB));
  qsort(AA, na, sizeof(*AA), cmp_entry_digest);
  qsort(BB, nb, sizeof(*BB), cmp_entry_digest);
  int ok = 1;
  for (size_t i = 0; i < na; ++i) {
    if (strcmp(AA[i].digest, BB[i].digest) != 0) { ok = 0; break; }
  }
  free(AA); free(BB);
  return ok;
}


// Helper to read semver
static uint64_t semver_read_uint(const char** p)
{
  uint64_t v = 0;
  while (**p && isdigit((unsigned char)**p)) {
    v = (v * 10) + (uint64_t)(**p - '0');
    (*p)++;
  }
  return v;
}

// Compare verion number
int semver_cmp(const char* a, const char* b)
{
  if (!a && !b) return 0;
  if (!a) return -1;
  if (!b) return 1;

  const char* pa = a;
  const char* pb = b;

  for (int part = 0; part < 3; part++) {
    // stop if suffix starts
    if (*pa == '-' || *pa == '+') pa = ""; // treat remaining as 0
    if (*pb == '-' || *pb == '+') pb = "";

    uint64_t va = semver_read_uint(&pa);
    uint64_t vb = semver_read_uint(&pb);

    if (va < vb) return -1;
    if (va > vb) return 1;

    // Skip dot if present (otherwise missing parts treated as 0)
    if (*pa == '.') pa++;
    if (*pb == '.') pb++;
  }

  return 0;
}