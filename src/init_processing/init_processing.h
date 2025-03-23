#ifndef INIT_PROCESSING_H
#define INIT_PROCESSING_H

#include <stdbool.h>
#include <sys/sysinfo.h>
#include "config.h"
#include "globals.h"
#include "structures.h"
#include "node_functions.h"
#include "db_sync.h"

void print_starter_state(const arg_config_t* arg_config);
bool init_processing(const arg_config_t* arg_config);
bool configure_uv_threadpool(const arg_config_t* arg_config);

#endif // INIT_PROCESSING_H