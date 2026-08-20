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
// ---------------------------------------------------------------------------
static CURL *makeCurl(const char *url, CurlBuf &outBody,
                      char outCurlError[CURL_ERROR_SIZE])
{
    CURL *c = curl_easy_init();
    if (!c) return nullptr;

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
    curl_easy_setopt(c, CURLOPT_DNS_CACHE_TIMEOUT,   0L);   // don't cache failed DNS
    curl_easy_setopt(c, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4); // 3DS is IPv4 only
    curl_easy_setopt(c, CURLOPT_ERRORBUFFER,         outCurlError);

    return c;
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

// ---------------------------------------------------------------------------
// GET
// ---------------------------------------------------------------------------
long httpGet(const char *path,
             CurlBuf    &outBody,
             CURLcode   &outCurlCode,
             char        outCurlError[CURL_ERROR_SIZE])
{
    char url[256];
    snprintf(url, sizeof(url), "%s%s", BASE_URL, path);

    CURL *c = makeCurl(url, outBody, outCurlError);
    if (!c) { outCurlCode = CURLE_FAILED_INIT; return 0; }

    CURLcode res = performWithDnsRetry(c, outBody);
    outCurlCode  = res;

    printf("[net] GET %s -> CURLcode %d (%s)\n", url, res, curl_easy_strerror(res));
    if (outCurlError[0]) printf("[net] error detail: %s\n", outCurlError);

    long code = 0;
    if (res == CURLE_OK)
        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);

    printf("[net] HTTP status: %ld\n", code);
    curl_easy_cleanup(c);
    return code;
}

// ---------------------------------------------------------------------------
// POST (JSON body)
// ---------------------------------------------------------------------------
long httpPost(const char *path,
              const char *jsonBody,
              CurlBuf    &outBody,
              CURLcode   &outCurlCode,
              char        outCurlError[CURL_ERROR_SIZE])
{
    char url[256];
    snprintf(url, sizeof(url), "%s%s", BASE_URL, path);

    CURL *c = makeCurl(url, outBody, outCurlError);
    if (!c) { outCurlCode = CURLE_FAILED_INIT; return 0; }

    struct curl_slist *hdrs = nullptr;
    hdrs = curl_slist_append(hdrs, "Content-Type: application/json");

    curl_easy_setopt(c, CURLOPT_POSTFIELDS, jsonBody);
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);

    CURLcode res = performWithDnsRetry(c, outBody);
    outCurlCode  = res;

    printf("[net] POST %s -> CURLcode %d (%s)\n", url, res, curl_easy_strerror(res));
    if (outCurlError[0]) printf("[net] error detail: %s\n", outCurlError);

    long code = 0;
    if (res == CURLE_OK)
        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);

    printf("[net] HTTP status: %ld\n", code);
    curl_slist_free_all(hdrs);
    curl_easy_cleanup(c);
    return code;
}

// ---------------------------------------------------------------------------
// Networking worker thread
//
// A single thread owns every libcurl handle (each request creates and frees
// its own handle), so the render/input loop never blocks on I/O. The main
// thread enqueues jobs; the worker executes them and signals completion.
// ---------------------------------------------------------------------------
static LightLock       g_queueLock;
static NetJob         *g_queueHead = nullptr;
static NetJob         *g_queueTail = nullptr;
static LightSemaphore  g_pendingJobs;
static volatile bool   g_shutdown = false;
static Thread          g_thread = nullptr;

static void netWorker(void *arg)
{
    (void)arg;
    while (true)
    {
        LightSemaphore_Acquire(&g_pendingJobs, 1);

        LightLock_Lock(&g_queueLock);
        if (g_shutdown)
        {
            LightLock_Unlock(&g_queueLock);
            break;
        }
        NetJob *job = g_queueHead;
        if (job)
        {
            g_queueHead = job->next;
            if (!g_queueHead) g_queueTail = nullptr;
        }
        LightLock_Unlock(&g_queueLock);
        if (!job) continue;

        long code = (job->op == NetOp::POST)
            ? httpPost(job->path, job->body, job->response, job->curlCode, job->curlError)
            : httpGet (job->path, job->response, job->curlCode, job->curlError);
        job->httpCode = code;

        LightLock_Lock(&job->lock);
        job->done = true;
        LightLock_Unlock(&job->lock);
        LightEvent_Signal(&job->doneEvent);
    }
}

void netThreadStart()
{
    LightLock_Init(&g_queueLock);
    LightSemaphore_Init(&g_pendingJobs, 0, 32);
    g_shutdown = false;
    g_queueHead = g_queueTail = nullptr;
    g_thread = threadCreate(netWorker, nullptr, 0x20000, 0x30, -1, false);
}

bool netThreadStop()
{
    if (!g_thread) return true;
    LightLock_Lock(&g_queueLock);
    g_shutdown = true;
    LightLock_Unlock(&g_queueLock);
    LightSemaphore_Release(&g_pendingJobs, 1);
    const s64 timeout = 3LL * 1000LL * 1000LL * 1000LL;
    const Result res = threadJoin(g_thread, timeout);
    threadFree(g_thread);
    g_thread = nullptr;
    return R_SUCCEEDED(res);
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

void netJobSubmit(NetJob *job)
{
    if (!job) return;
    LightLock_Lock(&g_queueLock);
    job->next = nullptr;
    if (g_queueTail) g_queueTail->next = job;
    else g_queueHead = job;
    g_queueTail = job;
    LightLock_Unlock(&g_queueLock);
    LightSemaphore_Release(&g_pendingJobs, 1);
}

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


