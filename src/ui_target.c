#include "ui_target.h"
#include "entity.h"
#include "skill.h"
#include "raylib.h"
#include "ui_font.h"
#include "ui_hit.h"
#include "ui_theme.h"
#include <stdio.h>

#define PLAYER_INDEX 0

void UI_DrawTargetPanel(int screenWidth, int screenHeight, int startY) {
    Entity *player = Entity_Get(PLAYER_INDEX);
    if (!player) return;
    Entity *target = Entity_Resolve(player->targetRef);
    if (!target || !target->alive) return;

    float scale = UI_Scale(screenHeight);
    int panelW = (int)(340 * scale);
    int barH = (int)(20 * scale);
    int font = (int)(14 * scale);
    int smallFont = (int)(10 * scale);
    int pad = (int)(4 * scale);

    int x = (screenWidth - panelW) / 2;
    int y = startY;

    // Claim the panel's maximum footprint (name + HP bar + cast bar) so
    // clicks near the top-center HUD never leak into the world.
    UIHit_Claim((Rectangle){ (float)x, (float)y,
                             (float)panelW, (float)(font + barH + (int)(18 * scale) + pad * 3) });

    Color teamColor = (target->team == 0) ? SKYBLUE : (Color){ 255, 140, 140, 255 };
    UIText(target->name, x, y, font, teamColor);
    y += font + pad;

    float hpPct = (target->maxHp > 0) ? (float)target->hp / (float)target->maxHp : 0.0f;
    char hpLabel[32];
    snprintf(hpLabel, sizeof(hpLabel), "%d / %d", target->hp, target->maxHp);
    UI_ThemeBar((Rectangle){ (float)x, (float)y, (float)panelW, (float)barH }, hpPct,
                (target->team == 0) ? (Color){ 70, 170, 90, 255 } : (Color){ 185, 45, 45, 255 },
                hpLabel, smallFont);
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

            char label[48];
            if (!isLive && target->lastCastInterrupted) {
                snprintf(label, sizeof(label), "%s (interrupted)", s->name);
            } else {
                snprintf(label, sizeof(label), "%s", s->name);
            }
            UI_ThemeBar((Rectangle){ (float)x, (float)y, (float)panelW, (float)castBarH },
                        pct, fillColor, label, smallFont);
            y += castBarH + pad;
        }
    }

    if (target->interruptFlashTimer > 0.0f) {
        const char *label = "INTERRUPTED!";
        int tw = UITextWidth(label, font);
        UIText(label, x + (panelW - tw) / 2, y, font, GOLD);
    }
}
