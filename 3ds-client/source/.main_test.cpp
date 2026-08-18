/*
 * Slide 3DS Client — UI Test Build
 *
 * No networking. Hardcodes a QR code for:
 *   https://slide.wiizardsoftware.uk/qr/123456789A
 * and goes straight into QR_LOGIN state so the full UI can be verified
 * in an emulator without a server connection.
 *
 * Build target: slide-3ds-test.3dsx
 */

#include <3ds.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

// Nayuki QR Code generator (bundled)
extern "C" {
#include "qrcodegen.h"
}

// ---------------------------------------------------------------------------
// Screen constants
// ---------------------------------------------------------------------------
static constexpr int TOP_W  = 400;
static constexpr int TOP_H  = 240;
static constexpr int BOT_W  = 320;
static constexpr int BOT_H  = 240;

// ---------------------------------------------------------------------------
// Theme colours
// ---------------------------------------------------------------------------
struct Color { uint8_t r, g, b; };

static constexpr Color C_BG          = {244, 241, 234};
static constexpr Color C_BG_LIGHT    = {255, 255, 255};
static constexpr Color C_BG_DARK     = {227, 217, 198};
static constexpr Color C_TEXT        = { 74,  55,  40};
static constexpr Color C_PRIMARY     = {139,  69,  19};
static constexpr Color C_PRIMARY_TXT = {255, 255, 255};
static constexpr Color C_ACCENT      = {210, 180, 140};

// ---------------------------------------------------------------------------
// Pixel / rect helpers
// ---------------------------------------------------------------------------
static inline void drawPixel(uint8_t* fb, int w, int h, int x, int y, Color c) {
    if (x < 0 || x >= w || y < 0 || y >= h) return;
    int offset = (x * h + (h - 1 - y)) * 3;
    fb[offset + 0] = c.b;
    fb[offset + 1] = c.g;
    fb[offset + 2] = c.r;
}

static inline void fillRect(uint8_t* fb, int w, int h,
                             int x0, int y0, int rw, int rh, Color c) {
    for (int dy = 0; dy < rh; ++dy)
        for (int dx = 0; dx < rw; ++dx)
            drawPixel(fb, w, h, x0 + dx, y0 + dy, c);
}

static void clearScreen(uint8_t* fb, int w, int h, Color c) {
    fillRect(fb, w, h, 0, 0, w, h, c);
}

static void drawRoundRect(uint8_t* fb, int w, int h,
                          int x0, int y0, int rw, int rh, int r,
                          int thick, Color c) {
    for (int t = 0; t < thick; ++t) {
        fillRect(fb, w, h, x0+r, y0+t,       rw-2*r, 1, c);
        fillRect(fb, w, h, x0+r, y0+rh-1-t,  rw-2*r, 1, c);
        fillRect(fb, w, h, x0+t,      y0+r, 1, rh-2*r, c);
        fillRect(fb, w, h, x0+rw-1-t, y0+r, 1, rh-2*r, c);
    }
    float innerR = (float)(r - thick);
    float outerR = (float)r;
    for (int dy = 0; dy < r; ++dy) {
        for (int dx = 0; dx < r; ++dx) {
            float cx2 = (float)(r - 1 - dx);
            float cy2 = (float)(r - 1 - dy);
            float dist = sqrtf(cx2*cx2 + cy2*cy2);
            if (dist >= innerR && dist < outerR) {
                drawPixel(fb, w, h, x0 + dx,          y0 + dy,          c);
                drawPixel(fb, w, h, x0 + rw - 1 - dx, y0 + dy,          c);
                drawPixel(fb, w, h, x0 + dx,          y0 + rh - 1 - dy, c);
                drawPixel(fb, w, h, x0 + rw - 1 - dx, y0 + rh - 1 - dy, c);
            }
        }
    }
}

static void fillRoundRect(uint8_t* fb, int w, int h,
                          int x0, int y0, int rw, int rh, int r, Color c) {
    for (int dy = 0; dy < rh; ++dy) {
        int y = y0 + dy;
        int margin = 0;
        if (dy < r)           margin = r - (int)sqrtf((float)(r*r - (r-dy)*(r-dy)));
        else if (dy >= rh-r)  margin = r - (int)sqrtf((float)(r*r - (r-(rh-1-dy))*(r-(rh-1-dy))));
        fillRect(fb, w, h, x0 + margin, y, rw - 2*margin, 1, c);
    }
}

// ---------------------------------------------------------------------------
// 8×8 bitmap font
// ---------------------------------------------------------------------------
static const uint8_t FONT8[95][8] = {
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 32 ' '
  {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, // 33 '!'
  {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00}, // 34 '"'
  {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00}, // 35 '#'
  {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00}, // 36 '$'
  {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00}, // 37 '%'
  {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00}, // 38 '&'
  {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00}, // 39 '\''
  {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00}, // 40 '('
  {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00}, // 41 ')'
  {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // 42 '*'
  {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00}, // 43 '+'
  {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06}, // 44 ','
  {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00}, // 45 '-'
  {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00}, // 46 '.'
  {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00}, // 47 '/'
  {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00}, // 48 '0'
  {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00}, // 49 '1'
  {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00}, // 50 '2'
  {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00}, // 51 '3'
  {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00}, // 52 '4'
  {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00}, // 53 '5'
  {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00}, // 54 '6'
  {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00}, // 55 '7'
  {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00}, // 56 '8'
  {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00}, // 57 '9'
  {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00}, // 58 ':'
  {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06}, // 59 ';'
  {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00}, // 60 '<'
  {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00}, // 61 '='
  {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, // 62 '>'
  {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00}, // 63 '?'
  {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00}, // 64 '@'
  {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00}, // 65 'A'
  {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00}, // 66 'B'
  {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00}, // 67 'C'
  {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00}, // 68 'D'
  {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00}, // 69 'E'
  {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00}, // 70 'F'
  {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00}, // 71 'G'
  {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00}, // 72 'H'
  {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // 73 'I'
  {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00}, // 74 'J'
  {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00}, // 75 'K'
  {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00}, // 76 'L'
  {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00}, // 77 'M'
  {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00}, // 78 'N'
  {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00}, // 79 'O'
  {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00}, // 80 'P'
  {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00}, // 81 'Q'
  {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00}, // 82 'R'
  {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00}, // 83 'S'
  {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // 84 'T'
  {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00}, // 85 'U'
  {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00}, // 86 'V'
  {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // 87 'W'
  {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00}, // 88 'X'
  {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00}, // 89 'Y'
  {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00}, // 90 'Z'
  {0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00}, // 91 '['
  {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00}, // 92 '\\'
  {0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00}, // 93 ']'
  {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00}, // 94 '^'
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // 95 '_'
  {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00}, // 96 '`'
  {0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00}, // 97 'a'
  {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00}, // 98 'b'
  {0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00}, // 99 'c'
  {0x38,0x30,0x30,0x3e,0x33,0x33,0x6E,0x00}, // 100 'd'
  {0x00,0x00,0x1E,0x33,0x3f,0x03,0x1E,0x00}, // 101 'e'
  {0x1C,0x36,0x06,0x0f,0x06,0x06,0x0F,0x00}, // 102 'f'
  {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F}, // 103 'g'
  {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00}, // 104 'h'
  {0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00}, // 105 'i'
  {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E}, // 106 'j'
  {0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00}, // 107 'k'
  {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // 108 'l'
  {0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00}, // 109 'm'
  {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00}, // 110 'n'
  {0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00}, // 111 'o'
  {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F}, // 112 'p'
  {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78}, // 113 'q'
  {0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00}, // 114 'r'
  {0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00}, // 115 's'
  {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00}, // 116 't'
  {0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00}, // 117 'u'
  {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00}, // 118 'v'
  {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00}, // 119 'w'
  {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00}, // 120 'x'
  {0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F}, // 121 'y'
  {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00}, // 122 'z'
  {0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00}, // 123 '{'
  {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, // 124 '|'
  {0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00}, // 125 '}'
  {0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00}, // 126 '~'
};

static void drawChar(uint8_t* fb, int fbW, int fbH, int x, int y,
                     char ch, int scale, Color fg, Color bg, bool hasBg = false) {
    if (ch < 32 || ch > 126) ch = '?';
    const uint8_t* glyph = FONT8[(uint8_t)ch - 32];
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            bool set = (glyph[row] >> col) & 1;
            if (set || hasBg) {
                Color c = set ? fg : bg;
                for (int sy = 0; sy < scale; ++sy)
                    for (int sx = 0; sx < scale; ++sx)
                        drawPixel(fb, fbW, fbH,
                                  x + col*scale + sx,
                                  y + row*scale + sy, c);
            }
        }
    }
}

static int drawText(uint8_t* fb, int fbW, int fbH, int x, int y,
                    const char* str, int scale, Color fg,
                    Color bg = C_BG, bool hasBg = false) {
    while (*str) {
        drawChar(fb, fbW, fbH, x, y, *str, scale, fg, bg, hasBg);
        x += 8 * scale;
        ++str;
    }
    return x;
}

static void drawTextWrapped(uint8_t* fb, int fbW, int fbH,
                            int x0, int y0, int maxW,
                            const char* str, int scale, Color fg) {
    int charW = 8 * scale;
    int lineH = 9 * scale;
    int cx = x0, cy = y0;
    char word[128];
    while (*str) {
        if (*str == ' ') { if (cx > x0) cx += charW; ++str; continue; }
        if (*str == '\n') { cx = x0; cy += lineH; ++str; continue; }
        int wlen = 0;
        while (*str && *str != ' ' && *str != '\n') { word[wlen++] = *str++; }
        word[wlen] = 0;
        int wordPx = wlen * charW;
        if (cx > x0 && cx + wordPx > x0 + maxW) { cx = x0; cy += lineH; }
        for (int i = 0; i < wlen; ++i) {
            drawChar(fb, fbW, fbH, cx, cy, word[i], scale, fg, C_BG, false);
            cx += charW;
        }
    }
}

// ---------------------------------------------------------------------------
// QR code rendering
// ---------------------------------------------------------------------------
static constexpr int QR_PIXEL = 5;
static constexpr int QR_QUIET = 4;

static void renderQR(uint8_t* fb, const uint8_t* qrcode) {
    int size = qrcodegen_getSize(qrcode);
    int totalMod = size + QR_QUIET * 2;
    int totalPx  = totalMod * QR_PIXEL;
    int margin   = (TOP_H - totalPx) / 2;
    int originX  = TOP_W - totalPx - margin;
    int originY  = margin;

    fillRect(fb, TOP_W, TOP_H, originX, originY, totalPx, totalPx, C_BG_LIGHT);

    for (int row = 0; row < totalMod; ++row) {
        for (int col = 0; col < totalMod; ++col) {
            int qx = col - QR_QUIET;
            int qy = row - QR_QUIET;
            bool black = false;
            if (qx >= 0 && qx < size && qy >= 0 && qy < size)
                black = qrcodegen_getModule(qrcode, qx, qy);
            if (!black) continue;
            Color c = {0, 0, 0};
            int px0 = originX + col * QR_PIXEL;
            int py0 = originY + row * QR_PIXEL;
            for (int dy = 0; dy < QR_PIXEL; ++dy)
                for (int dx = 0; dx < QR_PIXEL; ++dx)
                    drawPixel(fb, TOP_W, TOP_H, px0+dx, py0+dy, c);
        }
    }
}

// ---------------------------------------------------------------------------
// Button
// ---------------------------------------------------------------------------
struct Button {
    int x, y, w, h;
    const char* label;
    Color bgColor;
    Color textColor;
    Color borderColor;
};

static bool buttonHit(const Button& btn, int tx, int ty) {
    return tx >= btn.x && tx < btn.x + btn.w &&
           ty >= btn.y && ty < btn.y + btn.h;
}

static void drawButton(uint8_t* fb, const Button& btn, bool pressed, int textScale = 1) {
    Color bg   = pressed ? btn.borderColor : btn.bgColor;
    Color text = pressed ? btn.bgColor     : btn.textColor;
    int r = 12;
    fillRoundRect(fb, BOT_W, BOT_H, btn.x, btn.y, btn.w, btn.h, r, bg);
    if (!pressed)
        drawRoundRect(fb, BOT_W, BOT_H, btn.x, btn.y, btn.w, btn.h, r, 3, btn.borderColor);
    int charW   = 8 * textScale;
    int textLen = (int)strlen(btn.label);
    int textW   = textLen * charW;
    int tx = btn.x + (btn.w - textW) / 2;
    int ty = btn.y + (btn.h - 8 * textScale) / 2;
    drawText(fb, BOT_W, BOT_H, tx, ty, btn.label, textScale, text);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    gfxInitDefault();

    // ---------------------------------------------------------------------------
    // Generate QR code for the hardcoded test URL — no networking needed
    // ---------------------------------------------------------------------------
    static constexpr const char* TEST_URL =
        "https://slide.wiizardsoftware.uk/qr/123456789A";

    static uint8_t qrTempBuf[qrcodegen_BUFFER_LEN_FOR_VERSION(5)];
    static uint8_t qrData[qrcodegen_BUFFER_LEN_FOR_VERSION(5)];
    memset(qrTempBuf, 0, sizeof(qrTempBuf));
    memset(qrData,    0, sizeof(qrData));
    bool qrReady = qrcodegen_encodeText(
        TEST_URL,
        qrTempBuf, qrData,
        qrcodegen_Ecc_LOW,
        1, 5,
        qrcodegen_Mask_AUTO,
        false
    );

    // ---------------------------------------------------------------------------
    // Button layout
    // ---------------------------------------------------------------------------
    static const Button BTN_SIGNIN = {
        16,  135,  BOT_W - 32,  38,
        "Sign in on this device",
        C_PRIMARY, C_PRIMARY_TXT, {105, 50, 12}
    };
    static const Button BTN_QUIT = {
        16,  183,  BOT_W - 32,  36,
        "Quit",
        C_BG_DARK, C_TEXT, C_ACCENT
    };

    bool pressedSignIn = false;
    bool pressedQuit   = false;

    // ---------------------------------------------------------------------------
    // Main loop
    // ---------------------------------------------------------------------------
    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();

        if (kDown & KEY_START) break;

        touchPosition touch;
        hidTouchRead(&touch);
        bool touched   = (kDown & KEY_TOUCH) != 0;
        bool touchHeld = (kHeld & KEY_TOUCH)  != 0;

        pressedSignIn = touchHeld && buttonHit(BTN_SIGNIN, touch.px, touch.py);
        pressedQuit   = touchHeld && buttonHit(BTN_QUIT,   touch.px, touch.py);

        if (touched && buttonHit(BTN_QUIT, touch.px, touch.py)) break;

        // ----- Top screen -----
        uint8_t* topFb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, nullptr, nullptr);
        clearScreen(topFb, TOP_W, TOP_H, C_BG);

        // Title bar
        fillRect(topFb, TOP_W, TOP_H, 0, 0, TOP_W, 24, C_PRIMARY);
        drawText(topFb, TOP_W, TOP_H, 8, 8, "Slide", 1, C_PRIMARY_TXT);

        if (qrReady) {
            renderQR(topFb, qrData);
            int qrSize    = qrcodegen_getSize(qrData);
            int qrTotalPx = (qrSize + QR_QUIET * 2) * QR_PIXEL;
            int margin    = (TOP_H - qrTotalPx) / 2;
            int qrLeft    = TOP_W - qrTotalPx - margin;
            int textAreaW = qrLeft - 16;

            drawTextWrapped(topFb, TOP_W, TOP_H, 8, 36, textAreaW,
                            "Scan the QR code to log in using another device.", 1, C_TEXT);

            int lineY = TOP_H - 28;
            fillRect(topFb, TOP_W, TOP_H, 8, lineY, textAreaW, 1, C_ACCENT);
            drawText(topFb, TOP_W, TOP_H, 8, lineY + 5, "Waiting for scan...", 1, C_ACCENT);
        } else {
            drawText(topFb, TOP_W, TOP_H, 8, 36, "QR generation failed", 1, C_PRIMARY);
        }

        // ----- Bottom screen -----
        uint8_t* botFb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, nullptr, nullptr);
        clearScreen(botFb, BOT_W, BOT_H, C_BG);

        // Top separator bar
        fillRect(botFb, BOT_W, BOT_H, 0, 0, BOT_W, 3, C_PRIMARY);

        // Header
        {
            const char* title = "Slide Strategy Game";
            int titleW = (int)strlen(title) * 8 * 2;
            drawText(botFb, BOT_W, BOT_H, (BOT_W - titleW) / 2, 8, title, 2, C_PRIMARY);
        }
        fillRect(botFb, BOT_W, BOT_H, 8, 30, BOT_W - 16, 1, C_ACCENT);

        drawTextWrapped(botFb, BOT_W, BOT_H, 8, 38, BOT_W - 16,
                        "Or sign in on this device:", 1, C_TEXT);

        drawButton(botFb, BTN_SIGNIN, pressedSignIn, 1);
        drawButton(botFb, BTN_QUIT,   pressedQuit,   2);

        // Footer
        {
            const char* footer = "Made by Wiizard Software";
            int footerW = (int)strlen(footer) * 8;
            drawText(botFb, BOT_W, BOT_H, (BOT_W - footerW) / 2, BOT_H - 12, footer, 1, C_ACCENT);
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}
