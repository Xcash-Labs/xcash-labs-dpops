#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "daemon_functions.h"

/*---------------------------------------------------------------------------------------------------------
Name: get_current_block_height
Description: Gets the current block height of the network
Parameters:
  result - The string where you want the current block height to be saved to
Return: 0 if an error has occured, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int get_current_block_height(char *result)
{
  // Constants
  const char* HTTP_HEADERS[] = {"Content-Type: application/json","Accept: application/json"}; 
  const size_t HTTP_HEADERS_LENGTH = sizeof(HTTP_HEADERS)/sizeof(HTTP_HEADERS[0]);

  // Variables
  char data[SMALL_BUFFER_SIZE];

  memset(data,0,sizeof(data));

  if (send_http_request(data,XCASH_daemon_IP_address,"/json_rpc",XCASH_DAEMON_PORT,"POST", HTTP_HEADERS, HTTP_HEADERS_LENGTH,"{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"get_block_count\"}",SEND_OR_RECEIVE_SOCKET_DATA_TIMEOUT_SETTINGS) <= 0 || parse_json_data(data,"count",result, BUFFER_SIZE) == 0)
  {  
    memset(data,0,sizeof(data));
    memset(result,0,strlen(result));
    sleep(INVALID_RESERVE_PROOFS_SETTINGS);

    if (send_http_request(data,XCASH_daemon_IP_address,"/json_rpc",XCASH_DAEMON_PORT,"POST", HTTP_HEADERS, HTTP_HEADERS_LENGTH,"{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"get_block_count\"}",SEND_OR_RECEIVE_SOCKET_DATA_TIMEOUT_SETTINGS) <= 0 || parse_json_data(data,"count",result, BUFFER_SIZE) == 0)
    {
      ERROR_PRINT("Could not get the current block height");
      return XCASH_ERROR;
    }
  }
  return XCASH_OK;
}