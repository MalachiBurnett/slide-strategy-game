/*
 * uikit.h — retained-mode UI elements, scenes, and the screen transition.
 *
 * The old transition worked on pixels: each screen was carved into rectangles
 * and the framebuffer was blitted around in strips. That could only ever move
 * whatever happened to be inside a rectangle, needed a mask buffer to stop
 * widgets ghosting, and tore whenever a zone boundary cut through something.
 *
 * This replaces it with actual objects. Every button, line of text, icon,
 * card and board is a UiElem with its own layout rectangle; a screen is a
 * UiScene, a flat list of them rebuilt from application state every frame.
 * Nothing is ever blitted: an element is simply *drawn* at its layout
 * position plus an animated offset, so it can sit anywhere on (or off) the
 * screen with no artifacts, and the two screens animate independently.
 *
 * Transition, per screen, when that screen's scene key changes:
 *
 *   EXIT   every element currently on screen accelerates towards the edge
 *          nearest its own centre until it is fully off screen. Ties prefer
 *          left over right. The top screen never leaves via its bottom edge
 *          and the bottom screen never leaves via its top edge, since those
 *          two edges face each other across the hinge.
 *
 *   ENTER  every element of the new page flies in from the edge nearest its
 *          target position (ties prefer right over left, same edge rules),
 *          accelerating from rest, overshooting slightly, and settling.
 *
 * Elements that form one visual object — a card and its contents, a bar and
 * its wordmark — share a group id and are moved as a unit using their
 * combined bounding box, so a composite never pulls itself apart.
 */
#ifndef UIKIT_H
#define UIKIT_H

#include "render.h"

// ---------------------------------------------------------------------------
// Elements
// ---------------------------------------------------------------------------
enum class ElemKind : uint8_t
{
    Panel,     // rounded card, optional bottom accent strip and border
    Text,      // one line
    TextWrap,  // word-wrapped paragraph, `w` wide
    Button,    // raised card + centred label, optional leading glyph
    Pill,      // fully rounded chip with a centred label
    Icon,      // rounded icon tile with a glyph
    Qr,        // login QR code (ptr -> qrcodegen data, data = module size)
    Custom,    // app-specific widget, painted by the registered callback
};

// Small 12x12 glyph set, echoing the website's lucide icon tiles.
enum class Glyph : uint8_t
{
    None = 0, Play, Hash, Users, Eye, Zap, Trophy, Exit,
    Clock, Grid, Check, Cross, Person, Lock, Back, Warn,
    COUNT
};

enum : uint16_t
{
    EF_BOLD    = 1u << 0,   // faux-bold text
    EF_CENTER  = 1u << 1,   // centre the text inside `w`
    EF_RIGHT   = 1u << 2,   // right-align the text inside `w`
    EF_BORDER  = 1u << 3,   // stroke the panel/pill with `edge`
    EF_PRESSED = 1u << 4,   // button is being held down
    EF_FOCUS   = 1u << 5,   // button carries the D-pad focus ring
    EF_DIM     = 1u << 6,   // draw at reduced emphasis (inactive tab/pill)
};

struct UiElem
{
    ElemKind kind;
    uint8_t  group;    // 0 = moves alone; >0 = moves with every peer sharing it
    uint8_t  glyph;
    int8_t   scale;
    uint16_t flags;
    int16_t  x, y, w, h;
    int16_t  radius;
    int16_t  accent;   // height of the bottom accent strip, 0 = flat
    int16_t  data;     // kind-specific extra (QR module size, chip colour, ...)
    int16_t  rank;     // stagger order during ENTER
    const char *text;
    const void *ptr;
    Color fg, bg, edge;
    float ox, oy;      // live offset from the layout position
    float sx, sy;      // offset this phase started from
};

// ---------------------------------------------------------------------------
// Scenes
// ---------------------------------------------------------------------------
constexpr int UI_MAX_ELEMS  = 64;
constexpr int UI_TEXT_POOL  = 1024;
constexpr int UI_MAX_GROUPS = 16;

struct UiScene
{
    int32_t key;        // page identity — a change is what triggers a transition
    int16_t w, h;
    bool    topScreen;  // decides which edge is off-limits (see the header note)
    int16_t count;
    int16_t poolUsed;
    int16_t nextRank;
    uint8_t curGroup;
    uint8_t nextGroup;
    int8_t  groupRank[UI_MAX_GROUPS];
    UiElem  elems[UI_MAX_ELEMS];
    char    pool[UI_TEXT_POOL];
};

void        uiSceneBegin(UiScene &s, int w, int h, bool topScreen, int32_t key);
// Copies a formatted string into the scene's own storage, so builders can
// hand elements text that outlives the local buffer it was built in.
const char *uiStr(UiScene &s, const char *fmt, ...);
// Everything pushed between these two calls moves as one object.
void        uiGroupBegin(UiScene &s);
void        uiGroupEnd  (UiScene &s);

UiElem &uiPanel (UiScene &s, int x, int y, int w, int h, int radius,
                 Color bg, Color accentColor, int accentPx);
UiElem &uiText  (UiScene &s, int x, int y, const char *t, int scale, Color fg,
                 uint16_t flags = 0);
// Same, but aligned inside a `w`-wide box (use with EF_CENTER / EF_RIGHT).
UiElem &uiTextIn(UiScene &s, int x, int y, int w, const char *t, int scale,
                 Color fg, uint16_t flags);
UiElem &uiWrap  (UiScene &s, int x, int y, int w, const char *t, int scale, Color fg);
UiElem &uiButton(UiScene &s, int x, int y, int w, int h, const char *label,
                 Color bg, Color fg, Color edge, bool pressed, bool focused,
                 Glyph glyph = Glyph::None);
UiElem &uiPill  (UiScene &s, int x, int y, int w, int h, const char *t,
                 Color bg, Color fg, uint16_t flags = 0);
UiElem &uiIcon  (UiScene &s, int x, int y, int size, Glyph g, Color bg, Color fg);
UiElem &uiRaw   (UiScene &s, ElemKind kind, int x, int y, int w, int h);

void uiDrawElem(uint8_t *fb, int w, int h, const UiElem &e);

// ElemKind::Custom hands the actual painting back to the application, which
// keeps game-specific pixels (the board, piece tokens, the mini preview) out
// of the toolkit while still letting them slide like any other object. `x`/`y`
// are the already-offset screen coordinates to draw at; `e.data` says which
// widget it is.
typedef void (*UiCustomDraw)(uint8_t *fb, int w, int h, const UiElem &e, int x, int y);
void uiSetCustomDraw(UiCustomDraw fn);

// ---------------------------------------------------------------------------
// Transition
// ---------------------------------------------------------------------------
enum class UiPhase : uint8_t { Idle, Exit, Enter };

struct UiAnim
{
    UiPhase phase;
    bool    started;
    int     frame;
    int32_t key;
    UiScene shown;   // what is actually on the glass right now
};

void uiAnimInit(UiAnim &a);
// Advances one frame of `a` against this frame's freshly built `cur` and
// paints the result. Returns true while a transition is still running, which
// is the cue to ignore input aimed at controls that are not in place yet.
bool uiAnimRun(UiAnim &a, const UiScene &cur, uint8_t *fb, Color background);

inline bool uiAnimBusy(const UiAnim &a) { return a.phase != UiPhase::Idle; }

// True only while controls are still far enough from home that a press would
// land on something the player cannot see there. Deliberately shorter than
// the whole transition: by the tail of the spring everything is within a
// couple of pixels of its target, and swallowing input that late would just
// make the UI feel dead.
bool uiAnimInputBlocked(const UiAnim &a);

#endif // UIKIT_H
