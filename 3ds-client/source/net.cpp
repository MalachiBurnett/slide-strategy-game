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

SocketIoClient::SocketIoClient() : curl(nullptr), connected(false), receiveLength(0) {}

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
                     "https://slide.wiizardsoftware.uk/socket.io/?EIO=4&transport=websocket");
    curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 1L);
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

    const char upgradeRequest[] =
        "GET /socket.io/?EIO=4&transport=websocket HTTP/1.1\r\n"
        "Host: slide.wiizardsoftware.uk\r\n"
        "Origin: https://slide.wiizardsoftware.uk\r\n"
        "User-Agent: Slide-3DS/1.0\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n";
    size_t sent = 0;
    result = curl_easy_send(curl, upgradeRequest, sizeof(upgradeRequest) - 1, &sent);
    if (result != CURLE_OK || sent != sizeof(upgradeRequest) - 1)
    {
        snprintf(error, errorLen, "Socket.IO HTTP upgrade send failed (%d): %s",
                 (int)result, curl_easy_strerror(result));
        close();
        return false;
    }

    char response[1024] = {};
    size_t responseLength = 0;
    u64 deadline = svcGetSystemTick() + CPU_TICKS_PER_MSEC * 8000;
    while (responseLength + 1 < sizeof(response) && svcGetSystemTick() < deadline)
    {
        size_t received = 0;
        result = curl_easy_recv(curl, response + responseLength,
                                sizeof(response) - responseLength - 1, &received);
        if (result == CURLE_AGAIN)
        {
            svcSleepThread(CPU_TICKS_PER_MSEC * 10);
            continue;
        }
        if (result != CURLE_OK)
        {
            snprintf(error, errorLen, "Socket.IO HTTP upgrade receive failed (%d): %s",
                     (int)result, curl_easy_strerror(result));
            close();
            return false;
        }
        if (received == 0)
        {
            snprintf(error, errorLen, "Socket.IO upgrade peer closed before sending a response");
            close();
            return false;
        }
        responseLength += received;
        response[responseLength] = 0;
        if (strstr(response, "\r\n\r\n")) break;
    }
    if (strncmp(response, "HTTP/1.1 101", 12) != 0 &&
        strncmp(response, "HTTP/1.0 101", 12) != 0)
    {
        snprintf(error, errorLen, "Socket.IO upgrade rejected (%lu bytes): %.160s",
                 (unsigned long)responseLength, response);
        close();
        return false;
    }

    const char *headerEnd = strstr(response, "\r\n\r\n");
    const size_t headerBytes = (size_t)(headerEnd - response) + 4;
    const size_t leftoverBytes = responseLength - headerBytes;
    if (leftoverBytes > sizeof(receiveBuffer))
    {
        snprintf(error, errorLen, "Socket.IO handshake frame buffer overflow");
        close();
        return false;
    }
    if (leftoverBytes > 0)
        memcpy(receiveBuffer, response + headerBytes, leftoverBytes);
    receiveLength = leftoverBytes;

    connected = true;
    const char namespaceConnect[] = "40";
    if (!sendRaw(namespaceConnect, error, errorLen))
    {
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
            char ignored[1] = {};
            sendFrame(0x8, nullptr, 0, ignored, sizeof(ignored));
        }
        curl_easy_cleanup(curl);
    }
    curl = nullptr;
    connected = false;
    receiveLength = 0;
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

    char packet[1024];
    snprintf(packet, sizeof(packet), "42[\"%s\",%s]", event,
             jsonData && jsonData[0] ? jsonData : "null");
    return sendRaw(packet, error, errorLen);
}

bool SocketIoClient::sendRaw(const char *packet, char *error, int errorLen)
{
    return sendFrame(0x1, packet, strlen(packet), error, errorLen);
}

bool SocketIoClient::sendFrame(unsigned char opcode, const void *payload,
                               size_t payloadLength, char *error, int errorLen)
{
    if (!connected || !curl)
    {
        snprintf(error, errorLen, "Socket.IO is not connected");
        return false;
    }
    if (payloadLength > 1024)
    {
        snprintf(error, errorLen, "Socket.IO packet is too large");
        return false;
    }
    unsigned char frame[2 + 2 + 4 + 1024];
    frame[0] = 0x80 | (opcode & 0x0f);
    size_t headerLength = 2;
    if (payloadLength <= 125)
    {
        frame[1] = 0x80 | (unsigned char)payloadLength;
    }
    else
    {
        frame[1] = 0x80 | 126;
        frame[2] = (unsigned char)(payloadLength >> 8);
        frame[3] = (unsigned char)payloadLength;
        headerLength = 4;
    }
    unsigned char mask[4] = {0x53, 0x6c, 0x69, 0x64};
    memcpy(frame + headerLength, mask, sizeof(mask));
    const unsigned char *bytes = (const unsigned char *)payload;
    for (size_t i = 0; i < payloadLength; ++i)
        frame[headerLength + 4 + i] = bytes[i] ^ mask[i % 4];

    const size_t frameLength = headerLength + 4 + payloadLength;
    size_t offset = 0;
    while (offset < frameLength)
    {
        size_t sent = 0;
        CURLcode result = curl_easy_send(curl, frame + offset,
                                         frameLength - offset, &sent);
        if (result == CURLE_AGAIN)
        {
            svcSleepThread(CPU_TICKS_PER_MSEC * 2);
            continue;
        }
        if (result != CURLE_OK || sent == 0)
        {
            snprintf(error, errorLen, "Socket.IO send failed (%d): %s",
                     (int)result, curl_easy_strerror(result));
            return false;
        }
        offset += sent;
    }
    return true;
}

bool SocketIoClient::receive(char *out, int outLen, char *error, int errorLen)
{
    if (!connected || !curl) return false;
    if (!out || outLen < 2) return false;

    size_t received = 0;
    CURLcode result = curl_easy_recv(curl, receiveBuffer + receiveLength,
                                     sizeof(receiveBuffer) - receiveLength, &received);
    if (result == CURLE_AGAIN) return false;
    if (result != CURLE_OK)
    {
        snprintf(error, errorLen, "Socket.IO receive failed (%d): %s",
                 (int)result, curl_easy_strerror(result));
        connected = false;
        return false;
    }
    receiveLength += received;
    if (receiveLength < 2) return false;

    const unsigned char first = receiveBuffer[0];
    const unsigned char second = receiveBuffer[1];
    const unsigned int opcode = first & 0x0f;
    size_t headerLength = 2;
    size_t payloadLength = second & 0x7f;
    if (payloadLength == 126)
    {
        if (receiveLength < 4) return false;
        payloadLength = ((size_t)receiveBuffer[2] << 8) | receiveBuffer[3];
        headerLength = 4;
    }
    else if (payloadLength == 127)
    {
        snprintf(error, errorLen, "Socket.IO frame is too large");
        connected = false;
        return false;
    }
    if (second & 0x80) headerLength += 4;
    if (receiveLength < headerLength + payloadLength) return false;
    const size_t framePayloadLength = payloadLength;

    if (opcode == 0x8)
    {
        snprintf(error, errorLen, "Socket.IO server closed the connection");
        connected = false;
        return false;
    }
    if (opcode == 0x9)
    {
        char pongError[128] = {};
        if (!sendFrame(0xA, receiveBuffer + headerLength, payloadLength,
                       pongError, sizeof(pongError)))
        {
            snprintf(error, errorLen, "%s", pongError);
            connected = false;
            return false;
        }
    }
    else if (opcode == 0x1)
    {
        size_t outputLength = payloadLength;
        if ((int)outputLength >= outLen) outputLength = outLen - 1;
        memcpy(out, receiveBuffer + headerLength, outputLength);
        out[outputLength] = 0;
    }
    size_t frameLength = headerLength + framePayloadLength;
    if (frameLength < receiveLength)
        memmove(receiveBuffer, receiveBuffer + frameLength, receiveLength - frameLength);
    receiveLength -= frameLength;
    return opcode == 0x1 && framePayloadLength > 0;
}
