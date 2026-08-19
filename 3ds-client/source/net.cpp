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

SocketIoClient::SocketIoClient() : curl(nullptr), connected(false) {}

SocketIoClient::~SocketIoClient()
{
    close();
}

bool SocketIoClient::connect(char *error, int errorLen)
{
    close();
    if (error && errorLen > 0) error[0] = 0;

    curl = curl_easy_init();
    if (!curl)
    {
        snprintf(error, errorLen, "Socket.IO: curl initialization failed");
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL,
                     "wss://slide.wiizardsoftware.uk/socket.io/?EIO=4&transport=websocket");
    curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 2L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

    CURLcode result = curl_easy_perform(curl);
    if (result != CURLE_OK)
    {
        snprintf(error, errorLen, "Socket.IO connect failed (%d): %s",
                 (int)result, curl_easy_strerror(result));
        close();
        return false;
    }

    connected = true;
    size_t sent = 0;
    const char namespaceConnect[] = "40";
    result = curl_ws_send(curl, namespaceConnect, sizeof(namespaceConnect) - 1,
                          &sent, 0, CURLWS_TEXT);
    if (result != CURLE_OK)
    {
        snprintf(error, errorLen, "Socket.IO handshake failed (%d): %s",
                 (int)result, curl_easy_strerror(result));
        close();
        return false;
    }
    return true;
}

void SocketIoClient::close()
{
    if (curl)
    {
        if (connected)
        {
            size_t sent = 0;
            const char closeFrame[] = "41";
            curl_ws_send(curl, closeFrame, sizeof(closeFrame) - 1,
                         &sent, 0, CURLWS_TEXT);
        }
        curl_easy_cleanup(curl);
    }
    curl = nullptr;
    connected = false;
}

bool SocketIoClient::isConnected() const
{
    return connected;
}

bool SocketIoClient::sendEvent(const char *event, const char *jsonData,
                               char *error, int errorLen)
{
    if (!connected || !curl)
    {
        snprintf(error, errorLen, "Socket.IO is not connected");
        return false;
    }

    char packet[512];
    snprintf(packet, sizeof(packet), "42[\"%s\",%s]", event,
             jsonData && jsonData[0] ? jsonData : "null");
    return sendRaw(packet, error, errorLen);
}

bool SocketIoClient::sendRaw(const char *packet, char *error, int errorLen)
{
    if (!connected || !curl)
    {
        snprintf(error, errorLen, "Socket.IO is not connected");
        return false;
    }
    size_t sent = 0;
    CURLcode result = curl_ws_send(curl, packet, strlen(packet), &sent, 0, CURLWS_TEXT);
    if (result != CURLE_OK || sent != strlen(packet))
    {
        snprintf(error, errorLen, "Socket.IO send failed (%d): %s",
                 (int)result, curl_easy_strerror(result));
        return false;
    }
    return true;
}

bool SocketIoClient::receive(char *out, int outLen, char *error, int errorLen)
{
    if (!connected || !curl) return false;
    if (!out || outLen < 2) return false;

    size_t received = 0;
    const struct curl_ws_frame *meta = nullptr;
    CURLcode result = curl_ws_recv(curl, out, outLen - 1, &received, &meta);
    if (result == CURLE_AGAIN) return false;
    if (result != CURLE_OK)
    {
        snprintf(error, errorLen, "Socket.IO receive failed (%d): %s",
                 (int)result, curl_easy_strerror(result));
        connected = false;
        return false;
    }
    if (meta && (meta->flags & CURLWS_CLOSE))
    {
        snprintf(error, errorLen, "Socket.IO server closed the connection");
        connected = false;
        return false;
    }
    out[received] = 0;
    return received > 0;
}
