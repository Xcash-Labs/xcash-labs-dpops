#include "xcash_round.h"

void start_block_production(void) {
  struct timeval current_time, round_start_time, block_start_time;
  xcash_round_result_t round_result = ROUND_OK;
  size_t retries = 0;
  bool current_block_healthy = false;
  while (!current_block_healthy) {
    if (get_current_block_height(current_block_height) == XCASH_OK) {
      current_block_healthy = true;
    } else {
      WARNING_PRINT("Can't get current block height. Possible node is still syncing blocks. Waiting for recovery...");
      sleep(5);  // Sleep to prevent high CPU usage
    }
  }

  while (true) {
    gettimeofday(&current_time, NULL);
    size_t seconds_within_block = current_time.tv_sec % (BLOCK_TIME * 60);
    size_t minute_within_block = (current_time.tv_sec / 60) % BLOCK_TIME;

    // Skip block production if outside the first 25 seconds of the block interval
    if (seconds_within_block > 25) {
      retries = 0;

      // Refresh DB if previous round failed and we're late in the interval
      if (round_result != ROUND_OK && seconds_within_block > 280) {
        init_db_from_top();
        round_result = ROUND_OK;
      } else {
        INFO_STAGE_PRINT("Waiting for next round... Block %d in [%ld:%02ld]",
                         (int)atof(current_block_height),
                         BLOCK_TIME - 1 - minute_within_block,
                         59 - (current_time.tv_sec % 60));
        sleep(5);

      }
      continue;
    }

    // Check if current block height is healthy
    current_block_healthy = (get_current_block_height(current_block_height) == XCASH_OK);
    if (!current_block_healthy) {
      WARNING_PRINT("Block height unavailable. Node might be syncing. Retrying...");
      sleep(5);
      continue;
    }

    gettimeofday(&block_start_time, NULL);
    size_t round_number = 0;
    bool round_created = false;
    round_result = ROUND_OK;

    // Check for first PoS block
    if (strtoull(current_block_height, NULL, 10) == XCASH_PROOF_OF_STAKE_BLOCK_HEIGHT) {
      if (strncmp(network_nodes[0].seed_public_address, xcash_wallet_public_address, XCASH_WALLET_LENGTH) == 0) {
        if (start_current_round_start_blocks() != XCASH_ERROR) {
          round_created = true;
        } else {
          ERROR_PRINT("The function start_current_round_start_blocks failed");
          round_created = false;
        }
      } else {
        INFO_PRINT("Node is not the primary data network node. Sitting out this round.");
        sleep(SUBMIT_NETWORK_BLOCK_TIME_SECONDS);
        continue;
      }
    } else {
      // Standard round processing logic (up to 2 retries)
      for (retries = 0; retries < 2; retries++) {
        gettimeofday(&round_start_time, NULL);
        round_result = process_round(round_number);

        if (round_result == ROUND_RETRY) {
          sleep(5);
          continue;
        }

        if (round_result == ROUND_ERROR || round_result == ROUND_SKIP) {
          round_created = false;
          break;
        }

        if (round_result == ROUND_OK) {
          round_created = true;
          break;
        }

        round_number++;
      }


      // Final round result handling
      if (round_created) {
        INFO_PRINT_STATUS_OK("Block %s created successfully", current_block_height);
      } else {
        INFO_PRINT_STATUS_FAIL("Block %s not created after %zu attempt(s)", current_block_height, round_number + 1);
      }

//      break;  // Exit main production loop
    }

    break;  // Exit main production loop
  }
}