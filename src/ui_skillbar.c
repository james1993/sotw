#include "ui_skillbar.h"
#include "entity.h"
#include "skill.h"
#include "raylib.h"
#include <stdio.h>

#define PLAYER_INDEX 0

// Every UI dimension below is defined at this reference window height,
// then multiplied by UIScale() at draw time. This keeps the skill bar,
// resource bars, and text a consistent proportion of the window instead
// of staying a fixed pixel size that shrinks to nothing on a large,
// high-res window (or a fixed size that overflows a small one).
#define UI_REFERENCE_HEIGHT 800.0f
#define UI_MIN_SCALE 0.85f
#define UI_MAX_SCALE 2.5f

static float UIScale(int screenHeight) {
    float scale = (float)screenHeight / UI_REFERENCE_HEIGHT;
    if (scale < UI_MIN_SCALE) scale = UI_MIN_SCALE;
    if (scale > UI_MAX_SCALE) scale = UI_MAX_SCALE;
    return scale;
}

int UI_ScaledFontSize(int screenHeight, int baseSize) {
    return (int)(baseSize * UIScale(screenHeight));
}

void UI_DrawSkillBar(int screenWidth, int screenHeight) {
    Entity *player = Entity_Get(PLAYER_INDEX);
    if (!player) return;

    float scale = UIScale(screenHeight);
    int slotSize = (int)(56 * scale);
    int gap = (int)(6 * scale);
    int font = (int)(10 * scale);
    int totalWidth = SKILL_BAR_SIZE * slotSize + (SKILL_BAR_SIZE - 1) * gap;
    int startX = (screenWidth - totalWidth) / 2;
    int y = screenHeight - slotSize - (int)(20 * scale);

    for (int i = 0; i < SKILL_BAR_SIZE; i++) {
        int x = startX + i * (slotSize + gap);
        Rectangle slotRect = { (float)x, (float)y, (float)slotSize, (float)slotSize };

        int skillIdx = player->skillBar[i];
        Color base = (skillIdx >= 0) ? (Color){ 45, 45, 60, 255 } : (Color){ 25, 25, 25, 255 };
        DrawRectangleRec(slotRect, base);
        DrawRectangleLinesEx(slotRect, 2, (Color){ 180, 180, 180, 255 });

        if (skillIdx >= 0) {
            Skill *s = &g_skillDB[skillIdx];
            DrawText(s->name, x + 4, y + 4, font, s->isElite ? GOLD : RAYWHITE);

            if (player->skillRecharge[i] > 0.0f) {
                float pct = player->skillRecharge[i] / s->recharge;
                if (pct > 1.0f) pct = 1.0f;
                int overlayH = (int)(slotSize * pct);
                DrawRectangle(x, y + (slotSize - overlayH), slotSize, overlayH, (Color){ 0, 0, 0, 160 });
                char buf[8];
                snprintf(buf, sizeof(buf), "%.1f", player->skillRecharge[i]);
                DrawText(buf, x + 4, y + slotSize - (int)(16 * scale), font, GOLD);
            }

            char costBuf[16] = { 0 };
            if (s->energyCost > 0) snprintf(costBuf, sizeof(costBuf), "%dE", s->energyCost);
            else if (s->adrenalineCost > 0) snprintf(costBuf, sizeof(costBuf), "%dAd", s->adrenalineCost);
            DrawText(costBuf, x + 4, y + slotSize - (int)(30 * scale), font, SKYBLUE);
        }

        char keyLabel[4];
        snprintf(keyLabel, sizeof(keyLabel), "%d", i + 1);
        DrawText(keyLabel, x + slotSize - (int)(12 * scale), y + slotSize - (int)(14 * scale), font, LIGHTGRAY);
    }
}

static void DrawResourceBar(int x, int y, int w, int h, int font, float pct, Color fillColor, const char *label) {
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;
    DrawRectangle(x, y, w, h, (Color){ 30, 30, 30, 255 });
    DrawRectangle(x, y, (int)(w * pct), h, fillColor);
    DrawRectangleLines(x, y, w, h, BLACK);
    DrawText(label, x + 4, y + 2, font, RAYWHITE);
}

void UI_DrawResourceBars(int screenWidth, int screenHeight) {
    (void)screenWidth;
    Entity *player = Entity_Get(PLAYER_INDEX);
    if (!player) return;

    float scale = UIScale(screenHeight);
    int barW = (int)(220 * scale), barH = (int)(18 * scale);
    int font = (int)(10 * scale);
    int gap = (int)(6 * scale);
    int x = (int)(20 * scale), y = screenHeight - (int)(120 * scale);

    char hpLabel[32], enLabel[32], adLabel[32];
    snprintf(hpLabel, sizeof(hpLabel), "HP %d/%d", player->hp, player->maxHp);
    snprintf(enLabel, sizeof(enLabel), "Energy %d/%d", player->energy, player->maxEnergy);
    snprintf(adLabel, sizeof(adLabel), "Adrenaline %d%%", player->adrenaline);

    DrawResourceBar(x, y, barW, barH, font, (float)player->hp / (float)player->maxHp, RED, hpLabel);
    DrawResourceBar(x, y + barH + gap, barW, barH, font, (float)player->energy / (float)player->maxEnergy, SKYBLUE, enLabel);
    DrawResourceBar(x, y + 2 * (barH + gap), barW, barH, font, player->adrenaline / 100.0f, GOLD, adLabel);
}
