/*
 * ui.cpp — Button and screen-drawing implementations.
 */
#include "ui.h"
#include "render.h"
#include <cstdio>

extern "C" {
#include "qrcodegen.h"
}

// ---------------------------------------------------------------------------
// Button
// ---------------------------------------------------------------------------
bool buttonHit(const Button &btn, int tx, int ty)
{
    return tx >= btn.x && tx < btn.x + btn.w &&
           ty >= btn.y && ty < btn.y + btn.h;
}

void drawButton(uint8_t *fb, const Button &btn, bool pressed, int textScale)
{
    Color bg   = pressed ? btn.borderColor : btn.bgColor;
    Color text = pressed ? btn.bgColor     : btn.textColor;
    int r = btn.h >= 30 ? 10 : 7;
    fillRoundRect(fb, BOT_W, BOT_H, btn.x, btn.y, btn.w, btn.h, r, bg);
    if (!pressed)
        drawRoundRect(fb, BOT_W, BOT_H, btn.x, btn.y, btn.w, btn.h, r, 2, btn.borderColor);

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
                   const uint8_t *qrData, bool qrReady,
                   const char *statusMsg)
{
    clearScreen(fb, TOP_W, TOP_H, C_BG);

    // Title bar
    fillRect(fb, TOP_W, TOP_H, 0, 0, TOP_W, 24, C_PRIMARY);
    drawText(fb, TOP_W, TOP_H, 8, 8, "Slide", 1, C_PRIMARY_TXT);

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
        for (int c = 0; c < 6; ++c)
        {
            fillRoundRect(fb, TOP_W, TOP_H, 253 + c * 19, 72, 11, 11, 5, C_BG_LIGHT);
            fillRoundRect(fb, TOP_W, TOP_H, 253 + c * 19, 163, 11, 11, 5, C_TEXT);
        }
        drawText(fb, TOP_W, TOP_H, 290, 202, "ELO", 1, C_ACCENT);
        drawText(fb, TOP_W, TOP_H, 314, 202, safeElo, 1, C_PRIMARY);
        if (statusMsg && statusMsg[0])
            drawTextWrapped(fb, TOP_W, TOP_H, 12, 204, 210, statusMsg, 1, C_PRIMARY);
    }
    else if (state == AppState::ERROR_STATE)
    {
        drawText(fb, TOP_W, TOP_H, 8, 36, "Error:", 1, C_ERROR);
        if (statusMsg && statusMsg[0])
            drawTextWrapped(fb, TOP_W, TOP_H, 8, 50, TOP_W - 16, statusMsg, 1, C_ERROR);
    }
}

// ---------------------------------------------------------------------------
// Bottom screen
// ---------------------------------------------------------------------------
void drawBottomScreen(uint8_t *fb, AppState state,
                      LobbyPage lobbyPage,
                      const char *username, const char *elo,
                      bool isRated, int timeControl, int variant,
                      bool pressedSignIn, bool pressedGuest, bool pressedSignOut,
                      bool pressedQuit, const Button &btnSignIn,
                      const Button &btnGuest, const Button &btnSignOut,
                      const Button &btnQuit)
{
    clearScreen(fb, BOT_W, BOT_H, C_BG);

    fillRect(fb, BOT_W, BOT_H, 0, 0, BOT_W, 4, C_PRIMARY);

    {
        drawText(fb, BOT_W, BOT_H, 12, 8, "SLIDE", 2, C_PRIMARY);
        drawText(fb, BOT_W, BOT_H, 244, 10, "3DS", 1, C_ACCENT);
    }

    fillRect(fb, BOT_W, BOT_H, 8, 30, BOT_W - 16, 2, C_ACCENT);

    if (state == AppState::QR_LOGIN || state == AppState::ERROR_STATE)
    {
        const char *heading = (state == AppState::ERROR_STATE)
                              ? "Connection failed. Try again?"
                              : "Or sign in on this device:";
        drawTextWrapped(fb, BOT_W, BOT_H, 8, 38, BOT_W - 16, heading, 1, C_TEXT);

        drawButton(fb, btnSignIn, pressedSignIn, 1);
        drawButton(fb, btnGuest,  pressedGuest, 1);
        drawButton(fb, btnQuit,   pressedQuit,   1);
    }
    else if (state == AppState::INIT)
    {
        drawText(fb, BOT_W, BOT_H, 8, 38, "Please wait...", 1, C_TEXT);
        drawButton(fb, btnQuit, pressedQuit, 2);
    }
    else if (state == AppState::LOGGED_IN)
    {
        static const Button publicMatch = {8, 76, 148, 48, "Public match", C_PRIMARY, C_PRIMARY_TXT, {105, 50, 12}};
        static const Button privateRoom = {164, 76, 148, 48, "Private room", C_PRIMARY, C_PRIMARY_TXT, {105, 50, 12}};
        static const Button createRoom = {8, 76, 148, 48, "Create room", C_PRIMARY, C_PRIMARY_TXT, {105, 50, 12}};
        static const Button joinRoom = {164, 76, 148, 48, "Join room", C_PRIMARY, C_PRIMARY_TXT, {105, 50, 12}};
        static const Button localPlay = {8, 130, 148, 48, "Local play", C_ACCENT, C_BG_DARK, C_PRIMARY};
        static const Button spectate = {164, 130, 148, 48, "Spectate", C_BG_DARK, C_TEXT, C_ACCENT};
        static const Button back = {8, 174, 148, 30, "Back", C_BG_DARK, C_TEXT, C_ACCENT};
        static const Button continueButton = {164, 174, 148, 30, "Continue", C_PRIMARY, C_PRIMARY_TXT, {105, 50, 12}};

        if (lobbyPage == LobbyPage::HOME)
        {
            drawText(fb, BOT_W, BOT_H, 8, 38, "WELCOME BACK", 1, C_ACCENT);
            drawText(fb, BOT_W, BOT_H, 8, 50, username, 2, C_TEXT);
            char rating[32];
            snprintf(rating, sizeof(rating), "ELO %s", elo);
            drawText(fb, BOT_W, BOT_H, 236, 54, rating, 1, C_PRIMARY);
            drawText(fb, BOT_W, BOT_H, 8, 66, "CHOOSE A MODE", 1, C_TEXT);
            drawButton(fb, publicMatch, false);
            drawButton(fb, privateRoom, false);
            drawButton(fb, localPlay, false);
            drawButton(fb, spectate, false);
            drawButton(fb, btnSignOut, pressedSignOut, 1);
        }
        else if (lobbyPage == LobbyPage::PRIVATE_CHOICE)
        {
            drawText(fb, BOT_W, BOT_H, 8, 38, "PRIVATE ROOM", 1, C_ACCENT);
            drawTextWrapped(fb, BOT_W, BOT_H, 8, 52, BOT_W - 16, "Create a room with settings, or join with a code.", 1, C_TEXT);
            drawButton(fb, createRoom, false);
            drawButton(fb, joinRoom, false);
            drawButton(fb, back, false);
        }
        else if (lobbyPage == LobbyPage::PRIVATE_JOIN)
        {
            drawText(fb, BOT_W, BOT_H, 8, 38, "JOIN A ROOM", 1, C_ACCENT);
            drawTextWrapped(fb, BOT_W, BOT_H, 8, 52, BOT_W - 16, "Enter the host's join code.", 1, C_TEXT);
            drawButton(fb, continueButton, false);
            drawButton(fb, back, false);
        }
        else
        {
            static const char *timeLabels[] = {"15s + 3s", "1 minute", "3m + 2s"};
            static const char *variantLabels[] = {"Classic", "Fog of War", "Random Setup", "Schizophrenic"};
            const int timeIndex = timeControl < 0 || timeControl > 2 ? 0 : timeControl;
            const int variantIndex = variant < 0 || variant > 3 ? 0 : variant;
            fillRoundRect(fb, BOT_W, BOT_H, 8, 52, BOT_W - 16, 20, 5, C_BG_DARK);
            fillRoundRect(fb, BOT_W, BOT_H, 8, 76, BOT_W - 16, 20, 5, C_BG_DARK);
            fillRoundRect(fb, BOT_W, BOT_H, 8, 100, BOT_W - 16, 20, 5, C_BG_DARK);
            drawRoundRect(fb, BOT_W, BOT_H, 8, 52, BOT_W - 16, 20, 5, 1, C_ACCENT);
            drawRoundRect(fb, BOT_W, BOT_H, 8, 76, BOT_W - 16, 20, 5, 1, C_ACCENT);
            drawRoundRect(fb, BOT_W, BOT_H, 8, 100, BOT_W - 16, 20, 5, 1, C_ACCENT);
            drawText(fb, BOT_W, BOT_H, 8, 38,
                     lobbyPage == LobbyPage::PUBLIC_SETTINGS ? "Public settings" : "Create room",
                     1, C_TEXT);
            drawText(fb, BOT_W, BOT_H, 8, 58, "MATCH TYPE", 1, C_PRIMARY);
            drawText(fb, BOT_W, BOT_H, 132, 58, isRated ? "RANKED" : "CASUAL", 1, C_TEXT);
            drawText(fb, BOT_W, BOT_H, 8, 80, "TIME CONTROL", 1, C_PRIMARY);
            drawText(fb, BOT_W, BOT_H, 132, 80, timeLabels[timeIndex], 1, C_TEXT);
            drawText(fb, BOT_W, BOT_H, 8, 102, "VARIANT", 1, C_PRIMARY);
            drawText(fb, BOT_W, BOT_H, 132, 102, variantLabels[variantIndex], 1, C_TEXT);
            drawTextWrapped(fb, BOT_W, BOT_H, 8, 126, BOT_W - 16, "Settings are ready. Press continue when you are ready.", 1, C_ACCENT);
            drawButton(fb, continueButton, false);
            drawButton(fb, back, false);
        }

        drawButton(fb, btnQuit, pressedQuit, 1);
    }
    else if (state == AppState::KEYBOARD_LOGIN)
    {
        drawText(fb, BOT_W, BOT_H, 8, 38, "Using software keyboard...", 1, C_TEXT);
    }

    fillRect(fb, BOT_W, BOT_H, 8, 208, BOT_W - 16, 1, C_ACCENT);
    {
        const char *footer  = "Made by Wiizard Software";
        int         footerW = (int)strlen(footer) * 8;
        drawText(fb, BOT_W, BOT_H, (BOT_W - footerW) / 2, 232, footer, 1, C_ACCENT);
    }
}
