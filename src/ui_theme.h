#ifndef UI_THEME_H
#define UI_THEME_H

#include "raylib.h"

// Shared GW1-flavored window chrome: every panel, tracker, and dialog
// draws through these so the whole interface reads as one set - dark
// slate bodies, a thin gold trim, and beveled corner ticks, echoing
// GW1's parchment-and-gilt windows without any texture assets.

// ---------------------------------------------------------------------
// Design tokens
//
// Every panel used to pick its own paddings (10*scale here, 8 there, 12
// somewhere else) and its own greys, so nothing lined up and the same
// idea looked different in each window. These are the shared values;
// widgets below are built from them, and panels should reach for a
// token rather than inventing a number.

// Spacing scale: multiples of a 4px base. UI_SP(scale, 2) == 8px at 1x.
#define UI_SP(scale, n) ((int)(4.0f * (scale) * (n)))

// Type scale. Five steps is enough to build a clear hierarchy and few
// enough that sizes stay visibly distinct from each other.
typedef enum {
    UI_TEXT_XS = 0,  // captions, unit labels
    UI_TEXT_SM,      // secondary values, hints
    UI_TEXT_MD,      // body / list rows - the default
    UI_TEXT_LG,      // window titles
    UI_TEXT_XL       // screen headings
} UITextSize;

int UI_FontSize(float scale, UITextSize size);

// Semantic colors. Callers say what a thing MEANS, not what grey it is,
// so the palette can move in one place.
#define UI_GOLD          (Color){ 196, 168, 100, 255 }
#define UI_GOLD_DIM      (Color){ 140, 122, 78, 255 }
#define UI_TEXT_PRIMARY  (Color){ 236, 230, 214, 255 }
#define UI_TEXT_SECOND   (Color){ 176, 170, 156, 255 }
#define UI_TEXT_MUTED    (Color){ 122, 118, 108, 255 }
#define UI_TEXT_LINK     (Color){ 150, 198, 246, 255 }
#define UI_POSITIVE      (Color){ 126, 208, 138, 255 }
#define UI_NEGATIVE      (Color){ 216, 118, 108, 255 }
#define UI_SURFACE       (Color){ 24, 25, 33, 242 }
#define UI_SURFACE_RAISE (Color){ 42, 44, 57, 255 }
#define UI_SURFACE_HOVER (Color){ 58, 62, 82, 255 }

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

// ---------------------------------------------------------------------
// Shared widgets. Every list in the game is the same object: a full
// width row that highlights on hover, dims when unavailable, and shows a
// gold rule when selected. Having one implementation means the merchant,
// the trainer, the bags and the skill list all *behave* identically,
// which is most of what makes an interface feel finished.

typedef enum {
    UI_ROW_NORMAL = 0,
    UI_ROW_DISABLED,   // visible but not actionable
    UI_ROW_SELECTED    // currently chosen / equipped / armed
} UIRowState;

// Draws the row background for `rect` and returns true if it was
// clicked this frame (never true when disabled).
bool UI_Row(Rectangle rect, UIRowState state, bool hovered);

// A labelled button. Returns true on click.
bool UI_Button(Rectangle rect, const char *label, int font, bool enabled, bool highlighted);

// A horizontal tab strip. `count` labels laid out across `rect`;
// returns the index clicked this frame, or -1. `active` is drawn with a
// gold underline and lifted background.
int UI_Tabs(Rectangle rect, const char **labels, int count, int active, int font);

#endif
