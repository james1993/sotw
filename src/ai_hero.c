#include "ai_hero.h"
#include "entity.h"
#include "skill.h"
#include "combat.h"
#include "world.h"
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
    // The player merely SELECTING a foe is not an order to attack it -
    // that is what used to send the whole party charging the moment you
    // cycled targets. Only an engaged player pulls the party in.
    if (player && player->alive && player->engaged) {
        int playerTarget = Entity_RefIndex(player->targetRef);
        Entity *t = Entity_Get(playerTarget);
        if (t && t->alive && t->team != self->team) return playerTarget;
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

    // In outposts party members stand at their spots instead of
    // trailing the player around town - GW1's heroes and henchmen wait
    // near the gate until you actually leave.
    if (World_GetMode() == MODE_OUTPOST) return;

    Entity *player = Entity_Get(PLAYER_INDEX);
    int foe = FindHeroEngageTarget(self, player);
    self->targetRef = Entity_RefOf(foe);
    self->engaged = (foe >= 0);
    if (foe >= 0) {
        UseSkillBarOn(index, foe);
        return;
    }

    // No one worth fighting - drift back toward the player, but keep a
    // respectful follow distance instead of standing on top of them:
    // start regrouping only when well behind, and stop approaching at
    // ~arm's length like a GW1 henchman trailing the party leader.
    if (player && player->alive) {
        float d = Dist(self->pos, player->pos);
        if (d > 200.0f) {
            float t = (d - 110.0f) / d; // stop ~110 units short of the player
            self->moveTarget = (Vector2){
                self->pos.x + (player->pos.x - self->pos.x) * t,
                self->pos.y + (player->pos.y - self->pos.y) * t
            };
            self->hasMoveTarget = true;
        }
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
static int FindFoeInAggroRange(const Entity *self) {
    int best = -1;
    float bestDist = 1e9f;
    for (int i = 0; i < g_entityCount; i++) {
        Entity *other = &g_entities[i];
        if (!other->alive || other->team == self->team) continue;
        float d = Dist(self->pos, other->pos);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return (best >= 0 && bestDist <= self->aggroRange) ? best : -1;
}

static void UpdateMonster(int index) {
    Entity *self = &g_entities[index];
    if (!self->alive || Entity_IsCasting(self)) return;

    if (!self->aggroed) {
        if (self->hasPatrol) {
            // Patrols scan for foes the whole way along their route -
            // that's the interception threat: linger near their path
            // mid-fight and they'll walk into aggro range and join in.
            int foe = FindFoeInAggroRange(self);
            if (foe >= 0) {
                Entity_WakeMonsterGroup(self, Entity_RefOf(foe));
                return;
            }
            Vector2 wp = (self->patrolDir >= 0) ? self->patrolB : self->patrolA;
            if (Dist(self->pos, wp) < 10.0f) {
                self->patrolDir = -self->patrolDir;
            } else if (!self->hasMoveTarget) {
                self->moveTarget = wp;
                self->hasMoveTarget = true;
            }
            return;
        }

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

        int foe = FindFoeInAggroRange(self);
        if (foe >= 0) {
            Entity_WakeMonsterGroup(self, Entity_RefOf(foe));
        }
        return;
    }

    Entity *target = Entity_Resolve(self->targetRef);
    bool giveUp = !target || !target->alive || Dist(self->pos, self->spawnPos) > self->leashRange;
    if (giveUp) {
        self->aggroed = false;
        self->engaged = false;
        self->targetRef = Entity_NoRef();
        if (self->hasPatrol) {
            // Patrollers reset on the spot and resume the route from the
            // nearest waypoint (GW1 leash-regen, condensed).
            self->hp = self->maxHp;
            self->energy = self->maxEnergy;
            self->adrenaline = 0;
            for (int i = 0; i < MAX_ACTIVE_EFFECTS; i++) self->effects[i].active = false;
            self->patrolDir = (Dist(self->pos, self->patrolB) < Dist(self->pos, self->patrolA)) ? +1 : -1;
            self->moveTarget = (self->patrolDir >= 0) ? self->patrolB : self->patrolA;
        } else {
            self->moveTarget = self->spawnPos;
        }
        self->hasMoveTarget = true;
        return;
    }

    UseSkillBarOn(index, Entity_RefIndex(self->targetRef));
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
