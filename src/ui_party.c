#include "ui_party.h"
#include "entity.h"
#include "raylib.h"
#include "ui_font.h"
#include <stdio.h>

static float UIScale(int screenHeight) {
    float scale = (float)screenHeight / 800.0f;
    if (scale < 0.85f) scale = 0.85f;
    if (scale > 5.0f) scale = 5.0f;
    return scale;
}

// Small downward-pointing triangle, the GW1 party-window shorthand for
// "something is on this player": brown = condition, purple = hex.
static void DrawStatusArrow(int x, int y, int size, Color color) {
    DrawTriangle(
        (Vector2){ (float)x, (float)y },
        (Vector2){ (float)(x + size), (float)y },
        (Vector2){ (float)(x + size / 2), (float)(y + size) },
        color);
}

void UI_DrawPartyPanel(int screenWidth, int screenHeight) {
    float scale = UIScale(screenHeight);
    int panelW = (int)(200 * scale);
    int rowH = (int)(38 * scale);
    int pad = (int)(8 * scale);
    int font = (int)(11 * scale);
    int barH = (int)(12 * scale);
    int arrow = (int)(10 * scale);

    // Count party members first so the panel hugs its contents.
    int members = 0;
    for (int i = 0; i < g_entityCount; i++) {
        if (g_entities[i].team == 0) members++;
    }
    if (members == 0) return;

    int panelH = pad * 2 + members * rowH;
    int x = screenWidth - panelW - (int)(14 * scale);
    int y = (int)(80 * scale);

    DrawRectangle(x, y, panelW, panelH, (Color){ 20, 22, 30, 210 });
    DrawRectangleLines(x, y, panelW, panelH, (Color){ 120, 120, 140, 255 });
    UIText("Party", x + pad, y - font - 4, font, LIGHTGRAY);

    int rowY = y + pad;
    for (int i = 0; i < g_entityCount; i++) {
        Entity *e = &g_entities[i];
        if (e->team != 0) continue;

        Color nameColor = e->alive ? RAYWHITE : (Color){ 130, 130, 130, 255 };
        UIText(e->name, x + pad, rowY, font, nameColor);

        int barY = rowY + font + 2;
        int barW = panelW - 2 * pad - arrow - 6; // leave room for status arrows
        float pct = (e->maxHp > 0) ? (float)e->hp / (float)e->maxHp : 0.0f;
        DrawRectangle(x + pad, barY, barW, barH, (Color){ 40, 40, 40, 255 });
        if (e->alive) {
            DrawRectangle(x + pad, barY, (int)(barW * pct), barH, (Color){ 190, 40, 40, 255 });
        }
        DrawRectangleLines(x + pad, barY, barW, barH, BLACK);

        bool hasCondition = false, hasHex = false;
        for (int j = 0; j < MAX_ACTIVE_EFFECTS; j++) {
            if (!e->effects[j].active) continue;
            if (e->effects[j].isHex) hasHex = true;
            else hasCondition = true;
        }
        int arrowX = x + pad + barW + 4;
        if (hasCondition) {
            DrawStatusArrow(arrowX, barY, arrow, (Color){ 165, 110, 50, 255 });
        }
        if (hasHex) {
            DrawStatusArrow(arrowX, barY + (hasCondition ? arrow + 2 : 0), arrow, (Color){ 150, 70, 200, 255 });
        }

        rowY += rowH;
    }
}
