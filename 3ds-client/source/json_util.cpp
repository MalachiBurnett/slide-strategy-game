/*
 * json_util.cpp — see json_util.h.
 */
#include "json_util.h"

#include <cstdio>
#include <cstring>

bool jsonExtract(const char *json, const char *key, char *outVal, int maxVal)
{
    if (!json || !key || !outVal) return false;
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    const char *p = strstr(json, pat);
    if (!p) return false;
    p += strlen(pat);
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
    if (*p == '"')
    {
        ++p;
        int i = 0;
        while (*p && *p != '"' && i < maxVal - 1)
        {
            if (*p == '\\') ++p; // skip escape prefix
            outVal[i++] = *p++;
        }
        outVal[i] = 0;
        return true;
    }
    else
    {
        int i = 0;
        while (*p && *p != ',' && *p != '}' && *p != '\n' && i < maxVal - 1)
            outVal[i++] = *p++;
        outVal[i] = 0;
        return i > 0;
    }
}

bool parseBoard(const char *json, char board[6][6])
{
    const char *p = strstr(json, "\"board\":[");
    if (!p) return false;
    int count = 0;
    for (; *p && count < 36; ++p)
    {
        if (*p == 'W' || *p == 'B' || *p == '0')
        {
            board[count / 6][count % 6] = *p;
            ++count;
        }
    }
    return count == 36;
}
