#ifndef UI_HEALTHBAR_H
#define UI_HEALTHBAR_H

#include "raylib.h"

struct Entity;

// GW1 reads a character's state off their health bar, not off an icon
// tray, and it does it three ways at once:
//
//   * the FILL is tinted by what is on them, in a precedence order -
//     bleeding is overridden by poison, and a hex overrides both;
//   * Deep Wound greys out the 20% of the bar it has taken away, drawn
//     ALONGSIDE whatever tint is in effect rather than instead of it;
//   * a downward arrow beside the bar says "something negative", grey
//     for a condition and purple for a hex.
//
// One function so the HUD's own bar and the party window cannot show
// the same character differently - which they did, because only the
// party window showed anything at all.
void UIHealthBar_Draw(Rectangle r, const struct Entity *e, int font, bool showValue);

// The arrows, drawn beside a bar. Split out because the party window
// has room for them and the HUD bar does not.
void UIHealthBar_DrawStatusArrows(int x, int y, int size, const struct Entity *e);

#endif
