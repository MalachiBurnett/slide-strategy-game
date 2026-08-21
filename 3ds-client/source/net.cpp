/*
 * net.cpp — libcurl HTTP helpers for the 3DS Slide client.
 */
#include "net.h"
#include <3ds.h>

static constexpr int DNS_RETRY_ATTEMPTS = 3;
static constexpr u64 DNS_RETRY_DELAY_TICKS = CPU_TICKS_PER_MSEC * 500;

// ---------------------------------------------------------------------------
// CurlBuf helpers
// ---------------------------------------------------------------------------
static size_t curlWriteCb(char *ptr, size_t size, size_t nmemb, void *ud)
{
    CurlBuf *buf = (CurlBuf *)ud;
    size_t total = size * nmemb;
    if (buf->len + total + 1 > buf->cap)
    {
        size_t newcap = buf->cap + total + 256;
        char  *tmp    = (char *)realloc(buf->data, newcap);
        if (!tmp) return 0;
        buf->data = tmp;
        buf->cap  = newcap;
    }
    memcpy(buf->data + buf->len, ptr, total);
    buf->len += total;
    buf->data[buf->len] = 0;
    return total;
}

CurlBuf allocBuf()
{
    CurlBuf b;
    b.data = (char *)malloc(512);
    if (b.data) b.data[0] = 0;
    b.len = 0;
    b.cap = 512;
    return b;
}

void freeBuf(CurlBuf &b)
{
    free(b.data);
    b.data = nullptr;
    b.len = b.cap = 0;
}

// ---------------------------------------------------------------------------
// Shared curl setup
//
// The handle itself is owned by the calling worker thread and lives for the
// whole session; this only (re)configures it for one request.
// curl_easy_reset clears every option but deliberately keeps the handle's
// live connections, DNS cache and TLS session-ID cache, which is the entire
// point of holding onto it — see the note in net.h.
// ---------------------------------------------------------------------------
static void configureCurl(CURL *c, const char *url, CurlBuf &outBody,
                          char outCurlError[CURL_ERROR_SIZE])
{
    curl_easy_reset(c);

    outCurlError[0] = 0;

    curl_easy_setopt(c, CURLOPT_URL,                url);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION,      curlWriteCb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA,          &outBody);
    curl_easy_setopt(c, CURLOPT_TIMEOUT,            10L);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT,      8L);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION,      1L);
    curl_easy_setopt(c, CURLOPT_MAXREDIRS,           5L);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL,            1L);   // required for use from a worker thread
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER,      0L);   // 3DS lacks a CA bundle
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST,      0L);
    curl_easy_setopt(c, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4); // 3DS is IPv4 only
    curl_easy_setopt(c, CURLOPT_ERRORBUFFER,         outCurlError);

    // Resolve once and remember it. This used to be 0 — meaning "never cache"
    // rather than the intended "don't cache failures", which curl does not do
    // in the first place — so every request paid a fresh DNS round trip.
    curl_easy_setopt(c, CURLOPT_DNS_CACHE_TIMEOUT, 300L);
    // Explicit rather than load-bearing (both are curl defaults), but these
    // two are exactly what makes reusing the handle worth anything.
    curl_easy_setopt(c, CURLOPT_SSL_SESSIONID_CACHE, 1L);
    curl_easy_setopt(c, CURLOPT_FORBID_REUSE,        0L);
    // Probe an idle connection so a home router's NAT table does not quietly
    // drop it while the player is thinking about their move.
    curl_easy_setopt(c, CURLOPT_TCP_KEEPALIVE,  1L);
    curl_easy_setopt(c, CURLOPT_TCP_KEEPIDLE,  30L);
    curl_easy_setopt(c, CURLOPT_TCP_KEEPINTVL, 15L);
}

static CURLcode performWithDnsRetry(CURL *c, CurlBuf &outBody)
{
    CURLcode res = CURLE_OK;
    for (int attempt = 0; attempt < DNS_RETRY_ATTEMPTS; ++attempt)
    {
        res = curl_easy_perform(c);
        if (res != CURLE_COULDNT_RESOLVE_HOST || attempt + 1 >= DNS_RETRY_ATTEMPTS)
            return res;

        outBody.len = 0;
        if (outBody.data) outBody.data[0] = 0;
        svcSleepThread(DNS_RETRY_DELAY_TICKS);
    }
    return res;
}

// Shared tail: run the request on the worker's handle and read the status
// back off it. `c` is the caller's own long-lived handle, never cleaned up
// here — tearing it down is what would throw away the warm connection.
static long performOn(CURL *c, const char *verb, const char *url,
                      CurlBuf &outBody, CURLcode &outCurlCode,
                      char outCurlError[CURL_ERROR_SIZE])
{
    CURLcode res = performWithDnsRetry(c, outBody);
    outCurlCode  = res;

    printf("[net] %s %s -> CURLcode %d (%s)\n", verb, url, res, curl_easy_strerror(res));
    if (outCurlError[0]) printf("[net] error detail: %s\n", outCurlError);

    long code = 0;
    if (res == CURLE_OK)
        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);

    printf("[net] HTTP status: %ld\n", code);
    return code;
}

// ---------------------------------------------------------------------------
// GET
// ---------------------------------------------------------------------------
static long httpGet(CURL       *c,
                    const char *path,
                    CurlBuf    &outBody,
                    CURLcode   &outCurlCode,
                    char        outCurlError[CURL_ERROR_SIZE])
{
    if (!c) { outCurlCode = CURLE_FAILED_INIT; return 0; }

    char url[256];
    snprintf(url, sizeof(url), "%s%s", BASE_URL, path);

    configureCurl(c, url, outBody, outCurlError);
    return performOn(c, "GET", url, outBody, outCurlCode, outCurlError);
}

// ---------------------------------------------------------------------------
// POST (JSON body)
// ---------------------------------------------------------------------------
static long httpPost(CURL              *c,
                     struct curl_slist *jsonHeaders,
                     const char        *path,
                     const char        *jsonBody,
                     CurlBuf           &outBody,
                     CURLcode          &outCurlCode,
                     char               outCurlError[CURL_ERROR_SIZE])
{
    if (!c) { outCurlCode = CURLE_FAILED_INIT; return 0; }

    char url[256];
    snprintf(url, sizeof(url), "%s%s", BASE_URL, path);

    configureCurl(c, url, outBody, outCurlError);
    // configureCurl reset the handle back to GET, so setting the body is what
    // makes this a POST again.
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, jsonBody);
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, jsonHeaders);

    const long code = performOn(c, "POST", url, outBody, outCurlCode, outCurlError);
    // Drop the borrowed header list before returning: the worker owns it, and
    // leaving it referenced by an idle handle is asking for a dangling read.
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, nullptr);
    return code;
}

// ---------------------------------------------------------------------------
// Networking worker threads
//
// Two independent queue/thread pairs, each owning one long-lived libcurl
// handle, so the render/input loop never blocks on I/O:
//   g_priorityQueue — moves, queue/room join, login: latency-sensitive
//                     one-shot actions the player is actively waiting on.
//   g_pollQueue     — status polling, QR-login polling: recurring
//                     background checks that can tolerate extra latency.
// Splitting these matters because a single shared queue meant a move
// submitted while a status poll's curl_easy_perform was already running
// would sit blocked behind that poll's full round-trip before even starting.
// ---------------------------------------------------------------------------
struct NetQueue
{
    LightLock      lock;
    NetJob        *head;
    NetJob        *tail;
    LightSemaphore pending;
    volatile bool  shutdown;
    Thread         thread;
};

static NetQueue g_priorityQueue;
static NetQueue g_pollQueue;

static void netWorker(void *arg)
{
    NetQueue *q = (NetQueue *)arg;

    // Created here rather than in netThreadStart so the handle is only ever
    // touched by the one thread that owns it. It deliberately outlives every
    // individual request: that is what carries the open connection, the
    // resolved address and the TLS session across to the next one.
    CURL *curl = curl_easy_init();
    struct curl_slist *jsonHeaders =
        curl_slist_append(nullptr, "Content-Type: application/json");

    while (true)
    {
        LightSemaphore_Acquire(&q->pending, 1);

        LightLock_Lock(&q->lock);
        if (q->shutdown)
        {
            LightLock_Unlock(&q->lock);
            break;
        }
        NetJob *job = q->head;
        if (job)
        {
            q->head = job->next;
            if (!q->head) q->tail = nullptr;
        }
        LightLock_Unlock(&q->lock);
        if (!job) continue;

        long code = (job->op == NetOp::POST)
            ? httpPost(curl, jsonHeaders, job->path, job->body,
                       job->response, job->curlCode, job->curlError)
            : httpGet (curl, job->path, job->response, job->curlCode, job->curlError);
        job->httpCode = code;

        LightLock_Lock(&job->lock);
        job->done = true;
        LightLock_Unlock(&job->lock);
        LightEvent_Signal(&job->doneEvent);
    }

    curl_slist_free_all(jsonHeaders);
    if (curl) curl_easy_cleanup(curl);
}

static void queueInit(NetQueue &q)
{
    LightLock_Init(&q.lock);
    LightSemaphore_Init(&q.pending, 0, 32);
    q.shutdown = false;
    q.head = q.tail = nullptr;
    q.thread = nullptr;
}

static bool queueStop(NetQueue &q)
{
    if (!q.thread) return true;
    LightLock_Lock(&q.lock);
    q.shutdown = true;
    LightLock_Unlock(&q.lock);
    LightSemaphore_Release(&q.pending, 1);
    const s64 timeout = 3LL * 1000LL * 1000LL * 1000LL;
    const Result res = threadJoin(q.thread, timeout);
    threadFree(q.thread);
    q.thread = nullptr;
    return R_SUCCEEDED(res);
}

static void queueSubmit(NetQueue &q, NetJob *job)
{
    if (!job) return;
    LightLock_Lock(&q.lock);
    job->next = nullptr;
    if (q.tail) q.tail->next = job;
    else q.head = job;
    q.tail = job;
    LightLock_Unlock(&q.lock);
    LightSemaphore_Release(&q.pending, 1);
}

void netThreadStart()
{
    queueInit(g_priorityQueue);
    queueInit(g_pollQueue);
    g_priorityQueue.thread = threadCreate(netWorker, &g_priorityQueue, 0x20000, 0x30, -1, false);
    g_pollQueue.thread     = threadCreate(netWorker, &g_pollQueue,     0x20000, 0x30, -1, false);
}

bool netThreadStop()
{
    const bool a = queueStop(g_priorityQueue);
    const bool b = queueStop(g_pollQueue);
    return a && b;
}

NetJob *netJobCreate(NetOp op, const char *path, const char *body)
{
    NetJob *job = (NetJob *)malloc(sizeof(NetJob));
    if (!job) return nullptr;
    memset(job, 0, sizeof(*job));
    job->op = op;
    snprintf(job->path, sizeof(job->path), "%s", path ? path : "");
    if (body) snprintf(job->body, sizeof(job->body), "%s", body);
    job->response = allocBuf();
    LightLock_Init(&job->lock);
    LightEvent_Init(&job->doneEvent, RESET_STICKY);
    return job;
}

void netJobSubmit(NetJob *job)     { queueSubmit(g_priorityQueue, job); }
void netJobSubmitPoll(NetJob *job) { queueSubmit(g_pollQueue, job); }

void netJobWait(NetJob *job)
{
    if (!job) return;
    LightEvent_Wait(&job->doneEvent);
}

bool netJobReady(NetJob *job)
{
    if (!job) return true;
    LightLock_Lock(&job->lock);
    bool d = job->done;
    LightLock_Unlock(&job->lock);
    return d;
}

void netJobDestroy(NetJob *job)
{
    if (!job) return;
    freeBuf(job->response);
    free(job);
}

// ---------------------------------------------------------------------------
// NetCall / buildNetError
// ---------------------------------------------------------------------------
NetCall::NetCall(NetOp op, const char *path, const char *body)
{
    job = netJobCreate(op, path, body);
    if (job) { netJobSubmit(job); netJobWait(job); }
}

NetCall::~NetCall() { netJobDestroy(job); }

void buildNetError(char *out, int outLen,
                    long httpCode, CURLcode curlCode,
                    const char curlError[CURL_ERROR_SIZE])
{
    if (curlCode != CURLE_OK)
    {
        // Transport-level failure — show the curl symbolic name + detail string
        const char *detail = (curlError && curlError[0]) ? curlError
                                                         : curl_easy_strerror(curlCode);
        snprintf(out, outLen, "Network error %d: %s", (int)curlCode, detail);
    }
    else
    {
        snprintf(out, outLen, "Server error (HTTP %ld)", httpCode);
    }
}
