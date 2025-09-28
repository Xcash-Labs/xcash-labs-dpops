#include "net_multi.h"

static int send_all_with_timeout(int fd, const uint8_t* buf, size_t len, int ms_timeout) {
    size_t off = 0;
    while (off < len) {
        struct timeval tv = { ms_timeout/1000, (ms_timeout%1000)*1000 };
        fd_set wfds; FD_ZERO(&wfds); FD_SET(fd, &wfds);
        int r = select(fd+1, NULL, &wfds, NULL, &tv);
        if (r <= 0) return -1; // timeout or error
        ssize_t n = send(fd, buf + off, len - off, 0);
        if (n < 0) return -1;
        off += (size_t)n;
    }
    return 0;
}



#define WORKERS 10

/* ---- factor the body of your for-loop into this helper ---- */
static response_t* send_to_one_host(const char* host, int port,
                                    const uint8_t* z, size_t zlen) {
    response_t* r = calloc(1, sizeof(*r));
    if (!r) return NULL;
    r->host = strdup(host);
    r->status = STATUS_PENDING;
    r->req_time_start = time(NULL);

    char port_str[6]; snprintf(port_str, sizeof port_str, "%d", port);

    struct addrinfo hints = {0}, *res = NULL, *ai = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICSERV;

    if (getaddrinfo(host, port_str, &hints, &res) != 0 || !res) {
        r->status = STATUS_ERROR;
        ERROR_PRINT("DNS resolution failed for %s", host);
        return r;
    }

    int sock = -1;
    for (ai = res; ai; ai = ai->ai_next) {
        sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (sock < 0) continue;

        int of = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, of | O_NONBLOCK);
        (void)connect(sock, ai->ai_addr, ai->ai_addrlen);

        fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf);
        struct timeval ctv = { CONNECT_TIMEOUT_SEC, 0 };
        int sel = select(sock + 1, NULL, &wf, NULL, &ctv);
        if (sel > 0) {
            int err = 0; socklen_t sl = sizeof(err);
            if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &sl) == 0 && err == 0) {
                fcntl(sock, F_SETFL, of); // back to blocking
                break;
            }
        }
        close(sock); sock = -1;
    }

    if (sock < 0) {
        r->status = STATUS_TIMEOUT;
        WARNING_PRINT("Connect timeout/fail to %s", host);
        freeaddrinfo(res);
        r->req_time_end = time(NULL);
        return r;
    }

    struct timeval sto = { 0, 3000000 }; // 3s
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &sto, sizeof(sto));

    if (send_all_with_timeout(sock, z, zlen, 3000) != 0) {
        r->status = STATUS_ERROR;
        ERROR_PRINT("Send failed to %s", host);
    } else {
        r->status = STATUS_OK;
    }

    r->req_time_end = time(NULL);
    DEBUG_PRINT("Host:%s | %s | %lds",
                host,
                r->status == STATUS_OK ? "OK" :
                r->status == STATUS_TIMEOUT ? "TIMEOUT" : "ERROR",
                r->req_time_end - r->req_time_start);

    close(sock);
    freeaddrinfo(res);
    return r;
}

/* ---- simple worker pool ---- */
typedef struct {
    const char** hosts;
    size_t total;
    int port;
    const uint8_t* z;
    size_t zlen;
    response_t** responses;
    size_t next_idx;
    pthread_mutex_t mu;
} work_ctx_t;

static void* worker_fn(void* arg) {
    work_ctx_t* ctx = (work_ctx_t*)arg;
    for (;;) {
        size_t i;
        pthread_mutex_lock(&ctx->mu);
        i = ctx->next_idx++;
        pthread_mutex_unlock(&ctx->mu);
        if (i >= ctx->total) break;

        const char* host = ctx->hosts[i];
        ctx->responses[i] = send_to_one_host(host, ctx->port, ctx->z, ctx->zlen);
        if (!ctx->responses[i]) {
            // best-effort error stub
            response_t* r = calloc(1, sizeof(*r));
            if (r) { r->host = strdup(host); r->status = STATUS_ERROR; }
            ctx->responses[i] = r;
        }
    }
    return NULL;
}

/* ---- drop-in replacement: now runs in parallel ---- */
response_t** send_multi_request(const char** hosts, int port, const char* message) {
    // count
    size_t total_hosts = 0; while (hosts[total_hosts]) total_hosts++;

    response_t** responses = calloc(total_hosts + 1, sizeof(*responses));
    if (!responses) { perror("calloc responses"); return NULL; }

    // compress ONCE
    uint8_t* z = NULL; size_t zlen = 0;
    {
        size_t mlen = strlen(message) + 1;
        if (!compress_gzip_with_prefix((const unsigned char*)message, mlen, &z, &zlen)) {
            ERROR_PRINT("gzip failed");
            return responses; // entries remain NULL
        }
    }

    work_ctx_t ctx = {
        .hosts = hosts, .total = total_hosts, .port = port,
        .z = z, .zlen = zlen, .responses = responses,
        .next_idx = 0, .mu = PTHREAD_MUTEX_INITIALIZER
    };

    size_t nworkers = WORKERS;
    if (nworkers > total_hosts) nworkers = total_hosts ? total_hosts : 1;

    pthread_t th[WORKERS];
    for (size_t t = 0; t < nworkers; ++t) {
        pthread_create(&th[t], NULL, worker_fn, &ctx);
    }
    for (size_t t = 0; t < nworkers; ++t) {
        pthread_join(th[t], NULL);
    }

    free(z);
    return responses;
}