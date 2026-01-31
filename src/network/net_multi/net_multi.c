#include "net_multi.h"

/* --- write whole buffer with per-attempt timeout (select + send) --- */
static int send_all_with_timeout(int fd, const uint8_t* buf, size_t len, int ms_timeout) {
    size_t off = 0;
    while (off < len) {
        struct timeval tv;
        tv.tv_sec  = ms_timeout / 1000;
        tv.tv_usec = (ms_timeout % 1000) * 1000;

        fd_set wfds; FD_ZERO(&wfds); FD_SET(fd, &wfds);
        int r = select(fd + 1, NULL, &wfds, NULL, &tv);
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        if (r == 0 || !FD_ISSET(fd, &wfds)) return -1; /* timeout */

        ssize_t n = send(fd, buf + off, len - off, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
            return -1;
        }
        if (n == 0) return -1; /* peer closed */
        off += (size_t)n;
    }
    return 0;
}

/* ---- single-host sender ---- */
static response_t* send_to_one_host(const char* host, int port,
                                    const uint8_t* z, size_t zlen) {
    response_t* r = (response_t*)calloc(1, sizeof(*r));
    if (!r) return NULL;

    r->host = strdup(host);
    r->status = STATUS_PENDING;
    r->req_time_start = time(NULL);

    char port_str[6]; snprintf(port_str, sizeof port_str, "%d", port);

    struct addrinfo hints = {0}, *res = NULL, *ai = NULL;
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_NUMERICSERV | AI_ADDRCONFIG;

    if (getaddrinfo(host, port_str, &hints, &res) != 0 || !res) {
        r->status = STATUS_ERROR;
        ERROR_PRINT("DNS resolution failed for %s", host);
        r->req_time_end = time(NULL);
        return r;
    }

    int sock = -1;
    for (ai = res; ai; ai = ai->ai_next) {
        sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (sock < 0) continue;

        /* non-blocking connect with deadline */
        int of = fcntl(sock, F_GETFL, 0);
        if (of == -1) of = 0;
        (void)fcntl(sock, F_SETFL, of | O_NONBLOCK);

        int rc = connect(sock, ai->ai_addr, ai->ai_addrlen);
        if (rc == 0) {
            /* connected immediately */
            (void)fcntl(sock, F_SETFL, of);
            break;
        }
        if (errno != EINPROGRESS) { close(sock); sock = -1; continue; }

        for (;;) {
          fd_set wf;
          FD_ZERO(&wf);
          FD_SET(sock, &wf);
          struct timeval ctv = (struct timeval){CONNECT_TIMEOUT_SEC, 0};
          int sel = select(sock + 1, NULL, &wf, NULL, &ctv);

          if (sel < 0 && errno == EINTR) continue;

          if (sel < 0) {
            ERROR_PRINT("connect select() failed: errno=%d (%s)", errno, strerror(errno));
            close(sock);
            sock = -1;
            break;
          }

          if (sel == 0 || !FD_ISSET(sock, &wf)) {
            ERROR_PRINT("connect %s:%d timeout after %d sec", host, port, CONNECT_TIMEOUT_SEC);
            close(sock);
            sock = -1;
            break;
          }

          int err = 0;
          socklen_t sl = sizeof(err);
          if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &sl) != 0) {
            ERROR_PRINT("getsockopt(SO_ERROR) failed: errno=%d (%s)", errno, strerror(errno));
            close(sock);
            sock = -1;
            break;
          }

          if (err != 0) {
            ERROR_PRINT("connect failed: SO_ERROR=%d (%s)", err, strerror(err));
            close(sock);
            sock = -1;
            break;
          }

          (void)fcntl(sock, F_SETFL, of); /* back to blocking */
          break;
        }

        if (sock >= 0) break; /* success for this ai */
    }

    if (sock < 0) {
        r->status = STATUS_TIMEOUT;
//        WARNING_PRINT("Connect timeout/fail to %s", host);
                ERROR_PRINT("Connect timeout/fail to %s", host);
        freeaddrinfo(res);
        r->req_time_end = time(NULL);
        return r;
    }

    /* align SO_SNDTIMEO with helper timeout */
    struct timeval sto;
    sto.tv_sec  = SEND_TIMEOUT_MS / 1000;
    sto.tv_usec = (SEND_TIMEOUT_MS % 1000) * 1000;
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &sto, sizeof(sto)) < 0) {
//        WARNING_PRINT("setsockopt(SO_SNDTIMEO) failed: %s", strerror(errno));
                ERROR_PRINT("setsockopt(SO_SNDTIMEO) failed: %s", strerror(errno));
    }

    if (send_all_with_timeout(sock, z, zlen, SEND_TIMEOUT_MS) != 0) {
        r->status = STATUS_ERROR;
        ERROR_PRINT("Send failed to %s: %s", host, strerror(errno));
    } else {
        r->status = STATUS_OK;
    }

    r->req_time_end = time(NULL);
//    DEBUG_PRINT("Host:%s | %s | %lds",
    ERROR_PRINT("Host:%s | %s | %lds",
                host,
                r->status == STATUS_OK ? "OK" :
                r->status == STATUS_TIMEOUT ? "TIMEOUT" : "ERROR",
                (long)(r->req_time_end - r->req_time_start));

    close(sock);
    freeaddrinfo(res);
    return r;
}

/* ---- simple worker pool ---- */
typedef struct {
    const char**    hosts;
    size_t          total;
    int             port;
    const uint8_t*  z;
    size_t          zlen;
    response_t**    responses;
    size_t          next_idx;
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
            response_t* r = (response_t*)calloc(1, sizeof(*r));
            if (r) { r->host = strdup(host); r->status = STATUS_ERROR; }
            ctx->responses[i] = r;
        }
    }
    return NULL;
}

/* ---- public entry: parallel fan-out ---- */
response_t** send_multi_request(const char** hosts, int port, const char* message) {
    /* count */
    size_t total_hosts = 0;
    while (hosts[total_hosts]) total_hosts++;

    response_t** responses = (response_t**)calloc(total_hosts + 1, sizeof(*responses));
    if (!responses) { perror("calloc responses"); return NULL; }
    if (total_hosts == 0) return responses; /* nothing to do */

    /* compress once */
    uint8_t* z = NULL; size_t zlen = 0;
    {
        size_t mlen = strlen(message) + 1;
        if (!compress_gzip_with_prefix((const unsigned char*)message, mlen, &z, &zlen)) {
            ERROR_PRINT("gzip failed");
            return responses; /* caller can still cleanup */
        }
    }

    work_ctx_t ctx;
    ctx.hosts     = hosts;
    ctx.total     = total_hosts;
    ctx.port      = port;
    ctx.z         = z;
    ctx.zlen      = zlen;
    ctx.responses = responses;
    ctx.next_idx  = 0;
    pthread_mutex_init(&ctx.mu, NULL);

    size_t nworkers = NET_MULTI_WORKERS;
    if (nworkers > total_hosts) nworkers = total_hosts ? total_hosts : 1;

    pthread_t* th = (pthread_t*)malloc(nworkers * sizeof(*th));
    if (!th) {
        pthread_mutex_destroy(&ctx.mu);
        free(z);
        return responses;
    }

    for (size_t t = 0; t < nworkers; ++t) pthread_create(&th[t], NULL, worker_fn, &ctx);
    for (size_t t = 0; t < nworkers; ++t) pthread_join(th[t], NULL);

    free(th);
    pthread_mutex_destroy(&ctx.mu);
    free(z);
    return responses;
}

/* ---- cleanup helper ---- */
void cleanup_responses(response_t** responses) {
    if (!responses) return;
    for (size_t i = 0; responses[i] != NULL; ++i) {
        free(responses[i]->host);
        free(responses[i]->data);
        free(responses[i]);
    }
    free(responses);
}