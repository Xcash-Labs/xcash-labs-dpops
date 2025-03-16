#ifndef SERVER_FUNCTIONS_H_   /* Include guard */
#define SERVER_FUNCTIONS_H_

#include <stdio.h> 
#include <stdlib.h>
#include <string.h> 
#include <pthread.h>
#include <stdbool.h>
#include "config.h"
#include "globals.h"
#include "string_functions.h"

/*
-----------------------------------------------------------------------------------------------------------
Function prototypes
-----------------------------------------------------------------------------------------------------------
*/
int server_limit_IP_addresses(const int SETTINGS, const char* IP_ADDRESS);
int server_limit_public_addresses(const int SETTINGS, const char* MESSAGE);
#endif