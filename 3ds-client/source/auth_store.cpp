/*
 * auth_store.cpp — SD card persistence for the 32-char auth code.
 */
#include "auth_store.h"

#include <cstdio>
#include <cstring>
#include <sys/stat.h>

bool loadAuthCode(char *out)
{
    FILE *f = fopen(AUTHCODE_PATH, "r");
    if (!f) return false;
    bool ok = fgets(out, AUTHCODE_LEN + 1, f) != nullptr;
    fclose(f);
    if (ok)
    {
        int len = (int)strlen(out);
        while (len > 0 && (out[len-1] == '\n' || out[len-1] == '\r'))
            out[--len] = 0;
        ok = (len == AUTHCODE_LEN);
    }
    return ok;
}

bool saveAuthCode(const char *code)
{
    mkdir("/3ds",       0777);
    mkdir("/3ds/slide", 0777);
    FILE *f = fopen(AUTHCODE_PATH, "w");
    if (!f) return false;
    fprintf(f, "%s\n", code);
    fclose(f);
    return true;
}

void deleteAuthCode()
{
    remove(AUTHCODE_PATH);
}
