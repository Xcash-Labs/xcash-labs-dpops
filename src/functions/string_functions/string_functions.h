#ifndef STRING_FUNCTIONS_H_   /* Include guard */
#define STRING_FUNCTIONS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <cjson/cJSON.h>
#include <sys/random.h>
#include <openssl/evp.h>
#include <openssl/md5.h> 
#include <unistd.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"

bool hex_to_byte_array(const char *hex_string, unsigned char *byte_array, size_t byte_array_size);
void bytes_to_hex(const unsigned char* bytes, size_t byte_len, char* hex_out, size_t hex_out_len);
int parse_json_data(const char* DATA, const char* FIELD_NAME, char *result, const size_t RESULT_TOTAL_LENGTH);
void string_replace(char *data, const size_t DATA_TOTAL_LENGTH, const char* STR1, const char* STR2);
int random_string(char *result, const size_t LENGTH);
size_t string_count(const char* DATA, const char* STRING);
void bin_to_hex(const unsigned char *bin_data, int data_size, char *buf);
void md5_hex(const char * src, char * dest);
void string_replace_limit(char *data, const size_t DATA_TOTAL_LENGTH, const char* STR1, const char* STR2, const int COUNT);

#endif