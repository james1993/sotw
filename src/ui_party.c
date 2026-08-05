#include "ui_party.h"
#include "entity.h"
#include "world.h"
#include "ui_compass.h"
#include "ui_hit.h"
#include "ui_theme.h"
#include "raylib.h"
#include "ui_font.h"
#include <stdio.h>

static Rectangle g_panelRect;

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
    float scale = UI_Scale(screenHeight);
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
    // hire and dismiss from) opens on the RIGHT - stacked below the
    // compass, which owns the top-right corner.
    bool outpost = (World_GetMode() == MODE_OUTPOST);
    int x = outpost ? screenWidth - panelW - (int)(14 * scale) : (int)(14 * scale);
    int y = outpost ? (int)UI_CompassBottom(screenHeight) + (int)(24 * scale) : (int)(80 * scale);
    g_panelRect = (Rectangle){ (float)x, (float)y, (float)panelW, (float)panelH };
    UIHit_Claim(g_panelRect);

    UI_ThemePanel(g_panelRect, scale, 0);
    UIText("Party", x + pad, y - font - 4, font, UI_GOLD);

    bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    Vector2 mouse = GetMousePosition();

    Entity *player = Entity_Get(0);
    int playerTarget = player ? Entity_RefIndex(player->targetRef) : -1;
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
            player->targetRef = Entity_RefOf(i);
            playerTarget = i;
        }
        if (rowHovered) {
            DrawRectangleRec(rowRect, (Color){ 50, 55, 75, 120 });
        }
        // Selected-member highlight
        if (playerTarget == i) {
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
                if (player && Entity_RefIndex(player->targetRef) == i) {
                    player->targetRef = Entity_NoRef();
                }
            }
        }

        int barY = rowY + font + 2;
        int barW = panelW - 2 * pad - arrow - 6; // leave room for status arrows
        float pct = (e->maxHp > 0) ? (float)e->hp / (float)e->maxHp : 0.0f;
        UI_ThemeBar((Rectangle){ (float)(x + pad), (float)barY, (float)barW, (float)barH },
                    e->alive ? pct : 0.0f, (Color){ 190, 40, 40, 255 }, NULL, font);

        // Thin energy strip under each member's health, like GW1's party
        // window gives heroes.
        int enY = barY + barH + 2;
        float smoothEn = (float)e->energy + e->energyRegenAccum / ENERGY_REGEN_INTERVAL;
        float enPct = (e->maxEnergy > 0) ? smoothEn / (float)e->maxEnergy : 0.0f;
        if (enPct > 1.0f) enPct = 1.0f;
        UI_ThemeBar((Rectangle){ (float)(x + pad), (float)enY, (float)barW, (float)energyH },
                    e->alive ? enPct : 0.0f, (Color){ 60, 130, 220, 255 }, NULL, font);

        bool hasCondition = false, hasHex = false;
        for (int j = 0; j < MAX_ACTIVE_EFFECTS; j++) {
            if (!e->effects[j].active) continue;
            if (e->effects[j].category == EFFECT_HEX) hasHex = true;
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
