/*
 * screens.cpp — every page of the client, expressed as a list of objects.
 * See screens.h for the contract and uikit.h for how these get animated.
 */
#include "screens.h"

#include <cstdio>

// ---------------------------------------------------------------------------
// App-specific widgets, painted through uikit's Custom hook
// ---------------------------------------------------------------------------
enum : int16_t
{
    CW_BOARD   = 1,   // the 6x6 match board (ptr -> GameUiState)
    CW_PREVIEW = 2,   // decorative mini board on the lobby's top screen
    CW_CHIP_W  = 3,   // a White piece token
    CW_CHIP_B  = 4,   // a Black piece token
};

static const Color C_MOVE     = { 64, 190,  96};
static const Color C_SELECTED = { 45, 125, 220};

static const char PREVIEW_BOARD[6][6] = {
    {'B', '0', 'W', 'B', '0', 'W'},
    {'0', '0', '0', '0', '0', '0'},
    {'W', '0', '0', '0', '0', 'B'},
    {'B', '0', '0', '0', '0', 'W'},
    {'0', '0', '0', '0', '0', '0'},
    {'W', '0', 'B', 'W', '0', 'B'}
};

// A piece reads as a raised token: drop shadow, dark rim, then the face.
static void drawPiece(uint8_t *fb, int w, int h, int cx, int cy, char piece, int radius)
{
    if (radius < 2) radius = 2;
    fillCircle(fb, w, h, cx + 1, cy + 2, radius + 1, darken(C_BOARD_DARK, 0.4f));
    fillCircle(fb, w, h, cx, cy, radius + 1, C_TEXT);
    fillCircle(fb, w, h, cx, cy, radius - 1, piece == 'W' ? C_BG_LIGHT : C_TEXT);
    if (piece == 'W' && radius >= 6)
        fillCircle(fb, w, h, cx - radius / 3, cy - radius / 3, radius / 4, C_BG_DARK);
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

// The board element's rect includes its border frame, so the playfield starts
// one frame-width in. Everything is derived from the passed-in origin rather
// than the layout constants, which is what lets the whole board slide.
static void drawBoardWidget(uint8_t *fb, int w, int h, const GameUiState &game,
                            int x, int y)
{
    constexpr int FRAME = 6;
    fillRoundRect(fb, w, h, x, y, BOARD_PX + FRAME * 2, BOARD_PX + FRAME * 2, 10, C_BOARD_BORDER);
    const int ox = x + FRAME;
    const int oy = y + FRAME;
    const int tile = BOARD_TILE;

    for (int r = 0; r < 6; ++r)
    {
        for (int c = 0; c < 6; ++c)
        {
            const int px = ox + c * tile;
            const int py = oy + r * tile;
            const bool selected = game.pieceSelected && r == game.selectedRow && c == game.selectedCol;
            const bool target   = game.pieceSelected && r == game.targetRow && c == game.targetCol;
            const bool cursor   = r == game.cursorRow && c == game.cursorCol;
            const bool movable  = !game.pieceSelected && gameHasMove(game, r, c);
            const bool flashing = game.flashTimer > 0;

            fillRect(fb, w, h, px, py, tile - 1, tile - 1,
                     (r + c) % 2 == 0 ? C_BOARD_LIGHT : C_BOARD_DARK);
            if (movable || selected || target)
            {
                Color ring = C_MOVE;
                if (selected || target) ring = flashing ? C_ERROR : C_SELECTED;
                drawRoundRect(fb, w, h, px + 2, py + 2, tile - 5, tile - 5, 4, 3, ring);
            }
            if (cursor)
                drawRoundRect(fb, w, h, px + 1, py + 1, tile - 3, tile - 3, 4, 2, C_PRIMARY);
            if (game.board[r][c] != '0')
                drawPiece(fb, w, h, px + tile / 2, py + tile / 2, game.board[r][c], tile / 3);
        }
    }
}

static void drawPreviewWidget(uint8_t *fb, int w, int h, int x, int y, int side)
{
    const int tile = side / 6;
    fillRoundRect(fb, w, h, x, y, side, side, 6, C_BG_DARK);
    for (int r = 0; r < 6; ++r)
        for (int c = 0; c < 6; ++c)
            fillRect(fb, w, h, x + c * tile, y + r * tile, tile - 1, tile - 1,
                     (r + c) % 2 == 0 ? C_BOARD_LIGHT : C_BOARD_DARK);
    for (int r = 0; r < 6; ++r)
        for (int c = 0; c < 6; ++c)
            if (PREVIEW_BOARD[r][c] != '0')
                drawPiece(fb, w, h, x + c * tile + tile / 2, y + r * tile + tile / 2,
                          PREVIEW_BOARD[r][c], tile / 3);
}

static void customDraw(uint8_t *fb, int w, int h, const UiElem &e, int x, int y)
{
    switch (e.data)
    {
    case CW_BOARD:
        if (e.ptr) drawBoardWidget(fb, w, h, *(const GameUiState *)e.ptr, x, y);
        break;
    case CW_PREVIEW:
        drawPreviewWidget(fb, w, h, x, y, e.w);
        break;
    case CW_CHIP_W:
    case CW_CHIP_B:
        drawPiece(fb, w, h, x + e.w / 2, y + e.h / 2,
                  e.data == CW_CHIP_W ? 'W' : 'B', e.w / 2 - 1);
        break;
    }
}

void screensInit()
{
    uiSetCustomDraw(customDraw);
}

// ---------------------------------------------------------------------------
// Shared labels and small layout helpers
// ---------------------------------------------------------------------------
static const char *timeLabel(int i)
{
    static const char *labels[] = {"15s + 3s", "1 minute", "3m + 2s"};
    return labels[(i < 0 || i > 2) ? 0 : i];
}

static const char *variantLabel(int i)
{
    static const char *labels[] = {"Classic", "Fog of War", "Random Setup", "Schizophrenic"};
    return labels[(i < 0 || i > 3) ? 0 : i];
}

static UiElem &addButton(UiScene &s, const Button &b, const UiContext &c,
                         int focusSlot, Glyph glyph)
{
    const bool pressed = c.touchActive && buttonHit(b, c.touchX, c.touchY);
    const bool focused = c.focusVisible && focusSlot >= 0 && c.focusIndex == focusSlot;
    return uiButton(s, b.x, b.y, b.w, b.h, b.label,
                    b.bgColor, b.textColor, b.borderColor, pressed, focused, glyph);
}

// The website's raised card: light face, subtle outline, coloured strip along
// the bottom edge (border-b-8).
static UiElem &addCard(UiScene &s, int x, int y, int w, int h, Color accent)
{
    UiElem &e = uiPanel(s, x, y, w, h, 10, C_BG_LIGHT, accent, 4);
    e.flags |= EF_BORDER;
    e.fg     = C_ACCENT;
    return e;
}

// Icon tile + title + subtitle: the masthead every page opens with.
static void addPageHeader(UiScene &s, int x, int y, Glyph glyph, Color tint,
                          const char *title, const char *subtitle)
{
    uiGroupBegin(s);
    uiIcon(s, x, y, 30, glyph, tint, C_PRIMARY_TXT);
    uiText(s, x + 38, y + 2, title, 2, C_TEXT, EF_BOLD);
    if (subtitle && subtitle[0])
        uiText(s, x + 38, y + 22, subtitle, 1, C_TEXT_SOFT);
    uiGroupEnd(s);
}

// Compact variant, for pages whose controls start high up the screen.
static void addTightHeader(UiScene &s, int x, int y, Glyph glyph, Color tint,
                           const char *title, const char *subtitle)
{
    uiGroupBegin(s);
    uiIcon(s, x, y, 20, glyph, tint, C_PRIMARY_TXT);
    uiText(s, x + 26, y, title, 1, C_PRIMARY, EF_BOLD);
    if (subtitle && subtitle[0])
        uiText(s, x + 26, y + 11, subtitle, 1, C_TEXT_SOFT);
    uiGroupEnd(s);
}

// A tappable "label ......... value" row. Geometry comes straight from the
// Button table so the row lines up with what main.cpp hit-tests.
static void addSettingRow(UiScene &s, const Button &row, const char *label,
                          const char *value, bool focused, bool dim)
{
    uiGroupBegin(s);
    UiElem &p = uiPanel(s, row.x, row.y, row.w, row.h, 6, C_BG_LIGHT,
                        dim ? C_BG_DARK : C_ACCENT, 2);
    p.flags |= EF_BORDER;
    p.fg     = focused ? C_SUCCESS : C_ACCENT;
    p.data   = focused ? 2 : 1;
    const int ty = row.y + (row.h - 2 - FONT_H) / 2;
    uiText(s, row.x + 10, ty, label, 1, dim ? C_TEXT_SOFT : C_PRIMARY, EF_BOLD);
    uiTextIn(s, row.x + 10, ty, row.w - 20, value, 1,
             dim ? C_TEXT_SOFT : C_TEXT, EF_RIGHT);
    uiGroupEnd(s);
}

// Status line. Skipped entirely when empty so it never reserves dead space,
// and clipped to `maxLines` — server errors run long, and an unbounded
// paragraph would grow down into the buttons underneath it.
static void addStatus(UiScene &s, int x, int y, int w, const char *msg,
                      Color fg, int maxLines)
{
    if (!msg || !msg[0]) return;
    const int maxH = (maxLines - 1) * LINE_H + FONT_H;
    if (textWrapHeight(msg, 1, w) <= maxH)
    {
        uiWrap(s, x, y, w, msg, 1, fg);
        return;
    }

    char buf[208];
    snprintf(buf, sizeof(buf), "%s", msg);
    int n = (int)strlen(buf);
    while (n > 4)
    {
        // Drop back to the previous word boundary, then re-measure with the
        // ellipsis included so the trimmed line really does fit.
        --n;
        while (n > 4 && buf[n] != ' ') --n;
        buf[n] = 0;
        char probe[212];
        snprintf(probe, sizeof(probe), "%s...", buf);
        if (textWrapHeight(probe, 1, w) <= maxH)
        {
            uiWrap(s, x, y, w, uiStr(s, "%s", probe), 1, fg);
            return;
        }
    }
}

// Long names would otherwise run under the widget beside them.
static const char *clampName(UiScene &s, const char *name, int maxChars)
{
    if (!name || !name[0]) return "Player";
    if ((int)strlen(name) <= maxChars) return name;
    return uiStr(s, "%.*s..", maxChars - 2, name);
}

// ---------------------------------------------------------------------------
// Scene keys
// ---------------------------------------------------------------------------
int32_t topSceneKey(const UiContext &c)
{
    if (c.confirmingQuit) return 900;
    // In a match only the outcome changes the page: the select/move/send
    // steps rewrite the panel in place, because throwing the whole screen
    // off and back for every tap would make play feel sluggish.
    if (c.gameActive) return 1000 + ((c.game && c.game->gameOver) ? 1 : 0);
    return (int32_t)c.state * 100 + (int32_t)c.page;
}

int32_t bottomSceneKey(const UiContext &c)
{
    if (c.confirmingQuit) return 900;
    if (c.gameActive) return 2000;
    return (int32_t)c.state * 100 + (int32_t)c.page;
}

// ---------------------------------------------------------------------------
// Top screen
// ---------------------------------------------------------------------------
static void topTitleBar(UiScene &s, const char *right)
{
    uiGroupBegin(s);
    UiElem &bar = uiPanel(s, 0, 0, TOP_W, 26, 0, C_PRIMARY, C_PRIMARY_DK, 3);
    (void)bar;
    uiText(s, 10, 7, "SLIDE", 1, C_PRIMARY_TXT, EF_BOLD);
    if (right && right[0])
        uiTextIn(s, TOP_W - 220, 7, 208, right, 1, C_ACCENT, EF_RIGHT);
    uiGroupEnd(s);
}

static void buildTopQuitConfirm(UiScene &s)
{
    topTitleBar(s, "CONFIRM");
    uiGroupBegin(s);
    UiElem &card = addCard(s, 40, 60, TOP_W - 80, 116, C_ERROR);
    card.accent = 6;
    uiIcon(s, 60, 82, 34, Glyph::Warn, C_ERROR, C_PRIMARY_TXT);
    uiText(s, 106, 84, "QUIT SLIDE?", 2, C_ERROR, EF_BOLD);
    uiWrap(s, 106, 110, 216,
           "Any match in progress will be left unresolved and may count as a loss.",
           1, C_TEXT);
    uiGroupEnd(s);
    uiTextIn(s, 40, 196, TOP_W - 80, "Answer on the bottom screen", 1, C_TEXT_SOFT, EF_CENTER);
}

static void buildTopQr(UiScene &s, const UiContext &c)
{
    // No title bar on this page: the QR wants the full height of the screen
    // to stay comfortably scannable, so the branding moves into a left column
    // beside it.
    const int pixel = 5;
    const int qrPx  = c.qrReady ? qrPixelSize(c.qrData, pixel) : 0;
    const int qrY   = qrPx ? (TOP_H - qrPx) / 2 : 0;
    const int qrX   = qrPx ? TOP_W - qrPx - qrY : 0;
    const int colW  = (qrPx ? qrX : TOP_W) - 24;

    uiGroupBegin(s);
    uiIcon(s, 12, 14, 30, Glyph::Lock, C_PRIMARY, C_PRIMARY_TXT);
    uiText(s, 50, 18, "SLIDE", 2, C_PRIMARY, EF_BOLD);
    uiGroupEnd(s);

    uiText(s, 12, 56, "SIGN IN", 1, C_PRIMARY, EF_BOLD);
    uiWrap(s, 12, 72, colW,
           "Scan this code with a phone or PC that is already signed in.", 1, C_TEXT);

    if (qrPx)
    {
        UiElem &qr = uiRaw(s, ElemKind::Qr, qrX, qrY, qrPx, qrPx);
        qr.ptr  = c.qrData;
        qr.data = pixel;
    }

    uiGroupBegin(s);
    uiPanel(s, 12, TOP_H - 48, colW, 1, 0, C_ACCENT, C_ACCENT, 0);
    if (c.statusMsg && c.statusMsg[0])
        addStatus(s, 12, TOP_H - 40, colW, c.statusMsg, C_PRIMARY, 4);
    else
        uiText(s, 12, TOP_H - 40, "Waiting for a scan", 1, C_TEXT_SOFT);
    uiGroupEnd(s);
}

static void buildTopError(UiScene &s, const UiContext &c)
{
    topTitleBar(s, "CONNECTION PROBLEM");
    uiGroupBegin(s);
    UiElem &card = addCard(s, 16, 44, TOP_W - 32, 122, C_ERROR);
    card.accent = 6;
    uiIcon(s, 34, 62, 32, Glyph::Warn, C_ERROR, C_PRIMARY_TXT);
    uiText(s, 78, 64, "SOMETHING WENT WRONG", 1, C_ERROR, EF_BOLD);
    uiWrap(s, 78, 82, TOP_W - 128,
           (c.statusMsg && c.statusMsg[0]) ? c.statusMsg : "The server could not be reached.",
           1, C_TEXT);
    uiGroupEnd(s);
    uiWrap(s, 16, 182, TOP_W - 32,
           "You can retry, sign in on this console, or play offline from the bottom screen.",
           1, C_TEXT_SOFT);
}

static void buildTopSimple(UiScene &s, const UiContext &c, const char *bar,
                           Glyph glyph, const char *title, const char *body)
{
    topTitleBar(s, bar);
    uiGroupBegin(s);
    UiElem &card = addCard(s, 16, 52, TOP_W - 32, 110, C_ACCENT);
    card.accent = 6;
    uiIcon(s, 34, 70, 32, glyph, C_PRIMARY, C_PRIMARY_TXT);
    uiText(s, 78, 72, title, 1, C_PRIMARY, EF_BOLD);
    uiWrap(s, 78, 90, TOP_W - 128, body, 1, C_TEXT);
    uiGroupEnd(s);
    addStatus(s, 16, 180, TOP_W - 32, c.statusMsg, C_PRIMARY, 6);
}

static void buildTopPrivateWait(UiScene &s, const UiContext &c)
{
    topTitleBar(s, "PRIVATE ROOM");
    const char *code = (c.privateCode && c.privateCode[0]) ? c.privateCode : "......";

    uiGroupBegin(s);
    uiTextIn(s, 0, 48, TOP_W, "ROOM CODE", 1, C_PRIMARY, EF_CENTER | EF_BOLD);
    const int codeW = textWidth(code, 3) + 48;
    UiElem &plate = uiPanel(s, (TOP_W - codeW) / 2, 64, codeW, 46, 12,
                            C_PRIMARY, C_PRIMARY_DK, 5);
    (void)plate;
    uiTextIn(s, (TOP_W - codeW) / 2, 76, codeW, code, 3, C_PRIMARY_TXT, EF_CENTER | EF_BOLD);
    uiGroupEnd(s);

    uiTextIn(s, 0, 124, TOP_W, "WAITING FOR AN OPPONENT", 1, C_TEXT_SOFT, EF_CENTER);
    uiWrap(s, 60, 148, TOP_W - 120,
           "Share this code so a friend can join your room.", 1, C_TEXT);
    addStatus(s, 16, 196, TOP_W - 32, c.statusMsg, C_PRIMARY, 4);
}

static void buildTopQueue(UiScene &s, const UiContext &c)
{
    topTitleBar(s, "MATCHMAKING");

    uiGroupBegin(s);
    uiIcon(s, 24, 52, 34, Glyph::Clock, C_PRIMARY, C_PRIMARY_TXT);
    uiText(s, 70, 54, "SEARCHING", 2, C_PRIMARY, EF_BOLD);
    uiText(s, 70, 76, "Looking for an opponent at your rating", 1, C_TEXT_SOFT);
    uiGroupEnd(s);

    static const char *names[3] = {"MODE", "CLOCK", "VARIANT"};
    const char *values[3];
    values[0] = c.isRated ? "Ranked" : "Casual";
    values[1] = timeLabel(c.timeControl);
    values[2] = variantLabel(c.variant);
    for (int i = 0; i < 3; ++i)
    {
        const int y = 108 + i * 26;
        uiGroupBegin(s);
        uiText(s, 24, y + 5, names[i], 1, C_TEXT_SOFT);
        uiPill(s, 120, y, 168, 20, values[i], C_BG_DARK, C_TEXT, EF_BOLD);
        uiGroupEnd(s);
    }

    addStatus(s, 24, 196, TOP_W - 48, c.statusMsg, C_PRIMARY, 4);
}

static void buildTopLobby(UiScene &s, const UiContext &c)
{
    const bool home  = c.page == LobbyPage::HOME;
    const bool local = c.page == LobbyPage::LOCAL_SETTINGS;
    topTitleBar(s, home ? "READY TO PLAY" : local ? "LOCAL MATCH" : "MATCH SETUP");

    const char *user = clampName(s, c.username, 10);   // scale-2 name, 180px of room
    const char *elo  = (c.elo && c.elo[0]) ? c.elo : "600";

    // Left column: who you are, then the loadout you would take into a match.
    uiGroupBegin(s);
    uiIcon(s, 12, 38, 34, local ? Glyph::Users : Glyph::Person, C_PRIMARY, C_PRIMARY_TXT);
    uiText(s, 56, 40, local ? "LOCAL PLAY" : user, 2, C_TEXT, EF_BOLD);
    uiText(s, 56, 62,
           local ? "ONE SHARED CONSOLE"
                 : (c.isRated ? "RANKED PLAYER" : "CASUAL PLAYER"),
           1, C_TEXT_SOFT);
    uiGroupEnd(s);

    uiText(s, 12, 88, "CURRENT LOADOUT", 1, C_PRIMARY, EF_BOLD);

    // A local match has no rating and no clock, and when joining someone
    // else's room the host's settings win — so in both cases say that rather
    // than showing settings the player is not actually about to play under.
    const bool joining = c.page == LobbyPage::PRIVATE_JOIN;
    static const char *names[3] = {"MODE", "CLOCK", "VARIANT"};
    const char *values[3];
    values[0] = joining ? "Set by host" : local ? "Local" : (c.isRated ? "Ranked" : "Casual");
    values[1] = joining ? "Set by host" : local ? "No limit" : timeLabel(c.timeControl);
    values[2] = joining ? "Set by host" : variantLabel(c.variant);
    for (int i = 0; i < 3; ++i)
    {
        const int y = 106 + i * 24;
        uiGroupBegin(s);
        uiText(s, 12, y + 5, names[i], 1, C_TEXT_SOFT);
        uiPill(s, 76, y, 140, 18, values[i], C_BG_DARK, C_TEXT, EF_BOLD);
        uiGroupEnd(s);
    }

    // Right column: a board that hints at what the game looks like, plus the
    // rating badge under it.
    uiGroupBegin(s);
    UiElem &frame = uiPanel(s, 236, 38, 152, 152, 12, C_PRIMARY, C_PRIMARY_DK, 6);
    (void)frame;
    UiElem &prev = uiRaw(s, ElemKind::Custom, 246, 48, 132, 132);
    prev.data = CW_PREVIEW;
    uiGroupEnd(s);

    uiPill(s, 236, 196, 152, 20,
           local ? "NOT RATED" : uiStr(s, "ELO %s", elo), C_ACCENT, C_TEXT, EF_BOLD);

    addStatus(s, 12, 182, 210, c.statusMsg, C_PRIMARY, 5);
}

// --- in-match top screen ---------------------------------------------------
// One side's clock, as a full card rather than the pill this used to be
// tucked into the title bar as. At 18px in the header it was legible but
// easy to miss entirely; a chess-clock pair with the time at scale 3 is the
// first thing the eye lands on, and the running side is the only lit panel
// so whose turn it is reads at a glance too.
static void addClockCard(UiScene &s, int x, int y, int w, int h,
                         const char *label, int seconds, bool active)
{
    if (seconds < 0) seconds = 0;
    const bool low = active && seconds < 10;

    Color face  = C_BG_LIGHT;
    Color text  = C_TEXT_SOFT;
    Color time  = C_TEXT;
    Color strip = C_ACCENT;
    if (low)        { face = C_ERROR;   text = C_PRIMARY_TXT; time = C_PRIMARY_TXT; strip = darken(C_ERROR, 0.35f); }
    else if (active){ face = C_ACCENT;  text = C_PRIMARY_DK;  time = C_TEXT;        strip = C_ACCENT_DK; }

    uiGroupBegin(s);
    UiElem &card = uiPanel(s, x, y, w, h, 10, face, strip, 4);
    card.flags |= EF_BORDER;
    card.fg     = active ? C_PRIMARY : C_ACCENT;
    card.data   = active ? 3 : 1;
    uiTextIn(s, x, y + 7, w, label, 1, text, EF_CENTER | EF_BOLD);
    uiTextIn(s, x, y + 20, w, uiStr(s, "%d:%02d", seconds / 60, seconds % 60), 3,
             time, EF_CENTER | EF_BOLD);
    uiGroupEnd(s);
}

static void buildTopGame(UiScene &s, const UiContext &c)
{
    const GameUiState &game = *c.game;

    if (game.gameOver)
    {
        const char *title = "DRAW";
        Color tint = C_TEXT;
        if (game.winner == game.player)                    { title = "YOU WIN!"; tint = C_SUCCESS; }
        else if (game.winner == 'W' || game.winner == 'B') { title = "YOU LOSE"; tint = C_ERROR; }

        uiGroupBegin(s);
        UiElem &card = addCard(s, 40, 62, TOP_W - 80, 124, tint);
        card.accent = 6;
        card.fg     = tint;
        card.data   = 3;
        uiTextIn(s, 40, 100, TOP_W - 80, title, 3, tint, EF_CENTER | EF_BOLD);
        uiTextIn(s, 40, 148, TOP_W - 80, "B or SELECT+B to return", 1, C_TEXT_SOFT, EF_CENTER);
        uiGroupEnd(s);
        addStatus(s, 16, 200, TOP_W - 32, game.statusMsg, C_PRIMARY, 4);
        return;
    }

    uiGroupBegin(s);
    uiPanel(s, 0, 0, TOP_W, 26, 0, C_PRIMARY, C_PRIMARY_DK, 3);
    uiText(s, 10, 7, "SLIDE", 1, C_PRIMARY_TXT, EF_BOLD);
    // Both sides are the same person in a local match, so "your move" would
    // be nonsense there — the turn card below already says whose it is.
    uiTextIn(s, TOP_W - 220, 7, 208,
             !game.isOnline    ? "LOCAL MATCH"
             : game.turn == game.player ? "YOUR MOVE" : "OPPONENT TO MOVE",
             1, C_ACCENT, EF_RIGHT);
    uiGroupEnd(s);

    // Chess-clock row. Your own side is always on the left, matching the
    // piece rails either side of the board on the bottom screen.
    const char leftPiece = game.player == 'B' ? 'B' : 'W';
    if (game.isOnline)
    {
        const int leftSecs  = leftPiece == 'W' ? game.timerW : game.timerB;
        const int rightSecs = leftPiece == 'W' ? game.timerB : game.timerW;
        addClockCard(s, 12, 30, 184, 54,
                     leftPiece == 'W' ? "WHITE - YOU" : "BLACK - YOU",
                     leftSecs, game.turn == leftPiece);
        addClockCard(s, 204, 30, 184, 54,
                     leftPiece == 'W' ? "BLACK" : "WHITE",
                     rightSecs, game.turn != leftPiece);
    }
    else
    {
        // Local matches have no clock, so the same band carries the one thing
        // that does change hands instead.
        uiGroupBegin(s);
        UiElem &turnCard = uiPanel(s, 12, 30, TOP_W - 24, 54, 10, C_ACCENT, C_ACCENT_DK, 4);
        turnCard.flags |= EF_BORDER;
        turnCard.fg     = C_PRIMARY;
        turnCard.data   = 3;
        uiTextIn(s, 12, 37, TOP_W - 24, "NO TIME LIMIT", 1, C_PRIMARY_DK, EF_CENTER | EF_BOLD);
        uiTextIn(s, 12, 50, TOP_W - 24,
                 game.turn == 'W' ? "WHITE TO MOVE" : "BLACK TO MOVE", 2, C_TEXT,
                 EF_CENTER | EF_BOLD);
        uiGroupEnd(s);
    }

    const bool waiting     = game.isOnline && game.turn != game.player;
    const bool stepConfirm = game.confirmMove;
    const bool stepMove    = !waiting && !stepConfirm && game.pieceSelected;
    const bool stepSelect  = !waiting && !stepConfirm && !game.pieceSelected;

    static const char *tabs[3] = {"SELECT", "MOVE", "SEND"};
    const bool active[3] = {stepSelect, stepMove, stepConfirm};
    for (int i = 0; i < 3; ++i)
        uiPill(s, 16 + i * 126, 90, 116, 20, tabs[i],
               active[i] ? C_PRIMARY : C_BG_DARK,
               active[i] ? C_PRIMARY_TXT : C_TEXT_SOFT, EF_BOLD);

    // One card, rewritten per step rather than swapped for a different
    // layout — that keeps the eye anchored while a move is being made.
    uiGroupBegin(s);
    UiElem &card = addCard(s, 12, 116, TOP_W - 24, 92, C_ACCENT);
    card.accent = 5;

    const char *quitAction = game.isOnline ? "CONCEDE" : "EXIT";
    struct Row { const char *key; const char *what; };
    Row rows[3];
    const char *heading;
    int rowCount;

    if (stepConfirm)
    {
        heading = game.isOnline ? "SENDING YOUR MOVE" : "APPLYING YOUR MOVE";
        rows[0] = {"WAIT", game.isOnline ? "TALKING TO THE SERVER" : "UPDATING THE BOARD"};
        rowCount = 1;
    }
    else if (waiting)
    {
        heading = "WAITING FOR OPPONENT";
        rows[0] = {"DPAD", "LOOK AROUND THE BOARD"};
        rows[1] = {"SEL+B", quitAction};
        rowCount = 2;
    }
    else if (stepMove)
    {
        heading = "MOVE YOUR PIECE";
        rows[0] = {"DPAD", "AIM THE DESTINATION"};
        rows[1] = {"A", "SEND THE MOVE"};
        rows[2] = {"B", "CANCEL SELECTION"};
        rowCount = 3;
    }
    else
    {
        heading = "SELECT A PIECE";
        rows[0] = {"DPAD", "MOVE THE CURSOR"};
        rows[1] = {"A", "SELECT A PIECE"};
        rows[2] = {"SEL+B", quitAction};
        rowCount = 3;
    }

    uiText(s, 28, 126, heading, 1, C_PRIMARY, EF_BOLD);
    for (int i = 0; i < rowCount; ++i)
    {
        const int y = 146 + i * 20;
        const int w = textWidth(rows[i].key, 1) + 16;
        uiPill(s, 28, y, w, 16, rows[i].key, C_PRIMARY, C_PRIMARY_TXT, EF_BOLD);
        uiText(s, 28 + w + 10, y + 4, rows[i].what, 1, C_TEXT);
    }
    uiGroupEnd(s);

    addStatus(s, 12, 214, TOP_W - 24, game.statusMsg, C_PRIMARY, 2);
}

void buildTopScene(UiScene &s, const UiContext &c)
{
    uiSceneBegin(s, TOP_W, TOP_H, true, topSceneKey(c));

    if (c.confirmingQuit)      { buildTopQuitConfirm(s); return; }
    if (c.gameActive && c.game) { buildTopGame(s, c);    return; }

    switch (c.state)
    {
    case AppState::ERROR_STATE:
        buildTopError(s, c);
        break;
    case AppState::QR_LOGIN:
        if (c.qrReady) buildTopQr(s, c);
        else buildTopSimple(s, c, "SIGN IN", Glyph::Lock, "PREPARING SIGN IN",
                            "Fetching a login code from the server.");
        break;
    case AppState::INIT:
        buildTopSimple(s, c, "CONNECTING", Glyph::Clock, "CONNECTING TO SLIDE",
                       "Checking your saved login and reaching the server.");
        break;
    case AppState::KEYBOARD_LOGIN:
        buildTopSimple(s, c, "SIGN IN", Glyph::Person, "SIGNING IN HERE",
                       "Enter your username and password on the keyboard.");
        break;
    case AppState::LOGGED_IN:
        switch (c.page)
        {
        case LobbyPage::PRIVATE_WAIT:    buildTopPrivateWait(s, c); break;
        case LobbyPage::QUEUE:           buildTopQueue(s, c); break;
        case LobbyPage::SPECTATE_COMING:
            buildTopSimple(s, c, "SPECTATE", Glyph::Eye, "COMING SOON",
                           "Watching other players from the console is not ready yet.");
            break;
        default:                         buildTopLobby(s, c); break;
        }
        break;
    }
}

// ---------------------------------------------------------------------------
// Bottom screen
// ---------------------------------------------------------------------------
static void buildBottomQuitConfirm(UiScene &s, const UiContext &c)
{
    uiGroupBegin(s);
    UiElem &card = addCard(s, 16, 18, BOT_W - 32, 96, C_ERROR);
    card.accent = 5;
    uiIcon(s, 32, 34, 32, Glyph::Warn, C_ERROR, C_PRIMARY_TXT);
    uiText(s, 76, 36, "ARE YOU SURE?", 1, C_ERROR, EF_BOLD);
    uiWrap(s, 76, 54, 212,
           "Quitting closes Slide and returns you to the home menu.", 1, C_TEXT);
    uiGroupEnd(s);

    addButton(s, BTN_QUIT_YES, c, -1, Glyph::Cross);
    addButton(s, BTN_QUIT_NO,  c, -1, Glyph::Back);
    uiTextIn(s, 16, 200, BOT_W - 32, "A to quit  -  B to stay", 1, C_TEXT_SOFT, EF_CENTER);
}

static void buildBottomLogin(UiScene &s, const UiContext &c)
{
    const bool failed = c.state == AppState::ERROR_STATE;
    addPageHeader(s, 16, 10, failed ? Glyph::Warn : Glyph::Lock,
                  failed ? C_ERROR : C_PRIMARY,
                  failed ? "RETRY" : "SIGN IN",
                  failed ? "Something went wrong" : "Pick how to continue");
    uiPanel(s, 16, 52, BOT_W - 32, 1, 0, C_ACCENT, C_ACCENT, 0);
    uiWrap(s, 16, 62, BOT_W - 32,
           failed ? "The server could not be reached. Try signing in again, or play offline."
                  : "Scan the code on the top screen, or use one of these instead.",
           1, C_TEXT_SOFT);

    addButton(s, BTN_SIGNIN,  c, 0, Glyph::Person);
    addButton(s, BTN_GUEST,   c, 1, Glyph::Zap);
    addButton(s, BTN_OFFLINE, c, 2, Glyph::Users);
    addButton(s, BTN_QUIT,    c, 3, Glyph::Exit);
}

static void buildBottomInit(UiScene &s, const UiContext &c)
{
    addPageHeader(s, 16, 20, Glyph::Clock, C_PRIMARY, "CONNECTING", "One moment");
    uiWrap(s, 16, 70, BOT_W - 32,
           (c.statusMsg && c.statusMsg[0]) ? c.statusMsg : "Talking to the Slide server...",
           1, C_TEXT);
    addButton(s, BTN_QUIT, c, 0, Glyph::Exit);
}

static void buildBottomHome(UiScene &s, const UiContext &c)
{
    const char *user = clampName(s, c.username, 21);   // stops short of the ELO pill
    const char *elo  = (c.elo && c.elo[0]) ? c.elo : "600";

    uiGroupBegin(s);
    addCard(s, 8, 8, BOT_W - 16, 46, C_ACCENT);
    uiIcon(s, 16, 15, 30, Glyph::Person, C_PRIMARY, C_PRIMARY_TXT);
    uiText(s, 54, 17, user, 1, C_TEXT, EF_BOLD);
    uiText(s, 54, 30, c.isRated ? "RANKED PLAYER" : "CASUAL PLAYER", 1, C_TEXT_SOFT);
    uiPill(s, 230, 20, 74, 18, uiStr(s, "ELO %s", elo), C_PRIMARY, C_PRIMARY_TXT, EF_BOLD);
    uiGroupEnd(s);

    uiText(s, 10, 62, "CHOOSE A MODE", 1, C_PRIMARY, EF_BOLD);
    addButton(s, BTN_PUBLIC_MATCH, c, 0, Glyph::Play);
    addButton(s, BTN_PRIVATE_ROOM, c, 1, Glyph::Hash);
    addButton(s, BTN_LOCAL_PLAY,   c, 2, Glyph::Users);
    addButton(s, BTN_SPECTATE,     c, 3, Glyph::Eye);
    addButton(s, BTN_SIGNOUT,      c, 4, Glyph::Exit);
    addButton(s, BTN_QUIT,         c, 5, Glyph::Cross);
    addStatus(s, 10, 170, BOT_W - 20, c.statusMsg, C_PRIMARY, 2);
}

static void buildBottomSettings(UiScene &s, const UiContext &c)
{
    const bool privateRoom = c.page == LobbyPage::PRIVATE_CREATE;
    addTightHeader(s, 8, 4, privateRoom ? Glyph::Hash : Glyph::Play, C_PRIMARY,
                   privateRoom ? "CREATE ROOM" : "PUBLIC MATCH",
                   privateRoom ? "Your friend inherits these" : "Tap a row to change it");

    addSettingRow(s, BTN_MATCH_SETTING, "MATCH TYPE", c.isRated ? "RANKED" : "CASUAL",
                  c.focusVisible && c.focusIndex == 0, false);
    addSettingRow(s, BTN_TIME_SETTING, "TIME CONTROL", timeLabel(c.timeControl),
                  c.focusVisible && c.focusIndex == 1, false);
    addSettingRow(s, BTN_VARIANT_SETTING, "VARIANT", variantLabel(c.variant),
                  c.focusVisible && c.focusIndex == 2, false);

    uiWrap(s, 8, 104, BOT_W - 16,
           privateRoom ? "Create the room and you will get a code to share."
                       : "You will be queued against players near your rating.",
           1, C_TEXT_SOFT);
    addStatus(s, 8, 136, BOT_W - 16, c.statusMsg, C_PRIMARY, 4);

    addButton(s, BTN_CONTINUE, c, 3, Glyph::Check);
    addButton(s, BTN_BACK,     c, 4, Glyph::Back);
    addButton(s, BTN_QUIT,     c, 5, Glyph::Cross);
}

static void buildBottomPrivateChoice(UiScene &s, const UiContext &c)
{
    addPageHeader(s, 16, 10, Glyph::Hash, C_PRIMARY, "PRIVATE", "Play with a friend");
    uiWrap(s, 16, 52, BOT_W - 32,
           "Create a room with your own settings, or join a friend using their code.",
           1, C_TEXT_SOFT);
    addButton(s, BTN_CREATE_ROOM, c, 0, Glyph::Zap);
    addButton(s, BTN_JOIN_ROOM,   c, 1, Glyph::Hash);
    addStatus(s, 16, 140, BOT_W - 32, c.statusMsg, C_PRIMARY, 3);
    addButton(s, BTN_BACK, c, 2, Glyph::Back);
    addButton(s, BTN_QUIT, c, 3, Glyph::Cross);
}

static void buildBottomPrivateJoin(UiScene &s, const UiContext &c)
{
    addPageHeader(s, 16, 10, Glyph::Hash, C_PRIMARY, "JOIN", "Enter a room code");
    uiWrap(s, 16, 52, BOT_W - 32,
           "Continue opens the keyboard so you can type the code your friend gave you.",
           1, C_TEXT_SOFT);

    const char *code = (c.joinCode && c.joinCode[0]) ? c.joinCode : "- - - - - -";
    uiGroupBegin(s);
    uiPanel(s, 16, 92, BOT_W - 32, 40, 10, C_PRIMARY, C_PRIMARY_DK, 4);
    uiTextIn(s, 16, 102, BOT_W - 32, code, 2, C_PRIMARY_TXT, EF_CENTER | EF_BOLD);
    uiGroupEnd(s);

    addStatus(s, 16, 140, BOT_W - 32, c.statusMsg, C_PRIMARY, 3);
    addButton(s, BTN_CONTINUE, c, 0, Glyph::Check);
    addButton(s, BTN_BACK,     c, 1, Glyph::Back);
    addButton(s, BTN_QUIT,     c, 2, Glyph::Cross);
}

static void buildBottomLocal(UiScene &s, const UiContext &c)
{
    addTightHeader(s, 8, 6, Glyph::Users, C_PRIMARY, "LOCAL PLAY", "Two players, one console");
    addSettingRow(s, BTN_LOCAL_VARIANT, "VARIANT", variantLabel(c.variant),
                  c.focusVisible && c.focusIndex == 0, false);
    addSettingRow(s, Button{8, 68, BOT_W - 16, 20, "", C_BG_LIGHT, C_TEXT, C_ACCENT},
                  "TIME CONTROL", "NO LIMIT", false, true);
    uiWrap(s, 8, 96, BOT_W - 16,
           "Pass the console back and forth. Pick a variant, then start the match.",
           1, C_TEXT_SOFT);
    addStatus(s, 8, 132, BOT_W - 16, c.statusMsg, C_PRIMARY, 4);
    addButton(s, BTN_START_LOCAL, c, 1, Glyph::Play);
    addButton(s, BTN_BACK,        c, 2, Glyph::Back);
}

static void buildBottomQueue(UiScene &s, const UiContext &c)
{
    addPageHeader(s, 16, 10, Glyph::Clock, C_PRIMARY, "IN QUEUE", "Hold tight");
    uiWrap(s, 16, 52, BOT_W - 32,
           "Your settings are locked while you are queued. Keep this screen open.",
           1, C_TEXT_SOFT);

    const char *values[3];
    values[0] = c.isRated ? "Ranked" : "Casual";
    values[1] = timeLabel(c.timeControl);
    values[2] = variantLabel(c.variant);
    static const int px[3] = {16, 114, 212};
    static const int pw[3] = {90, 90, 92};
    for (int i = 0; i < 3; ++i)
        uiPill(s, px[i], 92, pw[i], 20, values[i], C_BG_DARK, C_TEXT, EF_BOLD);

    addStatus(s, 16, 122, BOT_W - 32, c.statusMsg, C_PRIMARY, 4);
    addButton(s, BTN_CANCEL_QUEUE, c, 0, Glyph::Cross);
}

static void buildBottomPrivateWait(UiScene &s, const UiContext &c)
{
    addPageHeader(s, 16, 10, Glyph::Hash, C_PRIMARY, "ROOM OPEN", "Waiting for a friend");

    const char *code = (c.privateCode && c.privateCode[0]) ? c.privateCode : "......";
    uiGroupBegin(s);
    uiPanel(s, 16, 56, BOT_W - 32, 40, 10, C_PRIMARY, C_PRIMARY_DK, 4);
    uiTextIn(s, 16, 66, BOT_W - 32, code, 2, C_PRIMARY_TXT, EF_CENTER | EF_BOLD);
    uiGroupEnd(s);

    uiWrap(s, 16, 106, BOT_W - 32,
           "The match starts the moment they join with this code.", 1, C_TEXT_SOFT);
    addStatus(s, 16, 132, BOT_W - 32, c.statusMsg, C_PRIMARY, 3);
    addButton(s, BTN_CANCEL_PRIVATE, c, 0, Glyph::Cross);
}

static void buildBottomSpectate(UiScene &s, const UiContext &c)
{
    addPageHeader(s, 16, 10, Glyph::Eye, C_PURPLE, "SPECTATE", "Not on 3DS yet");
    uiWrap(s, 16, 56, BOT_W - 32,
           "Spectating is only on the website for now. It is coming to the console later.",
           1, C_TEXT_SOFT);
    addStatus(s, 16, 110, BOT_W - 32, c.statusMsg, C_PRIMARY, 6);
    addButton(s, BTN_BACK, c, 0, Glyph::Back);
    addButton(s, BTN_QUIT, c, -1, Glyph::Cross);
}

static void buildBottomGame(UiScene &s, const UiContext &c)
{
    const GameUiState &game = *c.game;
    const int frame = 6;

    UiElem &board = uiRaw(s, ElemKind::Custom, BOARD_X - frame, BOARD_Y - frame,
                          BOARD_PX + frame * 2, BOARD_PX + frame * 2);
    board.data = CW_BOARD;
    board.ptr  = c.game;

    // The two 40px gutters either side of the board are the only spare space
    // on this screen, so they carry the side-to-move readout. Labelling by
    // colour rather than YOU/OPP keeps it honest in a local match and stops
    // it flipping when an online game ends and the match stops being "live".
    // The left rail is still your own colour when you have one.
    const char leftPiece  = game.player == 'B' ? 'B' : 'W';
    const char rightPiece = leftPiece == 'W' ? 'B' : 'W';
    const bool leftToMove = game.turn == leftPiece;

    uiGroupBegin(s);
    uiTextIn(s, 0, 14, 40, leftPiece == 'W' ? "WHITE" : "BLACK", 1, C_TEXT_SOFT, EF_CENTER);
    UiElem &left = uiRaw(s, ElemKind::Custom, 8, 28, 24, 24);
    left.data = leftPiece == 'W' ? CW_CHIP_W : CW_CHIP_B;
    if (leftToMove && !game.gameOver)
        uiPill(s, 4, 58, 32, 14, "GO", C_SUCCESS, C_PRIMARY_TXT, EF_BOLD);
    uiGroupEnd(s);

    uiGroupBegin(s);
    uiTextIn(s, BOT_W - 40, 14, 40, rightPiece == 'W' ? "WHITE" : "BLACK", 1, C_TEXT_SOFT, EF_CENTER);
    UiElem &right = uiRaw(s, ElemKind::Custom, BOT_W - 32, 28, 24, 24);
    right.data = rightPiece == 'W' ? CW_CHIP_W : CW_CHIP_B;
    if (!leftToMove && !game.gameOver)
        uiPill(s, BOT_W - 36, 58, 32, 14, "GO", C_SUCCESS, C_PRIMARY_TXT, EF_BOLD);
    uiGroupEnd(s);
}

void buildBottomScene(UiScene &s, const UiContext &c)
{
    uiSceneBegin(s, BOT_W, BOT_H, false, bottomSceneKey(c));

    if (c.confirmingQuit)       { buildBottomQuitConfirm(s, c); return; }
    if (c.gameActive && c.game) { buildBottomGame(s, c);        return; }

    switch (c.state)
    {
    case AppState::QR_LOGIN:
    case AppState::ERROR_STATE:
        buildBottomLogin(s, c);
        break;
    case AppState::INIT:
        buildBottomInit(s, c);
        break;
    case AppState::KEYBOARD_LOGIN:
        addPageHeader(s, 16, 20, Glyph::Person, C_PRIMARY, "KEYBOARD", "Using the system keyboard");
        uiWrap(s, 16, 70, BOT_W - 32,
               "Finish entering your details on the keyboard, or cancel to go back.",
               1, C_TEXT_SOFT);
        break;
    case AppState::LOGGED_IN:
        switch (c.page)
        {
        case LobbyPage::HOME:            buildBottomHome(s, c); break;
        case LobbyPage::PRIVATE_CHOICE:  buildBottomPrivateChoice(s, c); break;
        case LobbyPage::PRIVATE_JOIN:    buildBottomPrivateJoin(s, c); break;
        case LobbyPage::LOCAL_SETTINGS:  buildBottomLocal(s, c); break;
        case LobbyPage::QUEUE:           buildBottomQueue(s, c); break;
        case LobbyPage::PRIVATE_WAIT:    buildBottomPrivateWait(s, c); break;
        case LobbyPage::SPECTATE_COMING: buildBottomSpectate(s, c); break;
        default:                         buildBottomSettings(s, c); break;
        }
        break;
    }
}
