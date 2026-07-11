#include "ui_target.h"
#include "entity.h"
#include "skill.h"
#include "raylib.h"
#include "ui_font.h"
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
    int barH = (int)(20 * scale);
    int font = (int)(14 * scale);
    int smallFont = (int)(10 * scale);
    int pad = (int)(4 * scale);

    int x = (screenWidth - panelW) / 2;
    int y = startY;

    Color teamColor = (target->team == 0) ? SKYBLUE : (Color){ 255, 140, 140, 255 };
    UIText(target->name, x, y, font, teamColor);
    y += font + pad;

    float hpPct = (target->maxHp > 0) ? (float)target->hp / (float)target->maxHp : 0.0f;
    DrawRectangle(x, y, panelW, barH, (Color){ 30, 30, 30, 255 });
    DrawRectangle(x, y, (int)(panelW * hpPct), barH, (target->team == 0) ? GREEN : RED);
    DrawRectangleLines(x, y, panelW, barH, BLACK);
    char hpLabel[32];
    snprintf(hpLabel, sizeof(hpLabel), "%d / %d", target->hp, target->maxHp);
    UIText(hpLabel, x + pad, y + 2, smallFont, RAYWHITE);
    y += barH + pad;

    // Only ever show the skill the target is *currently* using, or the
    // one they *just* used (for a few seconds after) - never their whole
    // kit. This matches GW1's own target bar: you have to actually watch
    // for a cast to react to it, not read it off a static skill list.
    int displaySlot = -1;
    bool isLive = false;
    if (Entity_IsCasting(target)) {
        displaySlot = target->castingSlot;
        isLive = true;
    } else if (target->postCastDisplayTimer > 0.0f && target->lastCastSkillSlot >= 0) {
        displaySlot = target->lastCastSkillSlot;
        isLive = false;
    }

    if (displaySlot >= 0) {
        int skillIdx = target->skillBar[displaySlot];
        if (skillIdx >= 0 && skillIdx < g_skillCount) {
            Skill *s = &g_skillDB[skillIdx];
            int castBarH = (int)(18 * scale);

            float pct;
            Color fillColor;
            if (isLive) {
                pct = (target->castTimeTotal > 0.0f)
                    ? 1.0f - (target->castTimeRemaining / target->castTimeTotal) : 1.0f;
                fillColor = SKYBLUE;
            } else if (target->lastCastInterrupted) {
                pct = 1.0f;
                fillColor = (Color){ 180, 70, 70, 255 };
            } else {
                pct = 1.0f;
                fillColor = (Color){ 80, 170, 90, 255 };
            }

            DrawRectangle(x, y, panelW, castBarH, (Color){ 30, 30, 30, 255 });
            DrawRectangle(x, y, (int)(panelW * pct), castBarH, fillColor);
            DrawRectangleLines(x, y, panelW, castBarH, BLACK);

            char label[48];
            if (!isLive && target->lastCastInterrupted) {
                snprintf(label, sizeof(label), "%s (interrupted)", s->name);
            } else {
                snprintf(label, sizeof(label), "%s", s->name);
            }
            UIText(label, x + pad, y + 2, smallFont, s->isElite ? GOLD : RAYWHITE);
            y += castBarH + pad;
        }
    }

    if (target->interruptFlashTimer > 0.0f) {
        const char *label = "INTERRUPTED!";
        int tw = UITextWidth(label, font);
        UIText(label, x + (panelW - tw) / 2, y, font, GOLD);
    }
}
