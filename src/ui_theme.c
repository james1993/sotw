#include "ui_theme.h"
#include "ui_font.h"
#include "ui_cursor.h"
#include "skill.h"
#include <math.h>
#include <stddef.h>

void UI_ThemePanel(Rectangle r, float scale, int headerH) {
    // Body: a slight vertical gradient so large panels don't read as
    // one flat slab.
    DrawRectangleGradientV((int)r.x, (int)r.y, (int)r.width, (int)r.height,
                           (Color){ 30, 32, 42, 242 }, (Color){ 18, 19, 26, 242 });

    if (headerH > 0) {
        DrawRectangle((int)r.x, (int)r.y, (int)r.width, headerH, (Color){ 40, 41, 52, 255 });
        DrawRectangle((int)r.x, (int)r.y + headerH - 1, (int)r.width, 1, UI_GOLD_DIM);
    }

    // Double border: dark outer edge, gold inner trim.
    DrawRectangleLinesEx(r, 2, (Color){ 8, 8, 12, 255 });
    DrawRectangleLinesEx((Rectangle){ r.x + 2, r.y + 2, r.width - 4, r.height - 4 },
                         1, UI_GOLD_DIM);

    // Corner ticks - the little gilt L-brackets that make it read as a
    // dressed window instead of a debug rect.
    int tick = (int)(9 * scale);
    if (tick < 6) tick = 6;
    int x0 = (int)r.x + 2, y0 = (int)r.y + 2;
    int x1 = (int)(r.x + r.width) - 3, y1 = (int)(r.y + r.height) - 3;
    DrawRectangle(x0, y0, tick, 2, UI_GOLD);
    DrawRectangle(x0, y0, 2, tick, UI_GOLD);
    DrawRectangle(x1 - tick + 1, y0, tick, 2, UI_GOLD);
    DrawRectangle(x1 - 1, y0, 2, tick, UI_GOLD);
    DrawRectangle(x0, y1 - 1, tick, 2, UI_GOLD);
    DrawRectangle(x0, y1 - tick + 1, 2, tick, UI_GOLD);
    DrawRectangle(x1 - tick + 1, y1 - 1, tick, 2, UI_GOLD);
    DrawRectangle(x1 - 1, y1 - tick + 1, 2, tick, UI_GOLD);
}

void UI_ThemeBar(Rectangle r, float pct, Color fill, const char *label, int font) {
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;
    int x = (int)r.x, y = (int)r.y, w = (int)r.width, h = (int)r.height;

    // Recessed well with a top inner shadow.
    DrawRectangle(x, y, w, h, (Color){ 22, 22, 27, 255 });
    DrawRectangle(x, y, w, h > 3 ? 2 : 1, (Color){ 10, 10, 13, 255 });

    int fillW = (int)(w * pct);
    if (fillW > 0) {
        DrawRectangle(x, y, fillW, h, fill);
        // Bevel: light top half, shaded bottom edge.
        DrawRectangle(x, y, fillW, h / 2, (Color){ 255, 255, 255, 42 });
        if (h > 4) DrawRectangle(x, y + h - 2, fillW, 2, (Color){ 0, 0, 0, 70 });
    }

    DrawRectangleLines(x, y, w, h, (Color){ 8, 8, 10, 255 });

    if (label) {
        int lw = UITextWidth(label, font);
        UIText(label, x + (w - lw) / 2, y + (h - font) / 2, font, RAYWHITE);
    }
}

int UI_FontSize(float scale, UITextSize size) {
    static const int base[] = { 10, 12, 14, 17, 24 };
    int n = (int)size;
    if (n < 0) n = 0;
    if (n > UI_TEXT_XL) n = UI_TEXT_XL;
    int px = (int)(base[n] * scale);
    return px < 8 ? 8 : px;
}

bool UI_Row(Rectangle rect, UIRowState state, bool hovered) {
    if (state == UI_ROW_SELECTED) {
        DrawRectangleRec(rect, (Color){ 46, 50, 66, 220 });
        // A gold spine on the leading edge: reads as "this one" without
        // washing the whole row in accent color.
        DrawRectangle((int)rect.x, (int)rect.y, (int)(3 * (rect.height / 24.0f) + 1),
                      (int)rect.height, UI_GOLD);
    } else if (hovered && state == UI_ROW_NORMAL) {
        DrawRectangleRec(rect, UI_SURFACE_HOVER);
    }
    return hovered && state != UI_ROW_DISABLED && UI_PointerClicked();
}

bool UI_Button(Rectangle rect, const char *label, int font, bool enabled, bool highlighted) {
    bool hovered = enabled && CheckCollisionPointRec(UI_PointerPos(), rect);
    Color top = !enabled ? (Color){ 38, 38, 44, 255 }
              : (hovered || highlighted) ? (Color){ 78, 86, 114, 255 }
                                         : (Color){ 48, 52, 70, 255 };
    DrawRectangleGradientV((int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height,
                           top, (Color){ (unsigned char)(top.r * 0.55f),
                                         (unsigned char)(top.g * 0.55f),
                                         (unsigned char)(top.b * 0.55f), 255 });
    DrawRectangleLinesEx(rect, highlighted ? 2.0f : 1.0f,
                         !enabled ? (Color){ 62, 60, 58, 255 }
                                  : (highlighted ? UI_GOLD : UI_GOLD_DIM));
    int tw = UITextWidth(label, font);
    UI_TextShadow(label, (int)(rect.x + (rect.width - tw) / 2),
                  (int)(rect.y + (rect.height - font) / 2), font,
                  enabled ? UI_TEXT_PRIMARY : UI_TEXT_MUTED);
    return hovered && UI_PointerClicked();
}

int UI_Tabs(Rectangle rect, const char **labels, int count, int active, int font) {
    if (count <= 0) return -1;
    int clicked = -1;
    float tabW = rect.width / (float)count;
    for (int i = 0; i < count; i++) {
        Rectangle t = { rect.x + i * tabW, rect.y, tabW, rect.height };
        bool isActive = (i == active);
        bool hovered = CheckCollisionPointRec(UI_PointerPos(), t);

        DrawRectangleRec(t, isActive ? UI_SURFACE_RAISE
                        : hovered ? (Color){ 34, 36, 48, 255 } : (Color){ 22, 23, 31, 255 });
        // Only the active tab gets the gold underline; inactive tabs get
        // a hairline so the strip still reads as one control.
        if (isActive) {
            DrawRectangle((int)t.x, (int)(t.y + t.height - 3), (int)t.width, 3, UI_GOLD);
        } else {
            DrawRectangle((int)t.x, (int)(t.y + t.height - 1), (int)t.width, 1, UI_GOLD_DIM);
        }
        int tw = UITextWidth(labels[i], font);
        UI_TextShadow(labels[i], (int)(t.x + (t.width - tw) / 2),
                      (int)(t.y + (t.height - font) / 2), font,
                      isActive ? UI_TEXT_PRIMARY : UI_TEXT_SECOND);
        if (hovered && UI_PointerClicked()) clicked = i;
    }
    return clicked;
}

void UI_TextShadow(const char *text, int x, int y, int size, Color color) {
    Color shadow = { 0, 0, 0, (unsigned char)(color.a * 0.72f) };
    UIText(text, x + 1, y + 1, size, shadow);
    UIText(text, x, y, size, color);
}

void UI_TextShadowCentered(const char *text, int cx, int y, int size, Color color) {
    UI_TextShadow(text, cx - UITextWidth(text, size) / 2, y, size, color);
}

int UI_KeyBadge(const char *label, int x, int y, int size, bool draw) {
    int textW = UITextWidth(label, size);
    int padX = size / 2;
    int w = textW + padX * 2;
    int h = size + size / 2;
    if (w < h) w = h; // single letters stay square-ish, not slivers
    if (draw) {
        Rectangle r = { (float)x, (float)y, (float)w, (float)h };
        DrawRectangleRounded(r, 0.3f, 6, (Color){ 18, 18, 24, 235 });
        DrawRectangleRoundedLines(r, 0.3f, 6, UI_GOLD);
        UIText(label, x + (w - textW) / 2, y + (h - size) / 2, size, (Color){ 245, 235, 205, 255 });
    }
    return w;
}

// ---------------------------------------------------------------------
// Procedural skill icons. GW1 gives every skill a painted icon; here
// each gets a school-colored tile and a small vector glyph that hints
// at what it does, so the bar is readable at a glance.

static Color SchoolColor(AttributeKind a) {
    switch (a) {
        case ATTR_FIRE_MAGIC:      return (Color){ 200, 92, 40, 255 };
        case ATTR_ENERGY_STORAGE:  return (Color){ 120, 95, 200, 255 };
        case ATTR_HEALING_PRAYERS: return (Color){ 62, 150, 92, 255 };
        case ATTR_SMITING_PRAYERS: return (Color){ 185, 155, 70, 255 };
        case ATTR_DIVINE_FAVOR:    return (Color){ 120, 155, 195, 255 };
        case ATTR_STRENGTH:        return (Color){ 150, 90, 70, 255 };
        case ATTR_TACTICS:         return (Color){ 110, 110, 130, 255 };
        default:                   return (Color){ 100, 100, 110, 255 };
    }
}

void UI_DrawSkillIcon(int skillId, Rectangle r) {
    if (skillId < 0 || skillId >= g_skillCount) return;
    const Skill *s = &g_skillDB[skillId];

    Color school = SchoolColor(s->attribute);
    Color dark = (Color){ (unsigned char)(school.r / 3), (unsigned char)(school.g / 3),
                          (unsigned char)(school.b / 3), 255 };
    DrawRectangleGradientV((int)r.x, (int)r.y, (int)r.width, (int)r.height, school, dark);

    float cx = r.x + r.width / 2.0f;
    float cy = r.y + r.height / 2.0f;
    float u = r.width / 56.0f; // glyphs authored against a 56px tile
    Color ink = (Color){ 245, 240, 225, 255 };
    Color glow = (Color){ 255, 255, 255, 90 };

    switch (skillId) {
        case SK_GASH:
            // Two parallel cuts.
            DrawLineEx((Vector2){ cx - 12 * u, cy - 10 * u }, (Vector2){ cx + 6 * u, cy + 12 * u }, 3 * u, ink);
            DrawLineEx((Vector2){ cx - 2 * u, cy - 12 * u }, (Vector2){ cx + 13 * u, cy + 6 * u }, 3 * u, (Color){ 220, 90, 80, 255 });
            break;
        case SK_RUSH_STRIKE:
            // Forward chevrons: momentum.
            for (int i = 0; i < 2; i++) {
                float ox = (i - 0.5f) * 12 * u;
                DrawLineEx((Vector2){ cx + ox - 5 * u, cy - 10 * u }, (Vector2){ cx + ox + 5 * u, cy }, 3 * u, ink);
                DrawLineEx((Vector2){ cx + ox + 5 * u, cy }, (Vector2){ cx + ox - 5 * u, cy + 10 * u }, 3 * u, ink);
            }
            break;
        case SK_BATTLE_CRY:
        case SK_FERAL_HOWL: {
            // A mouth-dot with sound arcs; the howl's arcs run hostile red.
            Color arc = (skillId == SK_FERAL_HOWL) ? (Color){ 230, 110, 90, 255 } : ink;
            DrawCircleV((Vector2){ cx - 8 * u, cy }, 4 * u, ink);
            for (int i = 1; i <= 3; i++) {
                DrawRing((Vector2){ cx - 8 * u, cy }, (4 + i * 5) * u - 1.2f * u, (4 + i * 5) * u,
                         -50, 50, 12, Fade(arc, 1.0f - i * 0.22f));
            }
            break;
        }
        case SK_DEATHBLOW:
            // A blade driven straight down.
            DrawLineEx((Vector2){ cx, cy - 13 * u }, (Vector2){ cx, cy + 8 * u }, 3.5f * u, ink);
            DrawLineEx((Vector2){ cx - 7 * u, cy - 8 * u }, (Vector2){ cx + 7 * u, cy - 8 * u }, 2.5f * u, ink);
            DrawTriangle((Vector2){ cx - 4 * u, cy + 7 * u }, (Vector2){ cx + 4 * u, cy + 7 * u },
                         (Vector2){ cx, cy + 14 * u }, (Color){ 220, 90, 80, 255 });
            break;
        case SK_FIRE_BOLT:
            // A single flame tongue.
            DrawTriangle((Vector2){ cx, cy - 13 * u }, (Vector2){ cx - 9 * u, cy + 10 * u },
                         (Vector2){ cx + 9 * u, cy + 10 * u }, (Color){ 255, 170, 60, 255 });
            DrawTriangle((Vector2){ cx, cy - 5 * u }, (Vector2){ cx - 4 * u, cy + 9 * u },
                         (Vector2){ cx + 4 * u, cy + 9 * u }, (Color){ 255, 240, 160, 255 });
            break;
        case SK_CINDER_STORM:
            // Embers raining in.
            for (int i = 0; i < 3; i++) {
                float ox = (i - 1) * 9 * u;
                DrawLineEx((Vector2){ cx + ox + 4 * u, cy - 12 * u + i * 3 * u },
                           (Vector2){ cx + ox, cy - 2 * u + i * 3 * u }, 2 * u, (Color){ 255, 200, 120, 200 });
                DrawCircleV((Vector2){ cx + ox, cy + i * 3 * u }, 3 * u, (Color){ 255, 150, 60, 255 });
            }
            break;
        case SK_MIND_SEAR:
            // A jagged arc of raw energy.
            DrawLineEx((Vector2){ cx - 11 * u, cy - 11 * u }, (Vector2){ cx + 2 * u, cy - 2 * u }, 3 * u, ink);
            DrawLineEx((Vector2){ cx + 2 * u, cy - 2 * u }, (Vector2){ cx - 4 * u, cy + 3 * u }, 3 * u, ink);
            DrawLineEx((Vector2){ cx - 4 * u, cy + 3 * u }, (Vector2){ cx + 10 * u, cy + 12 * u }, 3 * u, (Color){ 200, 170, 255, 255 });
            break;
        case SK_METEOR:
            // Falling rock with a bright tail.
            DrawLineEx((Vector2){ cx + 12 * u, cy - 12 * u }, (Vector2){ cx - 3 * u, cy + 4 * u }, 4 * u, (Color){ 255, 190, 110, 180 });
            DrawCircleV((Vector2){ cx - 5 * u, cy + 6 * u }, 7 * u, (Color){ 150, 90, 60, 255 });
            DrawCircleV((Vector2){ cx - 7 * u, cy + 4 * u }, 3 * u, glow);
            break;
        case SK_CLAW_SWIPE:
        case SK_RENDING_CLAWS:
            // Three raking claws; the rending version drips.
            for (int i = 0; i < 3; i++) {
                float ox = (i - 1) * 8 * u;
                DrawLineEx((Vector2){ cx + ox - 4 * u, cy - 11 * u }, (Vector2){ cx + ox + 4 * u, cy + 11 * u },
                           2.5f * u, (Color){ 230, 120, 90, 255 });
                if (skillId == SK_RENDING_CLAWS) {
                    DrawCircleV((Vector2){ cx + ox + 5 * u, cy + 14 * u }, 2.0f * u,
                                (Color){ 208, 70, 70, 255 });
                }
            }
            break;
        case SK_HOBBLING_STRIKE: {
            // A shackle round an ankle - Crippled made literal.
            DrawRing((Vector2){ cx, cy + 3 * u }, 7 * u, 10 * u, 0, 360, 20,
                     (Color){ 190, 190, 200, 255 });
            DrawLineEx((Vector2){ cx, cy - 7 * u }, (Vector2){ cx, cy - 14 * u }, 3 * u,
                       (Color){ 150, 150, 160, 255 });
            DrawCircleV((Vector2){ cx, cy - 15 * u }, 3.5f * u, (Color){ 190, 190, 200, 255 });
            break;
        }
        case SK_SHROUD_OF_DOUBT: {
            // A veil drawn over the target, with the weight of it
            // dragging downward - the hex that slows what you do.
            DrawCircleSector((Vector2){ cx, cy + 2 * u }, 12 * u, 180, 360, 16,
                             (Color){ 150, 110, 200, 255 });
            DrawRectangle((int)(cx - 12 * u), (int)(cy + 1 * u), (int)(24 * u), (int)(5 * u),
                          (Color){ 120, 86, 168, 255 });
            for (int i = -1; i <= 1; i++) {
                float ox = i * 7 * u;
                DrawLineEx((Vector2){ cx + ox - 3 * u, cy + 7 * u },
                           (Vector2){ cx + ox, cy + 13 * u }, 2 * u, ink);
                DrawLineEx((Vector2){ cx + ox, cy + 13 * u },
                           (Vector2){ cx + ox + 3 * u, cy + 7 * u }, 2 * u, ink);
            }
            break;
        }
        case SK_PRICE_OF_FAITH: {
            // A hex diamond with the cost being drawn out of it.
            DrawPoly((Vector2){ cx, cy - 3 * u }, 4, 11 * u, 45, (Color){ 150, 110, 200, 255 });
            DrawPolyLines((Vector2){ cx, cy - 3 * u }, 4, 11 * u, 45, ink);
            DrawLineEx((Vector2){ cx, cy + 2 * u }, (Vector2){ cx, cy + 13 * u }, 3 * u, ink);
            DrawTriangle((Vector2){ cx - 5 * u, cy + 10 * u }, (Vector2){ cx + 5 * u, cy + 10 * u },
                         (Vector2){ cx, cy + 16 * u }, (Color){ 220, 90, 80, 255 });
            break;
        }
        case SK_MEND_AILMENT: {
            // The healer's cross lifting an affliction clear of a body.
            DrawRectangle((int)(cx - 2.5f * u), (int)(cy - 4 * u), (int)(5 * u), (int)(18 * u), ink);
            DrawRectangle((int)(cx - 9 * u), (int)(cy + 1.5f * u), (int)(18 * u), (int)(5 * u), ink);
            // The condition coming off the top.
            DrawCircleV((Vector2){ cx + 8 * u, cy - 11 * u }, 4 * u, (Color){ 208, 90, 80, 220 });
            DrawLineEx((Vector2){ cx + 4 * u, cy - 15 * u }, (Vector2){ cx + 12 * u, cy - 7 * u },
                       2 * u, (Color){ 255, 245, 225, 255 });
            break;
        }
        case SK_SMITE_HEX: {
            // A hex diamond struck apart by a bolt of smiting light.
            DrawPolyLines((Vector2){ cx, cy }, 4, 12 * u, 45, (Color){ 150, 110, 200, 255 });
            DrawPolyLines((Vector2){ cx, cy }, 4, 9 * u, 45, (Color){ 120, 86, 168, 200 });
            DrawLineEx((Vector2){ cx - 10 * u, cy - 13 * u }, (Vector2){ cx + 3 * u, cy + 1 * u },
                       3.5f * u, (Color){ 255, 240, 180, 255 });
            DrawLineEx((Vector2){ cx + 3 * u, cy + 1 * u }, (Vector2){ cx - 2 * u, cy + 4 * u },
                       3.5f * u, (Color){ 255, 240, 180, 255 });
            DrawLineEx((Vector2){ cx - 2 * u, cy + 4 * u }, (Vector2){ cx + 10 * u, cy + 14 * u },
                       3.5f * u, (Color){ 255, 240, 180, 255 });
            break;
        }
        case SK_DISTRACTING_BLOW:
            // A starburst - the interrupt's "smack".
            for (int i = 0; i < 4; i++) {
                float a = i * 0.785f;
                DrawLineEx((Vector2){ cx - cosf(a) * 12 * u, cy - sinf(a) * 12 * u },
                           (Vector2){ cx + cosf(a) * 12 * u, cy + sinf(a) * 12 * u }, 2.5f * u, ink);
            }
            DrawCircleV((Vector2){ cx, cy }, 4 * u, (Color){ 255, 220, 120, 255 });
            break;
        case SK_ORISON_OF_HEALING:
            // The healer's cross.
            DrawRectangle((int)(cx - 3 * u), (int)(cy - 12 * u), (int)(6 * u), (int)(24 * u), ink);
            DrawRectangle((int)(cx - 12 * u), (int)(cy - 3 * u), (int)(24 * u), (int)(6 * u), ink);
            break;
        case SK_BANISH:
            // Radiant holy light.
            DrawCircleV((Vector2){ cx, cy }, 6 * u, (Color){ 255, 245, 200, 255 });
            for (int i = 0; i < 8; i++) {
                float a = i * 0.785f;
                DrawLineEx((Vector2){ cx + cosf(a) * 8 * u, cy + sinf(a) * 8 * u },
                           (Vector2){ cx + cosf(a) * 14 * u, cy + sinf(a) * 14 * u }, 2 * u, ink);
            }
            break;
        case SK_SMITE:
            // A bolt of judgement from above.
            DrawTriangle((Vector2){ cx - 6 * u, cy - 13 * u }, (Vector2){ cx + 6 * u, cy - 13 * u },
                         (Vector2){ cx, cy + 6 * u }, (Color){ 255, 235, 150, 255 });
            DrawCircleV((Vector2){ cx, cy + 8 * u }, 5 * u, glow);
            DrawCircleV((Vector2){ cx, cy + 8 * u }, 3 * u, (Color){ 255, 250, 210, 255 });
            break;
        case SK_BANE_SIGNET: {
            // A signet ring stamp.
            DrawRing((Vector2){ cx, cy }, 8 * u, 11 * u, 0, 360, 24, ink);
            DrawCircleV((Vector2){ cx, cy }, 4 * u, (Color){ 220, 200, 140, 255 });
            break;
        }
        default:
            // Unmapped skill: a plain diamond, still school-colored.
            DrawPoly((Vector2){ cx, cy }, 4, 10 * u, 45, ink);
            break;
    }

    // Tile edge: dark seat + a whisper of gold, matching the panels.
    DrawRectangleLinesEx(r, 2, (Color){ 10, 10, 14, 255 });
    DrawRectangleLinesEx((Rectangle){ r.x + 2, r.y + 2, r.width - 4, r.height - 4 }, 1,
                         (Color){ 255, 255, 255, 40 });
}
