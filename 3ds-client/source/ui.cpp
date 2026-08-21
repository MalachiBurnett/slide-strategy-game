/*
 * ui.cpp — see ui.h. Layout lives in the header as constants; the only thing
 * that needs code is asking whether a touch landed inside one of them.
 */
#include "ui.h"

bool buttonHit(const Button &btn, int tx, int ty)
{
    return tx >= btn.x && tx < btn.x + btn.w &&
           ty >= btn.y && ty < btn.y + btn.h;
}
