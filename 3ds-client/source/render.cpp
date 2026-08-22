/*
 * render.cpp — implementation of drawing primitives, font, and QR rendering.
 */
#include "render.h"
#include "font8x8.h"

extern "C" {
#include "qrcodegen.h"
}

// ---------------------------------------------------------------------------
// Pixel / rect
// ---------------------------------------------------------------------------
void drawPixel(uint8_t *fb, int w, int h, int x, int y, Color c)
{
    if (x < 0 || x >= w || y < 0 || y >= h) return;
    int offset = (x * h + (h - 1 - y)) * 3;
    fb[offset + 0] = c.b;
    fb[offset + 1] = c.g;
    fb[offset + 2] = c.r;
}

// Clipped, run-based fill. The framebuffer is column-major with y flipped, so
// every row-window of a column is one contiguous byte range: fill the first
// column by hand and memcpy it across the rest. The whole UI is now redrawn
// from scratch every frame (see uikit.cpp), so this is the hot path.
void fillRect(uint8_t *fb, int w, int h, int x0, int y0, int rw, int rh, Color c)
{
    int x1 = x0 + rw, y1 = y0 + rh;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > w) x1 = w;
    if (y1 > h) y1 = h;
    if (x0 >= x1 || y0 >= y1) return;

    const size_t colOff = (size_t)(h - y1) * 3;   // byte offset of the row window
    const int    rows   = y1 - y0;

    if (rows == 1)
    {
        for (int x = x0; x < x1; ++x)
        {
            uint8_t *p = fb + ((size_t)x * h) * 3 + colOff;
            p[0] = c.b; p[1] = c.g; p[2] = c.r;
        }
        return;
    }

    uint8_t *first = fb + ((size_t)x0 * h) * 3 + colOff;
    for (int i = 0; i < rows; ++i)
    {
        first[i * 3 + 0] = c.b;
        first[i * 3 + 1] = c.g;
        first[i * 3 + 2] = c.r;
    }
    const size_t len = (size_t)rows * 3;
    for (int x = x0 + 1; x < x1; ++x)
        memcpy(fb + ((size_t)x * h) * 3 + colOff, first, len);
}

void clearScreen(uint8_t *fb, int w, int h, Color c)
{
    fillRect(fb, w, h, 0, 0, w, h, c);
}

void drawPixelBlend(uint8_t *fb, int w, int h, int x, int y, Color c, float alpha)
{
    if (x < 0 || x >= w || y < 0 || y >= h) return;
    if (alpha <= 0.0f) return;
    if (alpha >= 1.0f) { drawPixel(fb, w, h, x, y, c); return; }
    int offset = (x * h + (h - 1 - y)) * 3;
    fb[offset + 0] = (uint8_t)(fb[offset + 0] * (1.0f - alpha) + c.b * alpha);
    fb[offset + 1] = (uint8_t)(fb[offset + 1] * (1.0f - alpha) + c.g * alpha);
    fb[offset + 2] = (uint8_t)(fb[offset + 2] * (1.0f - alpha) + c.r * alpha);
}

void fillRectBlend(uint8_t *fb, int w, int h, int x0, int y0, int rw, int rh, Color c, float alpha)
{
    for (int dy = 0; dy < rh; ++dy)
        for (int dx = 0; dx < rw; ++dx)
            drawPixelBlend(fb, w, h, x0 + dx, y0 + dy, c, alpha);
}

void drawRoundRect(uint8_t *fb, int w, int h,
                   int x0, int y0, int rw, int rh, int r, int thick, Color c)
{
    if (rw <= 0 || rh <= 0) return;
    if (r * 2 > rw) r = rw / 2;
    if (r * 2 > rh) r = rh / 2;
    for (int t = 0; t < thick; ++t)
    {
        fillRect(fb, w, h, x0 + r,           y0 + t,          rw - 2*r, 1, c); // top
        fillRect(fb, w, h, x0 + r,           y0 + rh - 1 - t, rw - 2*r, 1, c); // bottom
        fillRect(fb, w, h, x0 + t,           y0 + r,          1, rh - 2*r, c); // left
        fillRect(fb, w, h, x0 + rw - 1 - t,  y0 + r,          1, rh - 2*r, c); // right
    }
    // Corners: walk the same per-row margin() used by fillRoundRect below and
    // paint `thick` pixels starting right at that margin, so the border sits
    // flush against the fill's own corner curve rather than being derived
    // from a circle centred a half-pixel off (which used to leave stray
    // background-coloured gaps speckled into the corner).
    for (int dy = 0; dy < r; ++dy)
    {
        int margin = r - (int)sqrtf((float)(r*r - (r - dy)*(r - dy)));
        for (int t = 0; t < thick && margin + t < rw - margin - t; ++t)
        {
            int dx = margin + t;
            drawPixel(fb, w, h, x0 + dx,          y0 + dy,          c); // TL
            drawPixel(fb, w, h, x0 + rw - 1 - dx, y0 + dy,          c); // TR
            drawPixel(fb, w, h, x0 + dx,          y0 + rh - 1 - dy, c); // BL
            drawPixel(fb, w, h, x0 + rw - 1 - dx, y0 + rh - 1 - dy, c); // BR
        }
    }
}

void fillRoundRect(uint8_t *fb, int w, int h,
                   int x0, int y0, int rw, int rh, int r, Color c)
{
    if (rw <= 0 || rh <= 0) return;
    if (r * 2 > rw) r = rw / 2;
    if (r * 2 > rh) r = rh / 2;
    // The straight middle band is one run; only the rounded caps need a
    // per-row margin.
    if (rh > 2 * r)
        fillRect(fb, w, h, x0, y0 + r, rw, rh - 2 * r, c);
    for (int dy = 0; dy < r; ++dy)
    {
        int margin = r - (int)sqrtf((float)(r*r - (r - dy)*(r - dy)));
        fillRect(fb, w, h, x0 + margin, y0 + dy,          rw - 2*margin, 1, c);
        fillRect(fb, w, h, x0 + margin, y0 + rh - 1 - dy, rw - 2*margin, 1, c);
    }
}

void fillCircle(uint8_t *fb, int w, int h, int cx, int cy, int radius, Color c)
{
    if (radius <= 0) return;
    for (int dy = -radius; dy <= radius; ++dy)
    {
        int half = (int)sqrtf((float)(radius * radius - dy * dy));
        fillRect(fb, w, h, cx - half, cy + dy, half * 2 + 1, 1, c);
    }
}

Color darken(Color c, float amount)
{
    if (amount < 0.0f) amount = 0.0f;
    if (amount > 1.0f) amount = 1.0f;
    return Color{
        (uint8_t)(c.r * (1.0f - amount)),
        (uint8_t)(c.g * (1.0f - amount)),
        (uint8_t)(c.b * (1.0f - amount))
    };
}

Color mix(Color a, Color b, float amount)
{
    if (amount < 0.0f) amount = 0.0f;
    if (amount > 1.0f) amount = 1.0f;
    return Color{
        (uint8_t)(a.r + (b.r - a.r) * amount),
        (uint8_t)(a.g + (b.g - a.g) * amount),
        (uint8_t)(a.b + (b.b - a.b) * amount)
    };
}

// Keeps a *fixed* ink — the error red, the success green, a button's own
// label colour — legible against whatever surface the current theme has put
// behind it. On a dark face the ink is lifted towards white; on a light one
// it is handed back untouched, so nothing about the light themes moves.
// Without this, Sign out (dark red on the theme's card colour) is unreadable
// the moment a theme makes that card dark, as Midnight and Connect 4 do.
Color inkOn(Color surface, Color ink)
{
    const float luma = (0.299f * surface.r + 0.587f * surface.g + 0.114f * surface.b) / 255.0f;
    return luma < 0.5f ? mix(ink, Color{255, 255, 255}, 0.55f) : ink;
}

// Resolved fresh on every lookup rather than cached, because the whole point
// is that a theme swap takes effect on the next frame with nothing to
// invalidate. See the Role comment in render.h.
Color roleColor(Role r)
{
    switch (r)
    {
    case Role::Bg:         return C_BG;
    case Role::BgLight:    return C_BG_LIGHT;
    case Role::BgDark:     return C_BG_DARK;
    case Role::Text:       return C_TEXT;
    case Role::TextSoft:   return C_TEXT_SOFT;
    case Role::Primary:    return C_PRIMARY;
    case Role::PrimaryTxt: return C_PRIMARY_TXT;
    case Role::PrimaryDk:  return C_PRIMARY_DK;
    case Role::Accent:     return C_ACCENT;
    case Role::AccentTxt:  return C_ACCENT_TXT;
    case Role::AccentDk:   return C_ACCENT_DK;
    case Role::FixedTxt:   return C_FIXED_TXT;
    case Role::Success:    return C_SUCCESS;
    case Role::Error:      return C_ERROR;
    case Role::ErrorDk:    return C_ERROR_DK;
    case Role::Purple:     return C_PURPLE;
    case Role::PurpleDk:   return C_PURPLE_DK;
    }
    return C_TEXT;
}

void fillRoundRectAccented(uint8_t *fb, int w, int h,
                           int x0, int y0, int rw, int rh, int r,
                           Color cardColor, Color accentColor, int accentPx)
{
    if (accentPx <= 0)
    {
        fillRoundRect(fb, w, h, x0, y0, rw, rh, r, cardColor);
        return;
    }
    fillRoundRect(fb, w, h, x0, y0, rw, rh, r, accentColor);
    fillRoundRect(fb, w, h, x0, y0, rw, rh - accentPx, r, cardColor);
}

// ---------------------------------------------------------------------------
// Text
// ---------------------------------------------------------------------------
void drawChar(uint8_t *fb, int fbW, int fbH, int x, int y,
              char ch, int scale, Color fg, Color bg, bool hasBg)
{
    if (ch < 32 || ch > 126) ch = 63; // '?'
    // Wholly off screen — bail before touching a single pixel. Text drawn at
    // an animated offset spends most of a transition out here.
    if (x + FONT_W * scale <= 0 || x >= fbW || y + FONT_H * scale <= 0 || y >= fbH) return;
    const uint8_t *glyph = FONT8[(uint8_t)ch - 32];
    for (int row = 0; row < 8; ++row)
    {
        for (int col = 0; col < 8; ++col)
        {
            bool set = (glyph[row] >> col) & 1;
            if (!set && !hasBg) continue;
            Color c = set ? fg : bg;
            if (scale == 1)
                drawPixel(fb, fbW, fbH, x + col, y + row, c);
            else
                fillRect(fb, fbW, fbH, x + col*scale, y + row*scale, scale, scale, c);
        }
    }
}

int drawText(uint8_t *fb, int fbW, int fbH, int x, int y,
             const char *str, int scale, Color fg, Color bg, bool hasBg)
{
    if (!str) return x;
    while (*str)
    {
        drawChar(fb, fbW, fbH, x, y, *str, scale, fg, bg, hasBg);
        x += FONT_W * scale;
        ++str;
    }
    return x;
}

int drawTextBold(uint8_t *fb, int fbW, int fbH, int x, int y,
                 const char *str, int scale, Color fg)
{
    drawText(fb, fbW, fbH, x + 1, y, str, scale, fg);
    return drawText(fb, fbW, fbH, x, y, str, scale, fg);
}

int textWidth(const char *str, int scale)
{
    return str ? (int)strlen(str) * FONT_W * scale : 0;
}

// Single shared word-wrap walk. `fb` may be null, in which case nothing is
// drawn and only the resulting height is measured — that keeps a paragraph's
// measured bounding box and its rendering from ever disagreeing.
static int wrapWalk(uint8_t *fb, int fbW, int fbH,
                    int x0, int y0, int maxW,
                    const char *str, int scale, Color fg)
{
    if (!str || !*str) return 0;
    const int charW = FONT_W * scale;
    const int lineH = LINE_H * scale;
    int cx = x0, cy = y0;
    int lines = 1;

    char word[128];
    while (*str)
    {
        if (*str == 32)  { if (cx > x0) cx += charW; ++str; continue; }  // space
        if (*str == 10)  { cx = x0; cy += lineH; ++lines; ++str; continue; }  // newline

        int wlen = 0;
        while (*str && *str != 32 && *str != 10 && wlen < (int)sizeof(word) - 1)
            word[wlen++] = *str++;
        word[wlen] = 0;

        int wordPx = wlen * charW;
        if (cx > x0 && cx + wordPx > x0 + maxW) { cx = x0; cy += lineH; ++lines; }
        if (fb)
        {
            for (int i = 0; i < wlen; ++i)
            {
                drawChar(fb, fbW, fbH, cx, cy, word[i], scale, fg, C_BG, false);
                cx += charW;
            }
        }
        else
            cx += wordPx;
    }
    return (lines - 1) * lineH + FONT_H * scale;
}

void drawTextWrapped(uint8_t *fb, int fbW, int fbH,
                     int x0, int y0, int maxW,
                     const char *str, int scale, Color fg)
{
    wrapWalk(fb, fbW, fbH, x0, y0, maxW, str, scale, fg);
}

int textWrapHeight(const char *str, int scale, int maxW)
{
    return wrapWalk(nullptr, 0, 0, 0, 0, maxW, str, scale, C_TEXT);
}

// ---------------------------------------------------------------------------
// QR code
// ---------------------------------------------------------------------------
static constexpr int QR_QUIET = 4;

int qrPixelSize(const uint8_t *qrcode, int pixel)
{
    if (!qrcode) return 0;
    return (qrcodegen_getSize(qrcode) + QR_QUIET * 2) * pixel;
}

void renderQRAt(uint8_t *fb, int fbW, int fbH, int x0, int y0,
                const uint8_t *qrcode, int pixel)
{
    if (!qrcode) return;
    const int size    = qrcodegen_getSize(qrcode);
    const int totalPx = (size + QR_QUIET * 2) * pixel;

    if (x0 + totalPx <= 0 || x0 >= fbW || y0 + totalPx <= 0 || y0 >= fbH) return;

    fillRect(fb, fbW, fbH, x0, y0, totalPx, totalPx, C_BG_LIGHT);

    const Color black = {0, 0, 0};
    for (int row = 0; row < size; ++row)
    {
        const int py = y0 + (row + QR_QUIET) * pixel;
        for (int col = 0; col < size; ++col)
        {
            if (!qrcodegen_getModule(qrcode, col, row)) continue;
            fillRect(fb, fbW, fbH, x0 + (col + QR_QUIET) * pixel, py, pixel, pixel, black);
        }
    }
}
