#include "ai_hero.h"
#include "entity.h"
#include "skill.h"
#include "combat.h"
#include <math.h>

static float g_aiTimer = 0.0f;

static float Dist(Vector2 a, Vector2 b) {
    float dx = a.x - b.x, dy = a.y - b.y;
    return sqrtf(dx * dx + dy * dy);
}

static int FindNearestFoe(const Entity *self) {
    int best = -1;
    float bestDist = 1e9f;
    for (int i = 0; i < g_entityCount; i++) {
        Entity *other = &g_entities[i];
        if (!other->alive || other->team == self->team) continue;
        float d = Dist(self->pos, other->pos);
        if (d < bestDist) {
            bestDist = d;
            best = i;
        }
    }
    return best;
}

// Skill-bar priority scan: try slots in order, fire the first legal one.
// This mirrors GW1's actual hero AI, which uses a player-configurable
// priority list rather than picking "optimally."
static void UpdateAIEntity(int index) {
    Entity *self = &g_entities[index];
    if (!self->alive || Entity_IsCasting(self)) return;

    int foe = FindNearestFoe(self);
    self->targetIndex = foe;
    if (foe < 0) return;

    for (int slot = 0; slot < SKILL_BAR_SIZE; slot++) {
        if (self->skillBar[slot] < 0) continue;
        if (Combat_ActivateSkill(index, slot, foe)) break;
    }
}

void AI_Update(float dt) {
    g_aiTimer += dt;
    if (g_aiTimer < 0.5f) return; // "thinking" cadence, not every frame
    g_aiTimer = 0.0f;

    for (int i = 0; i < g_entityCount; i++) {
        Entity *e = &g_entities[i];
        if (e->kind == ENT_HERO || e->kind == ENT_MONSTER) {
            UpdateAIEntity(i);
        }
    }
}
