/*
 * ui.h — application state enums, the shared button layout table, and the
 * match-view state that the scene builders in screens.cpp read from.
 *
 * Drawing lives in screens.cpp (which turns all of this into UiScene objects)
 * and uikit.cpp (which draws and animates them). What stays here is the
 * geometry, because it is the single source of truth for both the rendered
 * position of a control and the tap zone main.cpp hit-tests against — those
 * two can never be allowed to drift apart.
 */
#ifndef UI_H
#define UI_H

#include "render.h"

// Centred game-board layout (bottom screen, 320x240; board is 228x228).
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
    THEMES,
};

// ---------------------------------------------------------------------------
// Button
// ---------------------------------------------------------------------------
// Colours are palette *roles*, not literal Colors: this table is built at
// compile time but has to follow the player's theme, so screens.cpp resolves
// each role through roleColor() at the moment the button is drawn.
struct Button
{
    int         x, y, w, h;
    const char *label;
    Role        bgColor;
    Role        textColor;
    Role        borderColor;
};

// ---------------------------------------------------------------------------
// Shared button layouts — the single source of truth for both scene building
// (screens.cpp) and touch hit-testing (main.cpp).
// ---------------------------------------------------------------------------
constexpr Button BTN_SIGNIN         = {16, 108, BOT_W - 32, 34, "Sign in on this device", Role::Primary, Role::PrimaryTxt, Role::PrimaryDk};
constexpr Button BTN_GUEST          = {16, 148, BOT_W - 32, 34, "Play as guest", Role::BgLight, Role::Primary, Role::Accent};
constexpr Button BTN_OFFLINE        = {16, 188, BOT_W - 32, 20, "Offline local play", Role::BgLight, Role::Text, Role::Accent};
constexpr Button BTN_QUIT           = {16, 212, BOT_W - 32, 20, "Quit", Role::BgDark, Role::Text, Role::AccentDk};
constexpr Button BTN_THEMES         = {8, 188, 148, 24, "Themes", Role::Accent, Role::AccentTxt, Role::AccentDk};
constexpr Button BTN_SIGNOUT        = {164, 188, 148, 24, "Sign out", Role::BgLight, Role::Error, Role::Accent};
constexpr Button BTN_PUBLIC_MATCH   = {8, 76, 148, 42, "Public match", Role::Primary, Role::PrimaryTxt, Role::PrimaryDk};
constexpr Button BTN_PRIVATE_ROOM   = {164, 76, 148, 42, "Private room", Role::BgLight, Role::Primary, Role::Accent};
constexpr Button BTN_CREATE_ROOM    = {8, 88, 148, 42, "Create room", Role::Primary, Role::PrimaryTxt, Role::PrimaryDk};
constexpr Button BTN_JOIN_ROOM      = {164, 88, 148, 42, "Join room", Role::BgLight, Role::Primary, Role::Accent};
constexpr Button BTN_LOCAL_PLAY     = {8, 124, 148, 42, "Local play", Role::Accent, Role::AccentTxt, Role::AccentDk};
constexpr Button BTN_TUTORIAL       = {164, 124, 148, 42, "Tutorial", Role::Purple, Role::FixedTxt, Role::PurpleDk};
constexpr Button BTN_BACK           = {8, 172, 148, 26, "Back", Role::BgLight, Role::Text, Role::Accent};
constexpr Button BTN_CONTINUE       = {164, 172, 148, 26, "Continue", Role::Primary, Role::PrimaryTxt, Role::PrimaryDk};
constexpr Button BTN_START_LOCAL    = {164, 172, 148, 26, "Start local", Role::Primary, Role::PrimaryTxt, Role::PrimaryDk};
constexpr Button BTN_CANCEL_QUEUE   = {64, 160, 192, 30, "Cancel queue", Role::BgLight, Role::Error, Role::Accent};
constexpr Button BTN_CANCEL_PRIVATE = {64, 160, 192, 30, "Close room", Role::BgLight, Role::Error, Role::Accent};

// Theme picker. Prev/next step through the table in theme.cpp and recolour
// the client live, so the two screens behind this page *are* the preview;
// Apply is what actually commits the choice.
constexpr Button BTN_THEME_PREV  = {8, 40, 46, 46, "<", Role::BgLight, Role::Primary, Role::Accent};
constexpr Button BTN_THEME_NEXT  = {266, 40, 46, 46, ">", Role::BgLight, Role::Primary, Role::Accent};
constexpr Button BTN_THEME_APPLY = {164, 172, 148, 26, "Apply theme", Role::Primary, Role::PrimaryTxt, Role::PrimaryDk};

// Full-screen "are you sure?" quit modal.
constexpr Button BTN_QUIT_YES = {40, 140, 110, 44, "Quit", Role::Error, Role::FixedTxt, Role::ErrorDk};
constexpr Button BTN_QUIT_NO  = {170, 140, 110, 44, "Stay", Role::Primary, Role::PrimaryTxt, Role::PrimaryDk};

// Settings rows on PUBLIC_SETTINGS / PRIVATE_CREATE / LOCAL_SETTINGS — drawn
// as label+value rows rather than through the button path, but the geometry
// is shared all the same so rendering and hit-testing always agree.
constexpr Button BTN_MATCH_SETTING   = {8, 28, BOT_W - 16, 20, "", Role::BgLight, Role::Text, Role::Accent};
constexpr Button BTN_TIME_SETTING    = {8, 52, BOT_W - 16, 20, "", Role::BgLight, Role::Text, Role::Accent};
constexpr Button BTN_VARIANT_SETTING = {8, 76, BOT_W - 16, 20, "", Role::BgLight, Role::Text, Role::Accent};
constexpr Button BTN_LOCAL_VARIANT   = {8, 44, BOT_W - 16, 20, "", Role::BgLight, Role::Text, Role::Accent};

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

bool buttonHit(const Button &btn, int tx, int ty);

#endif // UI_H
