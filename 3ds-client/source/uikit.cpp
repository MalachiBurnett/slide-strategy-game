/*
 * uikit.cpp — element drawing, scene building, and the screen transition.
 * See uikit.h for the design of the whole thing.
 */
#include "uikit.h"

#include <cstdarg>
#include <cstdio>

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
// The 3DS renders at 60 fps, so a frame is ~16.7 ms. Exit is deliberately
// short and everything leaves together: the old page should get out of the
// way immediately so a press feels acknowledged. Entry is longer, lands with
// a spring, and fans out slightly so the new page assembles instead of
// appearing all at once.
static constexpr int   UI_EXIT_FRAMES  = 9;    // ~150 ms
static constexpr int   UI_ENTER_FRAMES = 15;   // ~250 ms per object
static constexpr int   UI_STAGGER      = 2;    // frames between objects
static constexpr int   UI_STAGGER_MAX  = 8;    // ...capped, so long pages stay snappy
static constexpr float UI_MAX_OVERSHOOT = 10.0f; // px past target, at most

// ---------------------------------------------------------------------------
// Glyphs — 12x12, one bit per pixel, most significant bit leftmost.
// ---------------------------------------------------------------------------
static constexpr int GLYPH_PX = 12;

static const uint16_t GLYPHS[(int)Glyph::COUNT][GLYPH_PX] = {
    { // None
        0,0,0,0,0,0,0,0,0,0,0,0
    },
    { // Play
        0x000, 0x200, 0x300, 0x3C0, 0x3F0, 0x3FC,
        0x3FC, 0x3F0, 0x3C0, 0x300, 0x200, 0x000
    },
    { // Hash
        0x000, 0x090, 0x090, 0x090, 0x7FC, 0x090,
        0x090, 0x7FC, 0x090, 0x090, 0x090, 0x000
    },
    { // Users
        0x000, 0x318, 0x7BC, 0x7BC, 0x318, 0x000,
        0x39C, 0x7FE, 0xFFF, 0xFFF, 0xFFF, 0x000
    },
    { // Eye
        0x000, 0x0F0, 0x30C, 0x462, 0x8F1, 0x8F1,
        0x462, 0x30C, 0x0F0, 0x000, 0x000, 0x000
    },
    { // Zap
        0x0F8, 0x1F0, 0x3E0, 0x7C0, 0x7F8, 0x03C,
        0x078, 0x0F0, 0x1E0, 0x3C0, 0x780, 0x000
    },
    { // Trophy
        0x000, 0xDFB, 0xDFB, 0xFFF, 0xDFB, 0x1F8,
        0x0F0, 0x060, 0x060, 0x1F8, 0x3FC, 0x000
    },
    { // Exit
        0x000, 0x7C0, 0x440, 0x440, 0x448, 0x47C,
        0x47C, 0x448, 0x440, 0x440, 0x7C0, 0x000
    },
    { // Clock
        0x000, 0x1F8, 0x30C, 0x636, 0x432, 0xC33,
        0xC3F, 0x402, 0x606, 0x30C, 0x1F8, 0x000
    },
    { // Grid
        0xFC0, 0xFC0, 0xFC0, 0xFC0, 0xFC0, 0xFC0,
        0x03F, 0x03F, 0x03F, 0x03F, 0x03F, 0x03F
    },
    { // Check
        0x000, 0x006, 0x00E, 0x01C, 0x638, 0xF70,
        0xFE0, 0x7C0, 0x380, 0x000, 0x000, 0x000
    },
    { // Cross
        0x000, 0xC03, 0xE07, 0x70E, 0x39C, 0x1F8,
        0x1F8, 0x39C, 0x70E, 0xE07, 0xC03, 0x000
    },
    { // Person
        0x000, 0x0F0, 0x1F8, 0x1F8, 0x0F0, 0x000,
        0x3FC, 0x7FE, 0xFFF, 0xFFF, 0xFFF, 0x000
    },
    { // Lock
        0x000, 0x1F8, 0x30C, 0x30C, 0x30C, 0xFFF,
        0xFFF, 0xF3F, 0xF3F, 0xFFF, 0xFFF, 0x000
    },
    { // Back
        0x000, 0x060, 0x0C0, 0x180, 0x300, 0x7FE,
        0x7FE, 0x300, 0x180, 0x0C0, 0x060, 0x000
    },
    { // Warn
        0x060, 0x0F0, 0x0F0, 0x0F0, 0x0F0, 0x0F0,
        0x060, 0x060, 0x000, 0x0F0, 0x0F0, 0x000
    },
};

// Paints a glyph centred on (cx, cy) at `scale` px per glyph pixel.
static void drawGlyph(uint8_t *fb, int w, int h, int cx, int cy,
                      Glyph g, int scale, Color c)
{
    if (g == Glyph::None || g >= Glyph::COUNT) return;
    const uint16_t *rows = GLYPHS[(int)g];
    const int side = GLYPH_PX * scale;
    const int x0 = cx - side / 2;
    const int y0 = cy - side / 2;
    if (x0 + side <= 0 || x0 >= w || y0 + side <= 0 || y0 >= h) return;
    for (int r = 0; r < GLYPH_PX; ++r)
    {
        uint16_t bits = rows[r];
        if (!bits) continue;
        int run = 0;
        for (int col = 0; col <= GLYPH_PX; ++col)
        {
            const bool on = col < GLYPH_PX && ((bits >> (GLYPH_PX - 1 - col)) & 1);
            if (on) { ++run; continue; }
            if (run)
            {
                fillRect(fb, w, h, x0 + (col - run) * scale, y0 + r * scale,
                         run * scale, scale, c);
                run = 0;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Scene building
// ---------------------------------------------------------------------------
void uiSceneBegin(UiScene &s, int w, int h, bool topScreen, int32_t key)
{
    s.key       = key;
    s.w         = (int16_t)w;
    s.h         = (int16_t)h;
    s.topScreen = topScreen;
    s.count     = 0;
    s.poolUsed  = 0;
    s.nextRank  = 0;
    s.curGroup  = 0;
    s.nextGroup = 1;
    for (int i = 0; i < UI_MAX_GROUPS; ++i) s.groupRank[i] = -1;
}

const char *uiStr(UiScene &s, const char *fmt, ...)
{
    const int space = UI_TEXT_POOL - s.poolUsed;
    if (space <= 1) return "";
    char *dst = s.pool + s.poolUsed;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(dst, (size_t)space, fmt, ap);
    va_end(ap);
    if (n < 0) { dst[0] = 0; n = 0; }
    if (n >= space) n = space - 1;
    s.poolUsed = (int16_t)(s.poolUsed + n + 1);
    return dst;
}

void uiGroupBegin(UiScene &s)
{
    // Out of group slots: fall back to every element moving alone, which is
    // ugly but never wrong — reusing a live id would drag unrelated widgets
    // around together.
    s.curGroup = (s.nextGroup < UI_MAX_GROUPS) ? s.nextGroup++ : 0;
}

void uiGroupEnd(UiScene &s)
{
    s.curGroup = 0;
}

// Every element gets a rank: its position in the fan-out during ENTER.
// Grouped elements share the rank of whichever peer was pushed first, so a
// card and its contents arrive together.
static UiElem &push(UiScene &s, ElemKind kind, int x, int y, int w, int h)
{
    static UiElem sink;   // overflow lands here and is never drawn
    UiElem *e = &sink;
    if (s.count < UI_MAX_ELEMS) e = &s.elems[s.count++];

    *e = UiElem{};
    e->kind   = kind;
    e->group  = s.curGroup;
    e->glyph  = (uint8_t)Glyph::None;
    e->scale  = 1;
    e->x = (int16_t)x; e->y = (int16_t)y;
    e->w = (int16_t)w; e->h = (int16_t)h;
    e->fg = C_TEXT; e->bg = C_BG_LIGHT; e->edge = C_ACCENT;

    if (s.curGroup && s.groupRank[s.curGroup] >= 0)
        e->rank = s.groupRank[s.curGroup];
    else
    {
        e->rank = s.nextRank++;
        if (s.curGroup) s.groupRank[s.curGroup] = (int8_t)e->rank;
    }
    return *e;
}

UiElem &uiRaw(UiScene &s, ElemKind kind, int x, int y, int w, int h)
{
    return push(s, kind, x, y, w, h);
}

UiElem &uiPanel(UiScene &s, int x, int y, int w, int h, int radius,
                Color bg, Color accentColor, int accentPx)
{
    UiElem &e = push(s, ElemKind::Panel, x, y, w, h);
    e.radius = (int16_t)radius;
    e.bg     = bg;
    e.edge   = accentColor;
    e.accent = (int16_t)accentPx;
    return e;
}

UiElem &uiText(UiScene &s, int x, int y, const char *t, int scale, Color fg,
               uint16_t flags)
{
    UiElem &e = push(s, ElemKind::Text, x, y, textWidth(t, scale), FONT_H * scale);
    e.text  = t;
    e.scale = (int8_t)scale;
    e.fg    = fg;
    e.flags = flags;
    return e;
}

UiElem &uiTextIn(UiScene &s, int x, int y, int w, const char *t, int scale,
                 Color fg, uint16_t flags)
{
    UiElem &e = push(s, ElemKind::Text, x, y, w, FONT_H * scale);
    e.text  = t;
    e.scale = (int8_t)scale;
    e.fg    = fg;
    e.flags = flags;
    return e;
}

UiElem &uiWrap(UiScene &s, int x, int y, int w, const char *t, int scale, Color fg)
{
    UiElem &e = push(s, ElemKind::TextWrap, x, y, w, textWrapHeight(t, scale, w));
    e.text  = t;
    e.scale = (int8_t)scale;
    e.fg    = fg;
    return e;
}

UiElem &uiButton(UiScene &s, int x, int y, int w, int h, const char *label,
                 Color bg, Color fg, Color edge, bool pressed, bool focused,
                 Glyph glyph)
{
    UiElem &e = push(s, ElemKind::Button, x, y, w, h);
    e.text  = label;
    e.bg    = bg;
    e.fg    = fg;
    e.edge  = edge;
    e.glyph = (uint8_t)glyph;
    e.flags = (uint16_t)((pressed ? EF_PRESSED : 0) | (focused ? EF_FOCUS : 0));
    return e;
}

UiElem &uiPill(UiScene &s, int x, int y, int w, int h, const char *t,
               Color bg, Color fg, uint16_t flags)
{
    UiElem &e = push(s, ElemKind::Pill, x, y, w, h);
    e.text  = t;
    e.bg    = bg;
    e.fg    = fg;
    e.flags = flags;
    return e;
}

UiElem &uiIcon(UiScene &s, int x, int y, int size, Glyph g, Color bg, Color fg)
{
    UiElem &e = push(s, ElemKind::Icon, x, y, size, size);
    e.glyph  = (uint8_t)g;
    e.bg     = bg;
    e.fg     = fg;
    e.radius = (int16_t)(size / 3);
    return e;
}

// ---------------------------------------------------------------------------
// Element drawing
// ---------------------------------------------------------------------------
static UiCustomDraw g_customDraw = nullptr;

void uiSetCustomDraw(UiCustomDraw fn) { g_customDraw = fn; }

// Where a line of text starts inside its box, honouring the alignment flags.
static int alignedTextX(const UiElem &e, int x)
{
    const int tw = textWidth(e.text, e.scale);
    if (e.flags & EF_CENTER) return x + (e.w - tw) / 2;
    if (e.flags & EF_RIGHT)  return x + e.w - tw;
    return x;
}

static void drawButtonElem(uint8_t *fb, int w, int h, const UiElem &e, int x, int y)
{
    const bool pressed = (e.flags & EF_PRESSED) != 0;
    const bool focused = (e.flags & EF_FOCUS) != 0;
    const Color bg   = pressed ? mix(e.bg, C_SUCCESS, 0.75f) : e.bg;
    const Color fg   = pressed ? C_PRIMARY_TXT : e.fg;
    const int   r    = e.h >= 30 ? 12 : 8;
    // Raised while idle — the website's border-b-8 card — and flush when
    // pressed, so the button reads as physically pushed in rather than
    // merely recoloured.
    const int accentPx = pressed ? 0 : (e.h >= 30 ? 4 : 3);
    const int faceH    = e.h - accentPx;

    // Focus reads mostly off the colour change, so its ring only needs to be
    // a touch heavier than the resting outline.
    fillRoundRectAccented(fb, w, h, x, y, e.w, e.h, r, bg, darken(bg, 0.35f), accentPx);
    if (focused)
        drawRoundRect(fb, w, h, x, y, e.w, e.h, r, 4, C_SUCCESS);
    else if (!pressed)
        drawRoundRect(fb, w, h, x, y, e.w, e.h, r, 2, e.edge);

    const Glyph g       = (Glyph)e.glyph;
    const int   gScale  = e.h >= 30 ? 2 : 1;
    const int   gSide   = (g == Glyph::None) ? 0 : GLYPH_PX * gScale;
    const int   gap     = gSide ? 6 : 0;
    const int   labelW  = textWidth(e.text, 1);
    const int   contentW = gSide + gap + labelW;
    int cx = x + (e.w - contentW) / 2;
    const int cy = y + faceH / 2;

    if (gSide)
    {
        drawGlyph(fb, w, h, cx + gSide / 2, cy, g, gScale, fg);
        cx += gSide + gap;
    }
    drawTextBold(fb, w, h, cx, cy - FONT_H / 2, e.text, 1, fg);
}

void uiDrawElem(uint8_t *fb, int w, int h, const UiElem &e)
{
    const int x = e.x + (int)(e.ox >= 0 ? e.ox + 0.5f : e.ox - 0.5f);
    const int y = e.y + (int)(e.oy >= 0 ? e.oy + 0.5f : e.oy - 0.5f);

    // Cheap reject for anything fully off screen — during a transition that
    // is most of the scene, most of the time.
    if (x + e.w <= 0 || x >= w || y + e.h <= 0 || y >= h) return;

    switch (e.kind)
    {
    case ElemKind::Panel:
        // For a panel `bg` fills it, `edge` is the bottom accent strip, `fg`
        // strokes the outline, and `data` is how thick that stroke is.
        fillRoundRectAccented(fb, w, h, x, y, e.w, e.h, e.radius, e.bg, e.edge, e.accent);
        if (e.flags & EF_BORDER)
            drawRoundRect(fb, w, h, x, y, e.w, e.h, e.radius, e.data > 0 ? e.data : 1, e.fg);
        break;

    case ElemKind::Text:
        if (e.flags & EF_BOLD)
            drawTextBold(fb, w, h, alignedTextX(e, x), y, e.text, e.scale, e.fg);
        else
            drawText(fb, w, h, alignedTextX(e, x), y, e.text, e.scale, e.fg);
        break;

    case ElemKind::TextWrap:
        drawTextWrapped(fb, w, h, x, y, e.w, e.text, e.scale, e.fg);
        break;

    case ElemKind::Button:
        drawButtonElem(fb, w, h, e, x, y);
        break;

    case ElemKind::Pill:
    {
        const int r = e.h / 2;
        fillRoundRect(fb, w, h, x, y, e.w, e.h, r, e.bg);
        if (e.flags & EF_BORDER)
            drawRoundRect(fb, w, h, x, y, e.w, e.h, r, 1, e.edge);
        if (e.text && e.text[0])
        {
            const int tx = x + (e.w - textWidth(e.text, e.scale)) / 2;
            const int ty = y + (e.h - FONT_H * e.scale) / 2;
            if (e.flags & EF_BOLD)
                drawTextBold(fb, w, h, tx, ty, e.text, e.scale, e.fg);
            else
                drawText(fb, w, h, tx, ty, e.text, e.scale, e.fg);
        }
        break;
    }

    case ElemKind::Icon:
    {
        fillRoundRect(fb, w, h, x, y, e.w, e.h, e.radius, e.bg);
        const int scale = e.w >= 28 ? 2 : 1;
        drawGlyph(fb, w, h, x + e.w / 2, y + e.h / 2, (Glyph)e.glyph, scale, e.fg);
        break;
    }

    case ElemKind::Qr:
        renderQRAt(fb, w, h, x, y, (const uint8_t *)e.ptr, e.data);
        break;

    case ElemKind::Custom:
        if (g_customDraw) g_customDraw(fb, w, h, e, x, y);
        break;
    }
}

// ---------------------------------------------------------------------------
// Transition
// ---------------------------------------------------------------------------
namespace
{

enum class Edge : uint8_t { Left, Right, Up, Down };

struct Box { int x, y, w, h; };

// Accelerating departure.
float easeInQuad(float t) { return t * t; }

// Arrival: the step response of an underdamped spring. Starts from rest,
// accelerates, overshoots by about 5%, and settles — which is exactly the
// "speeds up, tips past, comes back" feel we want, and unlike an ease-back
// curve it never starts by moving the wrong way.
float springStep(float t)
{
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    constexpr float a = 8.0f, b = 8.5f;
    return 1.0f - expf(-a * t) * (cosf(b * t) + (a / b) * sinf(b * t));
}

// The edge an object leaves by, or arrives from: whichever is nearest, out of
// the three this screen is allowed to use. The screens face each other across
// the hinge, so the top screen never uses its bottom edge and the bottom
// screen never uses its top edge — moving that way would read as sliding onto
// the other display. Ties go left when leaving and right when arriving, so a
// page change reads as one consistent left-to-right sweep.
Edge pickEdge(const Box &b, int W, int H, bool topScreen, bool entering)
{
    const float cx = b.x + b.w * 0.5f;
    const float cy = b.y + b.h * 0.5f;
    const float dL = cx;
    const float dR = (float)W - cx;
    const Edge  vEdge = topScreen ? Edge::Up : Edge::Down;
    const float dV    = topScreen ? cy : (float)H - cy;

    Edge  cand[3];
    float dist[3];
    if (entering) { cand[0] = Edge::Right; dist[0] = dR; cand[1] = Edge::Left;  dist[1] = dL; }
    else          { cand[0] = Edge::Left;  dist[0] = dL; cand[1] = Edge::Right; dist[1] = dR; }
    cand[2] = vEdge; dist[2] = dV;

    int best = 0;
    for (int i = 1; i < 3; ++i)
        if (dist[i] < dist[best]) best = i;   // strict, so ties keep the earlier candidate
    return cand[best];
}

// Offset that puts `b` completely past `edge`, with a little clearance.
void offscreenOffset(const Box &b, Edge edge, int W, int H, float &ox, float &oy)
{
    constexpr int CLEAR = 6;
    ox = oy = 0.0f;
    switch (edge)
    {
    case Edge::Left:  ox = -(float)(b.x + b.w + CLEAR); break;
    case Edge::Right: ox =  (float)(W - b.x + CLEAR);   break;
    case Edge::Up:    oy = -(float)(b.y + b.h + CLEAR); break;
    case Edge::Down:  oy =  (float)(H - b.y + CLEAR);   break;
    }
}

// Combined bounding box per group, so composites move as one object.
void groupBoxes(const UiScene &s, Box *boxes, bool *used)
{
    for (int g = 0; g < UI_MAX_GROUPS; ++g) used[g] = false;
    for (int i = 0; i < s.count; ++i)
    {
        const UiElem &e = s.elems[i];
        if (!e.group || e.group >= UI_MAX_GROUPS) continue;
        Box &b = boxes[e.group];
        if (!used[e.group])
        {
            b.x = e.x; b.y = e.y; b.w = e.w; b.h = e.h;
            used[e.group] = true;
            continue;
        }
        const int x0 = b.x < e.x ? b.x : e.x;
        const int y0 = b.y < e.y ? b.y : e.y;
        const int x1 = (b.x + b.w) > (e.x + e.w) ? (b.x + b.w) : (e.x + e.w);
        const int y1 = (b.y + b.h) > (e.y + e.h) ? (b.y + b.h) : (e.y + e.h);
        b.x = x0; b.y = y0; b.w = x1 - x0; b.h = y1 - y0;
    }
}

Box boxOf(const UiElem &e, const Box *gb, const bool *gu)
{
    if (e.group && e.group < UI_MAX_GROUPS && gu[e.group]) return gb[e.group];
    Box b; b.x = e.x; b.y = e.y; b.w = e.w; b.h = e.h;
    return b;
}

void applyExit(UiScene &s, float t)
{
    Box gb[UI_MAX_GROUPS]; bool gu[UI_MAX_GROUPS];
    groupBoxes(s, gb, gu);
    const float e = easeInQuad(t);
    for (int i = 0; i < s.count; ++i)
    {
        UiElem &el = s.elems[i];
        const Box b  = boxOf(el, gb, gu);
        const Edge ed = pickEdge(b, s.w, s.h, s.topScreen, false);
        float tx, ty;
        offscreenOffset(b, ed, s.w, s.h, tx, ty);
        // Interpolating from wherever the object already is (rather than from
        // zero) means a page change that lands mid-transition picks up
        // smoothly instead of snapping back.
        el.ox = el.sx + (tx - el.sx) * e;
        el.oy = el.sy + (ty - el.sy) * e;
    }
}

void applyEnter(UiScene &s, int frame)
{
    Box gb[UI_MAX_GROUPS]; bool gu[UI_MAX_GROUPS];
    groupBoxes(s, gb, gu);
    for (int i = 0; i < s.count; ++i)
    {
        UiElem &el = s.elems[i];
        const Box  b  = boxOf(el, gb, gu);
        const Edge ed = pickEdge(b, s.w, s.h, s.topScreen, true);
        float sx, sy;
        offscreenOffset(b, ed, s.w, s.h, sx, sy);

        int delay = el.rank * UI_STAGGER;
        if (delay > UI_STAGGER_MAX) delay = UI_STAGGER_MAX;
        float t = (float)(frame - delay) / (float)UI_ENTER_FRAMES;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;

        const float p = springStep(t);
        el.ox = sx * (1.0f - p);
        el.oy = sy * (1.0f - p);
        // The spring's overshoot is a fraction of the distance travelled, so
        // cap how far past the target a long flight is allowed to tip.
        if (sx < 0.0f && el.ox >  UI_MAX_OVERSHOOT) el.ox =  UI_MAX_OVERSHOOT;
        if (sx > 0.0f && el.ox < -UI_MAX_OVERSHOOT) el.ox = -UI_MAX_OVERSHOOT;
        if (sy < 0.0f && el.oy >  UI_MAX_OVERSHOOT) el.oy =  UI_MAX_OVERSHOOT;
        if (sy > 0.0f && el.oy < -UI_MAX_OVERSHOOT) el.oy = -UI_MAX_OVERSHOOT;
        el.sx = sx;
        el.sy = sy;
    }
}

void settle(UiScene &s)
{
    for (int i = 0; i < s.count; ++i)
    {
        s.elems[i].ox = s.elems[i].oy = 0.0f;
        s.elems[i].sx = s.elems[i].sy = 0.0f;
    }
}

// Copying a scene copies its string pool byte for byte, but the elements'
// text pointers still aim at the *source* pool — and the source is a scratch
// scene that gets rebuilt from scratch every frame. An exiting page has to
// outlive that, so re-anchor anything that pointed into the old pool at the
// matching offset in our own. String literals live in .rodata and are left
// alone.
void rebaseText(UiScene &dst, const UiScene &src)
{
    const uintptr_t lo = (uintptr_t)src.pool;
    const uintptr_t hi = lo + UI_TEXT_POOL;
    for (int i = 0; i < dst.count; ++i)
    {
        const uintptr_t t = (uintptr_t)dst.elems[i].text;
        if (t >= lo && t < hi)
            dst.elems[i].text = dst.pool + (t - lo);
    }
}

} // namespace

bool uiAnimInputBlocked(const UiAnim &a)
{
    if (a.phase == UiPhase::Exit)  return true;
    if (a.phase == UiPhase::Enter) return a.frame <= (UI_ENTER_FRAMES * 2) / 3;
    return false;
}

void uiAnimInit(UiAnim &a)
{
    a.phase   = UiPhase::Idle;
    a.started = false;
    a.frame   = 0;
    a.key     = 0;
    a.shown.count = 0;
}

bool uiAnimRun(UiAnim &a, const UiScene &cur, uint8_t *fb, Color background)
{
    if (!a.started)
    {
        a.shown = cur;
        rebaseText(a.shown, cur);
        settle(a.shown);
        a.key     = cur.key;
        a.phase   = UiPhase::Idle;
        a.frame   = 0;
        a.started = true;
    }
    else if (cur.key != a.key)
    {
        a.key = cur.key;
        // Push out whatever is currently on the glass, starting from exactly
        // where it is right now.
        for (int i = 0; i < a.shown.count; ++i)
        {
            a.shown.elems[i].sx = a.shown.elems[i].ox;
            a.shown.elems[i].sy = a.shown.elems[i].oy;
        }
        a.phase = UiPhase::Exit;
        a.frame = 0;
    }

    if (a.phase == UiPhase::Exit)
    {
        ++a.frame;
        const float t = (float)a.frame / (float)UI_EXIT_FRAMES;
        if (t >= 1.0f)
        {
            // Hand straight over to the incoming page in the same frame —
            // no blank hold, which is what made the old transition drag.
            a.phase = UiPhase::Enter;
            a.frame = 0;
        }
        else
            applyExit(a.shown, t);
    }

    if (a.phase == UiPhase::Enter)
    {
        // Re-copy every frame so live content (status lines, clocks) stays
        // current while the page is still flying in.
        a.shown = cur;
        rebaseText(a.shown, cur);
        applyEnter(a.shown, a.frame);
        ++a.frame;
        if (a.frame > UI_ENTER_FRAMES + UI_STAGGER_MAX)
        {
            a.phase = UiPhase::Idle;
            settle(a.shown);
        }
    }
    else if (a.phase == UiPhase::Idle)
    {
        a.shown = cur;
        rebaseText(a.shown, cur);
        settle(a.shown);
    }

    clearScreen(fb, a.shown.w, a.shown.h, background);
    for (int i = 0; i < a.shown.count; ++i)
        uiDrawElem(fb, a.shown.w, a.shown.h, a.shown.elems[i]);

    return a.phase != UiPhase::Idle;
}
