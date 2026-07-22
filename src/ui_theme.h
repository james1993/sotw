#ifndef UI_THEME_H
#define UI_THEME_H

#include "raylib.h"

// Shared GW1-flavored window chrome: every panel, tracker, and dialog
// draws through these so the whole interface reads as one set - dark
// slate bodies, a thin gold trim, and beveled corner ticks, echoing
// GW1's parchment-and-gilt windows without any texture assets.

// The accent gold used for trim, titles, and highlights.
#define UI_GOLD (Color){ 196, 168, 100, 255 }
#define UI_GOLD_DIM (Color){ 140, 122, 78, 255 }

// Panel body + border + corner accents. headerH > 0 additionally draws
// a darker banded strip that tall across the top (callers keep drawing
// their own title text inside it, so no layout changes are needed).
void UI_ThemePanel(Rectangle r, float scale, int headerH);

// Beveled resource/progress bar: dark well, colored fill with a
// highlight bevel, optional label centered inside (NULL for none).
void UI_ThemeBar(Rectangle r, float pct, Color fill, const char *label, int font);

// Procedural skill icon: school-colored gradient tile plus a hand-drawn
// glyph per SkillId, filling the given rect (the skill bar's slots).
void UI_DrawSkillIcon(int skillId, Rectangle r);

#endif
