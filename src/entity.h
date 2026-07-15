#ifndef ENTITY_H
#define ENTITY_H

#include "raylib.h"
#include "attributes.h"
#include <stdbool.h>

// Sized for a GW1-scale party (8) plus a zone's worth of camps and
// patrols, with headroom for spawned reinforcements.
#define MAX_ENTITIES 32

// Radius of the player's drawn "danger bubble" (the world-space ring in
// render.c and the compass ring in ui_compass.c - one constant so the
// two can't drift). Monster aggroRange values in zone data stay at or
// below this, so the bubble never under-promises who can notice you.
#define AGGRO_RING_RADIUS 130.0f
#define SKILL_BAR_SIZE 8
#define MAX_ACTIVE_EFFECTS 8

typedef enum {
    ENT_PLAYER,
    ENT_HERO,
    ENT_MONSTER,
    ENT_NPC      // outpost service NPCs: quest giver, merchant, henchman
} EntityKind;

typedef enum {
    NPC_NONE = 0,
    NPC_QUEST_GIVER,
    NPC_MERCHANT,
    NPC_HENCHMAN
} NpcRole;

typedef enum {
    COND_NONE = 0,
    COND_BLEEDING,
    COND_BURNING,
    COND_CRIPPLED,
    COND_WEAKNESS
} ConditionKind;

typedef struct {
    bool active;
    bool isHex;        // hex vs condition - separate categories with separate
                       // removal counters in GW1, and shown differently in the
                       // party window (purple vs brown arrow)
    ConditionKind kind;
    float remaining;
    float tickDamage; // > 0 for DoT-style conditions (bleeding/burning)
    float tickAccum;
} ActiveEffect;

// Generational entity handle. A bare slot index stays "valid" after the
// entity in that slot dies AND after the slot is reused by a different
// entity - the second case silently retargets whoever moved in. The
// generation counter (bumped every time a slot is respawned) catches
// exactly that: a stale ref resolves to NULL instead of the wrong
// entity. Take refs with Entity_RefOf, read them with Entity_Resolve.
typedef struct {
    int idx;      // slot in g_entities, -1 = no entity
    unsigned gen; // g_entityGen[idx] at the time the ref was taken
} EntityRef;

// One energy pip of regen every this many seconds at baseline. Shared
// by the regen tick (combat.c) and the resource bars, which use the
// regen accumulator to fill smoothly between whole-point ticks.
#define ENERGY_REGEN_INTERVAL 3.0f

typedef struct Entity {
    bool alive;
    EntityKind kind;
    NpcRole npcRole; // only meaningful for ENT_NPC
    char name[32];
    int team; // 0 = player party, 1 = hostile

    Vector2 pos;
    Vector2 moveTarget;
    bool hasMoveTarget;
    float moveSpeed;
    float radius;
    Color color;

    int hp, maxHp;
    int energy, maxEnergy;
    // Death penalty (GW1's DP): each death costs 15% of max health and
    // energy, stacking to -60%, cleared by rezoning. maxHp/maxEnergy are
    // the *penalized* values; base* hold the real stats.
    int baseMaxHp, baseMaxEnergy;
    int deathPenalty; // percent, 0-60
    float energyRegenAccum;
    float hpRegenAccum;
    float timeSinceCombat; // seconds since this entity last dealt or took damage
    int adrenaline; // simplified 0-100 shared pool (GW1 tracks this per adrenaline skill)

    bool isHenchman; // hired help - dismissible in outposts, unlike heroes

    // GW1-style armor level (AL): incoming damage is scaled by
    // 2^((60 - AL) / 40), GW1's actual armor formula against the AL 60
    // caster baseline. 60 = neutral.
    int armor;

    int level;
    int xp;              // toward the next level
    int attributePoints; // earned but unspent

    Profession primaryProfession;
    Profession secondaryProfession;
    int attributeRank[ATTR_COUNT];

    int skillBar[SKILL_BAR_SIZE];      // index into g_skillDB, -1 = empty
    float skillRecharge[SKILL_BAR_SIZE];

    int castingSlot;        // -1 if not casting
    float castTimeRemaining;
    float castTimeTotal;
    EntityRef castTargetRef; // resolved target of the cast in progress -
                             // separate from targetRef so a self-fallback
                             // heal doesn't stomp your selected target

    EntityRef targetRef;    // current target, Entity_Resolve to read
    float attackTimer;
    float attackInterval;
    int attackDamageMin, attackDamageMax;
    float attackRange;

    float interruptFlashTimer; // > 0 briefly after being interrupted, for UI feedback
    float dodgeFlashTimer;     // > 0 briefly after dodging a projectile

    // Target-panel display: which skill to show as "currently/recently
    // used" (see ui_target.c). Set whenever a skill is activated; the
    // post-cast window keeps it visible for a few seconds after a cast
    // resolves or gets interrupted, matching how a GW1 target bar shows
    // what your target just did, not their whole skill bar.
    int lastCastSkillSlot;
    float postCastDisplayTimer;
    bool lastCastInterrupted;

    // Aggro/leash (monsters only - see docs/research/gw1-mechanics.md #7
    // and ai_hero.c). A monster is passive until something enters
    // aggroRange or hits it; if it or its target strays more than
    // leashRange from spawnPos, it gives up, walks home, and resets.
    Vector2 spawnPos;
    float aggroRange;
    float leashRange;
    bool aggroed;
    // Spawn group (monsters): > 0 links campmates together so they
    // aggro as one - pull any member and the whole group joins, like a
    // GW1 mob group. 0 = ungrouped (patrols hunt alone).
    int groupId;

    // Patrol route: monsters with hasPatrol ping-pong between patrolA
    // and patrolB while idle, scanning for foes the whole way - the
    // GW1 patrols that punish a badly timed pull by wandering into it.
    bool hasPatrol;
    Vector2 patrolA, patrolB;
    int patrolDir; // +1 toward B, -1 toward A

    ActiveEffect effects[MAX_ACTIVE_EFFECTS];
} Entity;

extern Entity g_entities[MAX_ENTITIES];
extern int g_entityCount;
extern unsigned g_entityGen[MAX_ENTITIES]; // per-slot generation counters

int Entity_Spawn(EntityKind kind, const char *name, int team, Vector2 pos, Color color);
Entity *Entity_Get(int index);
bool Entity_IsCasting(const Entity *e);

// Generational handles (see EntityRef above).
EntityRef Entity_NoRef(void);
EntityRef Entity_RefOf(int index);        // ref to a current slot, or NoRef
Entity *Entity_Resolve(EntityRef ref);    // NULL when none or stale
int Entity_RefIndex(EntityRef ref);       // slot index while valid, else -1

// Applies armor-scaled damage. `attacker` may be NULL (e.g. condition
// ticks). Monster deaths award party XP and roll loot drops here, so
// every damage source shares one death path.
void Entity_ApplyDamage(Entity *e, int amount, Entity *attacker);

// Resets the out-of-combat regen timer. Called whenever an entity deals
// or takes damage, matching GW1's "recent combat activity blocks fast
// regen" rule (see docs/research/gw1-mechanics.md - health here isn't a
// GW1 resource with its own pips, but the in/out-of-combat regen split
// is a faithful simplification of the same idea).
void Entity_MarkInCombat(Entity *e);

// Wakes a sleeping monster onto a foe and, when it belongs to a spawn
// group, wakes every groupmate onto the same foe - GW1 mobs aggro as a
// group: pull one and its campmates all come. This is THE aggro entry
// point; every path that used to flip `aggroed` directly (proximity
// scan, melee hit, spell damage, projectile impact) goes through here
// so no path can forget the group.
void Entity_WakeMonsterGroup(Entity *monster, EntityRef foe);

// Recomputes penalized maxHp/maxEnergy from base stats and the current
// death penalty. Call after changing baseMax*, deathPenalty, or both.
void Entity_RecomputePenalizedStats(Entity *e);

#endif
