#include "ai_hero.h"
#include "entity.h"
#include "skill.h"
#include "combat.h"
#include <math.h>

#define PLAYER_INDEX 0

static float g_aiTimer = 0.0f;

static float Dist(Vector2 a, Vector2 b) {
    float dx = a.x - b.x, dy = a.y - b.y;
    return sqrtf(dx * dx + dy * dy);
}

// Skill-bar priority scan: try slots in order, fire the first legal one.
// Shared by heroes and monsters - both just need "pick a target, then
// use the first affordable/off-cooldown skill that can legally hit it."
static void UseSkillBarOn(int index, int targetIndex) {
    Entity *self = &g_entities[index];
    for (int slot = 0; slot < SKILL_BAR_SIZE; slot++) {
        if (self->skillBar[slot] < 0) continue;
        if (Combat_ActivateSkill(index, slot, targetIndex)) return;
    }
}

// How far from the player a hero will keep fighting before giving up and
// regrouping. Without this, retreating the player alone doesn't actually
// break a pull - the hero would happily keep trading blows wherever the
// fight started, so the monster (targeting the hero) never leaves its
// own leash range either. This is what makes a full-party retreat work.
#define MAX_HERO_ENGAGE_DISTANCE 450.0f

// Heroes only engage foes that are already aggroed (fighting someone) or
// that the player has explicitly targeted - they don't wander off and
// wake up a whole group on their own initiative. This is what makes
// pulling with a hero in the party work: hang back and the hero does
// too, instead of charging in and aggroing everything nearby.
static int FindHeroEngageTarget(const Entity *self, const Entity *player) {
    if (player && player->alive && player->targetIndex >= 0) {
        Entity *t = Entity_Get(player->targetIndex);
        if (t && t->alive && t->team != self->team) return player->targetIndex;
    }

    int best = -1;
    float bestDist = 1e9f;
    for (int i = 0; i < g_entityCount; i++) {
        Entity *other = &g_entities[i];
        if (!other->alive || other->team == self->team) continue;
        if (other->kind == ENT_MONSTER && !other->aggroed) continue;
        if (player && player->alive && Dist(player->pos, other->pos) > MAX_HERO_ENGAGE_DISTANCE) continue;
        float d = Dist(self->pos, other->pos);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

static void UpdateHero(int index) {
    Entity *self = &g_entities[index];
    if (!self->alive || Entity_IsCasting(self)) return;

    Entity *player = Entity_Get(PLAYER_INDEX);
    int foe = FindHeroEngageTarget(self, player);
    self->targetIndex = foe;
    if (foe >= 0) {
        UseSkillBarOn(index, foe);
        return;
    }

    // No one worth fighting - regroup near the player instead of
    // standing wherever the last fight happened.
    if (player && player->alive && Dist(self->pos, player->pos) > 50.0f) {
        self->moveTarget = player->pos;
        self->hasMoveTarget = true;
    }
}

// Monster AI: passive until a foe wanders inside aggroRange (or hits it -
// see the reactive-aggro checks in combat.c/effect.c), then engages;
// gives up and walks home to fully reset if it or its target ends up
// more than leashRange from its spawn point. This is the whole basis for
// "pulling" - approach carefully and only the monster whose bubble you
// entered notices you, and an overextended pull can be broken off by
// retreating past the leash. See docs/research/gw1-mechanics.md #7 and
// docs/design/demake-design.md #2 (aggro bubble).
static void UpdateMonster(int index) {
    Entity *self = &g_entities[index];
    if (!self->alive || Entity_IsCasting(self)) return;

    if (!self->aggroed) {
        float distHome = Dist(self->pos, self->spawnPos);
        if (distHome > 4.0f) return; // still walking home - Combat_UpdateEntity is moving us

        if (self->hp < self->maxHp || self->energy < self->maxEnergy) {
            // Arrived home after giving up on a pull: full reset, same as
            // a GW1 monster that's leashed back to its spawn.
            self->hp = self->maxHp;
            self->energy = self->maxEnergy;
            self->adrenaline = 0;
            for (int i = 0; i < MAX_ACTIVE_EFFECTS; i++) self->effects[i].active = false;
        }

        int best = -1;
        float bestDist = 1e9f;
        for (int i = 0; i < g_entityCount; i++) {
            Entity *other = &g_entities[i];
            if (!other->alive || other->team == self->team) continue;
            float d = Dist(self->pos, other->pos);
            if (d < bestDist) { bestDist = d; best = i; }
        }
        if (best >= 0 && bestDist <= self->aggroRange) {
            self->aggroed = true;
            self->targetIndex = best;
        }
        return;
    }

    Entity *target = Entity_Get(self->targetIndex);
    bool giveUp = !target || !target->alive || Dist(self->pos, self->spawnPos) > self->leashRange;
    if (giveUp) {
        self->aggroed = false;
        self->targetIndex = -1;
        self->moveTarget = self->spawnPos;
        self->hasMoveTarget = true;
        return;
    }

    UseSkillBarOn(index, self->targetIndex);
}

void AI_Update(float dt) {
    g_aiTimer += dt;
    if (g_aiTimer < 0.5f) return; // "thinking" cadence, not every frame
    g_aiTimer = 0.0f;

    for (int i = 0; i < g_entityCount; i++) {
        Entity *e = &g_entities[i];
        if (e->kind == ENT_HERO) UpdateHero(i);
        else if (e->kind == ENT_MONSTER) UpdateMonster(i);
    }
}
