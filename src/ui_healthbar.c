#include "ui_healthbar.h"
#include "entity.h"
#include "gwmath.h"
#include "ui_font.h"
#include "ui_theme.h"
#include <stdio.h>

// The base bar. Everything else is layered on top of this colour.
#define HB_HEALTH (Color){ 190, 40, 40, 255 }

// GW1's tint order, weakest first. Bleeding is the pale wash most
// players recognise; poison's green sits over it; a hex's purple sits
// over everything. Only the strongest one present is drawn, which is
// what stops a stacked character's bar turning to mud.
static bool TintFor(const Entity *e, Color *out) {
    if (Entity_HasHex(e, HEX_FALTERING) || Entity_HasHex(e, HEX_BACKLASH)) {
        *out = (Color){ 150, 90, 200, 255 }; // hex overrides everything
        return true;
    }
    if (Entity_HasCondition(e, COND_POISON)) {
        *out = (Color){ 96, 168, 78, 255 };
        return true;
    }
    if (Entity_HasCondition(e, COND_BURNING)) {
        *out = (Color){ 232, 122, 48, 255 };
        return true;
    }
    if (Entity_HasCondition(e, COND_BLEEDING)) {
        // The light red the question named: paler and pinker than the
        // bar under it, so it reads as a change rather than as damage.
        *out = (Color){ 232, 106, 106, 255 };
        return true;
    }
    return false;
}

void UIHealthBar_Draw(Rectangle r, const Entity *e, int font, bool showValue) {
    if (!e) return;

    Color fill = HB_HEALTH;
    Color tint;
    if (TintFor(e, &tint)) fill = tint;

    float pct = (e->maxHp > 0) ? (float)e->hp / (float)e->maxHp : 0.0f;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;
    if (!e->alive) pct = 0.0f;

    char label[16];
    if (showValue) snprintf(label, sizeof(label), "%d", e->hp);

    // Deep Wound has already taken its 20% off maxHp, so the bar below
    // is drawn against the REDUCED maximum - which is why the lost slice
    // has to be painted separately, at the end, or the bar would simply
    // look shorter with no explanation. Drawing it as a grey cap is what
    // makes "you have less ceiling than you had" legible.
    bool deepWound = Entity_HasCondition(e, COND_DEEP_WOUND);
    Rectangle live = r;
    if (deepWound) {
        live.width = r.width * (1.0f - GW_DEEP_WOUND_HEALTH_LOSS);
    }

    UI_ThemeBar(live, pct, fill, showValue ? label : NULL, font);

    if (deepWound) {
        Rectangle lost = { live.x + live.width, r.y, r.width - live.width, r.height };
        DrawRectangleRec(lost, (Color){ 96, 96, 102, 255 });
        DrawRectangleLinesEx(lost, 1.0f, (Color){ 52, 52, 58, 255 });
        // A hatch, so the grey can't be mistaken for empty bar.
        for (float hx = lost.x + 3.0f; hx < lost.x + lost.width; hx += 5.0f) {
            DrawLineEx((Vector2){ hx, lost.y + 1 },
                       (Vector2){ hx - lost.height * 0.5f, lost.y + lost.height - 1 },
                       1.0f, (Color){ 132, 132, 138, 200 });
        }
    }
}

// A small downward triangle - GW1's shorthand for "something negative
// is on this character".
static void Arrow(int x, int y, int size, Color color) {
    DrawTriangle((Vector2){ (float)x, (float)y },
                 (Vector2){ (float)(x + size), (float)y },
                 (Vector2){ (float)(x + size / 2), (float)(y + size) },
                 color);
}

void UIHealthBar_DrawStatusArrows(int x, int y, int size, const Entity *e) {
    if (!e) return;
    bool hasCondition = false, hasHex = false;
    for (int i = 0; i < MAX_ACTIVE_EFFECTS; i++) {
        if (!e->effects[i].active) continue;
        if (e->effects[i].category == EFFECT_HEX) hasHex = true;
        else hasCondition = true;
    }
    // GW1's colours: grey for a condition, purple for a hex. The brown
    // this used to draw wasn't either of them.
    if (hasCondition) Arrow(x, y, size, (Color){ 176, 176, 182, 255 });
    if (hasHex) Arrow(x, y + (hasCondition ? size + 2 : 0), size, (Color){ 150, 70, 200, 255 });
}
