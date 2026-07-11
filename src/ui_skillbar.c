#include "ui_skillbar.h"
#include "entity.h"
#include "skill.h"
#include "progression.h"
#include "ui_font.h"
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
#define UI_MAX_SCALE 5.0f

static float UIScale(int screenHeight) {
    float scale = (float)screenHeight / UI_REFERENCE_HEIGHT;
    if (scale < UI_MIN_SCALE) scale = UI_MIN_SCALE;
    if (scale > UI_MAX_SCALE) scale = UI_MAX_SCALE;
    return scale;
}

int UI_ScaledFontSize(int screenHeight, int baseSize) {
    return (int)(baseSize * UIScale(screenHeight));
}

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
    L.scale = UIScale(screenHeight);
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

    for (int i = 0; i < SKILL_BAR_SIZE; i++) {
        int x = L.startX + i * (L.slotSize + L.gap);
        Rectangle slotRect = { (float)x, (float)L.y, (float)L.slotSize, (float)L.slotSize };

        int skillIdx = player->skillBar[i];
        Color base = (skillIdx >= 0) ? (Color){ 45, 45, 60, 255 } : (Color){ 25, 25, 25, 255 };
        DrawRectangleRec(slotRect, base);
        DrawRectangleLinesEx(slotRect, 2, (Color){ 180, 180, 180, 255 });

        if (skillIdx >= 0) {
            Skill *s = &g_skillDB[skillIdx];
            UIText(s->name, x + 4, L.y + 4, L.font, s->isElite ? GOLD : RAYWHITE);

            if (player->skillRecharge[i] > 0.0f) {
                float pct = player->skillRecharge[i] / s->recharge;
                if (pct > 1.0f) pct = 1.0f;
                int overlayH = (int)(L.slotSize * pct);
                DrawRectangle(x, L.y + (L.slotSize - overlayH), L.slotSize, overlayH, (Color){ 0, 0, 0, 160 });
                char buf[8];
                snprintf(buf, sizeof(buf), "%.1f", player->skillRecharge[i]);
                UIText(buf, x + 4, L.y + L.slotSize - (int)(16 * L.scale), L.font, GOLD);
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
    }

    // Level + XP progress, GW1-style thin strip tucked under the bar.
    {
        int xpH = (int)(6 * L.scale);
        int xpY = L.y + L.slotSize + 4;
        float xpPct = 1.0f;
        if (player->level < MAX_LEVEL) {
            int need = Progression_XPToNext(player->level);
            xpPct = (need > 0) ? (float)player->xp / (float)need : 0.0f;
        }
        DrawRectangle(L.startX, xpY, L.totalWidth, xpH, (Color){ 30, 30, 30, 255 });
        DrawRectangle(L.startX, xpY, (int)(L.totalWidth * xpPct), xpH, (Color){ 200, 180, 80, 255 });
        DrawRectangleLines(L.startX, xpY, L.totalWidth, xpH, BLACK);

        char lvl[32];
        snprintf(lvl, sizeof(lvl), "Lv %d", player->level);
        UIText(lvl, L.startX - UITextWidth(lvl, L.font) - 8, xpY - 2, L.font, (Color){ 200, 180, 80, 255 });
    }
}

static void DrawResourceBar(int x, int y, int w, int h, int font, float pct, Color fillColor, const char *label) {
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;
    DrawRectangle(x, y, w, h, (Color){ 30, 30, 30, 255 });
    DrawRectangle(x, y, (int)(w * pct), h, fillColor);
    DrawRectangleLines(x, y, w, h, BLACK);
    UIText(label, x + 4, y + 2, font, RAYWHITE);
}

void UI_DrawResourceBars(int screenWidth, int screenHeight) {
    Entity *player = Entity_Get(PLAYER_INDEX);
    if (!player) return;

    HudLayout L = ComputeHudLayout(screenWidth, screenHeight);
    // GW1 HUD placement: health bar immediately left of the skill bar,
    // energy bar immediately right, on the same row - not stacked in a
    // corner.
    int barW = (int)(200 * L.scale);
    int barH = (int)(20 * L.scale);
    int gap = (int)(10 * L.scale);
    int barY = L.y + (L.slotSize - barH) / 2;

    char hpLabel[32], enLabel[32];
    snprintf(hpLabel, sizeof(hpLabel), "%d/%d", player->hp, player->maxHp);
    snprintf(enLabel, sizeof(enLabel), "%d/%d", player->energy, player->maxEnergy);

    int hpX = L.startX - gap - barW;
    DrawResourceBar(hpX, barY, barW, barH, L.font,
                    (float)player->hp / (float)player->maxHp, (Color){ 190, 40, 40, 255 }, hpLabel);

    // Adrenaline is a Warrior mechanic; a Monk bar has no adrenaline
    // skills, so only show the strip when something equipped uses it.
    bool anyAdrenalineSkill = false;
    for (int i = 0; i < SKILL_BAR_SIZE; i++) {
        int idx = player->skillBar[i];
        if (idx >= 0 && idx < g_skillCount && g_skillDB[idx].adrenalineCost > 0) {
            anyAdrenalineSkill = true;
            break;
        }
    }
    if (anyAdrenalineSkill) {
        char adLabel[32];
        snprintf(adLabel, sizeof(adLabel), "Adr %d%%", player->adrenaline);
        int adrH = (int)(8 * L.scale);
        DrawResourceBar(hpX, barY + barH + 4, barW, adrH, L.font - 2 > 6 ? L.font - 2 : 6,
                        player->adrenaline / 100.0f, GOLD, adLabel);
    }

    int enX = L.startX + L.totalWidth + gap;
    DrawResourceBar(enX, barY, barW, barH, L.font,
                    (float)player->energy / (float)player->maxEnergy, (Color){ 60, 130, 220, 255 }, enLabel);
}
