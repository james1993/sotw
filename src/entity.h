#ifndef ENTITY_H
#define ENTITY_H

#include "raylib.h"
#include "attributes.h"
#include <stdbool.h>

#define MAX_ENTITIES 16
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

    int targetIndex;        // index into g_entities, -1 if none
    float attackTimer;
    float attackInterval;
    int attackDamageMin, attackDamageMax;
    float attackRange;

    float interruptFlashTimer; // > 0 briefly after being interrupted, for UI feedback

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

    ActiveEffect effects[MAX_ACTIVE_EFFECTS];
} Entity;

extern Entity g_entities[MAX_ENTITIES];
extern int g_entityCount;

int Entity_Spawn(EntityKind kind, const char *name, int team, Vector2 pos, Color color);
Entity *Entity_Get(int index);
bool Entity_IsCasting(const Entity *e);

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

// Recomputes penalized maxHp/maxEnergy from base stats and the current
// death penalty. Call after changing baseMax*, deathPenalty, or both.
void Entity_RecomputePenalizedStats(Entity *e);

#endif
