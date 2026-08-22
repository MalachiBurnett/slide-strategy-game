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
 * Threading:
 *   All networking runs on a dedicated worker thread (net.h / net.cpp). The
 *   render/input loop enqueues NetJobs and never blocks on I/O: gameplay
 *   status polls and move sends are handled asynchronously each frame, and
 *   the client only polls the server while waiting for the OPPONENT's turn.
 *
 * Modules:
 *   render      — pixel / text / QR drawing (render.h / render.cpp)
 *   uikit       — UI objects, scenes, and the slide transition between pages
 *                 (uikit.h / uikit.cpp)
 *   screens     — every page expressed as a list of those objects
 *                 (screens.h / screens.cpp)
 *   font8x8     — 8×8 bitmap font data     (font8x8.h)
 *   net         — worker thread + HTTP GET/POST via curl, plus the blocking
 *                 NetCall helper and buildNetError (net.h / net.cpp)
 *   auth_store  — SD card auth code         (auth_store.h / auth_store.cpp)
 *   ui          — button geometry + shared app state types (ui.h / ui.cpp)
 *   json_util   — tiny flat-JSON field/board extractor (json_util.h / .cpp)
 *   navigation  — D-pad/circle-pad focus traversal + lobby back button
 *                 (navigation.h / navigation.cpp)
 *   game_logic  — Slide board rules: setup, legal moves, move application
 *                 (game_logic.h / game_logic.cpp)
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
#include "uikit.h"
#include "screens.h"
#include "json_util.h"
#include "navigation.h"
#include "game_logic.h"
#include "tutorial.h"

extern "C" {
#include "qrcodegen.h"
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

    // curl global init (must happen before any curl handle is created,
    // including the ones on the network worker thread)
    curl_global_init(CURL_GLOBAL_ALL);

    // All HTTP traffic is handled on this worker thread.
    netThreadStart();

    // Hands screens.cpp's board / preview / token painters to the toolkit.
    screensInit();

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
    char privateCode[16] = {};
    LobbyPage lobbyPage = LobbyPage::HOME;
    int focusIndex = 0;
    bool focusVisible = false;
    bool isRated = true;
    int timeControl = 0;
    int variant = 0;
    bool gameActive = false;
    bool onlineGame = false;
    bool queueing = false;
    GameUiState game = {};
    resetGame(game);
    bool tutorialActive = false;
    GameUiState tutorial = {};
    int tutorialStep = 0;
    char pollingGameId[64] = {};
    u64 lastPollingTick = 0;
    // Waiting on the opponent is the latency-critical case; during our own
    // turn we are only watching for the game ending out from under us, so a
    // much slower cadence is plenty.
    constexpr u64 POLL_WAITING_TICKS  = CPU_TICKS_PER_MSEC * 500;
    constexpr u64 POLL_OUR_TURN_TICKS = CPU_TICKS_PER_MSEC * 2500;
    bool sendPending = false;
    bool focusPressActive = false;
    int  focusPressX = 0;
    int  focusPressY = 0;
    int  lastCircleDir = -1;
    u64  lastGameRepeatTick = 0;
    constexpr u64 GAME_REPEAT_TICKS = CPU_TICKS_PER_MSEC * 240;

    // Async networking (worker thread)
    NetJob *statusJob = nullptr;   // in-flight /api/3ds/status poll
    NetJob *moveJob   = nullptr;   // in-flight /api/3ds/move send
    NetJob *qrPollJob = nullptr;   // in-flight /api/external_login/poll
    int moveFromR = 0, moveFromC = 0, moveToR = 0, moveToC = 0; // for revert on failure
    bool forceStatusPoll = false;       // submit a status poll immediately
    bool statusPollForMoveError = false;// status poll was triggered by a failed move
    char moveErrorBuf[CURL_ERROR_SIZE] = {};
    u64 gameSession = 0;                // bumped on exit so stale responses are ignored
    u64 statusJobSession = 0;
    u64 moveJobSession = 0;
    NetJob *ffJobs[8] = {};
    int ffCount = 0;

    // Optimistic navigation: these jump the UI to the destination page the
    // instant the button is pressed, then resolve in the background — the
    // page is reverted with an error if the request turns out to have failed.
    NetJob *queueJoinJob  = nullptr;   // in-flight /api/3ds/queue
    NetJob *roomCreateJob = nullptr;   // in-flight /api/3ds/private/create
    NetJob *roomJoinJob   = nullptr;   // in-flight /api/3ds/private/join
    u64 queueJoinJobSession  = 0;
    u64 roomCreateJobSession = 0;
    u64 roomJoinJobSession   = 0;

    // Screen transition. Each screen is rebuilt every frame as a scene of
    // independent objects (screens.cpp) and handed to its own animator, which
    // slides the old page off and the new one in whenever that screen's page
    // key changes — see uikit.h. Top and bottom are entirely independent, so
    // e.g. winning a game (which changes the top screen but leaves the final
    // board on the bottom alone) animates only the top.
    //
    // These are static rather than automatic because a UiScene is a few KB
    // and the 3DS main thread's stack is not generous.
    static UiScene topScene, botScene;
    static UiAnim  topAnim,  botAnim;
    uiAnimInit(topAnim);
    uiAnimInit(botAnim);
    // True while the bottom screen's controls are still in flight, which is
    // the cue to ignore presses aimed at buttons that are not there yet.
    bool bottomBusy = false;

    // Match clock. The server is authoritative (timerSync snaps to its
    // values whenever a response includes them), but between requests we
    // tick the side-to-move's clock down locally each frame so it visibly
    // counts instead of only updating on network round-trips.
    int timerBaseW = 0, timerBaseB = 0;
    u64 timerSyncTick = 0;

    static uint8_t qrTempBuf[qrcodegen_BUFFER_LEN_FOR_VERSION(5)];
    static uint8_t qrData   [qrcodegen_BUFFER_LEN_FOR_VERSION(5)];
    bool qrReady = false;

    bool returnToErrorAfterKeyboardCancel = false;
    bool confirmingQuit = false;

    u64 lastPollTick = 0;
    constexpr u64 POLL_INTERVAL_TICKS = CPU_TICKS_PER_MSEC * 2000;
    int previousNavDx = 0;
    int previousNavDy = 0;

    // Fire-and-forget request: submit on the worker thread, reap it later.
    auto fireAndForget = [&](NetOp op, const char *path, const char *body)
    {
        if (ffCount < (int)(sizeof(ffJobs) / sizeof(ffJobs[0])))
        {
            NetJob *job = netJobCreate(op, path, body);
            if (job) { netJobSubmit(job); ffJobs[ffCount++] = job; }
        }
    };

    // Snap the match clock to an authoritative server reading.
    auto timerSync = [&](int w, int b)
    {
        timerBaseW = w;
        timerBaseB = b;
        timerSyncTick = svcGetSystemTick();
    };
    // Re-baseline the clock to whatever it's currently showing, without new
    // server data — used right before a local turn flip that isn't paired
    // with a fresh timerW/timerB (e.g. the move response), so the correct
    // side keeps ticking instead of freezing mid-count or double-counting.
    auto timerFreeze = [&]()
    {
        u64 elapsedSec = (svcGetSystemTick() - timerSyncTick) / (CPU_TICKS_PER_MSEC * 1000);
        int w = timerBaseW - (game.turn == 'W' ? (int)elapsedSec : 0);
        int b = timerBaseB - (game.turn == 'B' ? (int)elapsedSec : 0);
        timerSync(w < 0 ? 0 : w, b < 0 ? 0 : b);
    };

    // Assembles the read-only view of application state the scene builders
    // work from. Everything they draw is a pure function of this.
    auto buildContext = [&](int touchX, int touchY, bool touchActive) -> UiContext
    {
        UiContext ctx;
        ctx.state          = state;
        ctx.page           = lobbyPage;
        ctx.gameActive     = gameActive;
        ctx.tutorialActive = tutorialActive;
        ctx.tutorialStep   = tutorialStep;
        ctx.confirmingQuit = confirmingQuit;
        ctx.username       = username;
        ctx.elo            = elo;
        ctx.isRated        = isRated;
        ctx.timeControl    = timeControl;
        ctx.variant        = variant;
        ctx.focusIndex     = focusIndex;
        ctx.focusVisible   = focusVisible;
        ctx.qrData         = qrData;
        ctx.qrReady        = qrReady;
        ctx.statusMsg      = statusMsg;
        ctx.privateCode    = privateCode;
        ctx.joinCode       = joinCode;
        ctx.game           = tutorialActive ? &tutorial : &game;
        ctx.touchX         = touchX;
        ctx.touchY         = touchY;
        ctx.touchActive    = touchActive;
        return ctx;
    };

    // Render one frame from the current state.
    auto renderFrame = [&](int touchX, int touchY, bool touchActive)
    {
        uint8_t *topFb = gfxGetFramebuffer(GFX_TOP,    GFX_LEFT, nullptr, nullptr);
        uint8_t *botFb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, nullptr, nullptr);

        if (game.isOnline && !game.gameOver)
        {
            u64 elapsedSec = (svcGetSystemTick() - timerSyncTick) / (CPU_TICKS_PER_MSEC * 1000);
            int w = timerBaseW - (game.turn == 'W' ? (int)elapsedSec : 0);
            int b = timerBaseB - (game.turn == 'B' ? (int)elapsedSec : 0);
            game.timerW = w < 0 ? 0 : w;
            game.timerB = b < 0 ? 0 : b;
        }

        const UiContext ctx = buildContext(touchX, touchY, touchActive);
        buildTopScene   (topScene, ctx);
        buildBottomScene(botScene, ctx);

        uiAnimRun(topAnim, topScene, topFb, C_BG);
        uiAnimRun(botAnim, botScene, botFb, C_BG);
        // Read after the run, so next frame's input phase knows whether the
        // bottom screen's controls have arrived where the player can see them.
        bottomBusy = uiAnimInputBlocked(botAnim);

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    };

    // ---------------------------------------------------------------------------
    // Show "connecting" splash while we make the initial network calls
    // ---------------------------------------------------------------------------
    {
        // If SOC failed, say so immediately rather than making curl fail silently
        if (R_FAILED(socResult))
        {
            snprintf(statusMsg, sizeof(statusMsg),
                     "SOC init failed (0x%08lX) - check WiFi", socResult);
            state = AppState::ERROR_STATE;
            renderFrame(0, 0, false);
            // Skip network steps; fall straight through to the main loop
            goto main_loop;
        }

        snprintf(statusMsg, sizeof(statusMsg), "Checking saved login...");
        renderFrame(0, 0, false);
    }

    // ---------------------------------------------------------------------------
    // STEP 1 — try saved authCode from SD card
    // ---------------------------------------------------------------------------
    if (loadAuthCode(savedAuthCode))
    {
        char json[80];
        snprintf(json, sizeof(json), "{\"authCode\":\"%s\"}", savedAuthCode);

        NetCall call(NetOp::POST, "/api/auth_code_login", json);

        if (call.code() == 200)
        {
            char uname[64] = "Player";
            char idValue[16] = {};
            jsonExtract(call.data(), "username", uname, sizeof(uname));
            jsonExtract(call.data(), "id", idValue, sizeof(idValue));
            jsonExtract(call.data(), "elo", elo, sizeof(elo));
            snprintf(username,  sizeof(username),  "%s", uname);
            snprintf(statusMsg, sizeof(statusMsg), "%s  ELO %s", uname, elo);
            state = AppState::LOGGED_IN;
        }
        // Any other code → fall through to QR login below
    }

    // ---------------------------------------------------------------------------
    // STEP 2 — get external login code + build QR
    // ---------------------------------------------------------------------------
    if (state == AppState::INIT)
    {
        NetCall call(NetOp::GET, "/api/external_login");

        if (call.code() == 200 && call.data())
        {
            char codeVal[12]  = {};
            char urlVal[128]  = {};
            jsonExtract(call.data(), "code",     codeVal, sizeof(codeVal));
            jsonExtract(call.data(), "loginUrl", urlVal,  sizeof(urlVal));
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
            buildNetError(statusMsg, sizeof(statusMsg), call.code(), call.curl(), call.cerr());
        }
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

        // While the bottom screen's objects are still flying on or off, no
        // control is where the player thinks it is — so swallow presses
        // rather than acting on a button that is halfway across the screen.
        if (bottomBusy)
        {
            touched   = false;
            touchHeld = false;
            kDown    &= ~(KEY_A | KEY_B | KEY_START | KEY_TOUCH);
        }

        // "Are you sure?" quit modal — short-circuits everything else while
        // it's up; A/tap Quit actually quits, B/tap Stay/START cancels back.
        // It is a page like any other, so renderFrame slides it in and out.
        if (confirmingQuit)
        {
            const bool pressedYes = touched && buttonHit(BTN_QUIT_YES, touch.px, touch.py);
            const bool pressedNo  = touched && buttonHit(BTN_QUIT_NO,  touch.px, touch.py);

            if (pressedYes || (kDown & KEY_A)) break;
            if (pressedNo || (kDown & KEY_B) || (kDown & KEY_START)) confirmingQuit = false;

            renderFrame(touch.px, touch.py, touchHeld);
            continue;
        }

        // Reap any finished fire-and-forget network jobs (concede / leave /
        // cancel) so their buffers are freed without blocking the loop.
        {
            int i = 0;
            while (i < ffCount)
            {
                if (netJobReady(ffJobs[i]))
                {
                    netJobDestroy(ffJobs[i]);
                    ffJobs[i] = ffJobs[ffCount - 1];
                    --ffCount;
                }
                else ++i;
            }
        }

        // Queue the confirmed online move on the network thread so the frame
        // keeps rendering while the HTTP call runs.
        if (sendPending && !moveJob)
        {
            sendPending = false;
            moveFromR = game.selectedRow;
            moveFromC = game.selectedCol;
            moveToR = game.targetRow;
            moveToC = game.targetCol;
            char moveJson[192];
            snprintf(moveJson, sizeof(moveJson),
                     "{\"authCode\":\"%s\",\"gameId\":\"%s\",\"from\":{\"r\":%d,\"c\":%d},\"to\":{\"r\":%d,\"c\":%d}}",
                     savedAuthCode, pollingGameId, moveFromR, moveFromC, moveToR, moveToC);
            moveJob = netJobCreate(NetOp::POST, "/api/3ds/move", moveJson);
            if (moveJob)
            {
                moveJobSession = gameSession;
                netJobSubmit(moveJob);
            }
        }

        // Handle the move response when the network thread finishes. The game
        // can also be over at this point (opponent forfeited / timed out while
        // it was our turn) — the server replies "Game is not active", which we
        // resolve with an immediate status poll below.
        if (moveJob && netJobReady(moveJob))
        {
            NetJob *j = moveJob;
            moveJob = nullptr;
            const bool stale = moveJobSession != gameSession;
            moveJobSession = 0;

            if (!stale && j->httpCode == 200)
            {
                char respStatus[16] = {};
                char respWinner[4] = {};
                jsonExtract(j->response.data, "status", respStatus, sizeof(respStatus));
                jsonExtract(j->response.data, "winner", respWinner, sizeof(respWinner));
                game.confirmMove = false;
                game.pieceSelected = false;
                game.selectedRow = game.selectedCol = -1;
                game.targetRow = game.targetCol = -1;
                if (parseBoard(j->response.data, game.board))
                {
                    if (strcmp(respStatus, "finished") == 0)
                    {
                        game.gameOver = true;
                        game.winner = respWinner[0] ? respWinner[0] : 0;
                        game.isOnline = false;
                        onlineGame = false;
                        game.statusMsg = "Match complete";
                    }
                    else
                    {
                        timerFreeze();
                        game.turn = game.player == 'W' ? 'B' : 'W';
                        game.statusMsg = "Move sent. Waiting for server";
                    }
                }
                else
                {
                    timerFreeze();
                    game.turn = game.player == 'W' ? 'B' : 'W';
                    game.statusMsg = "Move sent. Waiting for server";
                }
                lastPollingTick = 0;
                forceStatusPoll = true;
            }
            else if (!stale)
            {
                // Revert the optimistic move so the board shows reality again.
                game.board[moveFromR][moveFromC] = game.player;
                game.board[moveToR][moveToC] = '0';
                game.confirmMove = false;
                char serverError[160] = {};
                jsonExtract(j->response.data, "error", serverError, sizeof(serverError));
                snprintf(moveErrorBuf, sizeof(moveErrorBuf), "%s", serverError[0] ? serverError :
                         (j->curlError[0] ? j->curlError : "Move failed"));
                // The game may have ended while it was our turn — check status.
                game.statusMsg = "Checking game status...";
                forceStatusPoll = true;
                statusPollForMoveError = true;
            }
            netJobDestroy(j);
        }

        // Async status poll. Runs on the worker thread while the frame keeps
        // rendering.
        //
        // This used to stop entirely once it was our turn, on the reasoning
        // that the server had nothing to tell us and a forfeit or timeout
        // would surface when we sent our move. Neither half holds: a game can
        // end on the server at any moment (opponent forfeit, disconnect, flag
        // fall), and "we find out when the move is rejected" means sitting on
        // a dead board until the player happens to try something.
        //
        // There is also a race that can strand us here. A winning move is not
        // written atomically — the server writes the board and flips `turn`
        // to us first, and only then does handleGameEnd mark the game
        // finished. A poll landing in that window reports "matched" with turn
        // already ours, and under the old rule that was the last poll we ever
        // made. So keep polling through our own turn too, just more slowly
        // since nothing is normally expected then.
        const bool inOnlineGame = onlineGame && !game.gameOver;
        const u64  pollInterval = (inOnlineGame && game.turn == game.player)
                                      ? POLL_OUR_TURN_TICKS
                                      : POLL_WAITING_TICKS;
        if (state == AppState::LOGGED_IN && !statusJob && !moveJob &&
            (forceStatusPoll || queueing || inOnlineGame) &&
            (forceStatusPoll || svcGetSystemTick() - lastPollingTick >= pollInterval))
        {
            lastPollingTick = svcGetSystemTick();
            forceStatusPoll = false;
            char pollJson[96];
            snprintf(pollJson, sizeof(pollJson), "{\"authCode\":\"%s\"}", savedAuthCode);
            statusJob = netJobCreate(NetOp::POST, "/api/3ds/status", pollJson);
            if (statusJob)
            {
                statusJobSession = gameSession;
                netJobSubmitPoll(statusJob);
            }
        }

        if (statusJob && netJobReady(statusJob))
        {
            NetJob *j = statusJob;
            statusJob = nullptr;
            const bool stale = statusJobSession != gameSession;
            const bool fromMoveError = statusPollForMoveError;
            statusPollForMoveError = false;
            statusJobSession = 0;

            if (!stale && j->httpCode == 200 && j->response.data)
            {
                char pollStatus[24] = {};
                jsonExtract(j->response.data, "status", pollStatus, sizeof(pollStatus));
                if (strcmp(pollStatus, "matched") == 0)
                {
                    char color[4] = "W";
                    char turn[4] = "W";
                    char timerWVal[8] = {};
                    char timerBVal[8] = {};
                    char prevGameId[48] = {};
                    snprintf(prevGameId, sizeof(prevGameId), "%s", pollingGameId);
                    jsonExtract(j->response.data, "gameId", pollingGameId, sizeof(pollingGameId));
                    jsonExtract(j->response.data, "color", color, sizeof(color));
                    jsonExtract(j->response.data, "turn", turn, sizeof(turn));
                    if (jsonExtract(j->response.data, "timerW", timerWVal, sizeof(timerWVal)) &&
                        jsonExtract(j->response.data, "timerB", timerBVal, sizeof(timerBVal)))
                        timerSync(atoi(timerWVal), atoi(timerBVal));
                    if (parseBoard(j->response.data, game.board))
                    {
                        const bool wasOurTurn = game.turn == game.player;
                        game.player = color[0] == 'B' ? 'B' : 'W';
                        game.turn = turn[0] == 'B' ? 'B' : 'W';
                        const bool nowOurTurn = game.turn == game.player;
                        // Only reset transient selection state when the game state
                        // changed (opponent moved) or we just joined a new game.
                        if (!wasOurTurn || !nowOurTurn || strcmp(prevGameId, pollingGameId) != 0)
                        {
                            game.selectedRow = game.selectedCol = -1;
                            game.targetRow = game.targetCol = -1;
                            game.cursorRow = game.cursorCol = 0;
                            game.pieceSelected = false;
                            game.confirmMove = false;
                            game.flashTimer = 0;
                            snprintf(statusMsg, sizeof(statusMsg),
                                     game.turn == game.player ? "Choose a piece to move" : "Waiting for opponent");
                            game.statusMsg = statusMsg;
                        }
                        game.gameOver = false;
                        game.winner = 0;
                        game.isOnline = true;
                        queueing = false;
                        onlineGame = true;
                        gameActive = true;
                        lobbyPage = LobbyPage::HOME;
                    }
                    // A failed move send means the board was reverted; if the
                    // game is still active and it's our turn, surface the error.
                    if (fromMoveError && game.turn == game.player)
                    {
                        snprintf(statusMsg, sizeof(statusMsg), "%s", moveErrorBuf);
                        game.statusMsg = statusMsg;
                    }
                }
                else if (strcmp(pollStatus, "finished") == 0)
                {
                    char color[4] = "W";
                    char winner[4] = {};
                    jsonExtract(j->response.data, "gameId", pollingGameId, sizeof(pollingGameId));
                    jsonExtract(j->response.data, "color", color, sizeof(color));
                    jsonExtract(j->response.data, "winner", winner, sizeof(winner));
                    // Take the final board if it parses, but never make ending
                    // the match conditional on it. This whole block used to sit
                    // inside the parseBoard check, so any hiccup reading the
                    // board meant the server's "this game is over" was thrown
                    // away silently and the console carried on showing a live
                    // board. Keeping the last known position is a far better
                    // failure mode than ignoring the result.
                    parseBoard(j->response.data, game.board);
                    game.player = color[0] == 'B' ? 'B' : 'W';
                    game.selectedRow = game.selectedCol = -1;
                    game.targetRow = game.targetCol = -1;
                    game.pieceSelected = false;
                    game.confirmMove = false;
                    game.gameOver = true;
                    game.winner = winner[0] ? winner[0] : 0;
                    game.isOnline = false;
                    game.statusMsg = "Match complete";
                    queueing = false;
                    onlineGame = false;
                    gameActive = true;
                    lobbyPage = LobbyPage::HOME;
                }
                else if (strcmp(pollStatus, "waiting") == 0 && queueing)
                {
                    // Private room still open — keep polling for the joiner.
                }
                else if (gameActive && onlineGame &&
                         (strcmp(pollStatus, "idle") == 0 ||
                          strcmp(pollStatus, "waiting") == 0))
                {
                    // We think we are in a game and the server does not agree:
                    // either it has no record of one ("idle"), or the only
                    // thing it can find for us is an old private room still
                    // sitting open, which shadows the finished-game lookup.
                    // Either way the match is over.
                    //
                    // This used to just set a status line and stop polling,
                    // leaving a board on screen that looked live but could
                    // never take another move — the same "nothing happens"
                    // the lost finished-poll produced.
                    game.gameOver      = true;
                    game.winner        = '-';   // ended, but with no result to report
                    game.isOnline      = false;
                    game.pieceSelected = false;
                    game.confirmMove   = false;
                    onlineGame         = false;
                    queueing           = false;
                    snprintf(statusMsg, sizeof(statusMsg),
                             "The match ended - your opponent may have disconnected");
                    game.statusMsg = statusMsg;
                }
            }
            else if (!stale && j->httpCode != 409)
            {
                snprintf(statusMsg, sizeof(statusMsg), "Polling failed (%ld): %.150s", j->httpCode,
                         j->curlError[0] ? j->curlError : "server unavailable");
            }
            netJobDestroy(j);
        }

        // Resolve the optimistic public-queue join. On success the queue
        // screen we already jumped to just needed its status message
        // updated; on failure, back out to the settings page with the error.
        if (queueJoinJob && netJobReady(queueJoinJob))
        {
            NetJob *j = queueJoinJob;
            queueJoinJob = nullptr;
            const bool stale = queueJoinJobSession != gameSession;
            queueJoinJobSession = 0;

            if (!stale)
            {
                if (j->httpCode == 200)
                {
                    snprintf(statusMsg, sizeof(statusMsg), "Waiting for an opponent...");
                }
                else
                {
                    char errMsg[128] = {};
                    if (!jsonExtract(j->response.data, "error", errMsg, sizeof(errMsg)))
                        snprintf(errMsg, sizeof(errMsg), "Queue failed (%ld): %.90s", j->httpCode,
                                 j->curlError[0] ? j->curlError : "server unavailable");
                    queueing = false;
                    lobbyPage = LobbyPage::PUBLIC_SETTINGS;
                    focusIndex = 0;
                    snprintf(statusMsg, sizeof(statusMsg), "%s", errMsg);
                }
            }
            netJobDestroy(j);
        }

        // Resolve the optimistic private-room creation. On success, fill in
        // the room code on the waiting screen we already jumped to; on
        // failure, back out to the create-room page with the error.
        if (roomCreateJob && netJobReady(roomCreateJob))
        {
            NetJob *j = roomCreateJob;
            roomCreateJob = nullptr;
            const bool stale = roomCreateJobSession != gameSession;
            roomCreateJobSession = 0;

            if (!stale)
            {
                if (j->httpCode == 200)
                {
                    char codeVal[16] = {};
                    char gameIdVal[64] = {};
                    char timerWVal[8] = {};
                    char timerBVal[8] = {};
                    jsonExtract(j->response.data, "code", codeVal, sizeof(codeVal));
                    jsonExtract(j->response.data, "gameId", gameIdVal, sizeof(gameIdVal));
                    if (jsonExtract(j->response.data, "timerW", timerWVal, sizeof(timerWVal)) &&
                        jsonExtract(j->response.data, "timerB", timerBVal, sizeof(timerBVal)))
                        timerSync(atoi(timerWVal), atoi(timerBVal));
                    snprintf(privateCode, sizeof(privateCode), "%s", codeVal);
                    snprintf(pollingGameId, sizeof(pollingGameId), "%s", gameIdVal);
                    snprintf(statusMsg, sizeof(statusMsg), "Room created — share code %s", codeVal);
                }
                else
                {
                    char errMsg[128] = {};
                    if (!jsonExtract(j->response.data, "error", errMsg, sizeof(errMsg)))
                        snprintf(errMsg, sizeof(errMsg), "Create failed (%ld): %.90s", j->httpCode,
                                 j->curlError[0] ? j->curlError : "server unavailable");
                    queueing = false;
                    lobbyPage = LobbyPage::PRIVATE_CREATE;
                    focusIndex = 0;
                    snprintf(statusMsg, sizeof(statusMsg), "%s", errMsg);
                }
            }
            netJobDestroy(j);
        }

        // Resolve joining a private room by code. There's no safe "optimistic"
        // destination here (a bad code must never drop the player into a
        // stale/empty board), so this just enters the game once confirmed.
        if (roomJoinJob && netJobReady(roomJoinJob))
        {
            NetJob *j = roomJoinJob;
            roomJoinJob = nullptr;
            const bool stale = roomJoinJobSession != gameSession;
            roomJoinJobSession = 0;

            if (!stale)
            {
                if (j->httpCode == 200 && parseBoard(j->response.data, game.board))
                {
                    char timerWVal[8] = {};
                    char timerBVal[8] = {};
                    jsonExtract(j->response.data, "gameId", pollingGameId, sizeof(pollingGameId));
                    if (jsonExtract(j->response.data, "timerW", timerWVal, sizeof(timerWVal)) &&
                        jsonExtract(j->response.data, "timerB", timerBVal, sizeof(timerBVal)))
                        timerSync(atoi(timerWVal), atoi(timerBVal));
                    game.player = 'B';
                    game.turn = 'W';
                    game.selectedRow = game.selectedCol = -1;
                    game.targetRow = game.targetCol = -1;
                    game.cursorRow = game.cursorCol = 0;
                    game.pieceSelected = false;
                    game.confirmMove = false;
                    game.gameOver = false;
                    game.winner = 0;
                    game.flashTimer = 0;
                    game.isOnline = true;
                    game.statusMsg = "Private match started";
                    onlineGame = true;
                    gameActive = true;
                    queueing = false;
                    lobbyPage = LobbyPage::HOME;
                    privateCode[0] = 0;
                    joinCode[0] = 0;
                    focusIndex = 0;
                }
                else
                {
                    char errMsg[128] = {};
                    if (!jsonExtract(j->response.data, "error", errMsg, sizeof(errMsg)))
                        snprintf(errMsg, sizeof(errMsg), "Join failed (%ld): %.90s", j->httpCode,
                                 j->curlError[0] ? j->curlError : "server unavailable");
                    snprintf(statusMsg, sizeof(statusMsg), "%s", errMsg);
                }
            }
            netJobDestroy(j);
        }

        int navDx = 0, navDy = 0;
        if (kDown & KEY_DLEFT) navDx = -1;
        else if (kDown & KEY_DRIGHT) navDx = 1;
        else if (kDown & KEY_DUP) navDy = -1;
        else if (kDown & KEY_DDOWN) navDy = 1;
        else if (circle.dx < -120) navDx = -1;
        else if (circle.dx > 120) navDx = 1;
        else if (circle.dy > 120) navDy = -1;
        else if (circle.dy < -120) navDy = 1;

        const int currentFocusCount = focusCount(state, lobbyPage);
        if (navDx == 0 && navDy == 0)
        {
            previousNavDx = 0;
            previousNavDy = 0;
        }
        else if ((navDx != previousNavDx || navDy != previousNavDy) && currentFocusCount > 0)
        {
            int xs[6], ys[6];
            int n = focusPoints(state, lobbyPage, xs, ys);
            if (n > 0)
            {
                focusIndex = focusMove(state, lobbyPage, focusIndex, navDx, navDy);
                focusVisible = true;
            }
            previousNavDx = navDx;
            previousNavDy = navDy;
        }

        if ((kDown & KEY_B) && state == AppState::LOGGED_IN && lobbyPage != LobbyPage::HOME)
        {
            if (lobbyPage == LobbyPage::PRIVATE_WAIT)
            {
                snprintf(statusMsg, sizeof(statusMsg), "Leaving room...");
                char cancelJson[96];
                snprintf(cancelJson, sizeof(cancelJson), "{\"authCode\":\"%s\"}", savedAuthCode);
                fireAndForget(NetOp::POST, "/api/3ds/private/cancel", cancelJson);
                ++gameSession;
                queueing = false;
                privateCode[0] = 0;
            }
            goBack(state, lobbyPage, statusMsg);
            focusIndex = 0;
        }

        if (!gameActive && !tutorialActive && (kDown & KEY_A))
        {
            int focusX = 0;
            int focusY = 0;
            if (focusPoint(state, lobbyPage, focusIndex, focusX, focusY))
            {
                touch.px = focusX;
                touch.py = focusY;
                touched = true;
                touchHeld = true;
                focusPressActive = true;
                focusPressX = focusX;
                focusPressY = focusY;
            }
        }
        else if (!gameActive && !tutorialActive && focusPressActive)
        {
            if (kHeld & KEY_A)
            {
                touch.px = focusPressX;
                touch.py = focusPressY;
                touchHeld = true;
            }
            else
                focusPressActive = false;
        }

        if (kDown & KEY_START) confirmingQuit = true;

        if (gameActive)
        {
            const bool quitComboB = (kDown & KEY_B) && (kHeld & KEY_SELECT);
            const bool quitComboS = (kDown & KEY_SELECT) && (kHeld & KEY_B);

            if (quitComboB || quitComboS)
            {
                if (onlineGame && !game.gameOver)
                {
                    game.statusMsg = "Forfeiting...";
                    char concedeJson[128];
                    snprintf(concedeJson, sizeof(concedeJson),
                             "{\"authCode\":\"%s\",\"gameId\":\"%s\"}", savedAuthCode, pollingGameId);
                    fireAndForget(NetOp::POST, "/api/3ds/concede", concedeJson);
                    onlineGame = false;
                    game.isOnline = false;
                    gameActive = false;
                    queueing = false;
                    ++gameSession;
                    lobbyPage = LobbyPage::HOME;
                    snprintf(statusMsg, sizeof(statusMsg), "You forfeited the game");
                }
                else
                {
                    gameActive = false;
                    onlineGame = false;
                    queueing = false;
                    game.isOnline = false;
                    ++gameSession;
                    statusMsg[0] = 0;
                }
                goto render;
            }
            else if (kDown & KEY_B)
            {
                if (game.pieceSelected)
                {
                    game.pieceSelected = false;
                    game.selectedRow = game.selectedCol = -1;
                    game.targetRow = game.targetCol = -1;
                    game.statusMsg = "Choose a piece to move";
                }
                else
                {
                    gameActive = false;
                    onlineGame = false;
                    queueing = false;
                    game.isOnline = false;
                    ++gameSession;
                    statusMsg[0] = 0;
                }
            }

            int circleDir = -1;
            if (circle.dx < -120) circleDir = 0;
            else if (circle.dy < -120) circleDir = 1;
            else if (circle.dx > 120) circleDir = 2;
            else if (circle.dy > 120) circleDir = 3;

            int gameDirection = -1;
            if (kDown & KEY_DLEFT) gameDirection = 0;
            else if (kDown & KEY_DDOWN) gameDirection = 1;
            else if (kDown & KEY_DRIGHT) gameDirection = 2;
            else if (kDown & KEY_DUP) gameDirection = 3;
            else if (circleDir >= 0 && circleDir != lastCircleDir) gameDirection = circleDir;
            else if (circleDir >= 0 && svcGetSystemTick() - lastGameRepeatTick >= GAME_REPEAT_TICKS) gameDirection = circleDir;

            lastCircleDir = circleDir;

            if (gameDirection >= 0 && !game.confirmMove)
            {
                if (moveGameCursor(game, gameDirection))
                    lastGameRepeatTick = svcGetSystemTick();
                else
                    game.flashTimer = 6;
            }

            if (!game.confirmMove && touched && touch.px >= BOARD_X && touch.px < BOARD_X + BOARD_PX &&
                touch.py >= BOARD_Y && touch.py < BOARD_Y + BOARD_PX)
            {
                int c = (touch.px - BOARD_X) / BOARD_TILE;
                int r = (touch.py - BOARD_Y) / BOARD_TILE;
                if (!game.pieceSelected)
                    selectGamePiece(game, r, c);
                else if (trySetDirectionToCell(game, r, c))
                {
                    game.cursorRow = game.targetRow;
                    game.cursorCol = game.targetCol;
                    if (onlineGame)
                        confirmOnlineMove(game, sendPending);
                    else
                        applyGameMove(game);
                }
                else if (game.board[r][c] == game.player)
                    selectGamePiece(game, r, c);
            }
            if ((kDown & KEY_A) && game.pieceSelected && !game.confirmMove)
            {
                if (onlineGame)
                    confirmOnlineMove(game, sendPending);
                else
                    applyGameMove(game);
            }
            else if (kDown & KEY_A)
                selectGamePiece(game, game.cursorRow, game.cursorCol);
            if (game.flashTimer > 0) --game.flashTimer;
            goto render;
        }

        if (tutorialActive)
        {
            const TutorialStep &step = TUTORIAL_STEPS[tutorialStep];
            const bool waitingOnMove = step.action == TutorialAction::UserMove ||
                                      step.action == TutorialAction::UserWin;
            const bool quitCombo = ((kDown & KEY_B) && (kHeld & KEY_SELECT)) ||
                                   ((kDown & KEY_SELECT) && (kHeld & KEY_B));

            if (quitCombo)
            {
                tutorialActive = false;
                goto render;
            }

            if (kDown & KEY_B)
            {
                if (waitingOnMove && tutorial.pieceSelected)
                {
                    tutorial.pieceSelected = false;
                    tutorial.selectedRow = tutorial.selectedCol = -1;
                    tutorial.targetRow = tutorial.targetCol = -1;
                    tutorial.statusMsg = step.text;
                }
                else
                {
                    tutorialActive = false;
                    goto render;
                }
            }

            if (waitingOnMove)
            {
                int circleDir = -1;
                if (circle.dx < -120) circleDir = 0;
                else if (circle.dy < -120) circleDir = 1;
                else if (circle.dx > 120) circleDir = 2;
                else if (circle.dy > 120) circleDir = 3;

                int gameDirection = -1;
                if (kDown & KEY_DLEFT) gameDirection = 0;
                else if (kDown & KEY_DDOWN) gameDirection = 1;
                else if (kDown & KEY_DRIGHT) gameDirection = 2;
                else if (kDown & KEY_DUP) gameDirection = 3;
                else if (circleDir >= 0 && circleDir != lastCircleDir) gameDirection = circleDir;
                else if (circleDir >= 0 && svcGetSystemTick() - lastGameRepeatTick >= GAME_REPEAT_TICKS) gameDirection = circleDir;
                lastCircleDir = circleDir;

                if (gameDirection >= 0)
                {
                    if (moveGameCursor(tutorial, gameDirection))
                        lastGameRepeatTick = svcGetSystemTick();
                    else
                        tutorial.flashTimer = 6;
                }

                if (touched && touch.px >= BOARD_X && touch.px < BOARD_X + BOARD_PX &&
                    touch.py >= BOARD_Y && touch.py < BOARD_Y + BOARD_PX)
                {
                    int c = (touch.px - BOARD_X) / BOARD_TILE;
                    int r = (touch.py - BOARD_Y) / BOARD_TILE;
                    if (!tutorial.pieceSelected)
                        tutorialSelectCell(tutorial, tutorialStep, r, c);
                    else if (trySetDirectionToCell(tutorial, r, c))
                    {
                        tutorial.cursorRow = tutorial.targetRow;
                        tutorial.cursorCol = tutorial.targetCol;
                        if (tutorialConfirmMove(tutorial, tutorialStep))
                            tutorialActive = false;
                    }
                    else if (tutorial.board[r][c] == tutorial.player)
                        tutorialSelectCell(tutorial, tutorialStep, r, c);
                }

                if ((kDown & KEY_A) && tutorial.pieceSelected)
                {
                    if (tutorialConfirmMove(tutorial, tutorialStep))
                        tutorialActive = false;
                }
                else if (kDown & KEY_A)
                    tutorialSelectCell(tutorial, tutorialStep, tutorial.cursorRow, tutorial.cursorCol);
            }
            else if (kDown & KEY_A)
            {
                if (tutorialAdvance(tutorial, tutorialStep))
                    tutorialActive = false;
            }

            if (tutorial.flashTimer > 0) --tutorial.flashTimer;
            goto render;
        }

        // ----- Button actions -----
        if (touched)
        {
            if (buttonHit(BTN_QUIT, touch.px, touch.py))
                confirmingQuit = true;

            if (state == AppState::LOGGED_IN && lobbyPage == LobbyPage::HOME &&
                buttonHit(BTN_SIGNOUT, touch.px, touch.py))
            {
                deleteAuthCode();
                username[0] = 0;
                statusMsg[0] = 0;
                loginCode[0] = 0;
                loginUrl[0] = 0;
                qrReady = false;

                snprintf(statusMsg, sizeof(statusMsg), "Signing out...");
                renderFrame(touch.px, touch.py, touchHeld);

                NetCall call(NetOp::GET, "/api/external_login");
                if (call.code() == 200 && call.data())
                {
                    char codeVal[12] = {};
                    char urlVal[128] = {};
                    jsonExtract(call.data(), "code", codeVal, sizeof(codeVal));
                    jsonExtract(call.data(), "loginUrl", urlVal, sizeof(urlVal));
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
                    buildNetError(statusMsg, sizeof(statusMsg), call.code(), call.curl(), call.cerr());
                }
                focusIndex = 0;
            }
            else if (state == AppState::LOGGED_IN)
            {
                const LobbyPage pageBefore = lobbyPage;
                if (lobbyPage == LobbyPage::HOME)
                {
                    if (buttonHit(BTN_PUBLIC_MATCH, touch.px, touch.py)) lobbyPage = LobbyPage::PUBLIC_SETTINGS;
                    else if (buttonHit(BTN_PRIVATE_ROOM, touch.px, touch.py)) lobbyPage = LobbyPage::PRIVATE_CHOICE;
                    else if (buttonHit(BTN_LOCAL_PLAY, touch.px, touch.py))
                    {
                        lobbyPage = LobbyPage::LOCAL_SETTINGS;
                        focusIndex = 0;
                    }
                    else if (buttonHit(BTN_TUTORIAL, touch.px, touch.py))
                    {
                        tutorialReset(tutorial, tutorialStep);
                        tutorialActive = true;
                    }
                }
                else if (buttonHit(BTN_BACK, touch.px, touch.py))
                {
                    if (lobbyPage == LobbyPage::PRIVATE_CREATE || lobbyPage == LobbyPage::PRIVATE_JOIN)
                        lobbyPage = LobbyPage::PRIVATE_CHOICE;
                    else
                        lobbyPage = LobbyPage::HOME;
                    statusMsg[0] = 0;
                }
                else if (lobbyPage == LobbyPage::PRIVATE_CHOICE)
                {
                    if (buttonHit(BTN_CREATE_ROOM, touch.px, touch.py)) lobbyPage = LobbyPage::PRIVATE_CREATE;
                    else if (buttonHit(BTN_JOIN_ROOM, touch.px, touch.py)) lobbyPage = LobbyPage::PRIVATE_JOIN;
                }
                else if (lobbyPage == LobbyPage::QUEUE && buttonHit(BTN_CANCEL_QUEUE, touch.px, touch.py))
                {
                    snprintf(statusMsg, sizeof(statusMsg), "Leaving queue...");
                    char leaveJson[96];
                    snprintf(leaveJson, sizeof(leaveJson), "{\"authCode\":\"%s\"}", savedAuthCode);
                    fireAndForget(NetOp::POST, "/api/3ds/leave", leaveJson);
                    ++gameSession;
                    queueing = false;
                    lobbyPage = LobbyPage::HOME;
                    statusMsg[0] = 0;
                    focusIndex = 0;
                }
                else if (lobbyPage == LobbyPage::PRIVATE_WAIT && buttonHit(BTN_CANCEL_PRIVATE, touch.px, touch.py))
                {
                    snprintf(statusMsg, sizeof(statusMsg), "Leaving room...");
                    char cancelJson[96];
                    snprintf(cancelJson, sizeof(cancelJson), "{\"authCode\":\"%s\"}", savedAuthCode);
                    fireAndForget(NetOp::POST, "/api/3ds/private/cancel", cancelJson);
                    ++gameSession;
                    queueing = false;
                    privateCode[0] = 0;
                    lobbyPage = LobbyPage::PRIVATE_CHOICE;
                    statusMsg[0] = 0;
                    focusIndex = 0;
                }
                else if (lobbyPage == LobbyPage::LOCAL_SETTINGS)
                {
                    if (buttonHit(BTN_LOCAL_VARIANT, touch.px, touch.py))
                    {
                        variant = (variant + 1) % 4;
                        snprintf(statusMsg, sizeof(statusMsg), "Local variant changed");
                    }
                    else if (buttonHit(BTN_START_LOCAL, touch.px, touch.py))
                    {
                        resetGame(game, variant);
                        onlineGame = false;
                        queueing = false;
                        gameActive = true;
                        lobbyPage = LobbyPage::HOME;
                    }
                }
                else if ((lobbyPage == LobbyPage::PUBLIC_SETTINGS || lobbyPage == LobbyPage::PRIVATE_CREATE) &&
                         !buttonHit(BTN_CONTINUE, touch.px, touch.py))
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
                else if (buttonHit(BTN_CONTINUE, touch.px, touch.py) && lobbyPage == LobbyPage::PRIVATE_JOIN && !roomJoinJob)
                {
                    if (showKeyboard("Join code (e.g. APPLE123)", joinCode, sizeof(joinCode)))
                    {
                        snprintf(statusMsg, sizeof(statusMsg), "Joining room...");
                        char joinJson[160];
                        snprintf(joinJson, sizeof(joinJson), "{\"authCode\":\"%s\",\"code\":\"%s\"}", savedAuthCode, joinCode);
                        roomJoinJob = netJobCreate(NetOp::POST, "/api/3ds/private/join", joinJson);
                        if (roomJoinJob)
                        {
                            roomJoinJobSession = gameSession;
                            netJobSubmit(roomJoinJob);
                        }
                    }
                }
                else if (buttonHit(BTN_CONTINUE, touch.px, touch.py) && lobbyPage == LobbyPage::PRIVATE_CREATE && !roomCreateJob)
                {
                    static const char *queueTimes[] = {"0.25|3", "1|0", "3|2"};
                    char createJson[160];
                    snprintf(createJson, sizeof(createJson),
                             "{\"authCode\":\"%s\",\"timeControl\":\"%s\",\"variant\":\"%s\",\"isRated\":%s}",
                             savedAuthCode, queueTimes[timeControl],
                             variant == 0 ? "classic" : variant == 1 ? "fog_of_war" :
                             variant == 2 ? "random_setup" : "schizophrenic",
                             isRated ? "true" : "false");
                    roomCreateJob = netJobCreate(NetOp::POST, "/api/3ds/private/create", createJson);
                    if (roomCreateJob)
                    {
                        roomCreateJobSession = gameSession;
                        netJobSubmit(roomCreateJob);
                        // Optimistic navigation: jump to the waiting room now — the
                        // code fills in once the response arrives below.
                        privateCode[0] = 0;
                        queueing = true;
                        lobbyPage = LobbyPage::PRIVATE_WAIT;
                        focusIndex = 0;
                        snprintf(statusMsg, sizeof(statusMsg), "Creating room...");
                    }
                }
                else if (buttonHit(BTN_CONTINUE, touch.px, touch.py))
                {
                    if (lobbyPage == LobbyPage::PUBLIC_SETTINGS && !queueJoinJob)
                    {
                        static const char *queueTimes[] = {"0.25|3", "1|0", "3|2"};
                        char queueJson[160];
                        snprintf(queueJson, sizeof(queueJson),
                                 "{\"authCode\":\"%s\",\"timeControl\":\"%s\",\"variant\":\"%s\",\"isRated\":%s}",
                                 savedAuthCode, queueTimes[timeControl],
                                 variant == 0 ? "classic" : variant == 1 ? "fog_of_war" :
                                 variant == 2 ? "random_setup" : "schizophrenic",
                                 isRated ? "true" : "false");
                        queueJoinJob = netJobCreate(NetOp::POST, "/api/3ds/queue", queueJson);
                        if (queueJoinJob)
                        {
                            queueJoinJobSession = gameSession;
                            netJobSubmit(queueJoinJob);
                            // Optimistic navigation: jump straight to the queue screen.
                            queueing = true;
                            lobbyPage = LobbyPage::QUEUE;
                            focusIndex = 0;
                            snprintf(statusMsg, sizeof(statusMsg), "Joining matchmaking...");
                        }
                    }
                    else if (lobbyPage != LobbyPage::PUBLIC_SETTINGS)
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
                renderFrame(touch.px, touch.py, touchHeld);

                NetCall call(NetOp::POST, "/api/guest", "{}");
                if (call.code() == 200 && call.data())
                {
                    char guestName[64] = "Guest";
                    char idValue[16] = {};
                    jsonExtract(call.data(), "username", guestName, sizeof(guestName));
                    jsonExtract(call.data(), "id", idValue, sizeof(idValue));
                    jsonExtract(call.data(), "elo", elo, sizeof(elo));
                    snprintf(username, sizeof(username), "%s", guestName);
                    snprintf(statusMsg, sizeof(statusMsg), "%s  ELO %s", guestName, elo);
                    state = AppState::LOGGED_IN;
                }
                else
                {
                    buildNetError(statusMsg, sizeof(statusMsg), call.code(), call.curl(), call.cerr());
                    state = AppState::ERROR_STATE;
                }
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

                snprintf(statusMsg, sizeof(statusMsg), "Signing in...");
                renderFrame(touch.px, touch.py, touchHeld);

                NetCall call(NetOp::POST, "/api/device_login", json);

                if (call.code() == 200)
                {
                    char uname[64] = "Player";
                    char ac[AUTHCODE_LEN + 2] = {};
                    char idValue[16] = {};
                    jsonExtract(call.data(), "username", uname, sizeof(uname));
                    jsonExtract(call.data(), "authCode", ac, sizeof(ac));
                    jsonExtract(call.data(), "id", idValue, sizeof(idValue));
                    jsonExtract(call.data(), "elo", elo, sizeof(elo));
                    snprintf(username,  sizeof(username),  "%s", uname);
                    snprintf(statusMsg, sizeof(statusMsg), "%s  ELO %s", uname, elo);
                    if (ac[0]) { saveAuthCode(ac); snprintf(savedAuthCode, sizeof(savedAuthCode), "%s", ac); }
                    state = AppState::LOGGED_IN;
                }
                else
                {
                    char errMsg[128] = {};
                    if (!jsonExtract(call.data(), "error", errMsg, sizeof(errMsg)))
                        buildNetError(errMsg, sizeof(errMsg), call.code(), call.curl(), call.cerr());
                    snprintf(statusMsg, sizeof(statusMsg), "%s", errMsg);
                    state = AppState::ERROR_STATE;
                }
            }
        }

        // ----- Poll for QR approval (async, on the worker thread) -----
        if (state == AppState::QR_LOGIN && loginCode[0] && !qrPollJob)
        {
            u64 now = svcGetSystemTick();
            if (now - lastPollTick >= POLL_INTERVAL_TICKS)
            {
                lastPollTick = now;

                char json[32];
                snprintf(json, sizeof(json), "{\"code\":\"%s\"}", loginCode);

                qrPollJob = netJobCreate(NetOp::POST, "/api/external_login/poll", json);
                if (qrPollJob) netJobSubmitPoll(qrPollJob);
            }
        }

        if (qrPollJob && netJobReady(qrPollJob))
        {
            NetJob *j = qrPollJob;
            qrPollJob = nullptr;

            if (j->httpCode == 200 && j->response.data)
            {
                char pollStatus[32] = {};
                jsonExtract(j->response.data, "status", pollStatus, sizeof(pollStatus));

                if (strcmp(pollStatus, "approved") == 0)
                {
                    char ac[AUTHCODE_LEN + 2] = {};
                    char uname[64] = "Player";
                    char idValue[16] = {};
                    jsonExtract(j->response.data, "authCode", ac, sizeof(ac));
                    const char *userObj = strstr(j->response.data, "\"user\":");
                    if (userObj)
                    {
                        jsonExtract(userObj, "username", uname, sizeof(uname));
                        jsonExtract(userObj, "elo", elo, sizeof(elo));
                    }
                    jsonExtract(j->response.data, "id", idValue, sizeof(idValue));

                    snprintf(username, sizeof(username), "%s", uname);
                    if (ac[0]) { saveAuthCode(ac); snprintf(savedAuthCode, sizeof(savedAuthCode), "%s", ac); }

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

                    NetCall refresh(NetOp::GET, "/api/external_login");

                    if (refresh.code() == 200 && refresh.data())
                    {
                        char cv[12]  = {};
                        char uv[128] = {};
                        jsonExtract(refresh.data(), "code",     cv, sizeof(cv));
                        jsonExtract(refresh.data(), "loginUrl", uv, sizeof(uv));
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
                        buildNetError(statusMsg, sizeof(statusMsg), refresh.code(), refresh.curl(), refresh.cerr());
                    }
                }
                // "pending" → do nothing, keep polling
            }
            else if (j->httpCode == 0)
            {
                // Transport error — show verbosely in status but keep trying
                buildNetError(statusMsg, sizeof(statusMsg), 0, j->curlCode, j->curlError);
            }

            netJobDestroy(j);
        }

    render:
        renderFrame(touch.px, touch.py, touchHeld);
    }

    if (netThreadStop())
        curl_global_cleanup();
    socExit();
    free(socBuf);
    gfxExit();
    return 0;
}
