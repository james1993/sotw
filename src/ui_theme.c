#include "ui_theme.h"
#include "ui_icons.h"
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

void UI_TextDisplayShadow(const char *text, int x, int y, int size, Color color) {
    Color shadow = { 0, 0, 0, (unsigned char)(color.a * 0.72f) };
    UITextDisplay(text, x + 1, y + 1, size, shadow);
    UITextDisplay(text, x, y, size, color);
}

void UI_TextDisplayShadowCentered(const char *text, int cx, int y, int size, Color color) {
    UI_TextDisplayShadow(text, cx - UITextDisplayWidth(text, size) / 2, y, size, color);
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
// Skill icons: a school-coloured tile with the skill's glyph on top.
// The glyphs are real art now (game-icons.net, packed into an atlas by
// tools/build_icon_atlas.py) rather than hand-authored vector shapes -
// thirty-one hand-drawn switch cases neither scaled to new skills nor
// looked like a painted GW1 icon.

// Tile colour per attribute line. Deliberately muted and darker than
// fx.c's palette: these sit behind a white glyph and under a gold frame
// all the way along the bar, where the FX colours are single flashes
// against a dark field and can afford to be loud.
static Color SchoolColor(AttributeKind a) {
    switch (a) {
        // Warrior: iron and rust.
        case ATTR_SWORDSMANSHIP:      return (Color){ 150,  90,  70, 255 };
        case ATTR_STRENGTH:           return (Color){ 132,  82,  64, 255 };
        case ATTR_TACTICS:            return (Color){ 110, 110, 130, 255 };
        // Ranger: field green.
        case ATTR_MARKSMANSHIP:       return (Color){  92, 130,  72, 255 };
        case ATTR_WILDERNESS_SURVIVAL:return (Color){  78, 118,  78, 255 };
        case ATTR_BEAST_MASTERY:      return (Color){ 108, 128,  66, 255 };
        case ATTR_EXPERTISE:          return (Color){  84, 116,  84, 255 };
        // Monk: cloth, gold and sky.
        case ATTR_HEALING_PRAYERS:    return (Color){  62, 150,  92, 255 };
        case ATTR_PROTECTION_PRAYERS: return (Color){  74, 142, 130, 255 };
        case ATTR_SMITING_PRAYERS:    return (Color){ 185, 155,  70, 255 };
        case ATTR_DIVINE_FAVOR:       return (Color){ 120, 155, 195, 255 };
        // Necromancer: blood, bone and bruise.
        case ATTR_BLOOD_MAGIC:        return (Color){ 148,  52,  62, 255 };
        case ATTR_DEATH_MAGIC:        return (Color){  92, 128,  82, 255 };
        case ATTR_CURSES:             return (Color){ 104,  74, 132, 255 };
        case ATTR_SOUL_REAPING:       return (Color){ 118,  92, 148, 255 };
        // Mesmer: court silk.
        case ATTR_DOMINATION_MAGIC:   return (Color){ 168,  74, 142, 255 };
        case ATTR_ILLUSION_MAGIC:     return (Color){ 146,  82, 160, 255 };
        case ATTR_INSPIRATION_MAGIC:  return (Color){ 178, 100, 154, 255 };
        case ATTR_FAST_CASTING:       return (Color){ 158,  88, 150, 255 };
        // Elementalist: the four schools.
        case ATTR_FIRE_MAGIC:         return (Color){ 200,  92,  40, 255 };
        case ATTR_WATER_MAGIC:        return (Color){  60, 122, 176, 255 };
        case ATTR_AIR_MAGIC:          return (Color){ 118, 150, 186, 255 };
        case ATTR_EARTH_MAGIC:        return (Color){ 140, 112,  62, 255 };
        case ATTR_ENERGY_STORAGE:     return (Color){ 120,  95, 200, 255 };
        // Monster skills: no profession, no identity colour.
        default:                      return (Color){ 100, 100, 110, 255 };
    }
}

void UI_DrawSkillIcon(int skillId, Rectangle r) {
    if (skillId < 0 || skillId >= g_skillCount) return;
    const Skill *s = &g_skillDB[skillId];

    // The school-coloured tile is still drawn here rather than baked into
    // the art: it means the palette can change without regenerating the
    // atlas, and a skill reads as "Fire" from its colour before you've
    // even parsed the glyph.
    Color school = SchoolColor(s->attribute);
    Color dark = (Color){ (unsigned char)(school.r / 3), (unsigned char)(school.g / 3),
                          (unsigned char)(school.b / 3), 255 };
    DrawRectangleGradientV((int)r.x, (int)r.y, (int)r.width, (int)r.height, school, dark);

    // The glyph is a white silhouette tinted on the way down, so the
    // atlas stays one greyscale sheet (ui_icons.c).
    Color ink = (Color){ 245, 240, 225, 255 };
    float inset = r.width * 0.12f;
    Rectangle glyph = { r.x + inset, r.y + inset, r.width - inset * 2, r.height - inset * 2 };
    if (UIIcons_Ready()) {
        // A dropped shadow one pixel down: the art is a flat silhouette
        // and needs the separation to read against a bright tile.
        UIIcons_Draw(skillId, (Rectangle){ glyph.x, glyph.y + 2, glyph.width, glyph.height },
                     (Color){ 0, 0, 0, 110 });
        UIIcons_Draw(skillId, glyph, ink);
    } else {
        // No atlas: a plain diamond, still school-coloured. Same policy
        // as the font falling back to raylib's built-in.
        DrawPoly((Vector2){ r.x + r.width / 2.0f, r.y + r.height / 2.0f }, 4,
                 r.width * 0.18f, 45, ink);
    }

    // Tile edge: dark seat + a whisper of gold, matching the panels.
    DrawRectangleLinesEx(r, 2, (Color){ 10, 10, 14, 255 });
    DrawRectangleLinesEx((Rectangle){ r.x + 2, r.y + 2, r.width - 4, r.height - 4 }, 1,
                         (Color){ 255, 255, 255, 40 });
}
