#include "ui_party.h"
#include "entity.h"
#include "world.h"
#include "ui_compass.h"
#include "ui_hit.h"
#include "ui_theme.h"
#include "ui_healthbar.h"
#include "raylib.h"
#include "ui_font.h"
#include <stdio.h>

static Rectangle g_panelRect;

float UI_PartyPanelBottom(void) {
    return g_panelRect.y + g_panelRect.height;
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
    // Always on the right, stacked under the compass. It used to jump to
    // the left in explorable areas, which meant the one window you watch
    // constantly moved every time you left town - and the quest tracker
    // now owns the top-left corner, where GW1 puts it.
    bool outpost = (World_GetMode() == MODE_OUTPOST);
    int x = screenWidth - panelW - (int)(14 * scale);
    int y = (int)UI_CompassBottom(screenHeight) + (int)(24 * scale);
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
        // Minions are allied creatures, not party members - GW1 keeps them
        // out of the party window even though they fight on your side.
        if (e->team != 0 || e->kind == ENT_NPC || e->isMinion) continue;

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

        // Level, right-aligned on the name row. Henchmen scale to the
        // party leader, so this is the one place you can see that they
        // have - and it keeps clear of the dismiss button, which claims
        // the same corner in outposts.
        {
            char lvl[16];
            snprintf(lvl, sizeof(lvl), "Lv %d", e->level);
            int reserve = (e->isHenchman && outpost) ? (int)(14 * scale) + 4 : 0;
            int lw = UITextWidth(lvl, font);
            UIText(lvl, x + panelW - pad - reserve - lw, rowY, font, UI_TEXT_SECOND);
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
        UIHealthBar_Draw((Rectangle){ (float)(x + pad), (float)barY, (float)barW, (float)barH },
                         e, font, false);

        // Thin energy strip under each member's health, like GW1's party
        // window gives heroes.
        int enY = barY + barH + 2;
        float smoothEn = (float)e->energy + e->energyRegenAccum / Entity_EnergyRegenInterval(e);
        float enPct = (e->maxEnergy > 0) ? smoothEn / (float)e->maxEnergy : 0.0f;
        if (enPct > 1.0f) enPct = 1.0f;
        UI_ThemeBar((Rectangle){ (float)(x + pad), (float)enY, (float)barW, (float)energyH },
                    e->alive ? enPct : 0.0f, (Color){ 60, 130, 220, 255 }, NULL, font);

        UIHealthBar_DrawStatusArrows(x + pad + barW + 4, barY, arrow, e);

        rowY += rowH;
    }
}
