#ifndef BLOCKCHAIN_FUNCTIONS_H_   /* Include guard */
#define BLOCKCHAIN_FUNCTIONS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "globals.h"
#include "macro_functions.h"
#include "VRF_functions.h"

int varint_encode(long long int number, char *result, const size_t RESULT_TOTAL_LENGTH);
size_t varint_decode(size_t varint);

#endif