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
// Colour type, runtime palette, and the fixed colours themes never touch
// ---------------------------------------------------------------------------
struct Color { uint8_t r, g, b; };

// The themed half of the palette. theme.cpp swaps this wholesale when the
// player picks a theme, and every screen reads it through the C_* names
// below — so a theme change recolours the whole client on the next frame
// without a single widget having to know themes exist.
struct Palette
{
    Color bg, bgLight, bgDark;
    Color text, textSoft;
    Color primary, primaryTxt, primaryDk, primarySoft;
    Color accent, accentTxt, accentDk;
    Color boardLight, boardDark, boardBorder;
};

extern Palette gPalette;   // starts out as the website's "Wooden (Classic)"

#define C_BG           (gPalette.bg)
#define C_BG_LIGHT     (gPalette.bgLight)
#define C_BG_DARK      (gPalette.bgDark)
#define C_TEXT         (gPalette.text)
// The website's 60%-opacity body copy.
#define C_TEXT_SOFT    (gPalette.textSoft)
#define C_PRIMARY      (gPalette.primary)
#define C_PRIMARY_TXT  (gPalette.primaryTxt)
// The `border-b-8` shade under a raised primary card.
#define C_PRIMARY_DK   (gPalette.primaryDk)
// Muted label on top of a primary-coloured bar — a theme's accent is not
// guaranteed to be readable there (Midnight's is not), so this is derived
// from the pair that is: primaryTxt faded towards primary.
#define C_PRIMARY_SOFT (gPalette.primarySoft)
#define C_ACCENT       (gPalette.accent)
// Label colour for anything sitting *on* an accent fill. Not the same as
// C_TEXT: on a dark theme the body text is near-white and the accent is a
// light grey, so reusing C_TEXT there would be invisible.
#define C_ACCENT_TXT   (gPalette.accentTxt)
#define C_ACCENT_DK    (gPalette.accentDk)
#define C_BOARD_LIGHT  (gPalette.boardLight)
#define C_BOARD_DARK   (gPalette.boardDark)
#define C_BOARD_BORDER (gPalette.boardBorder)

// Fixed colours. These stay put across every theme because they do on the
// website too: its status greens/reds and the purple Tutorial card are plain
// Tailwind classes rather than CSS variables.
static constexpr Color C_SUCCESS    = { 34, 139,  34};
static constexpr Color C_ERROR      = {180,  30,  30};
static constexpr Color C_ERROR_DK   = {140,  20,  20};
static constexpr Color C_PURPLE     = {139,  92, 246};
static constexpr Color C_PURPLE_DK  = { 88,  55, 168};
// "Look here" ring used by the tutorial to call out a square without
// implying it is actually selected (that's C_SELECTED, in screens.cpp).
static constexpr Color C_HINT       = {249, 115,  22};
// Label colour for anything drawn on one of those. They are all dark, and a
// theme's own primaryTxt only has to work on its primary — Midnight's is
// near-black, which on the purple Tutorial card would be unreadable.
static constexpr Color C_FIXED_TXT  = {255, 255, 255};

// Piece tokens are the *skin*, not the theme — on the website they are
// black/white SVGs that every theme draws unchanged — so they keep their own
// colours. Deriving them from the palette would sink the black piece into
// Midnight's near-white body text.
static constexpr Color C_PIECE_W     = {250, 250, 248};
static constexpr Color C_PIECE_B     = { 42,  38,  34};
static constexpr Color C_PIECE_RIM   = { 24,  21,  18};
static constexpr Color C_PIECE_GLINT = {214, 210, 202};

// ---------------------------------------------------------------------------
// Palette roles
// ---------------------------------------------------------------------------
// The shared button table in ui.h is built at compile time, but a theme swap
// has to be able to recolour every control on screen — so a Button names the
// palette *role* it wants and the actual Color is resolved when it is drawn.
enum class Role : uint8_t
{
    Bg, BgLight, BgDark, Text, TextSoft,
    Primary, PrimaryTxt, PrimaryDk,
    Accent, AccentTxt, AccentDk,
    FixedTxt,
    Success, Error, ErrorDk, Purple, PurpleDk,
};

Color roleColor(Role r);

// Lifts `ink` towards white when `surface` is dark, so a fixed colour stays
// readable on a themed background. A no-op on light surfaces.
Color inkOn(Color surface, Color ink);

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
