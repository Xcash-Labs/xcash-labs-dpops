#ifndef DPOPS_ROUND_H
#define DPOPS_ROUND_H

#include <stdbool.h>
#include <time.h>
#include "config.h"

typedef enum {
    ROUND_ERROR, // some system fault occurred. mostly communication errors or other non-fatal error. In that case better wait till next round
    ROUND_OK, //all the procedures finished successfully
    ROUND_SKIP, // wait till next round
    ROUND_RETRY,
    ROUND_NEXT,
} xcash_round_result_t;

void start_block_production(void);

#endif