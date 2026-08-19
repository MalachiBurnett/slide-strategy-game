/*
 * ui.h — Button layout, top-screen and bottom-screen drawing.
 */
#pragma once

#include "render.h"

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

bool buttonHit (const Button &btn, int tx, int ty);
void drawButton(uint8_t *fb, const Button &btn, bool pressed, int textScale = 1);

// ---------------------------------------------------------------------------
// Screen drawing
// ---------------------------------------------------------------------------
void drawTopScreen   (uint8_t *fb, AppState state,
                      const uint8_t *qrData, bool qrReady,
                      const char *statusMsg);

void drawBottomScreen(uint8_t *fb, AppState state,
                      LobbyPage lobbyPage,
                      bool pressedSignIn, bool pressedGuest, bool pressedSignOut,
                      bool pressedQuit, const Button &btnSignIn,
                      const Button &btnGuest, const Button &btnSignOut,
                      const Button &btnQuit);
