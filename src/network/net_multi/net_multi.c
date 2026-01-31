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
static response_t* OLD_send_to_one_host(const char* host, int port,
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
        WARNING_PRINT("Connect timeout/fail to %s", host);
        freeaddrinfo(res);
        r->req_time_end = time(NULL);
        return r;
    }

    /* align SO_SNDTIMEO with helper timeout */
    struct timeval sto;
    sto.tv_sec  = SEND_TIMEOUT_MS / 1000;
    sto.tv_usec = (SEND_TIMEOUT_MS % 1000) * 1000;
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &sto, sizeof(sto)) < 0) {
        WARNING_PRINT("setsockopt(SO_SNDTIMEO) failed: %s", strerror(errno));
    }

    if (send_all_with_timeout(sock, z, zlen, SEND_TIMEOUT_MS) != 0) {
        r->status = STATUS_ERROR;
        ERROR_PRINT("Send failed to %s: %s", host, strerror(errno));
    } else {
        r->status = STATUS_OK;
    }

    r->req_time_end = time(NULL);
    DEBUG_PRINT("Host:%s | %s | %lds",
                host,
                r->status == STATUS_OK ? "OK" :
                r->status == STATUS_TIMEOUT ? "TIMEOUT" : "ERROR",
                (long)(r->req_time_end - r->req_time_start));

    close(sock);
    freeaddrinfo(res);
    return r;
}









#ifndef CONNECT_RETRY_COUNT
#define CONNECT_RETRY_COUNT 2   /* total attempts per addr: 2 = one retry */
#endif

#ifndef CONNECT_RETRY_JITTER_MS
#define CONNECT_RETRY_JITTER_MS 120
#endif

static long monotonic_ms_now(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long)(ts.tv_sec * 1000L + ts.tv_nsec / 1000000L);
}

static void sleep_ms(int ms)
{
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (long)(ms % 1000) * 1000000L;
  nanosleep(&ts, NULL);
}

static void addr_to_ipstr(const struct addrinfo* ai, char* out, size_t out_sz)
{
  if (!out || out_sz == 0) return;
  out[0] = '\0';

  if (!ai || !ai->ai_addr) return;

  if (ai->ai_family == AF_INET) {
    const struct sockaddr_in* in = (const struct sockaddr_in*)ai->ai_addr;
    inet_ntop(AF_INET, &in->sin_addr, out, out_sz);
  } else if (ai->ai_family == AF_INET6) {
    const struct sockaddr_in6* in6 = (const struct sockaddr_in6*)ai->ai_addr;
    inet_ntop(AF_INET6, &in6->sin6_addr, out, out_sz);
  }
}

static int set_nonblocking(int fd, int* out_old_flags)
{
  int of = fcntl(fd, F_GETFL, 0);
  if (of == -1) of = 0;
  if (out_old_flags) *out_old_flags = of;
  return fcntl(fd, F_SETFL, of | O_NONBLOCK);
}

static void restore_blocking(int fd, int old_flags)
{
  (void)fcntl(fd, F_SETFL, old_flags);
}

/*
 * Attempt connect with a monotonic deadline (timeout_ms).
 * Returns 0 on success, -1 on failure and sets *out_err to the failure reason
 * (ETIMEDOUT, ECONNREFUSED, etc.).
 */
static int connect_with_deadline(int sock,
                                 const struct sockaddr* sa,
                                 socklen_t slen,
                                 int timeout_ms,
                                 int* out_err)
{
  if (out_err) *out_err = 0;

  int old_flags = 0;
  if (set_nonblocking(sock, &old_flags) != 0) {
    if (out_err) *out_err = errno;
    return -1;
  }

  int rc = connect(sock, sa, slen);
  if (rc == 0) {
    restore_blocking(sock, old_flags);
    return 0;
  }

  if (errno == EISCONN) {
    restore_blocking(sock, old_flags);
    return 0;
  }

  if (errno != EINPROGRESS && errno != EALREADY) {
    if (out_err) *out_err = errno;
    return -1;
  }

  const long deadline = monotonic_ms_now() + (long)timeout_ms;

  for (;;) {
    long now = monotonic_ms_now();
    long remaining = deadline - now;
    if (remaining <= 0) {
      if (out_err) *out_err = ETIMEDOUT;
      return -1;
    }

    struct timeval tv;
    tv.tv_sec  = (time_t)(remaining / 1000L);
    tv.tv_usec = (suseconds_t)((remaining % 1000L) * 1000L);

    fd_set wf;
    FD_ZERO(&wf);
    FD_SET(sock, &wf);

    int sel = select(sock + 1, NULL, &wf, NULL, &tv);
    if (sel < 0 && errno == EINTR) continue;

    if (sel < 0) {
      if (out_err) *out_err = errno;
      return -1;
    }

    if (sel == 0 || !FD_ISSET(sock, &wf)) {
      if (out_err) *out_err = ETIMEDOUT;
      return -1;
    }

    int soerr = 0;
    socklen_t sl = sizeof(soerr);
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &soerr, &sl) != 0) {
      if (out_err) *out_err = errno;
      return -1;
    }

    if (soerr != 0) {
      if (out_err) *out_err = soerr;
      return -1;
    }

    restore_blocking(sock, old_flags);
    return 0;
  }
}

static response_t* send_to_one_host(const char* host, int port,
                                    const uint8_t* z, size_t zlen)
{
  response_t* r = (response_t*)calloc(1, sizeof(*r));
  if (!r) return NULL;

  r->host = strdup(host);
  r->status = STATUS_PENDING;
  r->req_time_start = time(NULL);

  char port_str[6];
  snprintf(port_str, sizeof(port_str), "%d", port);

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family   = AF_UNSPEC;     /* allow v4/v6 */
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags    = AI_NUMERICSERV | AI_ADDRCONFIG;

  struct addrinfo* res = NULL;
  int gai = getaddrinfo(host, port_str, &hints, &res);
  if (gai != 0 || !res) {
    r->status = STATUS_ERROR;
    ERROR_PRINT("DNS resolution failed for %s: %s", host, gai_strerror(gai));
    r->req_time_end = time(NULL);
    return r;
  }

  int sock = -1;
  int last_err = 0;
  char last_ip[INET6_ADDRSTRLEN] = {0};

  for (struct addrinfo* ai = res; ai; ai = ai->ai_next) {
    char ipstr[INET6_ADDRSTRLEN] = {0};
    addr_to_ipstr(ai, ipstr, sizeof(ipstr));

    /* We only support stream sockets; skip others defensively */
    if (ai->ai_socktype != SOCK_STREAM) continue;

    /* Attempt connect (with one retry on timeout-ish errors) */
    for (int attempt = 0; attempt < CONNECT_RETRY_COUNT; attempt++) {
      sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
      if (sock < 0) {
        last_err = errno;
        continue;
      }

      /* harmless tuning */
      int one = 1;
      (void)setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
      (void)setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));

      INFO_PRINT("connect attempt host=%s port=%d ip=%s try=%d/%d",
                 host, port, ipstr[0] ? ipstr : "?", attempt + 1, CONNECT_RETRY_COUNT);

      int ce = 0;
      int timeout_ms = CONNECT_TIMEOUT_SEC * 1000;
      if (connect_with_deadline(sock, ai->ai_addr, ai->ai_addrlen, timeout_ms, &ce) == 0) {
        last_err = 0;
        snprintf(last_ip, sizeof(last_ip), "%s", ipstr);
        goto connected;
      }

      last_err = ce;
      snprintf(last_ip, sizeof(last_ip), "%s", ipstr);

      ERROR_PRINT("connect failed host=%s port=%d ip=%s err=%d (%s)",
                  host, port, ipstr[0] ? ipstr : "?", ce, strerror(ce));

      close(sock);
      sock = -1;

      /* Retry only on transient-ish cases */
      if (attempt + 1 < CONNECT_RETRY_COUNT &&
          (ce == ETIMEDOUT || ce == EHOSTUNREACH || ce == ENETUNREACH)) {
        /* small jitter helps avoid stampedes */
        sleep_ms(CONNECT_RETRY_JITTER_MS);
        continue;
      }

      break;
    }
  }

connected:
  freeaddrinfo(res);

  if (sock < 0) {
    /* classify a little better */
    if (last_err == ETIMEDOUT) r->status = STATUS_TIMEOUT;
    else r->status = STATUS_ERROR;

    WARNING_PRINT("Connect failed to %s:%d ip=%s final_err=%d (%s)",
                  host, port, last_ip[0] ? last_ip : "?", last_err, strerror(last_err));

    r->req_time_end = time(NULL);
    return r;
  }

  /* align SO_SNDTIMEO with helper timeout */
  struct timeval sto;
  sto.tv_sec  = SEND_TIMEOUT_MS / 1000;
  sto.tv_usec = (SEND_TIMEOUT_MS % 1000) * 1000;
  if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &sto, sizeof(sto)) < 0) {
    WARNING_PRINT("setsockopt(SO_SNDTIMEO) failed: %s", strerror(errno));
  }

  if (send_all_with_timeout(sock, z, zlen, SEND_TIMEOUT_MS) != 0) {
    r->status = STATUS_ERROR;
    ERROR_PRINT("Send failed to %s: %s", host, strerror(errno));
  } else {
    r->status = STATUS_OK;
  }

  r->req_time_end = time(NULL);
  DEBUG_PRINT("Host:%s | %s | %lds",
              host,
              r->status == STATUS_OK ? "OK" :
              r->status == STATUS_TIMEOUT ? "TIMEOUT" : "ERROR",
              (long)(r->req_time_end - r->req_time_start));

  close(sock);
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
    size_t launch_seq;
} work_ctx_t;

static void sleep_ms(int ms)
{
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (long)(ms % 1000) * 1000000L;
  nanosleep(&ts, NULL);
}

static void* worker_fn(void* arg) {
  work_ctx_t* ctx = (work_ctx_t*)arg;

  for (;;) {
    size_t i, seq = 0;
    pthread_mutex_lock(&ctx->mu);
    i = ctx->next_idx++;
    if (i < ctx->total) {
        seq = ctx->launch_seq++;
    }
    pthread_mutex_unlock(&ctx->mu);

    if (i >= ctx->total) {
        break;
    }

    // Stagger only for DPoPS traffic (optional but recommended)
    if (ctx->port == XCASH_DPOPS_PORT) {
      // deterministic spread: 12..32ms
      int delay_ms = 12 + (int)(seq % 6) * 4;  // 12,16,20,24,28,32 repeat
      sleep_ms(delay_ms);
    }

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
            responses[total_hosts] = NULL;
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
    ctx.launch_seq = 0;
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
    z = NULL;

    responses[total_hosts] = NULL;
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