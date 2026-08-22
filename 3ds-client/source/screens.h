/*
 * screens.h — turns application state into UiScene objects.
 *
 * Nothing here draws directly. Each builder describes a page as a list of
 * independent objects (see uikit.h); uikit then draws them, and slides them
 * on and off whenever the page key changes.
 */
#ifndef SCREENS_H
#define SCREENS_H

#include "ui.h"
#include "uikit.h"

// Everything the builders are allowed to read. Assembled fresh by main.cpp on
// every frame, so a scene is always a pure function of current state.
struct UiContext
{
    AppState    state;
    LobbyPage   page;
    bool        gameActive;
    bool        tutorialActive;
    int         tutorialStep;
    bool        confirmingQuit;
    const char *username;
    const char *elo;
    bool        isRated;
    int         timeControl;
    int         variant;
    int         focusIndex;
    bool        focusVisible;
    const uint8_t *qrData;
    bool        qrReady;
    const char *statusMsg;
    const char *privateCode;
    const char *joinCode;
    const GameUiState *game;
    int         touchX, touchY;
    bool        touchActive;
};

// Registers the painter for the app-specific widgets (board, mini preview,
// piece tokens) with uikit. Call once at start-up.
void screensInit();

// Page identity. A change here is what fires the slide transition, so these
// deliberately ignore live content — a status line or a ticking clock updates
// in place rather than throwing the whole page off screen.
int32_t topSceneKey   (const UiContext &c);
int32_t bottomSceneKey(const UiContext &c);

void buildTopScene   (UiScene &s, const UiContext &c);
void buildBottomScene(UiScene &s, const UiContext &c);

#endif // SCREENS_H
