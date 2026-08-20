/*
 * ui.h — Button layout, top-screen and bottom-screen drawing.
 */
#ifndef UI_H
#define UI_H

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

// ---------------------------------------------------------------------------
// Shared lobby button layouts — the single source of truth for both drawing
// (ui.cpp) and touch hit-testing (main.cpp), so the tap zone can never drift
// out of sync with what's actually on screen.
// ---------------------------------------------------------------------------
constexpr Button BTN_OFFLINE        = {16, 188, BOT_W - 32, 20, "Offline local play", C_BG_DARK, C_TEXT, C_ACCENT};
constexpr Button BTN_PUBLIC_MATCH   = {8, 76, 148, 42, "Public match", C_PRIMARY, C_PRIMARY_TXT, {105, 50, 12}};
constexpr Button BTN_PRIVATE_ROOM   = {164, 76, 148, 42, "Private room", C_PRIMARY, C_PRIMARY_TXT, {105, 50, 12}};
constexpr Button BTN_CREATE_ROOM    = {8, 88, 148, 42, "Create room", C_PRIMARY, C_PRIMARY_TXT, {105, 50, 12}};
constexpr Button BTN_JOIN_ROOM      = {164, 88, 148, 42, "Join room", C_PRIMARY, C_PRIMARY_TXT, {105, 50, 12}};
constexpr Button BTN_LOCAL_PLAY     = {8, 124, 148, 42, "Local play", C_ACCENT, C_BG_DARK, C_PRIMARY};
constexpr Button BTN_SPECTATE       = {164, 124, 148, 42, "Spectate", C_BG_DARK, C_TEXT, C_ACCENT};
constexpr Button BTN_BACK           = {8, 172, 148, 26, "Back", C_BG_DARK, C_TEXT, C_ACCENT};
constexpr Button BTN_CONTINUE       = {164, 172, 148, 26, "Continue", C_PRIMARY, C_PRIMARY_TXT, {105, 50, 12}};
constexpr Button BTN_START_LOCAL    = {164, 172, 148, 26, "Start local", C_PRIMARY, C_PRIMARY_TXT, {105, 50, 12}};
constexpr Button BTN_CANCEL_QUEUE   = {64, 160, 192, 30, "Cancel queue", C_BG_DARK, C_TEXT, C_ACCENT};
constexpr Button BTN_CANCEL_PRIVATE = {64, 160, 192, 30, "Cancel room", C_BG_DARK, C_TEXT, C_ACCENT};

// Settings-row "pseudo buttons" on PUBLIC_SETTINGS / PRIVATE_CREATE / LOCAL —
// drawn as a custom row widget rather than through drawButton, but the
// geometry is still shared so ui.cpp's rendering and main.cpp's hit-testing
// always agree on where the row actually is.
constexpr Button BTN_MATCH_SETTING   = {8, 28, BOT_W - 16, 20, "", C_BG_DARK, C_TEXT, C_ACCENT};
constexpr Button BTN_TIME_SETTING    = {8, 52, BOT_W - 16, 20, "", C_BG_DARK, C_TEXT, C_ACCENT};
constexpr Button BTN_VARIANT_SETTING = {8, 76, BOT_W - 16, 20, "", C_BG_DARK, C_TEXT, C_ACCENT};
constexpr Button BTN_LOCAL_VARIANT   = {8, 44, BOT_W - 16, 20, "", C_BG_DARK, C_TEXT, C_ACCENT};

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
    // Seconds remaining, for display only. main.cpp owns the tick-based
    // baseline and ticks these down locally between server syncs (see
    // timerSync/timerFreeze in main.cpp).
    int timerW;
    int timerB;
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

// Full-screen "are you sure?" modal shown before actually quitting the app.
void drawQuitConfirm(uint8_t *topFb, uint8_t *botFb,
                     const Button &yesBtn, const Button &noBtn,
                     bool pressedYes, bool pressedNo);

#endif // UI_H
