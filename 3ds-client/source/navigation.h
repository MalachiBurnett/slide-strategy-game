/*
 * navigation.h — D-pad / circle-pad focus traversal over the current screen's
 * buttons, and the "B button" page-back logic for the lobby.
 *
 * Focus navigation is linked-list style: each (state, page, focus index)
 * combination has a fixed set of on-screen touch points and a set of logical
 * neighbours (up/down/left/right) to jump between them, mirroring the
 * on-screen layout.
 */
#ifndef NAVIGATION_H
#define NAVIGATION_H

#include "ui.h"

// Maximum focusable controls on any one page (the lobby home screen).
constexpr int FOCUS_MAX = 8;

// Number of focusable buttons for the given state/page.
int focusCount(AppState state, LobbyPage page);

// Fills outX[FOCUS_MAX]/outY[FOCUS_MAX] with the touch point of each button for
// the given state/page. Returns the number of points written.
int focusPoints(AppState state, LobbyPage page, int outX[FOCUS_MAX], int outY[FOCUS_MAX]);

// Moves `focus` one step in direction (dx, dy) (exactly one of which should
// be nonzero) according to the current state/page's layout.
int focusMove(AppState state, LobbyPage page, int focus, int dx, int dy);

// Looks up the touch point (x, y) for the current focus index.
// Returns false if focus is out of range for the current state/page.
bool focusPoint(AppState state, LobbyPage page, int focus, int &x, int &y);

// Handles the "B button" back action for the lobby: pops PRIVATE_CREATE /
// PRIVATE_JOIN / PRIVATE_WAIT back to PRIVATE_CHOICE, otherwise back to HOME.
// Clears statusMsg on any transition. No-op outside AppState::LOGGED_IN.
void goBack(AppState state, LobbyPage &page, char *statusMsg);

#endif // NAVIGATION_H
