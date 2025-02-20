#include "dpops_round.h"
#include "daemon_functions.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

void start_block_production(void)
{
    struct timeval current_time, round_start_time, block_start_time;
    xcash_round_result_t round_result = ROUND_OK;
    size_t retries = 0;
    bool current_block_healthy = false;
    char current_block_height[BUFFER_SIZE] = {0};

    while (true)
    {
        gettimeofday(&current_time, NULL);
        size_t seconds_within_block = current_time.tv_sec % (BLOCK_TIME * 60);
        size_t minute_within_block = (current_time.tv_sec / 60) % BLOCK_TIME;

        // Fetch current block height
        current_block_healthy = get_current_block_height(current_block_height) == XCASH_OK;
        if (!current_block_healthy)
        {
            WARNING_PRINT("Can't get current block height. Possible node is still syncing blocks. Waiting for recovery...");
        }

        // Don't start block production if blockchain is not synced or block time is already past the start point
        if (seconds_within_block > 25 || !current_block_healthy)
        {
            retries = 0;

            // Refresh DB in case of last round error (only if syncing is complete)
            if (round_result != ROUND_OK && current_block_healthy && seconds_within_block > 280)
            {
                init_db_from_top();
                round_result = ROUND_OK;
            }
            else
            {
                INFO_STAGE_PRINT("Waiting for a [%s] block production. Starting in ... [%lu:%02lu]",
                                 current_block_height, BLOCK_TIME - 1 - minute_within_block, 59 - (current_time.tv_sec % 60));
                sleep(5);
            }
        }
        else
        {
            size_t round_number = 0;
            bool round_created = false;
            gettimeofday(&block_start_time, NULL);

            // Retry mechanism for syncing issues
            while (retries < 2 && round_number < 1)
            {
                gettimeofday(&round_start_time, NULL);
                round_result = process_round(round_number);

                if (round_result == ROUND_RETRY)
                {
                    retries++;
                    sleep(5);
                    continue;
                }
                else if (round_result == ROUND_ERROR || round_result == ROUND_SKIP)
                {
                    round_created = false;
                }
                else if (round_result == ROUND_OK)
                {
                    round_created = true;
                }

                if (round_created)
                {
                    INFO_PRINT_STATUS_OK("Block %s created successfully", current_block_height);
                }
                else
                {
                    INFO_PRINT_STATUS_FAIL("Block %s not created within %lu rounds", current_block_height, round_number);
                }
                
                break;
            }
        }
    }
}