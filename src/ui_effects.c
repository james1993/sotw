#include "ui_effects.h"
#include "entity.h"
#include "ui_font.h"
#include "ui_hit.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static float g_bottom = 0.0f;
float UI_EffectsMonitorBottom(void) { return g_bottom; }

// One or two capitals off the effect's name - enough to tell Bleeding
// from Burning at icon size without a tooltip.
static void Abbrev(const char *name, char out[3]) {
    out[0] = name[0];
    out[1] = name[1] ? name[1] : ' ';
    out[2] = '\0';
    for (int i = 0; i < 2; i++)
        if (out[i] >= 'a' && out[i] <= 'z') out[i] = (char)(out[i] - 32);
}

// A single chip: a coloured square with a dark abbreviation and, for a
// timed effect, its remaining seconds tucked in the corner. Returns true
// while the pointer is over it, so the caller can label it.
static bool Chip(int x, int y, int icon, Color c, const char *abbrev,
                 float remaining, int font, int tiny) {
    DrawRectangle(x, y, icon, icon, c);
    DrawRectangleLinesEx((Rectangle){ (float)x, (float)y, (float)icon, (float)icon }, 1.5f,
                         (Color){ 12, 12, 14, 200 });
    Color ink = { 20, 18, 16, 255 };
    int aw = UITextWidth(abbrev, font);
    UIText(abbrev, x + (icon - aw) / 2, y + (icon - font) / 2 - 1, font, ink);
    if (remaining > 0.0f) {
        char t[8];
        snprintf(t, sizeof(t), "%d", (int)ceilf(remaining));
        int tw = UITextWidth(t, tiny);
        int tx = x + icon - tw - 2, ty = y + icon - tiny - 1;
        UIText(t, tx + 1, ty + 1, tiny, (Color){ 0, 0, 0, 200 }); // shadow
        UIText(t, tx, ty, tiny, RAYWHITE);
    }
    UIHit_Claim((Rectangle){ (float)x, (float)y, (float)icon, (float)icon });
    return CheckCollisionPointRec(GetMousePosition(), (Rectangle){ (float)x, (float)y,
                                                                   (float)icon, (float)icon });
}

void UI_DrawEffectsMonitor(int screenWidth, int screenHeight) {
    (void)screenWidth;
    g_bottom = 0.0f;
    const Entity *p = Entity_Get(0);
    if (!p || !p->alive) return;

    float scale = UI_Scale(screenHeight);
    int icon = (int)(26 * scale);
    int gap  = (int)(4 * scale);
    int font = (int)(13 * scale);
    int tiny = (int)(9 * scale);
    int x0 = (int)(14 * scale);
    int y  = (int)(10 * scale);
    int x  = x0;

    const char *hoverLabel = NULL; // full name of the chip under the pointer

    // Morale first: a boost or a death penalty, as its own coloured chip
    // carrying the percentage. GW1 shows this on the health bar; here it
    // sits with the other status icons so one place answers "what's on me".
    if (p->morale != 0) {
        Color c = p->morale > 0 ? (Color){ 74, 150, 78, 255 } : (Color){ 168, 62, 58, 255 };
        char pct[8];
        snprintf(pct, sizeof(pct), "%+d%%", p->morale);
        int pw = UITextWidth(pct, tiny);
        int cw = icon + (pw > icon - 4 ? pw - (icon - 4) : 0); // widen if the number needs it
        DrawRectangle(x, y, cw, icon, c);
        DrawRectangleLinesEx((Rectangle){ (float)x, (float)y, (float)cw, (float)icon }, 1.5f,
                             (Color){ 12, 12, 14, 200 });
        const char *arrow = p->morale > 0 ? "\x18" : "\x19"; // up / down triangle (cp437)
        (void)arrow;
        UIText(pct, x + (cw - pw) / 2, y + (icon - tiny) / 2, tiny, RAYWHITE);
        if (CheckCollisionPointRec(GetMousePosition(),
                                   (Rectangle){ (float)x, (float)y, (float)cw, (float)icon }))
            hoverLabel = p->morale > 0 ? "Morale Boost" : "Death Penalty";
        UIHit_Claim((Rectangle){ (float)x, (float)y, (float)cw, (float)icon });
        x += cw + gap;
    }

    // Then enchantments, hexes and conditions, grouped the way GW1 groups
    // them so the colours read as blocks.
    EffectCategory order[3] = { EFFECT_ENCHANTMENT, EFFECT_HEX, EFFECT_CONDITION };
    for (int o = 0; o < 3; o++) {
        for (int s = 0; s < MAX_ACTIVE_EFFECTS; s++) {
            const ActiveEffect *fx = &p->effects[s];
            if (!fx->active || fx->category != order[o]) continue;
            char ab[3];
            Abbrev(Entity_EffectName(fx), ab);
            if (Chip(x, y, icon, Entity_EffectColor(fx), ab, fx->remaining, font, tiny))
                hoverLabel = Entity_EffectName(fx);
            x += icon + gap;
        }
    }

    if (x == x0) return; // nothing was drawn
    g_bottom = (float)(y + icon);

    // A single hover label under the row, so the icons stay legible but a
    // name is one hover away.
    if (hoverLabel) {
        int lf = (int)(12 * scale);
        int lw = UITextWidth(hoverLabel, lf);
        int lx = x0, ly = (int)(g_bottom + 3 * scale);
        DrawRectangle(lx - 3, ly - 2, lw + 6, lf + 4, (Color){ 18, 18, 24, 230 });
        DrawRectangleLines(lx - 3, ly - 2, lw + 6, lf + 4, (Color){ 90, 90, 110, 200 });
        UIText(hoverLabel, lx, ly, lf, RAYWHITE);
        g_bottom = (float)(ly + lf);
    }
}
