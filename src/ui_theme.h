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

// Text with a soft dark drop shadow. Floating world text sits on top of
// grass, dirt, sprites and effects, and a flat color washes out against
// half of them; the shadow makes every label legible on any background.
void UI_TextShadow(const char *text, int x, int y, int size, Color color);

// Same, horizontally centered on x.
void UI_TextShadowCentered(const char *text, int cx, int y, int size, Color color);

// A small key/button badge - "F", "X", "Esc" - in a rounded dark chip
// with gold trim. Returns the width drawn, so callers can lay out text
// after it. Pass draw=false to measure without drawing.
int UI_KeyBadge(const char *label, int x, int y, int size, bool draw);

#endif
