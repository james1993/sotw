#include "ui_skillbar.h"
#include "entity.h"
#include "skill.h"
#include "progression.h"
#include "world.h"
#include "ui_font.h"
#include "ui_hit.h"
#include "ui_theme.h"
#include "ui_healthbar.h"
#include "ui_tooltip.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>

#define PLAYER_INDEX 0

// One shared layout so the skill bar and the health/energy bars flanking
// it (GW1-style: HP to the left of the bar, energy to the right, all
// bottom-center) always agree on geometry.
typedef struct {
    float scale;
    int slotSize, gap, font;
    int startX, y;        // skill bar origin
    int totalWidth;
} HudLayout;

static HudLayout ComputeHudLayout(int screenWidth, int screenHeight) {
    HudLayout L;
    L.scale = UI_Scale(screenHeight);
    L.slotSize = (int)(56 * L.scale);
    L.gap = (int)(6 * L.scale);
    L.font = (int)(10 * L.scale);
    L.totalWidth = SKILL_BAR_SIZE * L.slotSize + (SKILL_BAR_SIZE - 1) * L.gap;
    L.startX = (screenWidth - L.totalWidth) / 2;
    L.y = screenHeight - L.slotSize - (int)(20 * L.scale);
    return L;
}

// Controller glyph for a skill slot ("L2+A" ... "R2+Y"), shown only
// while a gamepad is connected. Order matches input.c's mapping.
static const char *SlotGamepadGlyph(int slot) {
    static const char *glyphs[SKILL_BAR_SIZE] = {
        "L2+A", "L2+B", "L2+X", "L2+Y",
        "R2+A", "R2+B", "R2+X", "R2+Y",
    };
    return (slot >= 0 && slot < SKILL_BAR_SIZE) ? glyphs[slot] : "";
}

void UI_DrawSkillBar(int screenWidth, int screenHeight) {
    Entity *player = Entity_Get(PLAYER_INDEX);
    if (!player) return;

    HudLayout L = ComputeHudLayout(screenWidth, screenHeight);
    bool pad = IsGamepadAvailable(0);

    // The skill bar and the XP strip below it are UI, not walkable ground.
    UIHit_Claim((Rectangle){ (float)L.startX, (float)L.y,
                             (float)L.totalWidth, (float)(screenHeight - L.y) });

    // A backing rail behind the slots and the resource bars above them,
    // so the bottom HUD reads as one mounted assembly instead of loose
    // tiles floating over the world.
    {
        int railPad = (int)(9 * L.scale);
        int railTop = L.y - (int)(52 * L.scale);
        Rectangle rail = { (float)(L.startX - railPad), (float)railTop,
                           (float)(L.totalWidth + railPad * 2),
                           (float)(screenHeight - railTop) };
        DrawRectangleGradientV((int)rail.x, (int)rail.y, (int)rail.width, (int)rail.height,
                               (Color){ 20, 21, 28, 150 }, (Color){ 12, 12, 16, 225 });
        DrawRectangle((int)rail.x, (int)rail.y, (int)rail.width, 1, UI_GOLD_DIM);
        // Short gold uprights at either end frame the assembly.
        DrawRectangle((int)rail.x, (int)rail.y, 1, (int)rail.height, UI_GOLD_DIM);
        DrawRectangle((int)(rail.x + rail.width) - 1, (int)rail.y, 1, (int)rail.height, UI_GOLD_DIM);
    }

    for (int i = 0; i < SKILL_BAR_SIZE; i++) {
        int x = L.startX + i * (L.slotSize + L.gap);
        Rectangle slotRect = { (float)x, (float)L.y, (float)L.slotSize, (float)L.slotSize };

        int skillIdx = player->skillBar[i];
        if (skillIdx >= 0) {
            // GW1 shows icons, not names, in the bar; the name appears
            // as a hover tooltip instead.
            UI_DrawSkillIcon(skillIdx, slotRect);
        } else {
            DrawRectangleRec(slotRect, (Color){ 22, 22, 27, 255 });
            DrawRectangleLinesEx(slotRect, 2, (Color){ 10, 10, 14, 255 });
            DrawRectangleLinesEx((Rectangle){ slotRect.x + 2, slotRect.y + 2,
                                              slotRect.width - 4, slotRect.height - 4 },
                                 1, (Color){ 60, 58, 52, 255 });
        }

        if (skillIdx >= 0) {
            Skill *s = &g_skillDB[skillIdx];
            if (s->isElite) {
                // Elite skills get GW1's gold frame.
                DrawRectangleLinesEx(slotRect, 2, UI_GOLD);
            }
            // The bar shows icons, so hovering is the only way to read a
            // skill. It gets the same full tooltip the build editor
            // does - a bare name told you nothing you couldn't already
            // see from the icon.
            if (CheckCollisionPointRec(GetMousePosition(), slotRect)) {
                UITooltip_Request(skillIdx, slotRect);
            }

            // Activation: GW1 lights the slot it is casting from, and
            // without that there is nothing on screen tying the pause
            // before a spell lands to the button you pressed.
            if (player->castingSlot == i && player->castTimeRemaining > 0.0f &&
                player->castTimeTotal > 0.0f) {
                float prog = 1.0f - player->castTimeRemaining / player->castTimeTotal;
                if (prog < 0.0f) prog = 0.0f;
                if (prog > 1.0f) prog = 1.0f;
                // A wipe that fills the slot as the cast completes, so
                // progress is legible at a glance without reading a number.
                DrawRectangle(x, L.y, (int)(L.slotSize * prog), L.slotSize,
                              (Color){ 236, 208, 130, 70 });
                float pulse = 0.6f + 0.4f * sinf((float)GetTime() * 12.0f);
                DrawRectangleLinesEx(slotRect, 3.0f, Fade(UI_GOLD, pulse));
                // And a hard edge at the wavefront, which is what makes
                // a short cast readable at all.
                DrawRectangle(x + (int)(L.slotSize * prog) - 1, L.y, 2, L.slotSize,
                              (Color){ 255, 240, 190, 220 });
            }

            // Adrenaline charge, on the slot that owns it. GW1 shows a
            // skill's own charge on its icon, which is the only place it
            // can go now that each adrenal skill has its own pool - one
            // shared bar could not say WHICH skill was ready.
            if (s->adrenalineCost > 0) {
                float charge = (float)player->adrenaline[i] / (float)s->adrenalineCost;
                if (charge > 1.0f) charge = 1.0f;
                int fillH = (int)(L.slotSize * charge);
                DrawRectangle(x, L.y + (L.slotSize - fillH), (int)(4 * L.scale), fillH,
                              (Color){ 226, 176, 60, 230 });
                if (charge >= 1.0f) {
                    // Ready: a full gold edge, so a charged skill reads
                    // at a glance in a fight.
                    DrawRectangleLinesEx(slotRect, 2.0f, (Color){ 236, 196, 90, 255 });
                }
            }

            if (player->skillRecharge[i] > 0.0f) {
                float pct = player->skillRecharge[i] / s->recharge;
                if (pct > 1.0f) pct = 1.0f;
                int overlayH = (int)(L.slotSize * pct);
                DrawRectangle(x, L.y + (L.slotSize - overlayH), L.slotSize, overlayH, (Color){ 0, 0, 0, 160 });
                char buf[8];
                snprintf(buf, sizeof(buf), "%.1f", player->skillRecharge[i]);
                UIText(buf, x + 4, L.y + L.slotSize - (int)(16 * L.scale), L.font, GOLD);
            }

            // Ally-targeted skills can never land on a foe - GW1 falls
            // them back to the caster. Say so on the slot BEFORE it is
            // pressed, because a heal that silently redirected looked
            // from the outside like a heal aimed at the enemy.
            if (s->targeting == TARGET_SINGLE_ALLY) {
                Entity *t = Entity_Resolve(player->targetRef);
                bool validAlly = t && t->alive && t->team == player->team;
                if (!validAlly) {
                    const char *tag = "self";
                    int tw = UITextWidth(tag, L.font);
                    UIText(tag, x + (L.slotSize - tw) / 2, L.y + 2, L.font,
                           (Color){ 150, 210, 160, 230 });
                }
            }

            char costBuf[16] = { 0 };
            if (s->energyCost > 0) snprintf(costBuf, sizeof(costBuf), "%dE", s->energyCost);
            else if (s->adrenalineCost > 0) snprintf(costBuf, sizeof(costBuf), "%dAd", s->adrenalineCost);
            UIText(costBuf, x + 4, L.y + L.slotSize - (int)(30 * L.scale), L.font, SKYBLUE);
        }

        // Bottom-right activation label: controller glyph while a pad is
        // connected, keyboard number otherwise.
        if (pad) {
            const char *glyph = SlotGamepadGlyph(i);
            int gw = UITextWidth(glyph, L.font);
            UIText(glyph, x + L.slotSize - gw - 3, L.y + L.slotSize - (int)(14 * L.scale), L.font, (Color){ 150, 200, 150, 255 });
        } else {
            char keyLabel[4];
            snprintf(keyLabel, sizeof(keyLabel), "%d", i + 1);
            UIText(keyLabel, x + L.slotSize - (int)(12 * L.scale), L.y + L.slotSize - (int)(14 * L.scale), L.font, LIGHTGRAY);
        }

        // Skill use is disabled in outposts (see Combat_ActivateSkill);
        // dim the bar so it reads as inactive, like GW1's grayed town bar.
        if (World_GetMode() == MODE_OUTPOST) {
            DrawRectangleRec(slotRect, (Color){ 10, 10, 12, 150 });
        }
    }

    // Level + XP progress: a thin strip along the bottom edge of the
    // screen (GW1's spot for it) with the level label centered inside.
    // Our own flat styling and gold accent - position is the borrowed
    // convention, not the look.
    {
        int xpH = (int)(10 * L.scale);
        int xpY = screenHeight - xpH - 3;
        float xpPct = 1.0f;
        if (player->level < MAX_LEVEL) {
            int need = Progression_XPToNext(player->level);
            xpPct = (need > 0) ? (float)player->xp / (float)need : 0.0f;
        }
        UI_ThemeBar((Rectangle){ (float)L.startX, (float)xpY, (float)L.totalWidth, (float)xpH },
                    xpPct, (Color){ 160, 140, 60, 255 }, NULL, L.font);

        char lvl[32];
        snprintf(lvl, sizeof(lvl), "Level: %d", player->level);
        int lw = UITextWidth(lvl, L.font);
        UIText(lvl, L.startX + (L.totalWidth - lw) / 2, xpY - 1, L.font, RAYWHITE);
    }
}

// Beveled bar with the current value centered inside - themed chrome;
// the placement convention (current value, centered) follows GW1.
static void DrawResourceBar(int x, int y, int w, int h, int font, float pct, Color fillColor, int value) {
    char label[16];
    snprintf(label, sizeof(label), "%d", value);
    UI_ThemeBar((Rectangle){ (float)x, (float)y, (float)w, (float)h }, pct, fillColor, label, font);
}

void UI_DrawResourceBars(int screenWidth, int screenHeight) {
    Entity *player = Entity_Get(PLAYER_INDEX);
    if (!player) return;

    HudLayout L = ComputeHudLayout(screenWidth, screenHeight);
    // GW1 placement: health left-of-center and energy right-of-center,
    // sitting directly ABOVE the skill bar rather than flanking it.
    int barW = (int)(200 * L.scale);
    int barH = (int)(18 * L.scale);
    int centerGap = (int)(14 * L.scale);
    int barY = L.y - barH - (int)(8 * L.scale);
    int centerX = L.startX + L.totalWidth / 2;

    int hpX = centerX - centerGap / 2 - barW;
    UIHit_Claim((Rectangle){ (float)(centerX - centerGap / 2 - barW), (float)barY,
                             (float)(barW * 2 + centerGap), (float)barH });
    // Your own bar gets the same condition/hex treatment the party
    // window gets - it showed nothing at all before, which meant the one
    // bar you look at most was the one that told you least.
    UIHealthBar_Draw((Rectangle){ (float)hpX, (float)barY, (float)barW, (float)barH },
                     player, L.font, true);

    // The fill uses the regen accumulator as a fractional pip so the
    // bar rises smoothly instead of jumping a notch every few seconds;
    // the number stays whole, like GW1's energy readout.
    float smoothEnergy = (float)player->energy +
                         player->energyRegenAccum / Entity_EnergyRegenInterval(player);
    int enX = centerX + centerGap / 2;
    DrawResourceBar(enX, barY, barW, barH, L.font,
                    smoothEnergy / (float)player->maxEnergy, (Color){ 60, 130, 220, 255 }, player->energy);

    // Energy regen pips: small arrows in the energy bar, GW1's language
    // for "how fast this refills". Drawn from the character's actual pip
    // count, so anything that ever moves regen moves the readout too.
    {
        int pips = player->energyRegenPips;
        if (pips < 0) pips = 0;
        int pipW = (int)(6 * L.scale);
        int pipH = barH - (int)(8 * L.scale);
        if (pipH < 4) pipH = 4;
        int px = enX + barW - (int)(10 * L.scale) - pips * (pipW + 2);
        int py = barY + (barH - pipH) / 2;
        for (int i = 0; i < pips; i++) {
            DrawTriangle((Vector2){ (float)px, (float)py },
                         (Vector2){ (float)px, (float)(py + pipH) },
                         (Vector2){ (float)(px + pipW), (float)(py + pipH / 2) },
                         (Color){ 180, 210, 240, 220 });
            px += pipW + 2;
        }
    }

}
