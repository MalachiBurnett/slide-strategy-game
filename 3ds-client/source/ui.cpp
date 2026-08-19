/*
 * ui.cpp — Button and screen-drawing implementations.
 */
#include "ui.h"
#include "render.h"
#include <cstdio>
#include <cmath>

extern "C" {
#include "qrcodegen.h"
}

static const char PREVIEW_BOARD[6][6] = {
    {'B', '0', 'W', 'B', '0', 'W'},
    {'0', '0', '0', '0', '0', '0'},
    {'W', '0', '0', '0', '0', 'B'},
    {'B', '0', '0', '0', '0', 'W'},
    {'0', '0', '0', '0', '0', '0'},
    {'W', '0', 'B', 'W', '0', 'B'}
};

static void drawPreviewPiece(uint8_t *fb, int cx, int cy, char piece)
{
    for (int y = -7; y <= 7; ++y)
        for (int x = -7; x <= 7; ++x)
            if (x * x + y * y <= 49)
                drawPixel(fb, TOP_W, TOP_H, cx + x, cy + y, C_TEXT);
    for (int y = -5; y <= 5; ++y)
        for (int x = -5; x <= 5; ++x)
            if (x * x + y * y <= 25)
                drawPixel(fb, TOP_W, TOP_H, cx + x, cy + y, piece == 'W' ? C_BG_LIGHT : C_TEXT);
}

// ---------------------------------------------------------------------------
// Button
// ---------------------------------------------------------------------------
bool buttonHit(const Button &btn, int tx, int ty)
{
    return tx >= btn.x && tx < btn.x + btn.w &&
           ty >= btn.y && ty < btn.y + btn.h;
}

void drawButton(uint8_t *fb, const Button &btn, bool pressed, int textScale,
                bool focused)
{
    Color bg   = pressed ? C_SUCCESS : btn.bgColor;
    Color text = pressed ? C_PRIMARY_TXT : btn.textColor;
    int r = btn.h >= 30 ? 10 : 7;
    fillRoundRect(fb, BOT_W, BOT_H, btn.x, btn.y, btn.w, btn.h, r, bg);
    if (!pressed)
        drawRoundRect(fb, BOT_W, BOT_H, btn.x, btn.y, btn.w, btn.h, r,
                      focused ? 3 : 2, focused ? C_SUCCESS : btn.borderColor);

    int charW   = 8 * textScale;
    int textLen = (int)strlen(btn.label);
    int textW   = textLen * charW;
    int tx      = btn.x + (btn.w - textW) / 2;
    int ty      = btn.y + (btn.h - 8 * textScale) / 2;
    drawText(fb, BOT_W, BOT_H, tx, ty, btn.label, textScale, text);
}

// ---------------------------------------------------------------------------
// Top screen
// ---------------------------------------------------------------------------
void drawTopScreen(uint8_t *fb, AppState state,
                   LobbyPage lobbyPage,
                   const char *username, const char *elo,
                   bool isRated, int timeControl, int variant,
                   int focusIndex,
                   const uint8_t *qrData, bool qrReady,
                   const char *statusMsg)
{
    clearScreen(fb, TOP_W, TOP_H, C_BG);

    if (state == AppState::ERROR_STATE)
    {
        const Color white = {255, 255, 255};
        clearScreen(fb, TOP_W, TOP_H, {0, 0, 0});
        drawText(fb, TOP_W, TOP_H, 12, 12, "SLIDE ERROR", 2, white);
        if (statusMsg && statusMsg[0])
            drawTextWrapped(fb, TOP_W, TOP_H, 12, 42, TOP_W - 24, statusMsg, 1, white);
        return;
    }

    // Title bar
    fillRect(fb, TOP_W, TOP_H, 0, 0, TOP_W, 24, C_PRIMARY);
    drawText(fb, TOP_W, TOP_H, 8, 8, "Slide", 1, C_PRIMARY_TXT);
    drawText(fb, TOP_W, TOP_H, 204, 8, "Made by Wiizard Software", 1, C_PRIMARY_TXT);

    if (state == AppState::QR_LOGIN && qrReady)
    {
        renderQR(fb, qrData);

        int qrSize    = qrcodegen_getSize(qrData);
        int qrTotalPx = (qrSize + 4 * 2) * 5; // QR_QUIET=4, QR_PIXEL=5
        int margin    = (TOP_H - qrTotalPx) / 2;
        int qrLeft    = TOP_W - qrTotalPx - margin;
        int textAreaW = qrLeft - 16;

        int ty = 36;
        drawTextWrapped(fb, TOP_W, TOP_H, 8, ty, textAreaW,
                        "Scan the QR code to log in using another device.", 1, C_TEXT);

        int lineY = TOP_H - 28;
        fillRect(fb, TOP_W, TOP_H, 8, lineY, textAreaW, 1, C_ACCENT);

        if (statusMsg && statusMsg[0])
            drawText(fb, TOP_W, TOP_H, 8, lineY + 5, statusMsg, 1, C_TEXT);
        else
            drawText(fb, TOP_W, TOP_H, 8, lineY + 5, "Waiting for scan...", 1, C_ACCENT);
    }
    else if (state == AppState::INIT)
    {
        drawTextWrapped(fb, TOP_W, TOP_H, 8, 36, TOP_W - 16,
                        "Connecting to Slide...", 1, C_TEXT);
    }
    else if (state == AppState::KEYBOARD_LOGIN)
    {
        drawTextWrapped(fb, TOP_W, TOP_H, 8, 36, TOP_W - 16,
                        "Signing in on this device.\nEnter your username and password.",
                        1, C_TEXT);
    }
    else if (state == AppState::LOGGED_IN)
    {
        static const char *timeLabels[] = {"15s + 3s", "1 min", "3 min + 2s"};
        static const char *variantLabels[] = {"Classic", "Fog of War", "Random Setup", "Schizophrenic"};
        const char *safeUser = (username && username[0]) ? username : "Player";
        const char *safeElo = (elo && elo[0]) ? elo : "600";
        const int timeIndex = timeControl < 0 || timeControl > 2 ? 0 : timeControl;
        const int variantIndex = variant < 0 || variant > 3 ? 0 : variant;

        fillRect(fb, TOP_W, TOP_H, 0, 24, TOP_W, TOP_H - 24, C_BG);
        drawText(fb, TOP_W, TOP_H, 12, 38,
                 lobbyPage == LobbyPage::HOME ? "READY TO PLAY" : "MATCH SETUP",
                 2, C_PRIMARY);
        drawText(fb, TOP_W, TOP_H, 12, 68, safeUser, 2, C_TEXT);
        drawText(fb, TOP_W, TOP_H, 12, 88, isRated ? "RANKED PLAYER" : "CASUAL PLAYER", 1, C_ACCENT);
        drawText(fb, TOP_W, TOP_H, 12, 112, "CURRENT LOADOUT", 1, C_PRIMARY);
        drawText(fb, TOP_W, TOP_H, 12, 128, isRated ? "Ranked" : "Casual", 1, C_TEXT);
        drawText(fb, TOP_W, TOP_H, 12, 144, timeLabels[timeIndex], 1, C_TEXT);
        drawTextWrapped(fb, TOP_W, TOP_H, 12, 160, 170, variantLabels[variantIndex], 1, C_TEXT);

        fillRoundRect(fb, TOP_W, TOP_H, 236, 54, 140, 140, 10, C_PRIMARY);
        fillRoundRect(fb, TOP_W, TOP_H, 244, 62, 124, 124, 5, C_BG_DARK);
        for (int r = 0; r < 6; ++r)
        {
            for (int c = 0; c < 6; ++c)
            {
                const int x = 250 + c * 19;
                const int y = 68 + r * 19;
                fillRect(fb, TOP_W, TOP_H, x, y, 17, 17,
                         (r + c) % 2 == 0 ? C_BG_LIGHT : C_ACCENT);
            }
        }
        for (int r = 0; r < 6; ++r)
            for (int c = 0; c < 6; ++c)
                if (PREVIEW_BOARD[r][c] != '0')
                    drawPreviewPiece(fb, 258 + c * 19, 76 + r * 19, PREVIEW_BOARD[r][c]);
        drawText(fb, TOP_W, TOP_H, 290, 202, "ELO", 1, C_ACCENT);
        drawText(fb, TOP_W, TOP_H, 314, 202, safeElo, 1, C_PRIMARY);
        if (statusMsg && statusMsg[0])
            drawTextWrapped(fb, TOP_W, TOP_H, 12, 204, 210, statusMsg, 1, C_PRIMARY);
    }
}

// ---------------------------------------------------------------------------
// Bottom screen
// ---------------------------------------------------------------------------
void drawBottomScreen(uint8_t *fb, AppState state,
                      LobbyPage lobbyPage,
                      const char *username, const char *elo,
                      bool isRated, int timeControl, int variant,
                      int focusIndex, bool focusVisible,
                      bool pressedSignIn, bool pressedGuest, bool pressedSignOut,
                      bool pressedQuit, const Button &btnSignIn,
                      const Button &btnGuest, const Button &btnSignOut,
                      const Button &btnQuit)
{
    clearScreen(fb, BOT_W, BOT_H, C_BG);

    if (state == AppState::QR_LOGIN || state == AppState::ERROR_STATE)
    {
        const char *heading = (state == AppState::ERROR_STATE)
                              ? "Connection failed. Try again?"
                              : "Or sign in on this device:";
        drawTextWrapped(fb, BOT_W, BOT_H, 8, 38, BOT_W - 16, heading, 1, C_TEXT);

        drawButton(fb, btnSignIn, pressedSignIn, 1, focusVisible && focusIndex == 0);
        drawButton(fb, btnGuest,  pressedGuest, 1, focusVisible && focusIndex == 1);
        drawButton(fb, btnQuit,   pressedQuit,   1, focusVisible && focusIndex == 2);
    }
    else if (state == AppState::INIT)
    {
        drawText(fb, BOT_W, BOT_H, 8, 38, "Please wait...", 1, C_TEXT);
        drawButton(fb, btnQuit, pressedQuit, 2, focusVisible && focusIndex == 0);
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

        if (lobbyPage == LobbyPage::HOME)
        {
            drawText(fb, BOT_W, BOT_H, 8, 38, "WELCOME BACK", 1, C_ACCENT);
            drawText(fb, BOT_W, BOT_H, 8, 50, username, 1, C_TEXT);
            char rating[32];
            snprintf(rating, sizeof(rating), "ELO %s", elo);
            drawText(fb, BOT_W, BOT_H, 236, 50, rating, 1, C_PRIMARY);
            drawText(fb, BOT_W, BOT_H, 8, 64, "CHOOSE A MODE", 1, C_TEXT);
            drawButton(fb, publicMatch, false, 1, focusVisible && focusIndex == 0);
            drawButton(fb, privateRoom, false, 1, focusVisible && focusIndex == 1);
            drawButton(fb, localPlay, false, 1, focusVisible && focusIndex == 2);
            drawButton(fb, spectate, false, 1, focusVisible && focusIndex == 3);
            drawButton(fb, btnSignOut, pressedSignOut, 1, focusVisible && focusIndex == 4);
        }
        else if (lobbyPage == LobbyPage::PRIVATE_CHOICE)
        {
            drawText(fb, BOT_W, BOT_H, 8, 38, "PRIVATE ROOM", 1, C_ACCENT);
            drawTextWrapped(fb, BOT_W, BOT_H, 8, 52, BOT_W - 16, "Create a room with settings, or join with a code.", 1, C_TEXT);
            drawButton(fb, createRoom, false, 1, focusVisible && focusIndex == 0);
            drawButton(fb, joinRoom, false, 1, focusVisible && focusIndex == 1);
            drawButton(fb, back, false, 1, focusVisible && focusIndex == 2);
            drawButton(fb, btnQuit, pressedQuit, 1, focusVisible && focusIndex == 3);
        }
        else if (lobbyPage == LobbyPage::PRIVATE_JOIN)
        {
            drawText(fb, BOT_W, BOT_H, 8, 38, "JOIN A ROOM", 1, C_ACCENT);
            drawTextWrapped(fb, BOT_W, BOT_H, 8, 52, BOT_W - 16, "Enter the host's join code.", 1, C_TEXT);
            drawButton(fb, continueButton, false, 1, focusVisible && focusIndex == 0);
            drawButton(fb, back, false, 1, focusVisible && focusIndex == 1);
            drawButton(fb, btnQuit, pressedQuit, 1, focusVisible && focusIndex == 2);
        }
        else
        {
            static const char *timeLabels[] = {"15s + 3s", "1 minute", "3m + 2s"};
            static const char *variantLabels[] = {"Classic", "Fog of War", "Random Setup", "Schizophrenic"};
            const int timeIndex = timeControl < 0 || timeControl > 2 ? 0 : timeControl;
            const int variantIndex = variant < 0 || variant > 3 ? 0 : variant;
            fillRoundRect(fb, BOT_W, BOT_H, 8, 28, BOT_W - 16, 20, 5, C_BG_DARK);
            fillRoundRect(fb, BOT_W, BOT_H, 8, 52, BOT_W - 16, 20, 5, C_BG_DARK);
            fillRoundRect(fb, BOT_W, BOT_H, 8, 76, BOT_W - 16, 20, 5, C_BG_DARK);
            drawRoundRect(fb, BOT_W, BOT_H, 8, 28, BOT_W - 16, 20, 5, 1, C_ACCENT);
            drawRoundRect(fb, BOT_W, BOT_H, 8, 52, BOT_W - 16, 20, 5, 1, C_ACCENT);
            drawRoundRect(fb, BOT_W, BOT_H, 8, 76, BOT_W - 16, 20, 5, 1, C_ACCENT);
            drawText(fb, BOT_W, BOT_H, 8, 14,
                     lobbyPage == LobbyPage::PUBLIC_SETTINGS ? "Public settings" : "Create room",
                     1, C_TEXT);
            drawText(fb, BOT_W, BOT_H, 8, 34, "MATCH TYPE", 1, C_PRIMARY);
            drawText(fb, BOT_W, BOT_H, 132, 34, isRated ? "RANKED" : "CASUAL", 1, C_TEXT);
            drawText(fb, BOT_W, BOT_H, 8, 58, "TIME CONTROL", 1, C_PRIMARY);
            drawText(fb, BOT_W, BOT_H, 132, 58, timeLabels[timeIndex], 1, C_TEXT);
            drawText(fb, BOT_W, BOT_H, 8, 82, "VARIANT", 1, C_PRIMARY);
            drawText(fb, BOT_W, BOT_H, 132, 82, variantLabels[variantIndex], 1, C_TEXT);
            drawTextWrapped(fb, BOT_W, BOT_H, 8, 106, BOT_W - 16, "Settings are ready. Press continue when you are ready.", 1, C_ACCENT);
            if (focusVisible && focusIndex == 0) drawRoundRect(fb, BOT_W, BOT_H, 8, 28, BOT_W - 16, 20, 5, 3, C_SUCCESS);
            if (focusVisible && focusIndex == 1) drawRoundRect(fb, BOT_W, BOT_H, 8, 52, BOT_W - 16, 20, 5, 3, C_SUCCESS);
            if (focusVisible && focusIndex == 2) drawRoundRect(fb, BOT_W, BOT_H, 8, 76, BOT_W - 16, 20, 5, 3, C_SUCCESS);
            drawButton(fb, continueButton, false, 1, focusVisible && focusIndex == 3);
            drawButton(fb, back, false, 1, focusVisible && focusIndex == 4);
        }

        if (lobbyPage == LobbyPage::HOME ||
            lobbyPage == LobbyPage::PUBLIC_SETTINGS ||
            lobbyPage == LobbyPage::PRIVATE_CREATE)
            drawButton(fb, btnQuit, pressedQuit, 1, focusVisible && focusIndex == 5);
    }
    else if (state == AppState::KEYBOARD_LOGIN)
    {
        drawText(fb, BOT_W, BOT_H, 8, 38, "Using software keyboard...", 1, C_TEXT);
    }

}

static const Color C_BOARD_LIGHT = {232, 224, 202};
static const Color C_BOARD_DARK = {143, 108, 74};
static const Color C_MOVE = {64, 190, 96};
static const Color C_SELECTED = {45, 125, 220};

static void drawCircle(uint8_t *fb, int w, int h, int cx, int cy, int radius, Color color)
{
    for (int y = -radius; y <= radius; ++y)
        for (int x = -radius; x <= radius; ++x)
            if (x * x + y * y <= radius * radius)
                drawPixel(fb, w, h, cx + x, cy + y, color);
}

static void drawPiece(uint8_t *fb, int w, int h, int cx, int cy, char piece, int radius)
{
    drawCircle(fb, w, h, cx, cy, radius + 2, C_TEXT);
    drawCircle(fb, w, h, cx, cy, radius, piece == 'W' ? C_BG_LIGHT : C_TEXT);
    if (piece == 'W')
        drawCircle(fb, w, h, cx - radius / 3, cy - radius / 3, radius / 4, C_BG_DARK);
}

static bool gameHasMove(const GameUiState &game, int r, int c)
{
    if (game.turn != game.player) return false;
    if (game.board[r][c] != game.player) return false;
    static const int dirs[][2] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
    for (const auto &dir : dirs)
    {
        int nr = r + dir[0];
        int nc = c + dir[1];
        while (nr >= 0 && nr < 6 && nc >= 0 && nc < 6 && game.board[nr][nc] == '0')
        {
            nr += dir[0];
            nc += dir[1];
        }
        if (nr != r + dir[0] || nc != c + dir[1]) return true;
    }
    return false;
}

static void drawGameBoard(uint8_t *fb, int width, int height, const GameUiState &game,
                          int originX, int originY, int tile)
{
    for (int r = 0; r < 6; ++r)
    {
        for (int c = 0; c < 6; ++c)
        {
            const int x = originX + c * tile;
            const int y = originY + r * tile;
            const bool selected = game.pieceSelected && r == game.selectedRow && c == game.selectedCol;
            const bool target = game.pieceSelected && r == game.targetRow && c == game.targetCol;
            const bool cursor = r == game.cursorRow && c == game.cursorCol;
            const bool movable = !game.pieceSelected && gameHasMove(game, r, c);
            const Color tileColor = (r + c) % 2 == 0 ? C_BOARD_LIGHT : C_BOARD_DARK;
            fillRect(fb, width, height, x, y, tile - 1, tile - 1, tileColor);
            if (movable || selected || target)
            {
                Color ring = selected || target ? C_SELECTED : C_MOVE;
                drawRoundRect(fb, width, height, x + 2, y + 2, tile - 5, tile - 5, 4, 3, ring);
            }
            if (cursor)
                drawRoundRect(fb, width, height, x + 1, y + 1, tile - 3, tile - 3, 4, 2, C_PRIMARY);
            if (game.board[r][c] != '0')
                drawPiece(fb, width, height, x + tile / 2, y + tile / 2, game.board[r][c], tile / 3);
        }
    }
}

void drawGameTopScreen(uint8_t *fb, const GameUiState &game)
{
    clearScreen(fb, TOP_W, TOP_H, C_BG);
    fillRect(fb, TOP_W, TOP_H, 0, 0, TOP_W, 25, C_PRIMARY);
    drawText(fb, TOP_W, TOP_H, 8, 8, "SLIDE", 1, C_PRIMARY_TXT);
    drawText(fb, TOP_W, TOP_H, 250, 8, game.turn == 'W' ? "WHITE TO MOVE" : "BLACK TO MOVE", 1, C_PRIMARY_TXT);

    drawText(fb, TOP_W, TOP_H, 12, 38, game.confirmMove ? "CONFIRM MOVE?" :
             (game.pieceSelected ? "CHOOSE A DIRECTION" : "CHOOSE A PIECE TO MOVE"), 2, C_PRIMARY);
    if (game.confirmMove)
    {
        drawTextWrapped(fb, TOP_W, TOP_H, 12, 70, 220, "A: send move\nB: choose again", 1, C_TEXT);
        fillRoundRect(fb, TOP_W, TOP_H, 250, 52, 132, 76, 8, C_BG_DARK);
        drawText(fb, TOP_W, TOP_H, 264, 66, "A  CONFIRM", 1, C_SUCCESS);
        drawText(fb, TOP_W, TOP_H, 264, 84, "B  CANCEL", 1, C_ERROR);
    }
    else
    {
        drawTextWrapped(fb, TOP_W, TOP_H, 12, 70, 220,
                        game.pieceSelected ? "DPAD: choose destination\nA: confirm  B: cancel" :
                        "Touch a highlighted piece, or\nuse the DPAD and press A.", 1, C_TEXT);
        drawText(fb, TOP_W, TOP_H, 12, 128,  "BOTTOM: BOARD", 1, C_ACCENT);
        drawText(fb, TOP_W, TOP_H, 12, 148, "A  SELECT / CONFIRM", 1, C_TEXT);
        drawText(fb, TOP_W, TOP_H, 12, 164, "B  CANCEL", 1, C_TEXT);
        drawText(fb, TOP_W, TOP_H, 12, 180, "DPAD  MOVE CURSOR", 1, C_TEXT);
    }
    if (game.statusMsg && game.statusMsg[0])
        drawTextWrapped(fb, TOP_W, TOP_H, 12, 212, TOP_W - 24, game.statusMsg, 1, C_PRIMARY);
}

void drawGameBottomScreen(uint8_t *fb, const GameUiState &game)
{
    clearScreen(fb, BOT_W, BOT_H, C_BG_DARK);
    const int tile = 38;
    drawGameBoard(fb, BOT_W, BOT_H, game, 5, 5, tile);
}
