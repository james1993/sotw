#include "ui_target.h"
#include "entity.h"
#include "skill.h"
#include "raylib.h"
#include <stdio.h>

#define PLAYER_INDEX 0

static float PanelScale(int screenHeight) {
    float scale = (float)screenHeight / 800.0f;
    if (scale < 0.85f) scale = 0.85f;
    if (scale > 5.0f) scale = 5.0f;
    return scale;
}

void UI_DrawTargetPanel(int screenWidth, int screenHeight, int startY) {
    Entity *player = Entity_Get(PLAYER_INDEX);
    if (!player) return;
    Entity *target = Entity_Get(player->targetIndex);
    if (!target || !target->alive) return;

    float scale = PanelScale(screenHeight);
    int panelW = (int)(340 * scale);
    int slotSize = (int)(36 * scale);
    int gap = (int)(4 * scale);
    int barH = (int)(20 * scale);
    int font = (int)(14 * scale);
    int smallFont = (int)(10 * scale);
    int pad = (int)(4 * scale);

    int x = (screenWidth - panelW) / 2;
    int y = startY;

    Color teamColor = (target->team == 0) ? SKYBLUE : (Color){ 255, 140, 140, 255 };
    DrawText(target->name, x, y, font, teamColor);
    y += font + pad;

    float hpPct = (target->maxHp > 0) ? (float)target->hp / (float)target->maxHp : 0.0f;
    DrawRectangle(x, y, panelW, barH, (Color){ 30, 30, 30, 255 });
    DrawRectangle(x, y, (int)(panelW * hpPct), barH, (target->team == 0) ? GREEN : RED);
    DrawRectangleLines(x, y, panelW, barH, BLACK);
    char hpLabel[32];
    snprintf(hpLabel, sizeof(hpLabel), "%d / %d", target->hp, target->maxHp);
    DrawText(hpLabel, x + pad, y + 2, smallFont, RAYWHITE);
    y += barH + pad;

    if (Entity_IsCasting(target)) {
        float castPct = (target->castTimeTotal > 0.0f)
            ? 1.0f - (target->castTimeRemaining / target->castTimeTotal) : 0.0f;
        int castBarH = (int)(12 * scale);
        DrawRectangle(x, y, panelW, castBarH, (Color){ 30, 30, 30, 255 });
        DrawRectangle(x, y, (int)(panelW * castPct), castBarH, SKYBLUE);
        DrawRectangleLines(x, y, panelW, castBarH, BLACK);
        int castingSkillIdx = target->skillBar[target->castingSlot];
        if (castingSkillIdx >= 0 && castingSkillIdx < g_skillCount) {
            DrawText(g_skillDB[castingSkillIdx].name, x + pad, y, smallFont, RAYWHITE);
        }
        y += castBarH + pad;
    }

    if (target->interruptFlashTimer > 0.0f) {
        const char *label = "INTERRUPTED!";
        int tw = MeasureText(label, font);
        DrawText(label, x + (panelW - tw) / 2, y, font, GOLD);
        y += font + pad;
    }

    // Read-only display of the target's equipped skills, so you can see
    // what they might use (and, combined with the cast bar above, when
    // it's worth trying to interrupt).
    int totalSkillWidth = SKILL_BAR_SIZE * slotSize + (SKILL_BAR_SIZE - 1) * gap;
    int skillX = x + (panelW - totalSkillWidth) / 2;
    for (int i = 0; i < SKILL_BAR_SIZE; i++) {
        int sx = skillX + i * (slotSize + gap);
        Rectangle rect = { (float)sx, (float)y, (float)slotSize, (float)slotSize };

        int skillIdx = target->skillBar[i];
        bool isCastingThis = Entity_IsCasting(target) && target->castingSlot == i;
        Color base = (skillIdx < 0) ? (Color){ 20, 20, 20, 255 }
            : (isCastingThis ? (Color){ 70, 110, 160, 255 } : (Color){ 45, 45, 60, 255 });
        DrawRectangleRec(rect, base);
        DrawRectangleLinesEx(rect, isCastingThis ? 2 : 1, isCastingThis ? SKYBLUE : (Color){ 140, 140, 140, 255 });

        if (skillIdx >= 0 && skillIdx < g_skillCount) {
            Skill *s = &g_skillDB[skillIdx];
            DrawText(s->name, sx + 2, y + 2, smallFont, s->isElite ? GOLD : RAYWHITE);
            if (target->skillRecharge[i] > 0.0f) {
                float pct = target->skillRecharge[i] / s->recharge;
                if (pct > 1.0f) pct = 1.0f;
                int overlayH = (int)(slotSize * pct);
                DrawRectangle(sx, y + (slotSize - overlayH), slotSize, overlayH, (Color){ 0, 0, 0, 160 });
            }
        }
    }
}
