#ifndef XCASH_INITIALIZE_H
#define XCASH_INITIALIZE_H

#include <stdbool.h>
#include <sys/sysinfo.h>
#include "config.h"
#include "globals.h"
#include "structures.h"

void print_starter_state(const arg_config_t* arg_config);
bool init_processing(const arg_config_t* arg_config);
bool configure_uv_threadpool(const arg_config_t* arg_config);

#endif // INIT_PROCESSING_H