/*
 * theme.cpp — see theme.h.
 */
#include "theme.h"

#include <cstdio>
#include <cstring>
#include <sys/stat.h>

static constexpr const char *THEME_PATH = "/3ds/slide/theme.txt";

// The website's THEMES array, in the website's order. `secondary` is carried
// so the table stays a faithful copy of the source of truth even though the
// derivation below prefers a darkened primary for the raised-card strip —
// see themeApply().
const ThemeDef THEME_DEFS[] = {
    { "wooden", "Wooden (Classic)",
      {244, 241, 234}, {255, 255, 255}, {227, 217, 198},
      { 74,  55,  40},
      {139,  69,  19}, {255, 255, 255}, {109,  54,  16},
      {210, 180, 140}, { 74,  55,  40},
      {222, 203, 164}, {166, 124,  82}, { 93,  46,  10} },

    { "dark", "Midnight",
      { 15,  23,  42}, { 30,  41,  59}, {  2,   6,  23},
      {248, 250, 252},
      { 56, 189, 248}, { 15,  23,  42}, { 14, 165, 233},
      {148, 163, 184}, { 15,  23,  42},
      { 51,  65,  85}, { 15,  23,  42}, { 56, 189, 248} },

    { "light", "Panda White",
      {248, 250, 252}, {255, 255, 255}, {241, 245, 249},
      { 15,  23,  42},
      { 15,  23,  42}, {255, 255, 255}, { 51,  65,  85},
      {100, 116, 139}, {255, 255, 255},
      {241, 245, 249}, {148, 163, 184}, { 15,  23,  42} },

    { "beach", "Tropical Beach",
      {255, 249, 230}, {255, 253, 245}, {247, 237, 202},
      { 44,  62,  80},
      { 26, 188, 156}, {255, 255, 255}, { 22, 160, 133},
      {241, 196,  15}, { 44,  62,  80},
      {255, 234, 167}, {230, 126,  34}, {211,  84,   0} },

    { "connect4", "Connect 4",
      { 30,  64, 175}, { 37,  99, 235}, { 30,  58, 138},
      {255, 255, 255},
      {250, 204,  21}, { 30,  58, 138}, {234, 179,   8},
      {220,  38,  38}, {255, 255, 255},
      { 59, 130, 246}, { 30,  64, 175}, { 30,  58, 138} },

    { "wii", "Wii Menu",
      {255, 255, 255}, {248, 250, 252}, {241, 245, 249},
      { 77,  77,  77},
      {  0, 173, 239}, {255, 255, 255}, {226, 232, 240},
      {  0, 173, 239}, {255, 255, 255},
      {255, 255, 255}, {203, 213, 225}, {148, 163, 184} },

    { "oscar", "Oscar",
      {255, 241, 242}, {255, 255, 255}, {255, 228, 230},
      {136,  19,  55},
      {251, 113, 133}, {255, 255, 255}, {244,  63,  94},
      {225,  29,  72}, {255, 255, 255},
      {255, 241, 242}, {249, 168, 212}, {159,  18,  57} },

    { "sonic", "Green Hill Zone",
      {135, 206, 235}, {255, 255, 255}, { 93, 173, 226},
      { 44,  62,  80},
      { 34, 139,  34}, {255, 255, 255}, { 30, 107,  30},
      { 50, 205,  50}, {255, 255, 255},
      {205, 133,  63}, {139,  69,  19}, { 50, 205,  50} },
};

static constexpr int THEME_COUNT = (int)(sizeof(THEME_DEFS) / sizeof(THEME_DEFS[0]));

static int gThemeIndex = 0;

// gPalette starts on Wooden so the very first frame — drawn before the SD
// card or the server has been asked anything — looks exactly like it always
// has. themeApply() overwrites it as soon as a stored theme turns up.
Palette gPalette = {
    {244, 241, 234}, {255, 255, 255}, {227, 217, 198},
    { 74,  55,  40}, {143, 126, 110},
    {139,  69,  19}, {255, 255, 255}, { 92,  45,  12}, {214, 190, 173},
    {210, 180, 140}, { 74,  55,  40}, {158, 135, 105},
    {222, 203, 164}, {166, 124,  82}, { 93,  46,  10}
};

int themeCount() { return THEME_COUNT; }
int themeCurrent() { return gThemeIndex; }

int themeIndexById(const char *id)
{
    if (!id || !id[0]) return -1;
    for (int i = 0; i < THEME_COUNT; ++i)
        if (strcmp(THEME_DEFS[i].id, id) == 0) return i;
    return -1;
}

void themeApply(int index)
{
    if (index < 0 || index >= THEME_COUNT) return;
    const ThemeDef &t = THEME_DEFS[index];
    gThemeIndex = index;

    gPalette.bg          = t.bg;
    gPalette.bgLight     = t.bgLight;
    gPalette.bgDark      = t.bgDark;
    gPalette.text        = t.text;
    gPalette.primary     = t.primary;
    gPalette.primaryTxt  = t.primaryTxt;
    gPalette.accent      = t.accent;
    gPalette.accentTxt   = t.accentTxt;
    gPalette.boardLight  = t.boardLight;
    gPalette.boardDark   = t.boardDark;
    gPalette.boardBorder = t.boardBorder;

    // Derived, not stored. A darkened primary rather than the website's
    // `secondary`, because `secondary` is only a shadow on most themes —
    // Wii Menu's is a pale grey, which under a raised card would read as a
    // highlight rather than the lift it is meant to be. On Wooden the two
    // land within a few units of each other anyway.
    gPalette.primaryDk   = darken(t.primary, 0.34f);
    gPalette.accentDk    = darken(t.accent, 0.25f);
    gPalette.textSoft    = mix(t.text, t.bg, 0.40f);
    gPalette.primarySoft = mix(t.primaryTxt, t.primary, 0.35f);
}

int themeLoad()
{
    FILE *f = fopen(THEME_PATH, "r");
    if (!f) return -1;
    char id[32] = {};
    bool ok = fgets(id, sizeof(id), f) != nullptr;
    fclose(f);
    if (!ok) return -1;
    int len = (int)strlen(id);
    while (len > 0 && (id[len-1] == '\n' || id[len-1] == '\r')) id[--len] = 0;
    return themeIndexById(id);
}

void themeSave(int index)
{
    if (index < 0 || index >= THEME_COUNT) return;
    mkdir("/3ds",       0777);
    mkdir("/3ds/slide", 0777);
    FILE *f = fopen(THEME_PATH, "w");
    if (!f) return;
    fprintf(f, "%s\n", THEME_DEFS[index].id);
    fclose(f);
}
