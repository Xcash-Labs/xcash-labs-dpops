#ifndef STRING_FUNCTIONS_H_   /* Include guard */
#define STRING_FUNCTIONS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>
#include "config.h"
#include "globals.h"
#include "macro_functions.h"

int parse_json_data(const char* DATA, const char* FIELD_NAME, char *result, const size_t RESULT_TOTAL_LENGTH);
void string_replace(char *data, const size_t DATA_TOTAL_LENGTH, const char* STR1, const char* STR2);

#endif