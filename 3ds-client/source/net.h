/*
 * net.h — HTTP helpers for the 3DS Slide client (HTTPS polling only).
 *
 * Key fixes vs the original monolithic main.cpp:
 *   • BASE_URL uses HTTPS (was HTTP — caused redirect/connection failure).
 *   • CURLOPT_FOLLOWLOCATION is enabled so 301/302 redirects are followed.
 *   • Both httpGet and httpPost return the CURLcode and the human-readable
 *     error buffer so callers can surface a meaningful error message.
 *
 * All HTTP work runs on two dedicated worker threads (see netThreadStart) so
 * the render/input loop never stalls on network I/O. Callers create a NetJob,
 * submit it, then either block on it (netJobWait) for one-shot menu calls or
 * poll it each frame (netJobReady) for gameplay polling / move sending.
 *
 * Two threads, not one: background polling (status/QR) and latency-sensitive
 * one-shot actions (moves, queue/room join, login) used to share a single
 * FIFO queue, so a move submitted while a status poll's curl_easy_perform
 * was already in flight would sit blocked behind it for the poll's full
 * round-trip. netJobSubmit now goes on a dedicated "priority" queue/thread;
 * netJobSubmitPoll is for the background pollers, which can tolerate the
 * extra latency.
 */
#ifndef NET_H
#define NET_H

#include <curl/curl.h>
#include <3ds.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>

// ---------------------------------------------------------------------------
// Dynamic response-body buffer
// ---------------------------------------------------------------------------
struct CurlBuf
{
    char   *data;
    size_t  len;
    size_t  cap;
};

CurlBuf  allocBuf();
void     freeBuf(CurlBuf &b);

// ---------------------------------------------------------------------------
// HTTP helpers
//
// Both functions return the HTTP status code (200, 400, …) on success.
// On a transport-level failure (DNS, TCP, TLS, timeout…) they return 0.
//
// outCurlCode  — set to the CURLcode so callers know exactly what failed.
// outCurlError — human-readable error string (CURL_ERROR_SIZE bytes);
//                valid on both success and failure.
// ---------------------------------------------------------------------------
static constexpr const char *BASE_URL = "https://slide.wiizardsoftware.uk";

long httpGet (const char *path,
              CurlBuf    &outBody,
              CURLcode   &outCurlCode,
              char        outCurlError[CURL_ERROR_SIZE]);

long httpPost(const char *path,
              const char *jsonBody,
              CurlBuf    &outBody,
              CURLcode   &outCurlCode,
              char        outCurlError[CURL_ERROR_SIZE]);

// ---------------------------------------------------------------------------
// Networking worker thread
// ---------------------------------------------------------------------------
enum class NetOp { GET, POST };

struct NetJob
{
    NetOp      op;
    char       path[128];
    char       body[768];
    long       httpCode;
    CURLcode   curlCode;
    char       curlError[CURL_ERROR_SIZE];
    CurlBuf    response;
    bool       done;
    LightLock  lock;
    LightEvent doneEvent;
    NetJob    *next;          // queue linkage (owned by the worker thread)
};

void    netThreadStart();
bool    netThreadStop();    // true if both workers actually exited before returning
NetJob *netJobCreate(NetOp op, const char *path, const char *body);
void    netJobSubmit(NetJob *job);      // enqueue on the priority queue, non-blocking
void    netJobSubmitPoll(NetJob *job);  // enqueue on the background-polling queue
void    netJobWait(NetJob *job);        // block until the job is done
bool    netJobReady(NetJob *job);       // non-blocking "is it done yet?"
void    netJobDestroy(NetJob *job);     // frees response buffer + job

// ---------------------------------------------------------------------------
// Blocking one-shot request helper. Submits a job on the network thread and
// waits for it. The response buffer lives until the helper goes out of scope,
// so parse the result before that.
// ---------------------------------------------------------------------------
struct NetCall
{
    NetJob *job;
    NetCall(NetOp op, const char *path, const char *body = nullptr);
    ~NetCall();
    long        code() const { return job ? job->httpCode : 0; }
    const char *data() const { return job ? job->response.data : nullptr; }
    CURLcode    curl() const { return job ? job->curlCode : CURLE_FAILED_INIT; }
    const char *cerr() const { return job ? job->curlError : ""; }
};

// Builds a human-readable error string from an HTTP/curl result: a curl
// transport failure (DNS, TLS, timeout…) reports the symbolic name + detail,
// otherwise it reports the HTTP status code.
void buildNetError(char *out, int outLen,
                    long httpCode, CURLcode curlCode,
                    const char curlError[CURL_ERROR_SIZE]);

#endif // NET_H

