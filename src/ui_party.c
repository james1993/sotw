#include "ui_party.h"
#include "entity.h"
#include "world.h"
#include "raylib.h"
#include "ui_font.h"
#include <stdio.h>

static Rectangle g_panelRect;

static float UIScale(int screenHeight) {
    float scale = (float)screenHeight / 800.0f;
    if (scale < 0.85f) scale = 0.85f;
    if (scale > 5.0f) scale = 5.0f;
    return scale;
}

bool UI_PartyPanelContains(Vector2 point) {
    return CheckCollisionPointRec(point, g_panelRect);
}

float UI_PartyPanelBottom(void) {
    return g_panelRect.y + g_panelRect.height;
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
    int rowH = (int)(46 * scale); // room for a thin energy bar per member
    int pad = (int)(8 * scale);
    int font = (int)(11 * scale);
    int barH = (int)(12 * scale);
    int energyH = (int)(5 * scale);
    int arrow = (int)(10 * scale);

    // Count party members first so the panel hugs its contents.
    int members = 0;
    for (int i = 0; i < g_entityCount; i++) {
        if (g_entities[i].team == 0 && g_entities[i].kind != ENT_NPC) members++;
    }
    if (members == 0) {
        g_panelRect = (Rectangle){ 0 };
        return;
    }

    int panelH = pad * 2 + members * rowH;
    // GW1 uses both positions: the in-mission party health list sits
    // top-LEFT, while the outpost party-formation window (the one you
    // hire and dismiss from) opens on the RIGHT. Our panel plays both
    // roles, so it follows the mode.
    int x = (World_GetMode() == MODE_OUTPOST)
        ? screenWidth - panelW - (int)(14 * scale)
        : (int)(14 * scale);
    int y = (int)(80 * scale);
    g_panelRect = (Rectangle){ (float)x, (float)y, (float)panelW, (float)panelH };

    DrawRectangle(x, y, panelW, panelH, (Color){ 20, 22, 30, 210 });
    DrawRectangleLines(x, y, panelW, panelH, (Color){ 120, 120, 140, 255 });
    UIText("Party", x + pad, y - font - 4, font, LIGHTGRAY);

    bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    Vector2 mouse = GetMousePosition();

    Entity *player = Entity_Get(0);
    int rowY = y + pad;
    for (int i = 0; i < g_entityCount; i++) {
        Entity *e = &g_entities[i];
        if (e->team != 0 || e->kind == ENT_NPC) continue;

        // Clicking a member's row selects them - how GW1 targets allies
        // for heals from the party window. (The dismiss button, drawn
        // later over this row, wins when hovered because its click
        // handler converts the entity before targeting matters.)
        Rectangle rowRect = { (float)x + 2, (float)rowY - 2, (float)panelW - 4, (float)rowH - 4 };
        bool rowHovered = CheckCollisionPointRec(mouse, rowRect);
        if (rowHovered && click && e->alive && player) {
            player->targetIndex = i;
        }
        if (rowHovered) {
            DrawRectangleRec(rowRect, (Color){ 50, 55, 75, 120 });
        }
        // Selected-member highlight
        if (player && player->targetIndex == i) {
            DrawRectangleLinesEx(rowRect, 1, GOLD);
        }

        Color nameColor = e->alive ? RAYWHITE : (Color){ 130, 130, 130, 255 };
        UIText(e->name, x + pad, rowY, font, nameColor);

        // GW1 shows each member's stacked death penalty in the party list.
        if (e->deathPenalty > 0) {
            char dp[16];
            snprintf(dp, sizeof(dp), "-%d%%", e->deathPenalty);
            int nameW = UITextWidth(e->name, font);
            UIText(dp, x + pad + nameW + 8, rowY, font, (Color){ 220, 120, 120, 255 });
        }

        // Dismiss button for hired henchmen - outposts only, GW1's rule
        // for changing party composition.
        if (e->isHenchman && World_GetMode() == MODE_OUTPOST) {
            int btn = (int)(14 * scale);
            Rectangle dismiss = { (float)(x + panelW - pad - btn), (float)rowY, (float)btn, (float)btn };
            bool hovered = CheckCollisionPointRec(mouse, dismiss);
            DrawRectangleRec(dismiss, hovered ? (Color){ 140, 60, 60, 255 } : (Color){ 70, 45, 45, 255 });
            DrawRectangleLinesEx(dismiss, 1, LIGHTGRAY);
            UIText("x", (int)dismiss.x + btn / 3, (int)dismiss.y - 1, font, RAYWHITE);
            if (hovered && click) {
                World_DismissHenchman(e);
                // The row-click handler above may have just targeted him.
                if (player && player->targetIndex == i) player->targetIndex = -1;
            }
        }

        int barY = rowY + font + 2;
        int barW = panelW - 2 * pad - arrow - 6; // leave room for status arrows
        float pct = (e->maxHp > 0) ? (float)e->hp / (float)e->maxHp : 0.0f;
        DrawRectangle(x + pad, barY, barW, barH, (Color){ 40, 40, 40, 255 });
        if (e->alive) {
            DrawRectangle(x + pad, barY, (int)(barW * pct), barH, (Color){ 190, 40, 40, 255 });
        }
        DrawRectangleLines(x + pad, barY, barW, barH, BLACK);

        // Thin energy strip under each member's health, like GW1's party
        // window gives heroes.
        int enY = barY + barH + 2;
        float enPct = (e->maxEnergy > 0) ? (float)e->energy / (float)e->maxEnergy : 0.0f;
        DrawRectangle(x + pad, enY, barW, energyH, (Color){ 40, 40, 40, 255 });
        if (e->alive) {
            DrawRectangle(x + pad, enY, (int)(barW * enPct), energyH, (Color){ 60, 130, 220, 255 });
        }
        DrawRectangleLines(x + pad, enY, barW, energyH, BLACK);

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
