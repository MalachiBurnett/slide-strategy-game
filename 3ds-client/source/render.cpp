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

void fillRect(uint8_t *fb, int w, int h, int x0, int y0, int rw, int rh, Color c)
{
    for (int dy = 0; dy < rh; ++dy)
        for (int dx = 0; dx < rw; ++dx)
            drawPixel(fb, w, h, x0 + dx, y0 + dy, c);
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

void blitShiftedX(uint8_t *dst, const uint8_t *src, int w, int h, int shiftX)
{
    const size_t colBytes = (size_t)h * 3;
    for (int x = 0; x < w; ++x)
    {
        int srcX = x - shiftX;
        if (srcX < 0 || srcX >= w) continue;
        memcpy(dst + (size_t)x * colBytes, src + (size_t)srcX * colBytes, colBytes);
    }
}

void drawRoundRect(uint8_t *fb, int w, int h,
                   int x0, int y0, int rw, int rh, int r, int thick, Color c)
{
    for (int t = 0; t < thick; ++t)
    {
        fillRect(fb, w, h, x0 + r,         y0 + t,            rw - 2*r, 1, c); // top
        fillRect(fb, w, h, x0 + r,         y0 + rh - 1 - t,   rw - 2*r, 1, c); // bottom
        fillRect(fb, w, h, x0 + t,         y0 + r,            1, rh - 2*r, c); // left
        fillRect(fb, w, h, x0 + rw - 1 - t, y0 + r,           1, rh - 2*r, c); // right
    }
    float innerR = (float)(r - thick);
    float outerR = (float)r;
    for (int dy = 0; dy < r; ++dy)
    {
        for (int dx = 0; dx < r; ++dx)
        {
            float cx2 = (float)(r - 1 - dx);
            float cy2 = (float)(r - 1 - dy);
            float dist = sqrtf(cx2*cx2 + cy2*cy2);
            if (dist >= innerR && dist < outerR)
            {
                drawPixel(fb, w, h, x0 + dx,          y0 + dy,          c); // TL
                drawPixel(fb, w, h, x0 + rw - 1 - dx, y0 + dy,          c); // TR
                drawPixel(fb, w, h, x0 + dx,          y0 + rh - 1 - dy, c); // BL
                drawPixel(fb, w, h, x0 + rw - 1 - dx, y0 + rh - 1 - dy, c); // BR
            }
        }
    }
}

void fillRoundRect(uint8_t *fb, int w, int h,
                   int x0, int y0, int rw, int rh, int r, Color c)
{
    for (int dy = 0; dy < rh; ++dy)
    {
        int y = y0 + dy;
        int margin = 0;
        if (dy < r)
            margin = r - (int)sqrtf((float)(r*r - (r - dy)*(r - dy)));
        else if (dy >= rh - r)
            margin = r - (int)sqrtf((float)(r*r - (r - (rh - 1 - dy))*(r - (rh - 1 - dy))));
        fillRect(fb, w, h, x0 + margin, y, rw - 2*margin, 1, c);
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
    if (ch < 32 || ch > 126) ch = '?';
    const uint8_t *glyph = FONT8[(uint8_t)ch - 32];
    for (int row = 0; row < 8; ++row)
    {
        for (int col = 0; col < 8; ++col)
        {
            bool set = (glyph[row] >> col) & 1;
            if (set || hasBg)
            {
                Color c = set ? fg : bg;
                for (int sy = 0; sy < scale; ++sy)
                    for (int sx = 0; sx < scale; ++sx)
                        drawPixel(fb, fbW, fbH, x + col*scale + sx, y + row*scale + sy, c);
            }
        }
    }
}

int drawText(uint8_t *fb, int fbW, int fbH, int x, int y,
             const char *str, int scale, Color fg, Color bg, bool hasBg)
{
    while (*str)
    {
        drawChar(fb, fbW, fbH, x, y, *str, scale, fg, bg, hasBg);
        x += 8 * scale;
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

void drawTextWrapped(uint8_t *fb, int fbW, int fbH,
                     int x0, int y0, int maxW,
                     const char *str, int scale, Color fg)
{
    int charW = 8 * scale;
    int lineH = 9 * scale;
    int cx = x0, cy = y0;

    char word[128];
    while (*str)
    {
        if (*str == ' ')  { if (cx > x0) cx += charW; ++str; continue; }
        if (*str == '\n') { cx = x0; cy += lineH; ++str; continue; }

        int wlen = 0;
        while (*str && *str != ' ' && *str != '\n')
            word[wlen++] = *str++;
        word[wlen] = 0;

        int wordPx = wlen * charW;
        if (cx > x0 && cx + wordPx > x0 + maxW) { cx = x0; cy += lineH; }
        for (int i = 0; i < wlen; ++i) { drawChar(fb, fbW, fbH, cx, cy, word[i], scale, fg, C_BG, false); cx += charW; }
    }
}

// ---------------------------------------------------------------------------
// QR code
// ---------------------------------------------------------------------------
static constexpr int QR_PIXEL = 5;
static constexpr int QR_QUIET = 4;

void renderQR(uint8_t *fb, const uint8_t *qrcode)
{
    int size     = qrcodegen_getSize(qrcode);
    int totalMod = size + QR_QUIET * 2;
    int totalPx  = totalMod * QR_PIXEL;

    int margin  = (TOP_H - totalPx) / 2;
    int originX = TOP_W - totalPx - margin;
    int originY = margin;

    fillRect(fb, TOP_W, TOP_H, originX, originY, totalPx, totalPx, C_BG_LIGHT);

    for (int row = 0; row < totalMod; ++row)
    {
        for (int col = 0; col < totalMod; ++col)
        {
            int qx = col - QR_QUIET, qy = row - QR_QUIET;
            if (qx < 0 || qx >= size || qy < 0 || qy >= size) continue;
            if (!qrcodegen_getModule(qrcode, qx, qy)) continue;

            Color black = {0,0,0};
            int px0 = originX + col * QR_PIXEL;
            int py0 = originY + row * QR_PIXEL;
            for (int dy = 0; dy < QR_PIXEL; ++dy)
                for (int dx = 0; dx < QR_PIXEL; ++dx)
                    drawPixel(fb, TOP_W, TOP_H, px0 + dx, py0 + dy, black);
        }
    }
}
