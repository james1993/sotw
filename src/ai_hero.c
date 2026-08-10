#include "ai_hero.h"
#include "entity.h"
#include "skill.h"
#include "combat.h"
#include "world.h"
#include <math.h>

#define PLAYER_INDEX 0

static float g_aiTimer = 0.0f;

// Party flag (GW1): a ground point the party moves to and holds instead of
// trailing the player. Set/cleared from the compass.
static Vector2 g_partyFlag;
static bool g_partyFlagged = false;

void AI_SetPartyFlag(Vector2 pos) { g_partyFlag = pos; g_partyFlagged = true; }
void AI_ClearPartyFlag(void) { g_partyFlagged = false; }
bool AI_PartyFlagActive(Vector2 *out) {
    if (g_partyFlagged && out) *out = g_partyFlag;
    return g_partyFlagged;
}

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

// The nearest FALLEN party member a Resurrection Signet could raise -
// a real party member, so pets and minions (which have their own
// resurrections, or none) don't count.
static int FindFallenAlly(const Entity *self) {
    int best = -1;
    float bestDist = 1e9f;
    for (int i = 0; i < g_entityCount; i++) {
        Entity *o = &g_entities[i];
        if (o == self || o->team != self->team || o->alive) continue;
        if (o->kind != ENT_PLAYER && o->kind != ENT_HERO) continue;
        if (o->isPet || o->isMinion) continue;
        float d = Dist(self->pos, o->pos);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

// This hero's Resurrection Signet slot, if it's ready to fire - otherwise
// -1. (It only recharges on a morale boost, so "ready" means never used
// since the last boss.)
static int ReadyResSlot(const Entity *self) {
    for (int s = 0; s < SKILL_BAR_SIZE; s++) {
        if (self->skillBar[s] == SK_RESURRECTION_SIGNET && self->skillRecharge[s] <= 0.0f) return s;
    }
    return -1;
}

static void UpdateHero(int index) {
    Entity *self = &g_entities[index];
    if (!self->alive || Entity_IsCasting(self)) return;

    // In outposts party members stand at their spots instead of
    // trailing the player around town - GW1's heroes and henchmen wait
    // near the gate until you actually leave.
    if (World_GetMode() == MODE_OUTPOST) return;

    Entity *player = Entity_Get(PLAYER_INDEX);

    // Reviving a downed ally comes first, GW1's henchman priority: if this
    // hero still has a Resurrection Signet charged and someone is down,
    // cast it - closing the distance first if the body is out of reach.
    int resSlot = ReadyResSlot(self);
    if (resSlot >= 0) {
        int fallen = FindFallenAlly(self);
        if (fallen >= 0) {
            if (Combat_ActivateSkill(index, resSlot, fallen)) return; // rezzing
            self->engaged = false;
            self->targetRef = Entity_NoRef();
            self->moveTarget = g_entities[fallen].pos;   // run to the body
            self->hasMoveTarget = true;
            return;
        }
    }

    int foe = FindHeroEngageTarget(self, player);
    self->targetRef = Entity_RefOf(foe);
    self->engaged = (foe >= 0);
    if (foe >= 0) {
        UseSkillBarOn(index, foe);
        return;
    }

    // No one worth fighting. Hold the party flag if one is planted;
    // otherwise drift back toward the player, keeping a respectful follow
    // distance like a GW1 henchman trailing the party leader.
    Vector2 flag;
    if (AI_PartyFlagActive(&flag)) {
        if (Dist(self->pos, flag) > 16.0f) {
            self->moveTarget = flag;
            self->hasMoveTarget = true;
        }
    } else if (player && player->alive) {
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
            for (int a = 0; a < SKILL_BAR_SIZE; a++) self->adrenaline[a] = 0;
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
            for (int a = 0; a < SKILL_BAR_SIZE; a++) self->adrenaline[a] = 0;
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
