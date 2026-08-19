/*
 * net.h — HTTP helpers for the 3DS Slide client (HTTPS polling only).
 *
 * Key fixes vs the original monolithic main.cpp:
 *   • BASE_URL uses HTTPS (was HTTP — caused redirect/connection failure).
 *   • CURLOPT_FOLLOWLOCATION is enabled so 301/302 redirects are followed.
 *   • Both httpGet and httpPost return the CURLcode and the human-readable
 *     error buffer so callers can surface a meaningful error message.
 */
#pragma once

#include <curl/curl.h>
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


