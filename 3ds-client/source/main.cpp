/*
 * main.cpp — Slide 3DS Client — application entry point & state machine.
 *
 * Login flow:
 *   1. Try saved authCode from SD card → POST /api/auth_code_login
 *   2. GET  /api/external_login        → receive code + loginUrl
 *   3. Show QR code (top-screen right) + "Scan to log in" text (top-screen left)
 *   4. Bottom screen: [Sign in on this device]  [Quit]
 *   5. Poll POST /api/external_login/poll every ~2 s
 *   6. On approval: save authCode to SD card, enter game
 *   7. "Sign in on this device" → swkbd username + password → POST /api/login
 *
 * Modules:
 *   render      — pixel / text / QR drawing (render.h / render.cpp)
 *   font8x8     — 8×8 bitmap font data     (font8x8.h)
 *   net         — HTTP GET / POST via curl  (net.h / net.cpp)
 *   auth_store  — SD card auth code         (auth_store.h / auth_store.cpp)
 *   ui          — Button + screen layouts   (ui.h / ui.cpp)
 */

#include <3ds.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <malloc.h>
#include <curl/curl.h>

#include "render.h"
#include "net.h"
#include "auth_store.h"
#include "ui.h"

extern "C" {
#include "qrcodegen.h"
}

// ---------------------------------------------------------------------------
// Tiny JSON field extractor — no heap allocation, no full parser.
// Extracts the string or scalar value for `key` in a flat JSON object.
// Returns true and fills outVal[maxVal] (null-terminated) on success.
// ---------------------------------------------------------------------------
static bool jsonExtract(const char *json, const char *key, char *outVal, int maxVal)
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

// ---------------------------------------------------------------------------
// swkbd helper
// ---------------------------------------------------------------------------
static bool showKeyboard(const char *hint, char *buf, int bufLen,
                         bool isPassword = false)
{
    SwkbdState kbd;
    swkbdInit(&kbd, SWKBD_TYPE_QWERTY, 2, bufLen - 1);
    swkbdSetHintText(&kbd, hint);
    swkbdSetValidation(&kbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    if (isPassword)
        swkbdSetPasswordMode(&kbd, SWKBD_PASSWORD_HIDE_DELAY);
    SwkbdButton btn = swkbdInputText(&kbd, buf, bufLen);
    return btn == SWKBD_BUTTON_CONFIRM;
}

// ---------------------------------------------------------------------------
// Helpers: build a verbose error string from an HTTP result
// ---------------------------------------------------------------------------
static void buildNetError(char *out, int outLen,
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

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    gfxInitDefault();

    // SOC init — buffer must be page-aligned via memalign (NOT linearAlloc;
    // linearAlloc is GPU/DSP memory and the SOC service rejects it).
    static constexpr u32 SOC_ALIGN      = 0x1000;
    static constexpr u32 SOC_BUF_SIZE   = 0x100000; // 1 MB
    u32 *socBuf = (u32 *)memalign(SOC_ALIGN, SOC_BUF_SIZE);
    if (!socBuf)
    {
        uint8_t *fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, nullptr, nullptr);
        clearScreen(fb, TOP_W, TOP_H, C_BG);
        drawText(fb, TOP_W, TOP_H, 8, 8, "Fatal: out of memory for SOC", 1, C_ERROR);
        gfxFlushBuffers(); gfxSwapBuffers(); gspWaitForVBlank();
        gfxExit();
        return 1;
    }

    Result socResult = socInit(socBuf, SOC_BUF_SIZE);
    printf("[soc] socInit result: 0x%08lX\n", socResult);

    // curl global init
    curl_global_init(CURL_GLOBAL_ALL);

    // ---------------------------------------------------------------------------
    // Button layout
    // ---------------------------------------------------------------------------
    static const Button BTN_SIGNIN = {
        16, 108, BOT_W - 32, 30,
        "Sign in on this device",
        C_PRIMARY, C_PRIMARY_TXT, {105, 50, 12}
    };
    static const Button BTN_GUEST = {
        16, 144, BOT_W - 32, 30,
        "Play as guest",
        C_BG_DARK, C_TEXT, C_ACCENT
    };
    static const Button BTN_QUIT = {
        16, 180, BOT_W - 32, 30,
        "Quit",
        C_BG_DARK, C_TEXT, C_ACCENT
    };
    static const Button BTN_SIGNOUT = {
        16, 135, BOT_W - 32, 38,
        "Sign out",
        C_PRIMARY, C_PRIMARY_TXT, {105, 50, 12}
    };

    // ---------------------------------------------------------------------------
    // State
    // ---------------------------------------------------------------------------
    AppState state = AppState::INIT;
    char statusMsg[192] = {};  // wide enough for verbose curl errors
    char loginCode[12]  = {};
    char loginUrl[128]  = {};
    char savedAuthCode[AUTHCODE_LEN + 2] = {};
    char username[64]   = {};
    char elo[16]        = "600";
    char joinCode[32]   = {};
    LobbyPage lobbyPage = LobbyPage::HOME;

    static uint8_t qrTempBuf[qrcodegen_BUFFER_LEN_FOR_VERSION(5)];
    static uint8_t qrData   [qrcodegen_BUFFER_LEN_FOR_VERSION(5)];
    bool qrReady = false;

    bool pressedSignIn = false;
    bool pressedGuest = false;
    bool pressedSignOut = false;
    bool pressedQuit   = false;
    bool returnToErrorAfterKeyboardCancel = false;

    u64 lastPollTick = 0;
    constexpr u64 POLL_INTERVAL_TICKS = CPU_TICKS_PER_MSEC * 2000;

    // ---------------------------------------------------------------------------
    // Show "connecting" splash while we make the initial network calls
    // ---------------------------------------------------------------------------
    {
        uint8_t *topFb = gfxGetFramebuffer(GFX_TOP,    GFX_LEFT, nullptr, nullptr);
        uint8_t *botFb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, nullptr, nullptr);

        // If SOC failed, say so immediately rather than making curl fail silently
        if (R_FAILED(socResult))
        {
            snprintf(statusMsg, sizeof(statusMsg),
                     "SOC init failed (0x%08lX) - check WiFi", socResult);
            state = AppState::ERROR_STATE;
            drawTopScreen   (topFb, state, nullptr, false, statusMsg);
            drawBottomScreen(botFb, state, lobbyPage, false, false, false, false, BTN_SIGNIN, BTN_GUEST, BTN_SIGNOUT, BTN_QUIT);
            gfxFlushBuffers(); gfxSwapBuffers(); gspWaitForVBlank();
            // Skip network steps; fall straight through to the main loop
            goto main_loop;
        }

        drawTopScreen   (topFb, AppState::INIT, nullptr, false, "Checking saved login...");
        drawBottomScreen(botFb, AppState::INIT, lobbyPage, false, false, false, false, BTN_SIGNIN, BTN_GUEST, BTN_SIGNOUT, BTN_QUIT);
        gfxFlushBuffers(); gfxSwapBuffers(); gspWaitForVBlank();
    }

    // ---------------------------------------------------------------------------
    // STEP 1 — try saved authCode from SD card
    // ---------------------------------------------------------------------------
    if (loadAuthCode(savedAuthCode))
    {
        char json[80];
        snprintf(json, sizeof(json), "{\"authCode\":\"%s\"}", savedAuthCode);

        CurlBuf  resp      = allocBuf();
        CURLcode cc        = CURLE_OK;
        char     cerr[CURL_ERROR_SIZE] = {};
        long     code      = httpPost("/api/auth_code_login", json, resp, cc, cerr);

        if (code == 200)
        {
            char uname[64] = "Player";
            jsonExtract(resp.data, "username", uname, sizeof(uname));
            jsonExtract(resp.data, "elo", elo, sizeof(elo));
            snprintf(username,  sizeof(username),  "%s", uname);
            snprintf(statusMsg, sizeof(statusMsg), "%s  ELO %s", uname, elo);
            state = AppState::LOGGED_IN;
        }
        // Any other code → fall through to QR login below
        freeBuf(resp);
    }

    // ---------------------------------------------------------------------------
    // STEP 2 — get external login code + build QR
    // ---------------------------------------------------------------------------
    if (state == AppState::INIT)
    {
        CurlBuf  resp = allocBuf();
        CURLcode cc   = CURLE_OK;
        char     cerr[CURL_ERROR_SIZE] = {};
        long     code = httpGet("/api/external_login", resp, cc, cerr);

        if (code == 200 && resp.data)
        {
            char codeVal[12]  = {};
            char urlVal[128]  = {};
            jsonExtract(resp.data, "code",     codeVal, sizeof(codeVal));
            jsonExtract(resp.data, "loginUrl", urlVal,  sizeof(urlVal));
            snprintf(loginCode, sizeof(loginCode), "%s", codeVal);
            snprintf(loginUrl,  sizeof(loginUrl),  "%s", urlVal);

            memset(qrTempBuf, 0, sizeof(qrTempBuf));
            memset(qrData,    0, sizeof(qrData));
            qrReady = qrcodegen_encodeText(
                loginUrl, qrTempBuf, qrData,
                qrcodegen_Ecc_LOW,
                1, 5,
                qrcodegen_Mask_AUTO,
                false);
            state = AppState::QR_LOGIN;
            statusMsg[0] = 0;
        }
        else
        {
            state = AppState::ERROR_STATE;
            buildNetError(statusMsg, sizeof(statusMsg), code, cc, cerr);
        }
        freeBuf(resp);
    }

    // ---------------------------------------------------------------------------
    // Main loop
    // ---------------------------------------------------------------------------
main_loop:
    while (aptMainLoop())
    {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();

        touchPosition touch;
        hidTouchRead(&touch);
        bool touched  = (kDown & KEY_TOUCH) != 0;
        bool touchHeld = (kHeld & KEY_TOUCH) != 0;

        pressedSignIn = touchHeld && buttonHit(BTN_SIGNIN, touch.px, touch.py);
        pressedGuest = touchHeld && buttonHit(BTN_GUEST, touch.px, touch.py);
        pressedSignOut = touchHeld && buttonHit(BTN_SIGNOUT, touch.px, touch.py);
        pressedQuit   = touchHeld && buttonHit(BTN_QUIT,   touch.px, touch.py);

        if (kDown & KEY_START) break;

        // ----- Button actions -----
        if (touched)
        {
            if (buttonHit(BTN_QUIT, touch.px, touch.py))
                break;

            if (state == AppState::LOGGED_IN && buttonHit(BTN_SIGNOUT, touch.px, touch.py))
            {
                deleteAuthCode();
                username[0] = 0;
                statusMsg[0] = 0;
                loginCode[0] = 0;
                loginUrl[0] = 0;
                qrReady = false;

                CurlBuf resp = allocBuf();
                CURLcode cc = CURLE_OK;
                char cerr[CURL_ERROR_SIZE] = {};
                long http = httpGet("/api/external_login", resp, cc, cerr);
                if (http == 200 && resp.data)
                {
                    char codeVal[12] = {};
                    char urlVal[128] = {};
                    jsonExtract(resp.data, "code", codeVal, sizeof(codeVal));
                    jsonExtract(resp.data, "loginUrl", urlVal, sizeof(urlVal));
                    snprintf(loginCode, sizeof(loginCode), "%s", codeVal);
                    snprintf(loginUrl, sizeof(loginUrl), "%s", urlVal);
                    memset(qrTempBuf, 0, sizeof(qrTempBuf));
                    memset(qrData, 0, sizeof(qrData));
                    qrReady = qrcodegen_encodeText(
                        loginUrl, qrTempBuf, qrData,
                        qrcodegen_Ecc_LOW, 1, 5, qrcodegen_Mask_AUTO, false);
                    state = qrReady ? AppState::QR_LOGIN : AppState::ERROR_STATE;
                    if (!qrReady) snprintf(statusMsg, sizeof(statusMsg), "Could not create QR code");
                }
                else
                {
                    state = AppState::ERROR_STATE;
                    buildNetError(statusMsg, sizeof(statusMsg), http, cc, cerr);
                }
                freeBuf(resp);
            }

            if (state == AppState::LOGGED_IN)
            {
                static const Button publicMatch = {8, 42, 152, 48, "Public match", C_PRIMARY, C_PRIMARY_TXT, {105, 50, 12}};
                static const Button privateRoom = {168, 42, 152, 48, "Private room", C_PRIMARY, C_PRIMARY_TXT, {105, 50, 12}};
                static const Button localPlay = {8, 98, 152, 48, "Local play", C_ACCENT, C_BG_DARK, C_PRIMARY};
                static const Button spectate = {168, 98, 152, 48, "Spectate", C_BG_DARK, C_TEXT, C_ACCENT};
                static const Button back = {8, 174, 152, 34, "Back", C_BG_DARK, C_TEXT, C_ACCENT};
                static const Button continueButton = {168, 174, 152, 34, "Continue", C_PRIMARY, C_PRIMARY_TXT, {105, 50, 12}};

                if (lobbyPage == LobbyPage::HOME)
                {
                    if (buttonHit(publicMatch, touch.px, touch.py)) lobbyPage = LobbyPage::PUBLIC_SETTINGS;
                    else if (buttonHit(privateRoom, touch.px, touch.py)) lobbyPage = LobbyPage::PRIVATE_CHOICE;
                    else if (buttonHit(localPlay, touch.px, touch.py)) snprintf(statusMsg, sizeof(statusMsg), "%s  ELO %s  Local play menu", username, elo);
                    else if (buttonHit(spectate, touch.px, touch.py)) snprintf(statusMsg, sizeof(statusMsg), "%s  ELO %s  Spectate menu", username, elo);
                }
                else if (buttonHit(back, touch.px, touch.py))
                {
                    if (lobbyPage == LobbyPage::PRIVATE_CHOICE || lobbyPage == LobbyPage::PUBLIC_SETTINGS) lobbyPage = LobbyPage::HOME;
                    else if (lobbyPage == LobbyPage::PRIVATE_CREATE || lobbyPage == LobbyPage::PRIVATE_JOIN) lobbyPage = LobbyPage::PRIVATE_CHOICE;
                }
                else if (lobbyPage == LobbyPage::PRIVATE_CHOICE)
                {
                    if (buttonHit(publicMatch, touch.px, touch.py)) lobbyPage = LobbyPage::PRIVATE_CREATE;
                    else if (buttonHit(privateRoom, touch.px, touch.py)) lobbyPage = LobbyPage::PRIVATE_JOIN;
                }
                else if (buttonHit(continueButton, touch.px, touch.py) && lobbyPage == LobbyPage::PRIVATE_JOIN)
                {
                    showKeyboard("Join code", joinCode, sizeof(joinCode));
                    snprintf(statusMsg, sizeof(statusMsg), "%s  ELO %s  Join code ready", username, elo);
                }
                else if (buttonHit(continueButton, touch.px, touch.py))
                {
                    snprintf(statusMsg, sizeof(statusMsg), "%s  ELO %s  Settings saved", username, elo);
                }
            }

            if ((state == AppState::QR_LOGIN || state == AppState::ERROR_STATE) &&
                buttonHit(BTN_SIGNIN, touch.px, touch.py))
            {
                returnToErrorAfterKeyboardCancel = state == AppState::ERROR_STATE;
                state = AppState::KEYBOARD_LOGIN;
            }

            if ((state == AppState::QR_LOGIN || state == AppState::ERROR_STATE) &&
                buttonHit(BTN_GUEST, touch.px, touch.py))
            {
                state = AppState::INIT;
                snprintf(statusMsg, sizeof(statusMsg), "Starting guest session...");

                CurlBuf resp = allocBuf();
                CURLcode cc = CURLE_OK;
                char cerr[CURL_ERROR_SIZE] = {};
                long http = httpPost("/api/guest", "{}", resp, cc, cerr);
                if (http == 200 && resp.data)
                {
                    char guestName[64] = "Guest";
                    jsonExtract(resp.data, "username", guestName, sizeof(guestName));
                    jsonExtract(resp.data, "elo", elo, sizeof(elo));
                    snprintf(username, sizeof(username), "%s", guestName);
                    snprintf(statusMsg, sizeof(statusMsg), "%s  ELO %s", guestName, elo);
                    state = AppState::LOGGED_IN;
                }
                else
                {
                    buildNetError(statusMsg, sizeof(statusMsg), http, cc, cerr);
                    state = AppState::ERROR_STATE;
                }
                freeBuf(resp);
            }
        }

        // ----- Keyboard login -----
        if (state == AppState::KEYBOARD_LOGIN)
        {
            char kbdUser[64] = {};
            char kbdPass[64] = {};

            if (!showKeyboard("Username", kbdUser, sizeof(kbdUser)))
            {
                state = returnToErrorAfterKeyboardCancel ? AppState::ERROR_STATE : AppState::QR_LOGIN;
                goto render;
            }

            if (!showKeyboard("Password", kbdPass, sizeof(kbdPass), true))
            {
                state = returnToErrorAfterKeyboardCancel ? AppState::ERROR_STATE : AppState::QR_LOGIN;
                goto render;
            }

            {
                char json[256];
                snprintf(json, sizeof(json),
                         "{\"username\":\"%s\",\"password\":\"%s\"}", kbdUser, kbdPass);

                CurlBuf  resp = allocBuf();
                CURLcode cc   = CURLE_OK;
                char     cerr[CURL_ERROR_SIZE] = {};
                long     http = httpPost("/api/device_login", json, resp, cc, cerr);

                if (http == 200)
                {
                    char uname[64] = "Player";
                    char ac[AUTHCODE_LEN + 2] = {};
                    jsonExtract(resp.data, "username", uname, sizeof(uname));
                    jsonExtract(resp.data, "authCode", ac, sizeof(ac));
                    jsonExtract(resp.data, "elo", elo, sizeof(elo));
                    snprintf(username,  sizeof(username),  "%s", uname);
                    snprintf(statusMsg, sizeof(statusMsg), "%s  ELO %s", uname, elo);
                    if (ac[0]) saveAuthCode(ac);
                    state = AppState::LOGGED_IN;
                }
                else
                {
                    char errMsg[128] = {};
                    if (!jsonExtract(resp.data, "error", errMsg, sizeof(errMsg)))
                        buildNetError(errMsg, sizeof(errMsg), http, cc, cerr);
                    snprintf(statusMsg, sizeof(statusMsg), "%s", errMsg);
                    state = AppState::ERROR_STATE;
                }
                freeBuf(resp);
            }
        }

        // ----- Poll for QR approval -----
        if (state == AppState::QR_LOGIN && loginCode[0])
        {
            u64 now = svcGetSystemTick();
            if (now - lastPollTick >= POLL_INTERVAL_TICKS)
            {
                lastPollTick = now;

                char json[32];
                snprintf(json, sizeof(json), "{\"code\":\"%s\"}", loginCode);

                CurlBuf  resp = allocBuf();
                CURLcode cc   = CURLE_OK;
                char     cerr[CURL_ERROR_SIZE] = {};
                long     http = httpPost("/api/external_login/poll", json, resp, cc, cerr);

                if (http == 200 && resp.data)
                {
                    char pollStatus[32] = {};
                    jsonExtract(resp.data, "status", pollStatus, sizeof(pollStatus));

                    if (strcmp(pollStatus, "approved") == 0)
                    {
                        char ac[AUTHCODE_LEN + 2] = {};
                        char uname[64] = "Player";
                        jsonExtract(resp.data, "authCode", ac, sizeof(ac));
                        const char *userObj = strstr(resp.data, "\"user\":");
                        if (userObj)
                        {
                            jsonExtract(userObj, "username", uname, sizeof(uname));
                            jsonExtract(userObj, "elo", elo, sizeof(elo));
                        }

                        snprintf(username, sizeof(username), "%s", uname);
                        if (ac[0]) saveAuthCode(ac);

                        state = AppState::LOGGED_IN;
                        snprintf(statusMsg, sizeof(statusMsg), "%s  ELO %s", username, elo);
                    }
                    else if (strcmp(pollStatus, "expired")   == 0 ||
                             strcmp(pollStatus, "consumed")  == 0 ||
                             strcmp(pollStatus, "not_found") == 0)
                    {
                        // Refresh the QR code
                        qrReady    = false;
                        loginCode[0] = 0;
                        loginUrl[0]  = 0;

                        CurlBuf  r2   = allocBuf();
                        CURLcode cc2  = CURLE_OK;
                        char     cerr2[CURL_ERROR_SIZE] = {};
                        long     c2   = httpGet("/api/external_login", r2, cc2, cerr2);

                        if (c2 == 200 && r2.data)
                        {
                            char cv[12]  = {};
                            char uv[128] = {};
                            jsonExtract(r2.data, "code",     cv, sizeof(cv));
                            jsonExtract(r2.data, "loginUrl", uv, sizeof(uv));
                            snprintf(loginCode, sizeof(loginCode), "%s", cv);
                            snprintf(loginUrl,  sizeof(loginUrl),  "%s", uv);

                            memset(qrTempBuf, 0, sizeof(qrTempBuf));
                            memset(qrData,    0, sizeof(qrData));
                            qrReady = qrcodegen_encodeText(
                                loginUrl, qrTempBuf, qrData,
                                qrcodegen_Ecc_LOW, 1, 5, qrcodegen_Mask_AUTO, false);
                        }
                        else
                        {
                            state = AppState::ERROR_STATE;
                            buildNetError(statusMsg, sizeof(statusMsg), c2, cc2, cerr2);
                        }
                        freeBuf(r2);
                    }
                    // "pending" → do nothing, keep polling
                }
                else if (http == 0)
                {
                    // Transport error — show verbosely in status but keep trying
                    buildNetError(statusMsg, sizeof(statusMsg), 0, cc, cerr);
                }

                freeBuf(resp);
            }
        }

    render:
        {
            uint8_t *topFb = gfxGetFramebuffer(GFX_TOP,    GFX_LEFT, nullptr, nullptr);
            uint8_t *botFb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, nullptr, nullptr);

            drawTopScreen   (topFb, state, qrData, qrReady, statusMsg);
            drawBottomScreen(botFb, state, lobbyPage, pressedSignIn, pressedGuest, pressedSignOut,
                             pressedQuit, BTN_SIGNIN, BTN_GUEST, BTN_SIGNOUT, BTN_QUIT);

            gfxFlushBuffers();
            gfxSwapBuffers();
            gspWaitForVBlank();
        }
    }

    curl_global_cleanup();
    socExit();
    free(socBuf);
    gfxExit();
    return 0;
}
