#include "ui_skillbar.h"
#include "entity.h"
#include "skill.h"
#include "raylib.h"
#include <stdio.h>

#define PLAYER_INDEX 0

void UI_DrawSkillBar(int screenWidth, int screenHeight) {
    Entity *player = Entity_Get(PLAYER_INDEX);
    if (!player) return;

    int slotSize = 56;
    int gap = 6;
    int totalWidth = SKILL_BAR_SIZE * slotSize + (SKILL_BAR_SIZE - 1) * gap;
    int startX = (screenWidth - totalWidth) / 2;
    int y = screenHeight - slotSize - 20;

    for (int i = 0; i < SKILL_BAR_SIZE; i++) {
        int x = startX + i * (slotSize + gap);
        Rectangle slotRect = { (float)x, (float)y, (float)slotSize, (float)slotSize };

        int skillIdx = player->skillBar[i];
        Color base = (skillIdx >= 0) ? (Color){ 45, 45, 60, 255 } : (Color){ 25, 25, 25, 255 };
        DrawRectangleRec(slotRect, base);
        DrawRectangleLinesEx(slotRect, 2, (Color){ 180, 180, 180, 255 });

        if (skillIdx >= 0) {
            Skill *s = &g_skillDB[skillIdx];
            DrawText(s->name, x + 4, y + 4, 10, s->isElite ? GOLD : RAYWHITE);

            if (player->skillRecharge[i] > 0.0f) {
                float pct = player->skillRecharge[i] / s->recharge;
                if (pct > 1.0f) pct = 1.0f;
                int overlayH = (int)(slotSize * pct);
                DrawRectangle(x, y + (slotSize - overlayH), slotSize, overlayH, (Color){ 0, 0, 0, 160 });
                char buf[8];
                snprintf(buf, sizeof(buf), "%.1f", player->skillRecharge[i]);
                DrawText(buf, x + 4, y + slotSize - 16, 10, GOLD);
            }

            char costBuf[16] = { 0 };
            if (s->energyCost > 0) snprintf(costBuf, sizeof(costBuf), "%dE", s->energyCost);
            else if (s->adrenalineCost > 0) snprintf(costBuf, sizeof(costBuf), "%dAd", s->adrenalineCost);
            DrawText(costBuf, x + 4, y + slotSize - 30, 10, SKYBLUE);
        }

        char keyLabel[4];
        snprintf(keyLabel, sizeof(keyLabel), "%d", i + 1);
        DrawText(keyLabel, x + slotSize - 12, y + slotSize - 14, 10, LIGHTGRAY);
    }
}

static void DrawResourceBar(int x, int y, int w, int h, float pct, Color fillColor, const char *label) {
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;
    DrawRectangle(x, y, w, h, (Color){ 30, 30, 30, 255 });
    DrawRectangle(x, y, (int)(w * pct), h, fillColor);
    DrawRectangleLines(x, y, w, h, BLACK);
    DrawText(label, x + 4, y + 2, 10, RAYWHITE);
}

void UI_DrawResourceBars(int screenWidth, int screenHeight) {
    (void)screenWidth;
    Entity *player = Entity_Get(PLAYER_INDEX);
    if (!player) return;

    int barW = 220, barH = 18;
    int x = 20, y = screenHeight - 120;

    char hpLabel[32], enLabel[32], adLabel[32];
    snprintf(hpLabel, sizeof(hpLabel), "HP %d/%d", player->hp, player->maxHp);
    snprintf(enLabel, sizeof(enLabel), "Energy %d/%d", player->energy, player->maxEnergy);
    snprintf(adLabel, sizeof(adLabel), "Adrenaline %d%%", player->adrenaline);

    DrawResourceBar(x, y, barW, barH, (float)player->hp / (float)player->maxHp, RED, hpLabel);
    DrawResourceBar(x, y + barH + 6, barW, barH, (float)player->energy / (float)player->maxEnergy, SKYBLUE, enLabel);
    DrawResourceBar(x, y + 2 * (barH + 6), barW, barH, player->adrenaline / 100.0f, GOLD, adLabel);
}
