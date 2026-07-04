#include "entity.h"
#include <string.h>

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

    e->hp = e->maxHp = 100;
    e->energy = e->maxEnergy = 40;
    e->adrenaline = 0;

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

void Entity_ApplyDamage(Entity *e, int amount) {
    if (!e->alive) return;
    e->hp -= amount;
    Entity_MarkInCombat(e);
    if (e->hp <= 0) {
        e->hp = 0;
        e->alive = false;
        e->hasMoveTarget = false;
        e->castingSlot = -1;
    }
}
