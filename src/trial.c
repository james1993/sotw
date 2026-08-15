#include "trial.h"
#include "world.h"
#include "entity.h"
#include "ui_font.h"
#include "ui_theme.h"
#include "audio.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>

#define PLAYER_INDEX 0

// --- Run state -------------------------------------------------------
typedef enum {
    TRIAL_IDLE = 0,   // not in the arena
    TRIAL_BREATHER,   // between waves: a few seconds to reposition and recharge
    TRIAL_FIGHTING,
    TRIAL_OVER        // player down; summary showing
} TrialPhase;

static TrialPhase g_phase = TRIAL_IDLE;
static int   g_wave = 0;
static int   g_best = 0;
static float g_timer = 0.0f;      // breather countdown / banner age
static float g_bannerTime = 0.0f; // how long the wave banner has been up

#define BREATHER_SECONDS 4.0f
#define BANNER_SECONDS 2.2f

bool Trial_IsActive(void) { return g_phase != TRIAL_IDLE; }
int  Trial_Wave(void)     { return g_wave; }
int  Trial_BestWave(void) { return g_best; }
bool Trial_RunOver(void)  { return g_phase == TRIAL_OVER; }

void Trial_Start(void) {
    g_phase = TRIAL_BREATHER;
    g_wave = 0;
    g_timer = 2.0f;   // a beat before the first wave, to read your bar
    g_bannerTime = 0.0f;
}

void Trial_Stop(void) {
    g_phase = TRIAL_IDLE;  // g_best survives, so the session keeps a record
}

// --- Wave composition ------------------------------------------------
//
// The escalation is a curve, not a table, so the arena never runs out.
// What matters is the MIX, because the mix is the decision:
//
//   Grunt   - melee pressure. Ignorable individually, lethal in a pile.
//   Ash     - ranged caster. Punishes standing still and tunnel vision.
//   Shaman  - carries Feral Howl, an interruptible self-heal. THE target
//             priority question: kill it first, interrupt it, or watch
//             the wave heal back up faster than you can chew through it.
//   Warden  - boss every fifth wave. Big, hits hard, and brings friends.
//
// Shamans arrive at wave 3 - two waves to learn the buttons, then the
// game asks its real question.

static Vector2 RingPoint(int i, int count, float radius) {
    float a = (float)i / (float)(count > 0 ? count : 1) * 2.0f * PI;
    // Waves come in around the rim, so a run is a series of "they're
    // coming from over there" reads rather than a fixed conga line.
    a += (float)g_wave * 0.7f;
    return (Vector2){ cosf(a) * radius, sinf(a) * radius };
}

static void SpawnWave(int wave) {
    int level = 3 + wave;
    if (level > 20) level = 20;

    int grunts = 2 + wave / 2;
    if (grunts > 6) grunts = 6;
    int ashes  = (wave >= 2) ? 1 + (wave - 2) / 4 : 0;
    if (ashes > 3) ashes = 3;
    int shamans = (wave >= 3) ? 1 + (wave - 3) / 5 : 0;
    if (shamans > 3) shamans = 3;
    bool boss = (wave % 5 == 0);

    int total = grunts + ashes + shamans + (boss ? 1 : 0);
    int slot = 0;
    float radius = 380.0f;

    for (int i = 0; i < grunts; i++) {
        FoeSpec f = {
            .name = "Charr Grunt", .pos = RingPoint(slot++, total, radius),
            .level = level, .hp = 130 + level * 14, .armor = 40 + level,
            .strengthRank = 6 + wave / 3, .species = SPECIES_CHARR,
            .group = 1,
        };
        World_SpawnFoe(&f);
    }
    for (int i = 0; i < ashes; i++) {
        FoeSpec f = {
            .name = "Charr Ash Walker", .pos = RingPoint(slot++, total, radius),
            .level = level, .hp = 110 + level * 12, .armor = 36 + level,
            .strengthRank = 6 + wave / 3, .caster = true, .species = SPECIES_CHARR,
            .group = 1,
        };
        World_SpawnFoe(&f);
    }
    for (int i = 0; i < shamans; i++) {
        // The one that has to die (or be interrupted) first.
        FoeSpec f = {
            .name = "Charr Shaman", .pos = RingPoint(slot++, total, radius),
            .level = level, .hp = 120 + level * 12, .armor = 34 + level,
            .strengthRank = 7 + wave / 3, .caster = true, .withHowl = true,
            .species = SPECIES_CHARR, .group = 1,
        };
        World_SpawnFoe(&f);
    }
    if (boss) {
        FoeSpec f = {
            .name = "Charr Flame Warden", .pos = RingPoint(slot++, total, radius * 0.6f),
            .level = level + 2, .hp = 320 + level * 26, .armor = 50 + level,
            .strengthRank = 9 + wave / 3, .withHowl = true, .boss = true,
            .species = SPECIES_CHARR, .group = 1,
        };
        World_SpawnFoe(&f);
    }
}

static int LivingFoes(void) {
    int n = 0;
    for (int i = 0; i < g_entityCount; i++) {
        Entity *e = &g_entities[i];
        if (e->alive && e->kind == ENT_MONSTER) n++;
    }
    return n;
}

void Trial_Update(float dt) {
    if (g_phase == TRIAL_IDLE) return;

    if (g_bannerTime > 0.0f) g_bannerTime -= dt;

    Entity *player = Entity_Get(PLAYER_INDEX);
    if (!player) return;

    if (g_phase == TRIAL_OVER) return;

    // A run ends the moment you go down: no rez, no corpse run. That is
    // what gives every wave its teeth and makes the next attempt fast.
    if (!player->alive) {
        g_phase = TRIAL_OVER;
        if (g_wave > g_best) g_best = g_wave;
        return;
    }

    if (g_phase == TRIAL_BREATHER) {
        g_timer -= dt;
        if (g_timer <= 0.0f) {
            g_wave++;
            SpawnWave(g_wave);
            g_phase = TRIAL_FIGHTING;
            g_bannerTime = BANNER_SECONDS;
            Audio_Play(SFX_UI_CONFIRM);
        }
        return;
    }

    // Fighting: the wave is done when the floor is clear.
    if (LivingFoes() == 0) {
        g_phase = TRIAL_BREATHER;
        g_timer = BREATHER_SECONDS;
        if (g_wave > g_best) g_best = g_wave;
        // A breather heals, but only partly - clearing a wave at 20%
        // health should still feel like trouble carried forward.
        int heal = player->maxHp / 3;
        player->hp += heal;
        if (player->hp > player->maxHp) player->hp = player->maxHp;
        player->energy = player->maxEnergy;
    }
}

void Trial_Draw(int screenWidth, int screenHeight) {
    if (g_phase == TRIAL_IDLE) return;
    float scale = UI_Scale(screenHeight);
    int font = (int)(14 * scale);
    int big = (int)(34 * scale);

    // Wave + remaining, top centre - the two numbers a run is about.
    // Sits BELOW the persistent zone-name line (ui_hints.c draws that at
    // the very top), so the two don't print over each other.
    {
        int top = (int)(36 * scale);
        char line[64];
        snprintf(line, sizeof(line), "WAVE %d", g_wave > 0 ? g_wave : 1);
        int w = UITextWidth(line, font);
        UIText(line, (screenWidth - w) / 2, top, font, UI_GOLD);

        char sub[64];
        sub[0] = '\0';
        if (g_phase == TRIAL_FIGHTING) {
            snprintf(sub, sizeof(sub), "%d foes remain", LivingFoes());
        } else if (g_phase == TRIAL_BREATHER) {
            snprintf(sub, sizeof(sub), "next wave in %.0f",
                     g_timer < 0.0f ? 0.0f : g_timer + 0.9f);
        }
        if (sub[0]) {
            int sw = UITextWidth(sub, (int)(11 * scale));
            UIText(sub, (screenWidth - sw) / 2, top + font + (int)(2 * scale),
                   (int)(11 * scale), UI_TEXT_MUTED);
        }
    }

    // The wave banner, fading out.
    if (g_bannerTime > 0.0f && g_phase == TRIAL_FIGHTING) {
        float f = g_bannerTime / BANNER_SECONDS;
        char line[64];
        snprintf(line, sizeof(line), "WAVE %d", g_wave);
        int w = UITextWidth(line, big);
        Color c = UI_GOLD;
        c.a = (unsigned char)(255 * (f > 1.0f ? 1.0f : f));
        UIText(line, (screenWidth - w) / 2, screenHeight / 4, big, c);
    }

    // Run summary. Deliberately two lines and one key: the fastest thing
    // in the mode should be starting the next run.
    if (g_phase == TRIAL_OVER) {
        DrawRectangle(0, 0, screenWidth, screenHeight, (Color){ 0, 0, 0, 170 });
        char line[96];
        snprintf(line, sizeof(line), "You held %d wave%s", g_wave, g_wave == 1 ? "" : "s");
        int w = UITextWidth(line, big);
        UIText(line, (screenWidth - w) / 2, screenHeight / 2 - big, big, UI_GOLD);

        snprintf(line, sizeof(line), "best: %d", g_best);
        int w2 = UITextWidth(line, font);
        UIText(line, (screenWidth - w2) / 2, screenHeight / 2 + (int)(6 * scale), font,
               UI_TEXT_MUTED);

        const char *hint = "R to run again    Esc to leave";
        int w3 = UITextWidth(hint, font);
        UIText(hint, (screenWidth - w3) / 2, screenHeight / 2 + (int)(40 * scale), font,
               UI_TEXT_PRIMARY);
    }
}
