/*
 * render.h — drawing primitives, 8x8 bitmap font, and QR rendering.
 * All functions operate directly on raw libctru framebuffers (column-major BGR).
 *
 * Everything here clips silently against the framebuffer bounds, which is what
 * lets uikit.cpp draw a widget at an arbitrary animated offset — including
 * mostly or entirely off the edge of the screen — without any special casing.
 */
#ifndef RENDER_H
#define RENDER_H

#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <cstring>

// ---------------------------------------------------------------------------
// Screen constants
// ---------------------------------------------------------------------------
static constexpr int TOP_W = 400;
static constexpr int TOP_H = 240;
static constexpr int BOT_W = 320;
static constexpr int BOT_H = 240;

// ---------------------------------------------------------------------------
// Colour type and named palette  ("Wooden / Classic")
// ---------------------------------------------------------------------------
struct Color { uint8_t r, g, b; };

static constexpr Color C_BG         = {244, 241, 234};
static constexpr Color C_BG_LIGHT   = {255, 255, 255};
static constexpr Color C_BG_DARK    = {227, 217, 198};
static constexpr Color C_TEXT       = { 74,  55,  40};
static constexpr Color C_PRIMARY    = {139,  69,  19};
static constexpr Color C_PRIMARY_TXT= {255, 255, 255};
static constexpr Color C_ACCENT     = {210, 180, 140};
static constexpr Color C_SUCCESS    = { 34, 139,  34};
static constexpr Color C_ERROR      = {180,  30,  30};

// Website-derived extras: the `border-b-8` shade under a raised card, the
// 60%-opacity body copy, and the purple used for the Spectate card.
static constexpr Color C_PRIMARY_DK = { 92,  45,  12};
static constexpr Color C_ACCENT_DK  = {160, 132,  96};
static constexpr Color C_TEXT_SOFT  = {143, 126, 110};
static constexpr Color C_PURPLE     = {139,  92, 246};
static constexpr Color C_PURPLE_DK  = { 88,  55, 168};

// Board colours — match the website's default "Wooden (Classic)" theme.
static constexpr Color C_BOARD_LIGHT  = {222, 203, 164};
static constexpr Color C_BOARD_DARK   = {166, 124,  82};
static constexpr Color C_BOARD_BORDER = { 93,  46,  10};

// ---------------------------------------------------------------------------
// Pixel / rect / rounded-rect helpers
// ---------------------------------------------------------------------------
void drawPixel      (uint8_t *fb, int w, int h, int x, int y, Color c);
void fillRect       (uint8_t *fb, int w, int h, int x0, int y0, int rw, int rh, Color c);
void clearScreen    (uint8_t *fb, int w, int h, Color c);
// Blends `c` into the existing pixel (alpha 0 = unchanged, 1 = fully replaced).
void drawPixelBlend (uint8_t *fb, int w, int h, int x, int y, Color c, float alpha);
void fillRectBlend  (uint8_t *fb, int w, int h, int x0, int y0, int rw, int rh, Color c, float alpha);

void drawRoundRect  (uint8_t *fb, int w, int h,
                     int x0, int y0, int rw, int rh, int r, int thick, Color c);
void fillRoundRect  (uint8_t *fb, int w, int h,
                     int x0, int y0, int rw, int rh, int r, Color c);
void fillCircle     (uint8_t *fb, int w, int h, int cx, int cy, int radius, Color c);

// Darkens `c` towards black by `amount` (0 = unchanged, 1 = black).
Color darken(Color c, float amount);
// Mixes `a` towards `b` by `amount` (0 = a, 1 = b).
Color mix(Color a, Color b, float amount);

// A rounded card with a solid-colour accent strip along the bottom `accentPx`
// pixels — the raised, "lifted" card look used throughout the website
// (border-b-8). Pass accentPx = 0 for a flat, flush card (used for the
// pressed/pushed-in state of buttons).
void fillRoundRectAccented(uint8_t *fb, int w, int h,
                           int x0, int y0, int rw, int rh, int r,
                           Color cardColor, Color accentColor, int accentPx);

// ---------------------------------------------------------------------------
// Text
// ---------------------------------------------------------------------------
static constexpr int FONT_W = 8;   // glyph cell width  at scale 1
static constexpr int FONT_H = 8;   // glyph cell height at scale 1
static constexpr int LINE_H = 9;   // wrapped line pitch at scale 1

void drawChar       (uint8_t *fb, int fbW, int fbH, int x, int y,
                     char ch, int scale, Color fg, Color bg, bool hasBg = false);
int  drawText       (uint8_t *fb, int fbW, int fbH, int x, int y,
                     const char *str, int scale, Color fg,
                     Color bg = C_BG, bool hasBg = false);
// Faux-bold heading text (double-struck one pixel-column right) — used for
// the "font-black" headings that match the website's typographic weight.
int  drawTextBold   (uint8_t *fb, int fbW, int fbH, int x, int y,
                     const char *str, int scale, Color fg);
void drawTextWrapped(uint8_t *fb, int fbW, int fbH,
                     int x0, int y0, int maxW,
                     const char *str, int scale, Color fg);
// Pixel width of a single line, and the pixel height drawTextWrapped would
// occupy — the latter is what gives a paragraph an honest bounding box to
// slide around.
int  textWidth      (const char *str, int scale);
int  textWrapHeight (const char *str, int scale, int maxW);

// ---------------------------------------------------------------------------
// QR code (Nayuki qrcodegen)
// ---------------------------------------------------------------------------
// Pixel size the QR will occupy, including its quiet zone, at `pixel` px per
// module — so a caller can lay it out (and animate it) before drawing.
int  qrPixelSize(const uint8_t *qrcode, int pixel);
void renderQRAt (uint8_t *fb, int fbW, int fbH, int x0, int y0,
                 const uint8_t *qrcode, int pixel);

#endif // RENDER_H
