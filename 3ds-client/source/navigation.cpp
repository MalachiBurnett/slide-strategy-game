/*
 * navigation.cpp — see navigation.h.
 */
#include "navigation.h"

int focusCount(AppState state, LobbyPage page)
{
    if (state == AppState::QR_LOGIN || state == AppState::ERROR_STATE) return 4;
    if (state == AppState::INIT) return 1;
    if (state != AppState::LOGGED_IN) return 0;
    if (page == LobbyPage::QUEUE) return 1;
    if (page == LobbyPage::PRIVATE_WAIT) return 1;
    if (page == LobbyPage::HOME) return 7;
    if (page == LobbyPage::THEMES) return 5;
    if (page == LobbyPage::PRIVATE_CHOICE) return 4;
    if (page == LobbyPage::PRIVATE_JOIN) return 3;
    if (page == LobbyPage::LOCAL_SETTINGS) return 3;
    return 6;
}

int focusPoints(AppState state, LobbyPage page, int outX[FOCUS_MAX], int outY[FOCUS_MAX])
{
    const int (*pts)[2] = nullptr;
    int n = 0;
    if (state == AppState::QR_LOGIN || state == AppState::ERROR_STATE)
    {
        static const int p[][2] = {{BOT_W / 2, 125}, {BOT_W / 2, 165},
                                   {BOT_W / 2, 198}, {BOT_W / 2, 222}};
        pts = p; n = 4;
    }
    else if (state == AppState::INIT)
    {
        static const int p[][2] = {{BOT_W / 2, 222}};
        pts = p; n = 1;
    }
    else if (state == AppState::LOGGED_IN)
    {
        if (page == LobbyPage::HOME)
        {
            static const int p[][2] = {{82, 97}, {238, 97}, {82, 145},
                                       {238, 145}, {82, 200}, {238, 200},
                                       {160, 222}};
            pts = p; n = 7;
        }
        else if (page == LobbyPage::THEMES)
        {
            static const int p[][2] = {{31, 63}, {289, 63}, {238, 185},
                                       {82, 185}, {160, 222}};
            pts = p; n = 5;
        }
        else if (page == LobbyPage::PRIVATE_CHOICE)
        {
            static const int p[][2] = {{82, 109}, {238, 109}, {82, 185}, {160, 222}};
            pts = p; n = 4;
        }
        else if (page == LobbyPage::PRIVATE_JOIN)
        {
            static const int p[][2] = {{238, 185}, {82, 185}, {160, 222}};
            pts = p; n = 3;
        }
        else if (page == LobbyPage::LOCAL_SETTINGS)
        {
            static const int p[][2] = {{160, 54}, {238, 185}, {82, 185}};
            pts = p; n = 3;
        }
        else if (page == LobbyPage::QUEUE)
        {
            static const int p[][2] = {{BOT_W / 2, 175}};
            pts = p; n = 1;
        }
        else if (page == LobbyPage::PRIVATE_WAIT)
        {
            static const int p[][2] = {{BOT_W / 2, 175}};
            pts = p; n = 1;
        }
        else
        {
            static const int p[][2] = {{160, 38}, {160, 62}, {160, 86},
                                       {238, 173}, {82, 173}, {160, 222}};
            pts = p; n = 6;
        }
    }
    for (int i = 0; i < n; ++i) { outX[i] = pts[i][0]; outY[i] = pts[i][1]; }
    return n;
}

// ---------------------------------------------------------------------------
// FocusLinks — each focus index stores its logical neighbours
// (up/down/left/right). -1 = stay put. Internal to focus traversal.
// ---------------------------------------------------------------------------
namespace
{
    struct FocusLinks
    {
        int up, down, left, right;
    };

    const FocusLinks &focusLinksFor(AppState state, LobbyPage page, int focus)
    {
        // QR_LOGIN / ERROR_STATE: vertical stack of 4 full-width buttons.
        static const FocusLinks QR[4] = {{0, 1, 0, 0}, {0, 2, 1, 1}, {1, 3, 2, 2}, {2, 3, 3, 3}};
        static const FocusLinks INIT_ONE[1] = {{0, 0, 0, 0}};
        // HOME: 2x2 mode grid, a themes / sign-out pair, then quit.
        static const FocusLinks HOME[7] = {
            {0, 2, 0, 1}, {1, 3, 0, 1}, {0, 4, 2, 3}, {1, 5, 2, 3},
            {2, 6, 4, 5}, {3, 6, 4, 5}, {4, 6, 6, 6}
        };
        // THEMES: prev / next side by side, apply / back below, then quit.
        static const FocusLinks PICKER[5] = {
            {0, 3, 0, 1}, {1, 2, 0, 1}, {1, 4, 3, 2}, {0, 4, 3, 2}, {3, 4, 4, 4}
        };
        // PRIVATE_CHOICE: 2x2 grid + quit.
        static const FocusLinks PCHOICE[4] = {
            {0, 2, 0, 1}, {1, 3, 0, 1}, {0, 3, 2, 3}, {1, 2, 2, 3}
        };
        // PRIVATE_JOIN: continue / back side-by-side + quit below.
        static const FocusLinks PJOIN[3] = {
            {0, 2, 1, 0}, {1, 2, 1, 0}, {0, 2, 1, 1}
        };
        // LOCAL_SETTINGS: variant row on top, start/back below.
        static const FocusLinks LOCALSET[3] = {
            {0, 1, 0, 0}, {0, 1, 2, 1}, {0, 2, 2, 1}
        };
        static const FocusLinks QUEUE_ONE[1] = {{0, 0, 0, 0}};
        // PUBLIC_SETTINGS / PRIVATE_CREATE: 3 setting rows, continue/back, quit.
        static const FocusLinks SETTINGS[6] = {
            {0, 1, 0, 0}, {0, 2, 1, 1}, {1, 3, 2, 2}, {2, 5, 4, 3},
            {2, 5, 4, 3}, {3, 5, 4, 3}
        };

        if (state == AppState::QR_LOGIN || state == AppState::ERROR_STATE)
            return QR[focus < 0 ? 0 : (focus > 3 ? 3 : focus)];
        if (state == AppState::INIT) return INIT_ONE[0];
        if (state != AppState::LOGGED_IN) return INIT_ONE[0];
        if (page == LobbyPage::QUEUE) return QUEUE_ONE[0];
        if (page == LobbyPage::PRIVATE_WAIT) return QUEUE_ONE[0];
        if (page == LobbyPage::HOME) return HOME[focus < 0 ? 0 : (focus > 6 ? 6 : focus)];
        if (page == LobbyPage::THEMES) return PICKER[focus < 0 ? 0 : (focus > 4 ? 4 : focus)];
        if (page == LobbyPage::PRIVATE_CHOICE) return PCHOICE[focus < 0 ? 0 : (focus > 3 ? 3 : focus)];
        if (page == LobbyPage::PRIVATE_JOIN) return PJOIN[focus < 0 ? 0 : (focus > 2 ? 2 : focus)];
        if (page == LobbyPage::LOCAL_SETTINGS) return LOCALSET[focus < 0 ? 0 : (focus > 2 ? 2 : focus)];
        return SETTINGS[focus < 0 ? 0 : (focus > 5 ? 5 : focus)];
    }
}

int focusMove(AppState state, LobbyPage page, int focus, int dx, int dy)
{
    const FocusLinks &links = focusLinksFor(state, page, focus);
    if (dx < 0) return links.left;
    if (dx > 0) return links.right;
    if (dy < 0) return links.up;
    if (dy > 0) return links.down;
    return focus;
}

bool focusPoint(AppState state, LobbyPage page, int focus, int &x, int &y)
{
    int xs[FOCUS_MAX], ys[FOCUS_MAX];
    int n = focusPoints(state, page, xs, ys);
    if (focus < 0 || focus >= n) return false;
    x = xs[focus];
    y = ys[focus];
    return true;
}

void goBack(AppState state, LobbyPage &page, char *statusMsg)
{
    if (state == AppState::LOGGED_IN)
    {
        if (page == LobbyPage::PRIVATE_CREATE || page == LobbyPage::PRIVATE_JOIN ||
            page == LobbyPage::PRIVATE_WAIT)
            page = LobbyPage::PRIVATE_CHOICE;
        else
            page = LobbyPage::HOME;
        statusMsg[0] = 0;
    }
}
