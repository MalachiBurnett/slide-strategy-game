/*
 * theme.h — the website's colour themes, ported to the 3DS client.
 *
 * The table below is the same eight themes the web client offers, in the
 * same order, with the same names and the same hex values (see THEMES in
 * src/constants/game.ts). A theme carries only the twelve colours the
 * website stores; everything else the client draws with — the raised-card
 * shadow, the muted body copy, the label on a primary bar — is derived from
 * those, so adding a ninth theme means adding one row here and one row on
 * the website and nothing else.
 *
 * themeApply() writes into gPalette (render.h), which every screen already
 * reads through the C_* names, so a theme change is picked up by the whole
 * client on the very next frame.
 */
#ifndef THEME_H
#define THEME_H

#include "render.h"

struct ThemeDef
{
    const char *id;     // matches the website / the users.theme column
    const char *name;   // as shown in the picker
    Color bg, bgLight, bgDark;
    Color text;
    Color primary, primaryTxt, secondary;
    Color accent, accentTxt;
    Color boardLight, boardDark, boardBorder;
};

extern const ThemeDef THEME_DEFS[];
int themeCount();

// Index of the theme with this id, or -1. A theme the client does not know
// about (the website gained one first) has to be rejected rather than
// clamped, so the caller can leave the player's saved theme alone.
int themeIndexById(const char *id);

// Rebuilds gPalette from THEME_DEFS[index] (out-of-range values are ignored).
void themeApply(int index);
int  themeCurrent();

// SD-card persistence, so the chosen theme survives a restart and is there
// before the first frame — including offline, where there is no server to
// ask. Stored next to the auth code in /3ds/slide/.
int  themeLoad();              // index, or -1 if nothing valid was stored
void themeSave(int index);

#endif // THEME_H
