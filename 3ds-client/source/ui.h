/*
 * ui.h — Button layout, top-screen and bottom-screen drawing.
 */
#pragma once

#include "render.h"

// Centered game-board layout (bottom screen, 320x240; board is 228x228).
constexpr int BOARD_X    = 46;
constexpr int BOARD_Y    = 6;
constexpr int BOARD_TILE = 38;
constexpr int BOARD_PX   = 6 * BOARD_TILE;   // 228

// ---------------------------------------------------------------------------
// AppState
// ---------------------------------------------------------------------------
enum class AppState
{
    INIT,           // checking SD card / connecting
    QR_LOGIN,       // showing QR + polling
    KEYBOARD_LOGIN, // swkbd username/password entry
    LOGGED_IN,      // success
    ERROR_STATE,    // network or auth error
};

enum class LobbyPage
{
    HOME,
    PUBLIC_SETTINGS,
    PRIVATE_CHOICE,
    PRIVATE_CREATE,
    PRIVATE_JOIN,
    PRIVATE_WAIT,
    LOCAL_SETTINGS,
    QUEUE,
    SPECTATE_COMING,
};

// ---------------------------------------------------------------------------
// Button
// ---------------------------------------------------------------------------
struct Button
{
    int         x, y, w, h;
    const char *label;
    Color       bgColor;
    Color       textColor;
    Color       borderColor;
};

struct GameUiState
{
    char board[6][6];
    char player;
    char turn;
    int selectedRow;
    int selectedCol;
    int targetRow;
    int targetCol;
    int cursorRow;
    int cursorCol;
    bool pieceSelected;
    bool confirmMove;
    bool isOnline;
    bool gameOver;
    char winner;
    int  flashTimer;
    const char *statusMsg;
};

bool buttonHit (const Button &btn, int tx, int ty);
void drawButton(uint8_t *fb, const Button &btn, bool pressed, int textScale = 1,
                bool focused = false);

// ---------------------------------------------------------------------------
// Screen drawing
// ---------------------------------------------------------------------------
void drawTopScreen   (uint8_t *fb, AppState state,
                      LobbyPage lobbyPage,
                      const char *username, const char *elo,
                      bool isRated, int timeControl, int variant,
                      int focusIndex,
                      const uint8_t *qrData, bool qrReady,
                      const char *statusMsg, const char *privateCode);

void drawBottomScreen(uint8_t *fb, AppState state,
                      LobbyPage lobbyPage,
                      const char *username, const char *elo,
                      bool isRated, int timeControl, int variant,
                      int focusIndex, bool focusVisible,
                      bool pressedSignIn, bool pressedGuest, bool pressedSignOut,
                      bool pressedOffline, bool pressedQuit, const Button &btnSignIn,
                      const Button &btnGuest, const Button &btnSignOut,
                      const Button &btnQuit,
                      int touchX, int touchY, bool touchActive,
                      const char *privateCode);

void drawGameTopScreen(uint8_t *fb, const GameUiState &game);
void drawGameBottomScreen(uint8_t *fb, const GameUiState &game);
