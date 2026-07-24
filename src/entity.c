#include "entity.h"
#include "items.h"
#include "progression.h"
#include "quests.h"
#include "skill.h"
#include "skillbook.h"
#include "save.h"
#include "ui_hints.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define PLAYER_INDEX 0

Entity g_entities[MAX_ENTITIES];
int g_entityCount = 0;
unsigned g_entityGen[MAX_ENTITIES];

int Entity_Spawn(EntityKind kind, const char *name, int team, Vector2 pos, Color color) {
    if (g_entityCount >= MAX_ENTITIES) return -1;

    int idx = g_entityCount++;
    g_entityGen[idx]++; // this slot now holds a different entity; stale refs die here
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
    e->targetRef = Entity_NoRef();
    e->castTargetRef = Entity_NoRef();
    e->lastCastSkillSlot = -1;

    e->attackInterval = 1.33f; // matches a common GW1 weapon attack speed
    e->attackDamageMin = 6;
    e->attackDamageMax = 12;
    e->attackRange = 28.0f;

    e->spawnPos = pos;
    e->aggroed = false;

    e->species = SPECIES_HUMAN;
    e->capturedSkill = -1;
    e->prevPos = pos;
    e->facing = (Vector2){ 0.0f, 1.0f }; // face the camera (south)

    for (int i = 0; i < SKILL_BAR_SIZE; i++) e->skillBar[i] = -1;

    return idx;
}

Entity *Entity_Get(int index) {
    if (index < 0 || index >= g_entityCount) return NULL;
    return &g_entities[index];
}

EntityRef Entity_NoRef(void) {
    EntityRef r = { -1, 0 };
    return r;
}

EntityRef Entity_RefOf(int index) {
    if (index < 0 || index >= g_entityCount) return Entity_NoRef();
    EntityRef r = { index, g_entityGen[index] };
    return r;
}

Entity *Entity_Resolve(EntityRef ref) {
    if (ref.idx < 0 || ref.idx >= g_entityCount) return NULL;
    if (g_entityGen[ref.idx] != ref.gen) return NULL; // slot reused since
    return &g_entities[ref.idx];
}

int Entity_RefIndex(EntityRef ref) {
    return Entity_Resolve(ref) ? ref.idx : -1;
}

bool Entity_IsCasting(const Entity *e) {
    return e->castingSlot >= 0;
}

void Entity_MarkInCombat(Entity *e) {
    e->timeSinceCombat = 0.0f;
}

void Entity_WakeMonsterGroup(Entity *monster, EntityRef foe) {
    if (!monster || monster->kind != ENT_MONSTER || !monster->alive) return;
    if (!monster->aggroed) {
        monster->aggroed = true;
        monster->targetRef = foe;
    }
    if (monster->groupId <= 0) return;

    for (int i = 0; i < g_entityCount; i++) {
        Entity *e = &g_entities[i];
        if (e == monster || !e->alive || e->kind != ENT_MONSTER) continue;
        if (e->groupId != monster->groupId || e->aggroed) continue;
        e->aggroed = true;
        e->targetRef = foe;
    }
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
            Items_SpawnMonsterDrops(e->pos, e->level, e->species == SPECIES_CHARR);
            Quests_NotifyMonsterKill(e);

            // Elite capture: killing a boss teaches the elite it was
            // using. This is the only way an elite ever reaches your
            // bar - trainers refuse to sell them - so bosses are the
            // build-crafting destination, not just tougher monsters.
            if (e->capturedSkill >= 0 && Skillbook_Unlock(e->capturedSkill)) {
                char msg[96];
                snprintf(msg, sizeof(msg), "Elite captured:  %s",
                         g_skillDB[e->capturedSkill].name);
                UI_Notify(msg);
                Save_Write();
            }
        }
    }
}
