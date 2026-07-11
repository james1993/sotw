#include "entity.h"
#include "items.h"
#include "progression.h"
#include "quests.h"
#include <math.h>
#include <string.h>

#define PLAYER_INDEX 0

Entity g_entities[MAX_ENTITIES];
int g_entityCount = 0;

int Entity_Spawn(EntityKind kind, const char *name, int team, Vector2 pos, Color color) {
    if (g_entityCount >= MAX_ENTITIES) return -1;

    int idx = g_entityCount++;
    Entity *e = &g_entities[idx];
    memset(e, 0, sizeof(Entity));

    e->alive = true;
    e->kind = kind;
    strncpy(e->name, name, sizeof(e->name) - 1);
    e->team = team;
    e->pos = pos;
    e->moveTarget = pos;
    e->hasMoveTarget = false;
    e->moveSpeed = 90.0f;
    e->radius = 12.0f;
    e->color = color;

    e->hp = e->maxHp = e->baseMaxHp = 100;
    e->energy = e->maxEnergy = e->baseMaxEnergy = 40;
    e->deathPenalty = 0;
    e->adrenaline = 0;
    e->armor = 60; // neutral AL - no bonus, no penalty
    e->level = 1;

    e->castingSlot = -1;
    e->targetIndex = -1;
    e->lastCastSkillSlot = -1;

    e->attackInterval = 1.33f; // matches a common GW1 weapon attack speed
    e->attackDamageMin = 6;
    e->attackDamageMax = 12;
    e->attackRange = 28.0f;

    e->spawnPos = pos;
    e->aggroed = false;

    for (int i = 0; i < SKILL_BAR_SIZE; i++) e->skillBar[i] = -1;

    return idx;
}

Entity *Entity_Get(int index) {
    if (index < 0 || index >= g_entityCount) return NULL;
    return &g_entities[index];
}

bool Entity_IsCasting(const Entity *e) {
    return e->castingSlot >= 0;
}

void Entity_MarkInCombat(Entity *e) {
    e->timeSinceCombat = 0.0f;
}

void Entity_RecomputePenalizedStats(Entity *e) {
    e->maxHp = e->baseMaxHp * (100 - e->deathPenalty) / 100;
    e->maxEnergy = e->baseMaxEnergy * (100 - e->deathPenalty) / 100;
    if (e->hp > e->maxHp) e->hp = e->maxHp;
    if (e->energy > e->maxEnergy) e->energy = e->maxEnergy;
}

void Entity_ApplyDamage(Entity *e, int amount, Entity *attacker) {
    (void)attacker; // kills award party-wide XP regardless of who landed the blow
    if (!e->alive) return;

    // GW1's armor formula: every 40 AL above/below the 60 baseline
    // halves/doubles incoming damage.
    float scaled = (float)amount * powf(2.0f, (60.0f - (float)e->armor) / 40.0f);
    int finalDamage = (int)scaled;
    if (finalDamage < 1 && amount > 0) finalDamage = 1;

    e->hp -= finalDamage;
    Entity_MarkInCombat(e);
    if (e->hp <= 0) {
        e->hp = 0;
        e->alive = false;
        e->hasMoveTarget = false;
        e->castingSlot = -1;

        // GW1's death penalty: dying costs party members 15% of max
        // health and energy, stacking to -60%, until they rezone.
        if (e->kind == ENT_PLAYER || e->kind == ENT_HERO) {
            e->deathPenalty += 15;
            if (e->deathPenalty > 60) e->deathPenalty = 60;
            Entity_RecomputePenalizedStats(e);
        }

        if (e->kind == ENT_MONSTER) {
            // GW1 XP is party-wide: the player levels no matter whether
            // they or the hero landed the killing blow.
            Progression_AwardKillXP(Entity_Get(PLAYER_INDEX), e->level);
            Items_SpawnMonsterDrops(e->pos, e->level);
            Quests_NotifyMonsterKill();
        }
    }
}
