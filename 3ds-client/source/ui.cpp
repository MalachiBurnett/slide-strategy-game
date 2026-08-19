/*
 * ui.cpp — Button and screen-drawing implementations.
 */
#include "ui.h"
#include "render.h"

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
    int r = 12;
    fillRoundRect(fb, BOT_W, BOT_H, btn.x, btn.y, btn.w, btn.h, r, bg);
    if (!pressed)
        drawRoundRect(fb, BOT_W, BOT_H, btn.x, btn.y, btn.w, btn.h, r, 3, btn.borderColor);

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
        fillRect(fb, TOP_W, TOP_H, 0, 24, TOP_W, TOP_H - 24, C_BG);
        drawTextWrapped(fb, TOP_W, TOP_H, 8, 36, TOP_W - 16,
                        "You are now signed in!", 1, C_SUCCESS);
        if (statusMsg && statusMsg[0])
        {
            drawText(fb, TOP_W, TOP_H, 8,         60, "Welcome,", 1, C_TEXT);
            drawText(fb, TOP_W, TOP_H, 8 + 9 * 8, 60, statusMsg, 1, C_PRIMARY);
        }
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
                      bool pressedSignIn, bool pressedGuest, bool pressedSignOut,
                      bool pressedQuit, const Button &btnSignIn,
                      const Button &btnGuest, const Button &btnSignOut,
                      const Button &btnQuit)
{
    clearScreen(fb, BOT_W, BOT_H, C_BG);

    fillRect(fb, BOT_W, BOT_H, 0, 0, BOT_W, 3, C_PRIMARY);

    {
        const char *title  = "Slide Strategy Game";
        int         titleW = (int)strlen(title) * 8 * 2;
        drawText(fb, BOT_W, BOT_H, (BOT_W - titleW) / 2, 8, title, 2, C_PRIMARY);
    }

    fillRect(fb, BOT_W, BOT_H, 8, 30, BOT_W - 16, 1, C_ACCENT);

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
        static const Button publicMatch = {8, 42, 152, 48, "Public match", C_PRIMARY, C_PRIMARY_TXT, {105, 50, 12}};
        static const Button privateRoom = {168, 42, 152, 48, "Private room", C_PRIMARY, C_PRIMARY_TXT, {105, 50, 12}};
        static const Button localPlay = {8, 98, 152, 48, "Local play", C_ACCENT, C_BG_DARK, C_PRIMARY};
        static const Button spectate = {168, 98, 152, 48, "Spectate", C_BG_DARK, C_TEXT, C_ACCENT};
        static const Button back = {8, 174, 152, 34, "Back", C_BG_DARK, C_TEXT, C_ACCENT};
        static const Button continueButton = {168, 174, 152, 34, "Continue", C_PRIMARY, C_PRIMARY_TXT, {105, 50, 12}};

        if (lobbyPage == LobbyPage::HOME)
        {
            drawText(fb, BOT_W, BOT_H, 8, 34, "Choose a mode", 1, C_TEXT);
            drawButton(fb, publicMatch, false);
            drawButton(fb, privateRoom, false);
            drawButton(fb, localPlay, false);
            drawButton(fb, spectate, false);
            drawButton(fb, btnSignOut, pressedSignOut, 1);
        }
        else if (lobbyPage == LobbyPage::PRIVATE_CHOICE)
        {
            drawText(fb, BOT_W, BOT_H, 8, 34, "Private room", 1, C_TEXT);
            drawTextWrapped(fb, BOT_W, BOT_H, 8, 52, BOT_W - 16, "Create a room with settings, or join with a code.", 1, C_TEXT);
            drawButton(fb, publicMatch, false);
            drawButton(fb, privateRoom, false);
            drawButton(fb, back, false);
        }
        else if (lobbyPage == LobbyPage::PRIVATE_JOIN)
        {
            drawText(fb, BOT_W, BOT_H, 8, 34, "Join a room", 1, C_TEXT);
            drawTextWrapped(fb, BOT_W, BOT_H, 8, 52, BOT_W - 16, "Enter the host's join code.", 1, C_TEXT);
            drawButton(fb, continueButton, false);
            drawButton(fb, back, false);
        }
        else
        {
            drawText(fb, BOT_W, BOT_H, 8, 34,
                     lobbyPage == LobbyPage::PUBLIC_SETTINGS ? "Public settings" : "Create room",
                     1, C_TEXT);
            drawText(fb, BOT_W, BOT_H, 8, 54, "Ranked / Casual", 1, C_PRIMARY);
            drawText(fb, BOT_W, BOT_H, 8, 76, "Time: 15s + 3s", 1, C_TEXT);
            drawText(fb, BOT_W, BOT_H, 8, 98, "Variant: Classic", 1, C_TEXT);
            drawTextWrapped(fb, BOT_W, BOT_H, 8, 122, BOT_W - 16, "Settings are ready. Match modes are coming soon.", 1, C_ACCENT);
            drawButton(fb, continueButton, false);
            drawButton(fb, back, false);
        }

        drawButton(fb, btnQuit, pressedQuit, 1);
    }
    else if (state == AppState::KEYBOARD_LOGIN)
    {
        drawText(fb, BOT_W, BOT_H, 8, 38, "Using software keyboard...", 1, C_TEXT);
    }

    {
        const char *footer  = "Made by Wiizard Software";
        int         footerW = (int)strlen(footer) * 8;
        drawText(fb, BOT_W, BOT_H, (BOT_W - footerW) / 2, BOT_H - 12, footer, 1, C_ACCENT);
    }
}
