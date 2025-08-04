#include "network_functions.h"

/*---------------------------------------------------------------------------------------------------------
Name: check_if_IP_address_or_hostname
Description: Checks if the data is an IP address or a hostname
Parameters:
  HOST - The hostname or IP address
Return: 1 if an IP address, 2 if a hostname
---------------------------------------------------------------------------------------------------------*/
int check_if_IP_address_or_hostname(const char *HOST)
{
    struct in_addr addr;
    struct in6_addr addr6;
    // Check if IPv4
    if (inet_pton(AF_INET, HOST, &addr) == 1)
    {
        return IS_IP;
    }
    // Check if IPv6
    if (inet_pton(AF_INET6, HOST, &addr6) == 1)
    {
        return IS_IP;
    }
    // Otherwise, it's a hostname
    return IS_HOSTNAME;
}
/*---------------------------------------------------------------------------------------------------------
Name: write_callback
Description: Callback function to handle response data
---------------------------------------------------------------------------------------------------------*/
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t total_size = size * nmemb;
    ResponseBuffer *buffer = (ResponseBuffer *)userp;

    char *ptr = realloc(buffer->data, buffer->size + total_size + 1);
    if (ptr == NULL)
    {
        ERROR_PRINT("Memory allocation failed in write_callback()");
        return 0; // Stop writing
    }

    buffer->data = ptr;
    memcpy(&(buffer->data[buffer->size]), contents, total_size);
    buffer->size += total_size;
    buffer->data[buffer->size] = '\0'; // Null-terminate

    return total_size;
}

/*---------------------------------------------------------------------------------------------------------
Name: send_http_request
Description: Sends a HTTP request
Parameters:
  result - Where the result is stored
  HOST - The hostname or IP address
  URL - The URL
  PORT - The port
  HTTP_SETTINGS - The HTTP method
  HTTP_HEADERS - The HTTP headers
  HTTP_HEADERS_LENGTH - The length of the HTTP headers
  DATA - The request data. If sending a GET request, the data is appended to the url. If sending a POST request, the data is sent in the request body
  DATA_TIMEOUT_SETTINGS - The timeout settings for reading the data
Return: 0 if an error has occured, 1 if successfull
---------------------------------------------------------------------------------------------------------*/
int send_http_request(char *result, size_t return_buffer_size, const char *host, const char *url, int port,
                      const char *method, const char **headers, size_t headers_length,
                      const char *data, int timeout)
{

    INFO_PRINT("start ************xcash_wallet_public_address=%s",xcash_wallet_public_address);

    CURL *curl;
    CURLcode res;
    struct curl_slist *header_list = NULL;

    // Initialize the response buffer
    ResponseBuffer response = {malloc(1), 0};
    if (!response.data)
    {
        ERROR_PRINT("Memory allocation failed");
        return XCASH_ERROR;
    }

    curl = curl_easy_init();
    if (!curl)
    {
        ERROR_PRINT("Failed to initialize libcurl");
        free(response.data);
        return XCASH_ERROR;
    }

    // Construct full URL
    char full_url[256];
    snprintf(full_url, sizeof(full_url), "http://%s:%d%s", host, port, url);
    DEBUG_PRINT("Making HTTP request to URL: %s", full_url);
    curl_easy_setopt(curl, CURLOPT_URL, full_url);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // Handle HTTP headers
    for (size_t i = 0; i < headers_length; i++)
    {
        header_list = curl_slist_append(header_list, headers[i]);
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);

    // Handle HTTP method
    if (strcmp(method, "POST") == 0)
    {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
    }

    INFO_PRINT("1 ************xcash_wallet_public_address=%s",xcash_wallet_public_address);

    // Perform the request
    res = curl_easy_perform(curl);
    if (res != CURLE_OK)
    {
        ERROR_PRINT("HTTP request failed: %s", curl_easy_strerror(res));
        free(response.data);
        curl_easy_cleanup(curl);
        if (header_list)
            curl_slist_free_all(header_list);
        return XCASH_ERROR;
    }

    INFO_PRINT("2 ************xcash_wallet_public_address=%s",xcash_wallet_public_address);

    DEBUG_PRINT("Curl result %s", response.data);

    // Validate response before copying
    if (!response.data)
    {
        ERROR_PRINT("response.data is NULL");
        curl_easy_cleanup(curl);
        if (header_list)
            curl_slist_free_all(header_list);
        return XCASH_ERROR;
    }
    INFO_PRINT("3 ************xcash_wallet_public_address=%s",xcash_wallet_public_address);
    // Validate result buffer before copying
    if (!result)
    {
        ERROR_PRINT("result buffer is NULL");
        free(response.data);
        curl_easy_cleanup(curl);
        if (header_list)
            curl_slist_free_all(header_list);
        return XCASH_ERROR;
    }

    size_t response_len = strlen(response.data);
    DEBUG_PRINT("Response length: %zu", response_len);

    if (!result || return_buffer_size == 0) {
        ERROR_PRINT("Invalid result buffer");
        return XCASH_ERROR;
    }

    if (response_len >= return_buffer_size)
    {
        ERROR_PRINT("Response data too large (%zu bytes)", response_len);
        free(response.data);
        curl_easy_cleanup(curl);
        if (header_list)
            curl_slist_free_all(header_list);
        return XCASH_ERROR;
    }

    // Copy the response to result buffer
    strncpy(result, response.data, return_buffer_size - 1);
    result[return_buffer_size - 1] = '\0'; // Ensure null termination

    // Cleanup
    free(response.data);
    curl_easy_cleanup(curl);
    if (header_list)
        curl_slist_free_all(header_list);
    return XCASH_OK;
}