#include "area.h"
#include "entity.h"
#include "gwmath.h"
#include "fx.h"
#include <math.h>
#include <stddef.h>

// A modest budget: a fight rarely has more than a couple of wards, a well
// and a spirit up at once, and recasting replaces rather than stacks.
#define MAX_AREAS 12

typedef struct {
    bool active;
    AreaKind kind;
    Vector2 pos;
    float radius;
    float remaining;
    float magnitude;
    int ownerTeam;
    float tickAccum; // paces a well's once-a-second regen/degen
} Area;

static Area g_areas[MAX_AREAS];

void Area_Reset(void) {
    for (int i = 0; i < MAX_AREAS; i++) g_areas[i].active = false;
}

static bool Contains(const Area *a, Vector2 p) {
    float dx = p.x - a->pos.x, dy = p.y - a->pos.y;
    return dx * dx + dy * dy <= a->radius * a->radius;
}

void Area_Spawn(AreaKind kind, Vector2 pos, float radius, float duration,
                float magnitude, int ownerTeam) {
    // Recast replaces the same kind from the same owner; otherwise take a
    // free slot, and failing that the one expiring soonest.
    Area *slot = NULL;
    for (int i = 0; i < MAX_AREAS; i++) {
        if (g_areas[i].active && g_areas[i].kind == kind &&
            g_areas[i].ownerTeam == ownerTeam) { slot = &g_areas[i]; break; }
    }
    if (!slot) {
        for (int i = 0; i < MAX_AREAS; i++) if (!g_areas[i].active) { slot = &g_areas[i]; break; }
    }
    if (!slot) {
        slot = &g_areas[0];
        for (int i = 1; i < MAX_AREAS; i++)
            if (g_areas[i].remaining < slot->remaining) slot = &g_areas[i];
    }
    slot->active = true;
    slot->kind = kind;
    slot->pos = pos;
    slot->radius = radius;
    slot->remaining = duration;
    slot->magnitude = magnitude;
    slot->ownerTeam = ownerTeam;
    slot->tickAccum = 0.0f;
}

void Area_Update(float dt) {
    for (int i = 0; i < MAX_AREAS; i++) {
        Area *a = &g_areas[i];
        if (!a->active) continue;
        a->remaining -= dt;
        if (a->remaining <= 0.0f) { a->active = false; continue; }

        if (a->kind != AREA_WELL_BLOOD && a->kind != AREA_WELL_SUFFERING) continue;

        // Wells act once a second on everyone standing in them - allies
        // healed by a blood well, foes worn down by a suffering well.
        a->tickAccum += dt;
        while (a->tickAccum >= 1.0f) {
            a->tickAccum -= 1.0f;
            int amt = (int)(a->magnitude * GW_HEALTH_PER_PIP); // pips -> health/sec
            if (amt <= 0) continue;
            for (int e = 0; e < g_entityCount; e++) {
                Entity *ent = &g_entities[e];
                if (!ent->alive || !Contains(a, ent->pos)) continue;
                if (a->kind == AREA_WELL_BLOOD) {
                    if (ent->team == a->ownerTeam) {
                        ent->hp += amt;
                        if (ent->hp > ent->maxHp) ent->hp = ent->maxHp;
                    }
                } else { // AREA_WELL_SUFFERING
                    if (ent->team != a->ownerTeam) Entity_ApplyDamage(ent, amt, NULL);
                }
            }
        }
    }
}

int Area_BonusArmor(const Entity *e) {
    if (!e) return 0;
    int bonus = 0;
    for (int i = 0; i < MAX_AREAS; i++) {
        const Area *a = &g_areas[i];
        if (a->active && a->kind == AREA_WARD_HARM &&
            e->team == a->ownerTeam && Contains(a, e->pos)) {
            bonus += (int)a->magnitude;
        }
    }
    return bonus;
}

int Area_BonusAttackDamage(const Entity *e) {
    if (!e) return 0;
    int bonus = 0;
    for (int i = 0; i < MAX_AREAS; i++) {
        const Area *a = &g_areas[i];
        if (a->active && a->kind == AREA_SPIRIT_WINNOWING && Contains(a, e->pos)) {
            bonus += (int)a->magnitude; // friend and foe alike
        }
    }
    return bonus;
}

void Area_Draw(void) {
    for (int i = 0; i < MAX_AREAS; i++) {
        const Area *a = &g_areas[i];
        if (!a->active) continue;
        Color c;
        switch (a->kind) {
            case AREA_WARD_HARM:       c = (Color){ 120, 150, 220, 55 }; break; // stone blue
            case AREA_WELL_BLOOD:      c = (Color){ 200,  70,  70, 55 }; break; // blood red
            case AREA_WELL_SUFFERING:  c = (Color){ 150,  80, 180, 55 }; break; // rot purple
            case AREA_SPIRIT_WINNOWING:c = (Color){ 110, 200, 120, 45 }; break; // ritual green
            default:                   c = (Color){ 200, 200, 200, 50 }; break;
        }
        // Fade the fill out over the last couple of seconds.
        float f = a->remaining < 2.0f ? a->remaining / 2.0f : 1.0f;
        Color fill = (Color){ c.r, c.g, c.b, (unsigned char)(c.a * f) };
        DrawCircleV(a->pos, a->radius, fill);
        DrawCircleLines((int)a->pos.x, (int)a->pos.y, a->radius,
                        (Color){ c.r, c.g, c.b, (unsigned char)(150 * f) });

        if (a->kind == AREA_SPIRIT_WINNOWING) {
            // A little totem post marks the spirit at the centre.
            DrawRectangle((int)a->pos.x - 3, (int)a->pos.y - 16, 6, 20, (Color){ 92, 72, 52, 255 });
            DrawCircleV((Vector2){ a->pos.x, a->pos.y - 18.0f }, 6.0f, (Color){ 110, 200, 120, 255 });
            DrawCircleLines((int)a->pos.x, (int)(a->pos.y - 18), 6.0f, (Color){ 60, 120, 70, 255 });
        }
    }
}
