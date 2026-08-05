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
    int barH = (int)(18 * scale);
    int font = (int)(14 * scale);
    int smallFont = (int)(10 * scale);
    int pad = (int)(8 * scale);

    int x = (screenWidth - panelW) / 2;
    int y = startY;

    // Height depends on whether a cast bar is showing, so the frame
    // always hugs its contents instead of leaving a dead strip.
    bool showCast = Entity_IsCasting(target) ||
                    (target->postCastDisplayTimer > 0.0f && target->lastCastSkillSlot >= 0);
    int castBarH = (int)(16 * scale);
    int effectCount = Entity_CountEffects(target, EFFECT_CONDITION) +
                      Entity_CountEffects(target, EFFECT_HEX);
    int chipH = (int)(15 * scale);
    int panelH = pad * 2 + font + (int)(4 * scale) + barH +
                 (showCast ? castBarH + (int)(4 * scale) : 0) +
                 (effectCount > 0 ? chipH + (int)(4 * scale) : 0);

    Rectangle frame = { (float)x, (float)y, (float)panelW, (float)panelH };
    UIHit_Claim(frame);
    UI_ThemePanel(frame, scale, 0);

    x += pad;
    y += pad;
    int innerW = panelW - pad * 2;

    // Name on the left, level on the right - the two things you check
    // before committing to a fight.
    Color teamColor = (target->team == 0) ? (Color){ 158, 206, 255, 255 }
                                          : (Color){ 246, 138, 128, 255 };
    UIText(target->name, x, y, font, teamColor);
    if (target->kind == ENT_MONSTER && target->level > 0) {
        char lvl[24];
        snprintf(lvl, sizeof(lvl), "Level %d", target->level);
        int lw = UITextWidth(lvl, smallFont);
        UIText(lvl, x + innerW - lw, y + (font - smallFont) / 2, smallFont,
               (Color){ 206, 174, 130, 255 });
    }
    y += font + (int)(4 * scale);
    panelW = innerW;

    float hpPct = (target->maxHp > 0) ? (float)target->hp / (float)target->maxHp : 0.0f;
    char hpLabel[32];
    snprintf(hpLabel, sizeof(hpLabel), "%d / %d", target->hp, target->maxHp);
    UI_ThemeBar((Rectangle){ (float)x, (float)y, (float)panelW, (float)barH }, hpPct,
                (target->team == 0) ? (Color){ 70, 170, 90, 255 } : (Color){ 185, 45, 45, 255 },
                hpLabel, smallFont);
    y += barH + (int)(4 * scale);

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

    // --- Afflictions, named and counting down ---
    // The nameplate pips answer "is anything on it?" at a glance; the
    // focused target is where you get to read WHAT and HOW LONG, which
    // is the information a cleanse decision actually needs.
    if (effectCount > 0) {
        int chipX = x;
        for (int i = 0; i < MAX_ACTIVE_EFFECTS; i++) {
            const ActiveEffect *fx = &target->effects[i];
            if (!fx->active) continue;

            char label[48];
            snprintf(label, sizeof(label), "%s %.0fs", Entity_EffectName(fx), fx->remaining);
            int tw = UITextWidth(label, smallFont);
            int chipW = tw + (int)(14 * scale);
            if (chipX + chipW > x + panelW) break; // out of room; the pips still show the rest

            Color c = Entity_EffectColor(fx);
            Rectangle chip = { (float)chipX, (float)y, (float)chipW, (float)chipH };
            DrawRectangleRounded(chip, 0.4f, 6, (Color){ c.r / 5, c.g / 5, c.b / 5, 235 });
            DrawRectangleRoundedLines(chip, 0.4f, 6, c);
            // Hexes get a leading diamond so the category reads without
            // relying on color alone.
            int textX = chipX + (int)(6 * scale);
            if (fx->category == EFFECT_HEX) {
                DrawPoly((Vector2){ (float)(chipX + 6 * scale), (float)(y + chipH / 2) },
                         4, 3.0f * scale, 45.0f, c);
                textX += (int)(7 * scale);
            }
            UIText(label, textX, y + (chipH - smallFont) / 2, smallFont, c);
            chipX += chipW + (int)(4 * scale);
        }
        y += chipH + (int)(4 * scale);
    }

    if (target->interruptFlashTimer > 0.0f) {
        const char *label = "INTERRUPTED!";
        int tw = UITextWidth(label, font);
        UIText(label, x + (panelW - tw) / 2, y, font, GOLD);
    }
}
