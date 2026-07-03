#ifndef ENTITY_H
#define ENTITY_H

#include "raylib.h"
#include <stdbool.h>

#define MAX_ENTITIES 16
#define SKILL_BAR_SIZE 8
#define MAX_ACTIVE_EFFECTS 8

typedef enum {
    ENT_PLAYER,
    ENT_HERO,
    ENT_MONSTER
} EntityKind;

typedef enum {
    COND_NONE = 0,
    COND_BLEEDING,
    COND_BURNING,
    COND_CRIPPLED,
    COND_WEAKNESS
} ConditionKind;

typedef struct {
    bool active;
    ConditionKind kind;
    float remaining;
    float tickDamage; // > 0 for DoT-style conditions (bleeding/burning)
    float tickAccum;
} ActiveEffect;

typedef struct Entity {
    bool alive;
    EntityKind kind;
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
    float energyRegenAccum;
    int adrenaline; // simplified 0-100 shared pool (GW1 tracks this per adrenaline skill)

    int primaryProfession;
    int attributeRank[4]; // indexed by AttributeKind, see attributes.h

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

    ActiveEffect effects[MAX_ACTIVE_EFFECTS];
} Entity;

extern Entity g_entities[MAX_ENTITIES];
extern int g_entityCount;

int Entity_Spawn(EntityKind kind, const char *name, int team, Vector2 pos, Color color);
Entity *Entity_Get(int index);
bool Entity_IsCasting(const Entity *e);
void Entity_ApplyDamage(Entity *e, int amount);

#endif
