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
Name: hostname_to_ip
Description: Resolves a hostname or IP literal to a numeric IP string (IPv4 or IPv6).
Parameters:
  name      - Hostname or IP literal to resolve.
  ip_out    - Output buffer to receive the numeric IP (no port).
  ip_out_len- Size of ip_out.
Return: true on success (resolved or already numeric), false on failure.
---------------------------------------------------------------------------------------------------------*/
bool hostname_to_ip(const char* name, char* ip_out, size_t ip_out_len) {
  struct in_addr a4;
  struct in6_addr a6;
  if (inet_pton(AF_INET, name, &a4) == 1 || inet_pton(AF_INET6, name, &a6) == 1) {
    snprintf(ip_out, ip_out_len, "%s", name);
    return true;
  }
  struct addrinfo hints = {0}, *res = NULL;
  hints.ai_family = AF_UNSPEC;      // allow v4 or v6
  hints.ai_socktype = SOCK_STREAM;  // any
  if (getaddrinfo(name, NULL, &hints, &res) != 0 || !res) return false;

  char buf[NI_MAXHOST];
  bool ok = getnameinfo(res->ai_addr, res->ai_addrlen, buf, sizeof(buf), NULL, 0, NI_NUMERICHOST) == 0;
  if (ok) snprintf(ip_out, ip_out_len, "%s", buf);
  freeaddrinfo(res);
  return ok;
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
    CURL *curl;
    CURLcode res;
    struct curl_slist *header_list = NULL;

    // Init response buffer
    ResponseBuffer response = { malloc(1), 0 };
    if (!response.data) {
        ERROR_PRINT("Memory allocation failed");
        return XCASH_ERROR;
    }

    curl = curl_easy_init();
    if (!curl) {
        ERROR_PRINT("Failed to initialize libcurl");
        free(response.data);
        return XCASH_ERROR;
    }

    // --- Derive sane timeouts from existing 'timeout' (seconds) ---
    // total timeout (ms)
    long total_timeout_ms = (timeout > 0) ? (long)timeout * 1000L : 180000L;     // default 180s
    // connect timeout (ms) = min(15s, total/3) to leave room for server processing
    long connect_timeout_ms = total_timeout_ms / 3;
    if (connect_timeout_ms > 15000L) connect_timeout_ms = 15000L;
    if (connect_timeout_ms < 3000L)  connect_timeout_ms = 3000L;                 // at least 3s

    // Build URL
    char full_url[256];
    snprintf(full_url, sizeof(full_url), "http://%s:%d%s", host, port, url);
    DEBUG_PRINT("Making HTTP request to URL: %s", full_url);

    curl_easy_setopt(curl, CURLOPT_URL, full_url);

    // --- Robust timeouts / stall protection ---
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, connect_timeout_ms);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS,        total_timeout_ms);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT,   1L);                       // 1 byte/sec
    // LOW_SPEED_TIME must be < total timeout; keep ~1/2 of it but cap to 90s
    long low_speed_time = total_timeout_ms / 2000L; // ms->s /2
    if (low_speed_time > 90L) low_speed_time = 90L;
    if (low_speed_time < 10L) low_speed_time = 10L;
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME,    low_speed_time);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL,          1L);                       // thread-safe timeouts

    // HTTP version + TCP keepalive
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
//#ifdef CURLOPT_TCP_KEEPIDLE
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 30L);
//#endif
//#ifdef CURLOPT_TCP_KEEPINTVL
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 15L);
//#endif

    // Build headers (existing + keep-alive + disable 100-continue)
    for (size_t i = 0; i < headers_length; i++) {
        header_list = curl_slist_append(header_list, headers[i]);
    }
    header_list = curl_slist_append(header_list, "Connection: keep-alive");
    header_list = curl_slist_append(header_list, "Expect:"); // disable 100-continue
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);

    // Method + body
    if (strcmp(method, "POST") == 0) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (data) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(data)); // explicit size
        } else {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
        }
    } else {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    }

    // Response buffering
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // Perform
    res = curl_easy_perform(curl);

    // Rich diagnostics (useful even when it succeeds)
    double t_total=0, t_dns=0, t_conn=0, t_tls=0, t_fb=0;
    long http_code=0;
    char *peer_ip=NULL;
    long peer_port=0;
    curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME,         &t_total);
    curl_easy_getinfo(curl, CURLINFO_NAMELOOKUP_TIME,    &t_dns);
    curl_easy_getinfo(curl, CURLINFO_CONNECT_TIME,       &t_conn);
    curl_easy_getinfo(curl, CURLINFO_APPCONNECT_TIME,    &t_tls);
    curl_easy_getinfo(curl, CURLINFO_STARTTRANSFER_TIME, &t_fb);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE,      &http_code);
    curl_easy_getinfo(curl, CURLINFO_PRIMARY_IP,         &peer_ip);
    curl_easy_getinfo(curl, CURLINFO_PRIMARY_PORT,       &peer_port);

    if (res != CURLE_OK) {
        ERROR_PRINT("HTTP request failed: curl=%d (%s) http=%ld total=%.3fs dns=%.3fs conn=%.3fs tls=%.3fs first_byte=%.3fs peer=%s:%ld url=%s",
                    (int)res, curl_easy_strerror(res), http_code, t_total, t_dns, t_conn, t_tls, t_fb,
                    peer_ip ? peer_ip : "?", peer_port, full_url);
        free(response.data);
        curl_easy_cleanup(curl);
        if (header_list) curl_slist_free_all(header_list);
        return XCASH_ERROR;
    }

    DEBUG_PRINT("HTTP ok: http=%ld total=%.3fs peer=%s:%ld", http_code, t_total,
                peer_ip ? peer_ip : "?", peer_port);
    DEBUG_PRINT("Curl result %s", response.data ? response.data : "(null)");

    // Validate response before copying
    if (!response.data) {
        ERROR_PRINT("response.data is NULL");
        curl_easy_cleanup(curl);
        if (header_list) curl_slist_free_all(header_list);
        return XCASH_ERROR;
    }

    // Validate result buffer before copying
    if (!result || return_buffer_size == 0) {
        ERROR_PRINT("Invalid result buffer");
        free(response.data);
        curl_easy_cleanup(curl);
        if (header_list) curl_slist_free_all(header_list);
        return XCASH_ERROR;
    }

    size_t response_len = strlen(response.data);
    DEBUG_PRINT("Response length: %zu", response_len);

    if (response_len >= return_buffer_size) {
        ERROR_PRINT("Response data too large (%zu bytes)", response_len);
        free(response.data);
        curl_easy_cleanup(curl);
        if (header_list) curl_slist_free_all(header_list);
        return XCASH_ERROR;
    }

    // Copy the response to result buffer
    strncpy(result, response.data, return_buffer_size - 1);
    result[return_buffer_size - 1] = '\0';

    // Cleanup
    free(response.data);
    curl_easy_cleanup(curl);
    if (header_list) curl_slist_free_all(header_list);
    return XCASH_OK;
}