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

static bool parseSocketEvent(const char *packet, char *event, int eventLen,
                             char *payload, int payloadLen)
{
    if (!packet || strncmp(packet, "42[\"", 4) != 0) return false;
    const char *eventStart = packet + 4;
    const char *eventEnd = strchr(eventStart, '"');
    if (!eventEnd) return false;
    int eventSize = (int)(eventEnd - eventStart);
    if (eventSize <= 0 || eventSize >= eventLen) return false;
    memcpy(event, eventStart, eventSize);
    event[eventSize] = 0;

    const char *payloadStart = strchr(eventEnd + 1, ',');
    if (!payloadStart) return false;
    ++payloadStart;
    const char *payloadEnd = strrchr(payloadStart, ']');
    if (!payloadEnd || payloadEnd <= payloadStart) return false;
    int payloadSize = (int)(payloadEnd - payloadStart);
    if (payloadSize >= payloadLen) payloadSize = payloadLen - 1;
    memcpy(payload, payloadStart, payloadSize);
    payload[payloadSize] = 0;
    return true;
}

static bool parseBoard(const char *json, char board[6][6])
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

static int focusCount(AppState state, LobbyPage page)
{
    if (state == AppState::QR_LOGIN || state == AppState::ERROR_STATE) return 4;
    if (state == AppState::INIT) return 1;
    if (state != AppState::LOGGED_IN) return 0;
    if (page == LobbyPage::QUEUE) return 1;
    if (page == LobbyPage::HOME) return 6;
    if (page == LobbyPage::PRIVATE_CHOICE) return 4;
    if (page == LobbyPage::PRIVATE_JOIN) return 3;
    if (page == LobbyPage::LOCAL_SETTINGS) return 3;
    return 6;
}

static bool focusPoint(AppState state, LobbyPage page, int focus,
                       int &x, int &y)
{
    if (state == AppState::QR_LOGIN || state == AppState::ERROR_STATE)
    {
        if (focus == 0) { x = BOT_W / 2; y = 125; return true; }
        if (focus == 1) { x = BOT_W / 2; y = 165; return true; }
        if (focus == 2) { x = BOT_W / 2; y = 198; return true; }
        if (focus == 3) { x = BOT_W / 2; y = 222; return true; }
    }
    else if (state == AppState::INIT)
    {
        x = BOT_W / 2; y = 222; return true;
    }
    else if (state == AppState::LOGGED_IN)
    {
        if (page == LobbyPage::HOME)
        {
            static const int points[][2] = {{82, 97}, {238, 97}, {82, 145},
                                             {238, 145}, {160, 181}, {160, 222}};
            if (focus >= 0 && focus < 6) { x = points[focus][0]; y = points[focus][1]; return true; }
        }
        else if (page == LobbyPage::PRIVATE_CHOICE)
        {
            static const int points[][2] = {{82, 109}, {238, 109}, {82, 185}, {160, 222}};
            if (focus >= 0 && focus < 4) { x = points[focus][0]; y = points[focus][1]; return true; }
        }
        else if (page == LobbyPage::PRIVATE_JOIN)
        {
            static const int points[][2] = {{238, 185}, {82, 185}, {160, 222}};
            if (focus >= 0 && focus < 3) { x = points[focus][0]; y = points[focus][1]; return true; }
        }
        else if (page == LobbyPage::LOCAL_SETTINGS)
        {
            static const int points[][2] = {{160, 54}, {238, 185}, {82, 185}};
            if (focus >= 0 && focus < 3) { x = points[focus][0]; y = points[focus][1]; return true; }
        }
        else if (page == LobbyPage::QUEUE)
        {
            x = BOT_W / 2; y = 175; return focus == 0;
        }
        else
        {
            static const int points[][2] = {{160, 38}, {160, 62}, {160, 86},
                                             {238, 173}, {82, 173}, {160, 222}};
            if (focus >= 0 && focus < 6) { x = points[focus][0]; y = points[focus][1]; return true; }
        }
    }
    return false;
}

static void goBack(AppState state, LobbyPage &page, char *statusMsg)
{
    if (state == AppState::LOGGED_IN)
    {
        if (page == LobbyPage::PRIVATE_CREATE || page == LobbyPage::PRIVATE_JOIN)
            page = LobbyPage::PRIVATE_CHOICE;
        else
            page = LobbyPage::HOME;
        statusMsg[0] = 0;
    }
}

static void resetGame(GameUiState &game, int selectedVariant = 0)
{
    static const char initialBoard[6][6] = {
        {'B', '0', 'W', 'B', '0', 'W'},
        {'0', '0', '0', '0', '0', '0'},
        {'W', '0', '0', '0', '0', 'B'},
        {'B', '0', '0', '0', '0', 'W'},
        {'0', '0', '0', '0', '0', '0'},
        {'W', '0', 'B', 'W', '0', 'B'}
    };
    memcpy(game.board, initialBoard, sizeof(initialBoard));
    if (selectedVariant == 2)
    {
        memset(game.board, '0', sizeof(game.board));
        int placedWhite = 0;
        int placedBlack = 0;
        while (placedWhite < 6 || placedBlack < 6)
        {
            int row = rand() % 6;
            int col = rand() % 6;
            if (game.board[row][col] != '0') continue;
            if (placedWhite < 6)
            {
                game.board[row][col] = 'W';
                ++placedWhite;
            }
            else
            {
                game.board[row][col] = 'B';
                ++placedBlack;
            }
        }
    }
    game.player = 'W';
    game.turn = 'W';
    game.selectedRow = game.selectedCol = -1;
    game.targetRow = game.targetCol = -1;
    game.cursorRow = game.cursorCol = 0;
    game.pieceSelected = false;
    game.confirmMove = false;
    game.statusMsg = "Local game";
}

static bool hasLegalDestination(const GameUiState &game, int r, int c)
{
    static const int dirs[][2] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
    for (const auto &dir : dirs)
    {
        int nr = r + dir[0];
        int nc = c + dir[1];
        bool moved = false;
        while (nr >= 0 && nr < 6 && nc >= 0 && nc < 6 && game.board[nr][nc] == '0')
        {
            moved = true;
            nr += dir[0];
            nc += dir[1];
        }
        if (moved) return true;
    }
    return false;
}

static bool chooseFirstDestination(GameUiState &game)
{
    static const int dirs[][2] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
    for (const auto &dir : dirs)
    {
        int nr = game.selectedRow + dir[0];
        int nc = game.selectedCol + dir[1];
        int lastR = game.selectedRow;
        int lastC = game.selectedCol;
        while (nr >= 0 && nr < 6 && nc >= 0 && nc < 6 && game.board[nr][nc] == '0')
        {
            lastR = nr;
            lastC = nc;
            nr += dir[0];
            nc += dir[1];
        }
        if (lastR != game.selectedRow || lastC != game.selectedCol)
        {
            game.targetRow = lastR;
            game.targetCol = lastC;
            return true;
        }
    }
    return false;
}

static void selectGamePiece(GameUiState &game, int r, int c)
{
    if (game.turn != game.player || r < 0 || r >= 6 || c < 0 || c >= 6 || game.board[r][c] != game.player ||
        !hasLegalDestination(game, r, c)) return;
    game.selectedRow = r;
    game.selectedCol = c;
    game.cursorRow = r;
    game.cursorCol = c;
    game.pieceSelected = chooseFirstDestination(game);
    game.confirmMove = false;
    game.statusMsg = "Use DPAD to choose a direction";
}

static void moveGameCursor(GameUiState &game, int direction)
{
    if (!game.pieceSelected)
    {
        if (direction == 0) game.cursorCol = (game.cursorCol + 5) % 6;
        if (direction == 1) game.cursorRow = (game.cursorRow + 1) % 6;
        if (direction == 2) game.cursorCol = (game.cursorCol + 1) % 6;
        if (direction == 3) game.cursorRow = (game.cursorRow + 5) % 6;
        return;
    }
    static const int dirs[][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    const int dr = dirs[direction][0];
    const int dc = dirs[direction][1];
    int nr = game.selectedRow + dr;
    int nc = game.selectedCol + dc;
    int lastR = game.selectedRow;
    int lastC = game.selectedCol;
    while (nr >= 0 && nr < 6 && nc >= 0 && nc < 6 && game.board[nr][nc] == '0')
    {
        lastR = nr;
        lastC = nc;
        nr += dr;
        nc += dc;
    }
    if (lastR != game.selectedRow || lastC != game.selectedCol)
    {
        game.targetRow = lastR;
        game.targetCol = lastC;
        game.cursorRow = lastR;
        game.cursorCol = lastC;
    }
}

static void applyGameMove(GameUiState &game)
{
    game.board[game.targetRow][game.targetCol] = game.player;
    game.board[game.selectedRow][game.selectedCol] = '0';
    game.player = game.player == 'W' ? 'B' : 'W';
    game.turn = game.player;
    game.selectedRow = game.selectedCol = -1;
    game.targetRow = game.targetCol = -1;
    game.pieceSelected = false;
    game.confirmMove = false;
    game.statusMsg = "Move complete";
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
        16, 108, BOT_W - 32, 34,
        "Sign in on this device",
        C_PRIMARY, C_PRIMARY_TXT, {105, 50, 12}
    };
    static const Button BTN_GUEST = {
        16, 148, BOT_W - 32, 34,
        "Play as guest",
        C_BG_DARK, C_TEXT, C_ACCENT
    };
    static const Button BTN_QUIT = {
        16, 212, BOT_W - 32, 20,
        "Quit",
        C_BG_DARK, C_TEXT, C_ACCENT
    };
    static const Button BTN_SIGNOUT = {
        16, 188, BOT_W - 32, 24,
        "Sign out",
        C_PRIMARY, C_PRIMARY_TXT, {105, 50, 12}
    };
    static const Button BTN_MATCH_SETTING = {8, 28, BOT_W - 16, 20, "", C_BG_DARK, C_TEXT, C_ACCENT};
    static const Button BTN_TIME_SETTING = {8, 52, BOT_W - 16, 20, "", C_BG_DARK, C_TEXT, C_ACCENT};
    static const Button BTN_VARIANT_SETTING = {8, 76, BOT_W - 16, 20, "", C_BG_DARK, C_TEXT, C_ACCENT};

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
    int userId = 0;
    char joinCode[32]   = {};
    LobbyPage lobbyPage = LobbyPage::HOME;
    int focusIndex = 0;
    bool focusVisible = false;
    bool isRated = true;
    int timeControl = 0;
    int variant = 0;
    bool gameActive = false;
    bool onlineGame = false;
    bool queueing = false;
    bool socketAttempted = false;
    u64 nextSocketRetryTick = 0;
    char queuedTimeControl[16] = {};
    char queuedVariant[24] = {};
    bool queuedRated = false;
    char activeGameId[64] = {};
    GameUiState game = {};
    resetGame(game);
    SocketIoClient socket;
    char socketError[192] = {};
    char socketPacket[4096] = {};

    static uint8_t qrTempBuf[qrcodegen_BUFFER_LEN_FOR_VERSION(5)];
    static uint8_t qrData   [qrcodegen_BUFFER_LEN_FOR_VERSION(5)];
    bool qrReady = false;

    bool pressedSignIn = false;
    bool pressedGuest = false;
    bool pressedOffline = false;
    bool pressedSignOut = false;
    bool pressedQuit   = false;
    bool returnToErrorAfterKeyboardCancel = false;

    u64 lastPollTick = 0;
    constexpr u64 POLL_INTERVAL_TICKS = CPU_TICKS_PER_MSEC * 2000;
    int previousNavDirection = 0;

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
            drawTopScreen   (topFb, state, lobbyPage, username, elo, isRated, timeControl, variant, focusIndex, nullptr, false, statusMsg);
            drawBottomScreen(botFb, state, lobbyPage, username, elo, isRated, timeControl, variant, focusIndex, focusVisible, false, false, false, false, false, BTN_SIGNIN, BTN_GUEST, BTN_SIGNOUT, BTN_QUIT);
            gfxFlushBuffers(); gfxSwapBuffers(); gspWaitForVBlank();
            // Skip network steps; fall straight through to the main loop
            goto main_loop;
        }

        drawTopScreen   (topFb, AppState::INIT, lobbyPage, username, elo, isRated, timeControl, variant, focusIndex, nullptr, false, "Checking saved login...");
        drawBottomScreen(botFb, AppState::INIT, lobbyPage, username, elo, isRated, timeControl, variant, focusIndex, focusVisible, false, false, false, false, false, BTN_SIGNIN, BTN_GUEST, BTN_SIGNOUT, BTN_QUIT);
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
            char idValue[16] = {};
            jsonExtract(resp.data, "username", uname, sizeof(uname));
            jsonExtract(resp.data, "id", idValue, sizeof(idValue));
            jsonExtract(resp.data, "elo", elo, sizeof(elo));
            userId = atoi(idValue);
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

        circlePosition circle;
        hidCircleRead(&circle);

        if (state == AppState::LOGGED_IN && !socketAttempted &&
            (!queueing || svcGetSystemTick() >= nextSocketRetryTick))
        {
            socketAttempted = true;
            if (!socket.connect(socketError, sizeof(socketError)))
            {
                if (queueing)
                {
                    snprintf(statusMsg, sizeof(statusMsg), "Reconnecting to matchmaking...");
                    socketAttempted = false;
                    nextSocketRetryTick = svcGetSystemTick() + CPU_TICKS_PER_MSEC * 2000;
                }
                else
                {
                    snprintf(statusMsg, sizeof(statusMsg), "%s", socketError);
                    state = AppState::ERROR_STATE;
                }
            }
            else if (queueing)
            {
                char queueJson[192];
                char queueError[192] = {};
                snprintf(queueJson, sizeof(queueJson),
                         "{\"userId\":%d,\"elo\":%d,\"timeControl\":\"%s\",\"variant\":\"%s\",\"isRated\":%s}",
                         userId, atoi(elo), queuedTimeControl, queuedVariant,
                         queuedRated ? "true" : "false");
                if (!socket.sendEvent("join_queue", queueJson, queueError, sizeof(queueError)))
                {
                    snprintf(statusMsg, sizeof(statusMsg), "Queue reconnect failed: %s", queueError);
                    socket.close();
                    socketAttempted = false;
                    nextSocketRetryTick = svcGetSystemTick() + CPU_TICKS_PER_MSEC * 2000;
                }
            }
        }

        if (socket.isConnected())
        {
            while (socket.receive(socketPacket, sizeof(socketPacket), socketError, sizeof(socketError)))
            {
                if (strcmp(socketPacket, "2") == 0)
                {
                    socket.sendRaw("3", socketError, sizeof(socketError));
                    continue;
                }

                char eventName[64] = {};
                char eventPayload[3800] = {};
                if (!parseSocketEvent(socketPacket, eventName, sizeof(eventName),
                                      eventPayload, sizeof(eventPayload)))
                    continue;

                if (strcmp(eventName, "match_found") == 0)
                {
                    if (!queueing)
                        continue;
                    char color[4] = "W";
                    jsonExtract(eventPayload, "gameId", activeGameId, sizeof(activeGameId));
                    jsonExtract(eventPayload, "color", color, sizeof(color));
                    char joinError[192] = {};
                    char joinPayload[80];
                    snprintf(joinPayload, sizeof(joinPayload), "\"%s\"", activeGameId);
                    if (!socket.sendEvent("join_game", joinPayload, joinError, sizeof(joinError)))
                    {
                        snprintf(statusMsg, sizeof(statusMsg), "Could not join game room: %s", joinError);
                        state = AppState::ERROR_STATE;
                        gameActive = false;
                        break;
                    }
                    if (!parseBoard(eventPayload, game.board))
                    {
                        snprintf(statusMsg, sizeof(statusMsg), "Socket.IO match_found had no valid board");
                        state = AppState::ERROR_STATE;
                        gameActive = false;
                        break;
                    }
                    game.player = color[0] == 'B' ? 'B' : 'W';
                    game.turn = 'W';
                    game.selectedRow = game.selectedCol = -1;
                    game.targetRow = game.targetCol = -1;
                    game.cursorRow = game.cursorCol = 0;
                    game.pieceSelected = false;
                    game.confirmMove = false;
                    game.statusMsg = "Choose a piece to move";
                    onlineGame = true;
                    queueing = false;
                    gameActive = true;
                }
                else if (strcmp(eventName, "game_update") == 0 && onlineGame)
                {
                    char turn[4] = "W";
                    if (parseBoard(eventPayload, game.board))
                    {
                        jsonExtract(eventPayload, "turn", turn, sizeof(turn));
                        game.turn = turn[0] == 'B' ? 'B' : 'W';
                        game.pieceSelected = false;
                        game.confirmMove = false;
                        game.selectedRow = game.selectedCol = -1;
                        game.targetRow = game.targetCol = -1;
                        game.statusMsg = game.turn == game.player
                                       ? "Choose a piece to move" : "Waiting for opponent";
                        if (strstr(eventPayload, "\"status\":\"finished\""))
                        {
                            game.statusMsg = "Game ended";
                            onlineGame = false;
                        }
                    }
                }
                else if (strcmp(eventName, "error") == 0)
                {
                    char message[160] = {};
                    jsonExtract(eventPayload, "message", message, sizeof(message));
                    snprintf(statusMsg, sizeof(statusMsg), "%s", message[0] ? message : eventPayload);
                    state = AppState::ERROR_STATE;
                    gameActive = false;
                }
            }
            if (!socket.isConnected() && socketError[0])
            {
                if (queueing)
                {
                    snprintf(statusMsg, sizeof(statusMsg), "Matchmaking connection lost; reconnecting...");
                    socketAttempted = false;
                    nextSocketRetryTick = svcGetSystemTick() + CPU_TICKS_PER_MSEC * 2000;
                }
                else if (gameActive && !onlineGame)
                {
                    socketError[0] = 0;
                }
                else
                {
                    snprintf(statusMsg, sizeof(statusMsg), "%s", socketError);
                    state = AppState::ERROR_STATE;
                    gameActive = false;
                }
            }
        }

        int navDirection = 0;
        if (kDown & (KEY_DUP | KEY_DLEFT)) navDirection = -1;
        else if (kDown & (KEY_DDOWN | KEY_DRIGHT)) navDirection = 1;
        else if (circle.dy > 120 || circle.dx < -120) navDirection = -1;
        else if (circle.dy < -120 || circle.dx > 120) navDirection = 1;

        const int currentFocusCount = focusCount(state, lobbyPage);
        if (navDirection == 0)
            previousNavDirection = 0;
        else if (navDirection != previousNavDirection && currentFocusCount > 0)
        {
            focusIndex = (focusIndex + navDirection + currentFocusCount) % currentFocusCount;
            focusVisible = true;
            previousNavDirection = navDirection;
        }

        if ((kDown & KEY_B) && state == AppState::LOGGED_IN && lobbyPage != LobbyPage::HOME)
        {
            goBack(state, lobbyPage, statusMsg);
            focusIndex = 0;
        }

        if (kDown & KEY_A)
        {
            int focusX = 0;
            int focusY = 0;
            if (focusPoint(state, lobbyPage, focusIndex, focusX, focusY))
            {
                touch.px = focusX;
                touch.py = focusY;
                touched = true;
                touchHeld = true;
            }
        }

        pressedSignIn = touchHeld && buttonHit(BTN_SIGNIN, touch.px, touch.py);
        pressedGuest = touchHeld && buttonHit(BTN_GUEST, touch.px, touch.py);
        static const Button BTN_OFFLINE = {16, 188, BOT_W - 32, 20, "Offline local play", C_BG_DARK, C_TEXT, C_ACCENT};
        pressedOffline = touchHeld && buttonHit(BTN_OFFLINE, touch.px, touch.py);
        pressedSignOut = touchHeld && buttonHit(BTN_SIGNOUT, touch.px, touch.py);
        pressedQuit   = touchHeld && buttonHit(BTN_QUIT,   touch.px, touch.py);

        if (kDown & KEY_START) break;

        if (gameActive)
        {
            if (kDown & KEY_B)
            {
                if (game.confirmMove)
                {
                    game.confirmMove = false;
                    game.statusMsg = "Choose a direction";
                }
                else if (game.pieceSelected)
                {
                    game.pieceSelected = false;
                    game.selectedRow = game.selectedCol = -1;
                    game.targetRow = game.targetCol = -1;
                    game.statusMsg = "Choose a piece to move";
                }
                else
                {
                    gameActive = false;
                    statusMsg[0] = 0;
                }
            }

            int gameDirection = -1;
            if (kDown & KEY_DLEFT) gameDirection = 0;
            else if (kDown & KEY_DDOWN) gameDirection = 1;
            else if (kDown & KEY_DRIGHT) gameDirection = 2;
            else if (kDown & KEY_DUP) gameDirection = 3;
            else if (circle.dx < -120) gameDirection = 0;
            else if (circle.dy < -120) gameDirection = 1;
            else if (circle.dx > 120) gameDirection = 2;
            else if (circle.dy > 120) gameDirection = 3;
            if (gameDirection >= 0 && !game.confirmMove)
                moveGameCursor(game, gameDirection);

            if (touched && touch.px >= 5 && touch.px < 233 && touch.py >= 5 && touch.py < 233)
            {
                int c = (touch.px - 5) / 38;
                int r = (touch.py - 5) / 38;
                if (!game.pieceSelected) selectGamePiece(game, r, c);
                else if (r == game.targetRow && c == game.targetCol) game.confirmMove = true;
                else if (game.board[r][c] == game.player) selectGamePiece(game, r, c);
            }
            if ((kDown & KEY_A) && !game.confirmMove)
            {
                if (!game.pieceSelected) selectGamePiece(game, game.cursorRow, game.cursorCol);
                else game.confirmMove = true;
            }
            else if ((kDown & KEY_A) && game.confirmMove)
            {
                if (onlineGame)
                {
                    char moveJson[192];
                    char sendError[192] = {};
                    snprintf(moveJson, sizeof(moveJson),
                             "{\"gameId\":\"%s\",\"userId\":%d,\"from\":{\"r\":%d,\"c\":%d},\"to\":{\"r\":%d,\"c\":%d}}",
                             activeGameId, userId, game.selectedRow, game.selectedCol,
                             game.targetRow, game.targetCol);
                    if (!socket.sendEvent("make_move", moveJson, sendError, sizeof(sendError)))
                    {
                        snprintf(statusMsg, sizeof(statusMsg), "%s", sendError);
                        state = AppState::ERROR_STATE;
                        gameActive = false;
                    }
                    else
                    {
                        game.confirmMove = false;
                        game.pieceSelected = false;
                        game.statusMsg = "Move sent. Waiting for server";
                    }
                }
                else
                    applyGameMove(game);
            }
            goto render;
        }

        // ----- Button actions -----
        if (touched)
        {
            if (buttonHit(BTN_QUIT, touch.px, touch.py))
                break;

            if (state == AppState::LOGGED_IN && lobbyPage == LobbyPage::HOME &&
                buttonHit(BTN_SIGNOUT, touch.px, touch.py))
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
                focusIndex = 0;
            }
            else if (state == AppState::LOGGED_IN)
            {
                static const Button publicMatch = {8, 76, 148, 42, "Public match", C_PRIMARY, C_PRIMARY_TXT, {105, 50, 12}};
                static const Button privateRoom = {164, 76, 148, 42, "Private room", C_PRIMARY, C_PRIMARY_TXT, {105, 50, 12}};
                static const Button createRoom = {8, 88, 148, 42, "Create room", C_PRIMARY, C_PRIMARY_TXT, {105, 50, 12}};
                static const Button joinRoom = {164, 88, 148, 42, "Join room", C_PRIMARY, C_PRIMARY_TXT, {105, 50, 12}};
                static const Button localPlay = {8, 124, 148, 42, "Local play", C_ACCENT, C_BG_DARK, C_PRIMARY};
                static const Button spectate = {164, 124, 148, 42, "Spectate", C_BG_DARK, C_TEXT, C_ACCENT};
                static const Button back = {8, 172, 148, 26, "Back", C_BG_DARK, C_TEXT, C_ACCENT};
                static const Button continueButton = {164, 172, 148, 26, "Continue", C_PRIMARY, C_PRIMARY_TXT, {105, 50, 12}};
                static const Button localVariant = {8, 44, BOT_W - 16, 20, "", C_BG_DARK, C_TEXT, C_ACCENT};
                static const Button cancelQueue = {64, 160, 192, 30, "Cancel queue", C_BG_DARK, C_TEXT, C_ACCENT};

                const LobbyPage pageBefore = lobbyPage;
                if (lobbyPage == LobbyPage::HOME)
                {
                    if (buttonHit(publicMatch, touch.px, touch.py)) lobbyPage = LobbyPage::PUBLIC_SETTINGS;
                    else if (buttonHit(privateRoom, touch.px, touch.py)) lobbyPage = LobbyPage::PRIVATE_CHOICE;
                    else if (buttonHit(localPlay, touch.px, touch.py))
                    {
                        lobbyPage = LobbyPage::LOCAL_SETTINGS;
                        focusIndex = 0;
                    }
                    else if (buttonHit(spectate, touch.px, touch.py)) snprintf(statusMsg, sizeof(statusMsg), "%s  ELO %s  Spectate menu", username, elo);
                }
                else if (buttonHit(back, touch.px, touch.py))
                {
                    if (lobbyPage == LobbyPage::PRIVATE_CREATE || lobbyPage == LobbyPage::PRIVATE_JOIN)
                        lobbyPage = LobbyPage::PRIVATE_CHOICE;
                    else
                        lobbyPage = LobbyPage::HOME;
                    statusMsg[0] = 0;
                }
                else if (lobbyPage == LobbyPage::PRIVATE_CHOICE)
                {
                    if (buttonHit(createRoom, touch.px, touch.py)) lobbyPage = LobbyPage::PRIVATE_CREATE;
                    else if (buttonHit(joinRoom, touch.px, touch.py)) lobbyPage = LobbyPage::PRIVATE_JOIN;
                }
                else if (lobbyPage == LobbyPage::QUEUE && buttonHit(cancelQueue, touch.px, touch.py))
                {
                    char leaveJson[48];
                    snprintf(leaveJson, sizeof(leaveJson), "{\"userId\":%d}", userId);
                    char leaveError[192] = {};
                    socket.sendEvent("leave_queue", leaveJson, leaveError, sizeof(leaveError));
                    queueing = false;
                    lobbyPage = LobbyPage::HOME;
                    statusMsg[0] = 0;
                    focusIndex = 0;
                }
                else if (lobbyPage == LobbyPage::LOCAL_SETTINGS)
                {
                    if (buttonHit(localVariant, touch.px, touch.py))
                    {
                        variant = (variant + 1) % 4;
                        snprintf(statusMsg, sizeof(statusMsg), "Local variant changed");
                    }
                }
                else if ((lobbyPage == LobbyPage::PUBLIC_SETTINGS || lobbyPage == LobbyPage::PRIVATE_CREATE) &&
                         !buttonHit(continueButton, touch.px, touch.py))
                {
                    if (buttonHit(BTN_MATCH_SETTING, touch.px, touch.py))
                    {
                        isRated = !isRated;
                        snprintf(statusMsg, sizeof(statusMsg), "%s selected", isRated ? "Ranked" : "Casual");
                    }
                    else if (buttonHit(BTN_TIME_SETTING, touch.px, touch.py))
                    {
                        timeControl = (timeControl + 1) % 3;
                        snprintf(statusMsg, sizeof(statusMsg), "Time control changed");
                    }
                    else if (buttonHit(BTN_VARIANT_SETTING, touch.px, touch.py))
                    {
                        variant = (variant + 1) % 4;
                        snprintf(statusMsg, sizeof(statusMsg), "Variant changed");
                    }
                }
                else if (buttonHit(continueButton, touch.px, touch.py) && lobbyPage == LobbyPage::PRIVATE_JOIN)
                {
                    showKeyboard("Join code", joinCode, sizeof(joinCode));
                    snprintf(statusMsg, sizeof(statusMsg), "%s  ELO %s  Join code ready", username, elo);
                }
                else if (buttonHit(continueButton, touch.px, touch.py))
                {
                    if (lobbyPage == LobbyPage::LOCAL_SETTINGS)
                    {
                        resetGame(game, variant);
                        onlineGame = false;
                        gameActive = true;
                    }
                    else if (lobbyPage == LobbyPage::PUBLIC_SETTINGS && socket.isConnected())
                    {
                        static const char *queueTimes[] = {"0.25|3", "1|0", "3|2"};
                        char queueJson[160];
                        char sendError[192] = {};
                        snprintf(queueJson, sizeof(queueJson),
                                 "{\"userId\":%d,\"elo\":%d,\"timeControl\":\"%s\",\"variant\":\"%s\",\"isRated\":%s}",
                                 userId, atoi(elo), queueTimes[timeControl],
                                 variant == 0 ? "classic" : variant == 1 ? "fog_of_war" :
                                 variant == 2 ? "random_setup" : "schizophrenic",
                                 isRated ? "true" : "false");
                        if (socket.sendEvent("join_queue", queueJson, sendError, sizeof(sendError)))
                        {
                            static const char *queueTimes[] = {"0.25|3", "1|0", "3|2"};
                            snprintf(queuedTimeControl, sizeof(queuedTimeControl), "%s", queueTimes[timeControl]);
                            snprintf(queuedVariant, sizeof(queuedVariant), "%s",
                                     variant == 0 ? "classic" : variant == 1 ? "fog_of_war" :
                                     variant == 2 ? "random_setup" : "schizophrenic");
                            queuedRated = isRated;
                            queueing = true;
                            lobbyPage = LobbyPage::QUEUE;
                            focusIndex = 0;
                            snprintf(statusMsg, sizeof(statusMsg), "Waiting for an opponent...");
                        }
                        else
                        {
                            snprintf(statusMsg, sizeof(statusMsg), "%s", sendError);
                            state = AppState::ERROR_STATE;
                        }
                    }
                    else
                        snprintf(statusMsg, sizeof(statusMsg), "%s  ELO %s  Settings saved", username, elo);
                }
                if (lobbyPage != pageBefore)
                    focusIndex = 0;
            }

            else if ((state == AppState::QR_LOGIN || state == AppState::ERROR_STATE) &&
                buttonHit(BTN_SIGNIN, touch.px, touch.py))
            {
                returnToErrorAfterKeyboardCancel = state == AppState::ERROR_STATE;
                state = AppState::KEYBOARD_LOGIN;
            }

            else if ((state == AppState::QR_LOGIN || state == AppState::ERROR_STATE) &&
                buttonHit(BTN_OFFLINE, touch.px, touch.py))
            {
                socketAttempted = true;
                userId = 0;
                snprintf(username, sizeof(username), "Offline");
                snprintf(elo, sizeof(elo), "-");
                state = AppState::LOGGED_IN;
                lobbyPage = LobbyPage::LOCAL_SETTINGS;
                focusIndex = 0;
                statusMsg[0] = 0;
            }

            else if ((state == AppState::QR_LOGIN || state == AppState::ERROR_STATE) &&
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
                    char idValue[16] = {};
                    jsonExtract(resp.data, "username", guestName, sizeof(guestName));
                    jsonExtract(resp.data, "id", idValue, sizeof(idValue));
                    jsonExtract(resp.data, "elo", elo, sizeof(elo));
                    userId = atoi(idValue);
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
                    char idValue[16] = {};
                    jsonExtract(resp.data, "username", uname, sizeof(uname));
                    jsonExtract(resp.data, "authCode", ac, sizeof(ac));
                    jsonExtract(resp.data, "id", idValue, sizeof(idValue));
                    jsonExtract(resp.data, "elo", elo, sizeof(elo));
                    userId = atoi(idValue);
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
                        char idValue[16] = {};
                        jsonExtract(resp.data, "authCode", ac, sizeof(ac));
                        const char *userObj = strstr(resp.data, "\"user\":");
                        if (userObj)
                        {
                            jsonExtract(userObj, "username", uname, sizeof(uname));
                            jsonExtract(userObj, "elo", elo, sizeof(elo));
                        }
                        jsonExtract(resp.data, "id", idValue, sizeof(idValue));

                        snprintf(username, sizeof(username), "%s", uname);
                        userId = atoi(idValue);
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

            if (gameActive)
            {
                drawGameTopScreen(topFb, game);
                drawGameBottomScreen(botFb, game);
            }
            else
            {
                drawTopScreen   (topFb, state, lobbyPage, username, elo, isRated, timeControl, variant, focusIndex, qrData, qrReady, statusMsg);
                drawBottomScreen(botFb, state, lobbyPage, username, elo, isRated, timeControl, variant, focusIndex, focusVisible, pressedSignIn, pressedGuest, pressedSignOut,
                                 pressedOffline, pressedQuit, BTN_SIGNIN, BTN_GUEST, BTN_SIGNOUT, BTN_QUIT);
            }

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
