#include "entity.h"
#include "audio.h"
#include "gwmath.h"
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
    if (g_entityCount >= MAX_ENTITIES) {
        // Callers all handle the -1 by skipping the spawn, so a full
        // array degrades into a zone that is quietly missing NPCs or
        // monsters. Say so: silently absent content is far harder to
        // diagnose than a crash. (Worst zone today is 9 spawns plus a
        // 3-strong party, so this is headroom, not a limit being hit.)
        TraceLog(LOG_WARNING, "ENTITY: array full (%d) - '%s' was not spawned",
                 MAX_ENTITIES, name ? name : "?");
        return -1;
    }

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

    e->hp = e->maxHp = e->baseMaxHp = GW_BASE_HEALTH;
    // 20 energy for everyone, GW1's rule - a caster's pool comes from
    // Energy Storage, not from being a caster.
    e->energy = e->maxEnergy = e->baseMaxEnergy = GW_BASE_ENERGY;
    e->energyRegenPips = GW_BASE_ENERGY_PIPS;
    e->deathPenalty = 0;
    for (int i = 0; i < SKILL_BAR_SIZE; i++) e->adrenaline[i] = 0;
    e->armor = 60; // neutral AL - no bonus, no penalty
    e->level = 1;
    // -1 means "never chose hair at creation", which is everyone except
    // the player and the creation preview. sprite.c reads this rather
    // than testing entity identity, so both of them get it right.
    e->hairColor = -1;

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
        monster->engaged = true; // a woken monster is always committed
    }
    if (monster->groupId <= 0) return;

    for (int i = 0; i < g_entityCount; i++) {
        Entity *e = &g_entities[i];
        if (e == monster || !e->alive || e->kind != ENT_MONSTER) continue;
        if (e->groupId != monster->groupId || e->aggroed) continue;
        e->aggroed = true;
        e->targetRef = foe;
        e->engaged = true;
    }
}

void Entity_RecomputePenalizedStats(Entity *e) {
    e->maxHp = e->baseMaxHp * (100 - e->deathPenalty) / 100;
    e->maxEnergy = e->baseMaxEnergy * (100 - e->deathPenalty) / 100;

    // Deep Wound takes another 20% off the ceiling, on top of death
    // penalty. Applied here rather than at the call sites so nothing can
    // read a maximum that hasn't accounted for it.
    if (Entity_HasCondition(e, COND_DEEP_WOUND)) {
        e->maxHp = (int)((float)e->maxHp * (1.0f - GW_DEEP_WOUND_HEALTH_LOSS));
        if (e->maxHp < 1) e->maxHp = 1;
    }

    if (e->hp > e->maxHp) e->hp = e->maxHp;
    if (e->energy > e->maxEnergy) e->energy = e->maxEnergy;
}

int Entity_ScaleIncomingHeal(const Entity *e, int amount) {
    if (!e) return amount;
    // The other half of Deep Wound, and the half that makes it a spike
    // tool: the target's ceiling drops AND their healer's numbers get
    // smaller at the same moment.
    if (Entity_HasCondition(e, COND_DEEP_WOUND)) {
        amount = (int)((float)amount * (1.0f - GW_DEEP_WOUND_HEAL_LOSS));
    }
    return amount;
}

void Entity_RecomputeAttributeStats(Entity *e) {
    if (!e) return;
    e->baseMaxHp = GW_BASE_HEALTH + GW_HEALTH_PER_LEVEL * (e->level - 1);
    e->baseMaxEnergy = GW_MaxEnergy(e->attributeRank[ATTR_ENERGY_STORAGE]);
    Entity_RecomputePenalizedStats(e);
}

float Entity_EnergyRegenInterval(const Entity *e) {
    return GW_EnergyRegenInterval(e ? e->energyRegenPips : GW_BASE_ENERGY_PIPS);
}

// Soul Reaping: GW1 hands a Necromancer energy equal to their rank
// whenever a creature dies near them - friend, foe or minion - which is
// what lets the profession spend far past a 20-point pool. The 3-per-15s
// throttle is GW1's own, and without it a single wipe refills you.
static void AwardSoulReaping(const Entity *dying) {
    for (int i = 0; i < g_entityCount; i++) {
        Entity *e = &g_entities[i];
        if (e == dying || !e->alive) continue;
        int rank = e->attributeRank[ATTR_SOUL_REAPING];
        if (rank <= 0) continue;
        float dx = e->pos.x - dying->pos.x, dy = e->pos.y - dying->pos.y;
        if (sqrtf(dx * dx + dy * dy) > GW_SOUL_REAPING_RADIUS) continue;

        if (e->soulReapingWindow <= 0.0f) {
            e->soulReapingWindow = GW_SOUL_REAPING_WINDOW;
            e->soulReapingTriggers = 0;
        }
        if (e->soulReapingTriggers >= GW_SOUL_REAPING_MAX_TRIGGERS) continue;
        e->soulReapingTriggers++;

        e->energy += rank;
        if (e->energy > e->maxEnergy) e->energy = e->maxEnergy;
    }
}

void Entity_ApplyDamage(Entity *e, int amount, Entity *attacker) {
    Entity_ApplyDamagePen(e, amount, attacker, 0.0f);
}

void Entity_ApplyDamagePen(Entity *e, int amount, Entity *attacker, float armorPenetration) {
    if (!e->alive) return;

    // A defensive stance's armor bonus rides on top of worn AL while it
    // holds - Disciplined Stance's +10 is the reason it blunts a spike
    // and not just the attacks it happens to block.
    int armor = e->armor + (e->stanceTimer > 0.0f ? e->stanceArmorBonus : 0);
    int finalDamage = GW_ArmorScaledDamage(amount, armor, armorPenetration);

    // Only struck blows are audible. Condition ticks pass attacker=NULL
    // and would otherwise fire an impact every second, per affliction,
    // per character.
    if (attacker) {
        Entity *player = Entity_Get(PLAYER_INDEX);
        float dx = player ? e->pos.x - player->pos.x : 0.0f;
        Audio_PlayAt(finalDamage >= 25 ? SFX_HIT_HEAVY : SFX_HIT, dx);
    }

    e->hp -= finalDamage;
    Entity_MarkInCombat(e);
    if (e->hp <= 0) {
        e->hp = 0;
        e->alive = false;
        e->hasMoveTarget = false;
        e->castingSlot = -1;
        {
            Entity *player = Entity_Get(PLAYER_INDEX);
            Audio_PlayAt(SFX_DEATH, player ? e->pos.x - player->pos.x : 0.0f);
        }

        // Every death feeds nearby Soul Reaping, whichever side it was
        // on - GW1 makes no distinction, and that's what makes a
        // Necromancer strongest in exactly the fights that go badly.
        AwardSoulReaping(e);

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
            Items_SpawnMonsterDrops(e->pos, e->level, (int)e->species);
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

// ---------------------------------------------------------------------
// Conditions and hexes

bool Entity_HasCondition(const Entity *e, ConditionKind kind) {
    if (!e) return false;
    for (int i = 0; i < MAX_ACTIVE_EFFECTS; i++) {
        const ActiveEffect *fx = &e->effects[i];
        if (fx->active && fx->category == EFFECT_CONDITION && fx->kind == (int)kind) return true;
    }
    return false;
}

bool Entity_HasHex(const Entity *e, HexKind kind) {
    if (!e) return false;
    for (int i = 0; i < MAX_ACTIVE_EFFECTS; i++) {
        const ActiveEffect *fx = &e->effects[i];
        if (fx->active && fx->category == EFFECT_HEX && fx->kind == (int)kind) return true;
    }
    return false;
}

int Entity_CountEffects(const Entity *e, EffectCategory category) {
    if (!e) return 0;
    int n = 0;
    for (int i = 0; i < MAX_ACTIVE_EFFECTS; i++) {
        const ActiveEffect *fx = &e->effects[i];
        if (fx->active && fx->category == category) n++;
    }
    return n;
}

int Entity_RemoveEffects(Entity *e, EffectCategory category, int maxCount) {
    if (!e || maxCount <= 0) return 0;
    int removed = 0;
    while (removed < maxCount) {
        // Longest remaining first: a cleanse should take the affliction
        // you'd otherwise be stuck with, not whichever slot came first.
        int worst = -1;
        float worstRemaining = -1.0f;
        for (int i = 0; i < MAX_ACTIVE_EFFECTS; i++) {
            ActiveEffect *fx = &e->effects[i];
            if (!fx->active || fx->category != category) continue;
            if (fx->remaining > worstRemaining) {
                worstRemaining = fx->remaining;
                worst = i;
            }
        }
        if (worst < 0) break;
        e->effects[worst].active = false;
        removed++;
    }
    return removed;
}

float Entity_MoveSpeed(const Entity *e) {
    if (!e) return 0.0f;
    // GW1's Crippled is a flat halving, and it's brutal precisely
    // because it takes kiting away rather than shaving a few percent.
    return Entity_HasCondition(e, COND_CRIPPLED) ? e->moveSpeed * 0.5f : e->moveSpeed;
}

float Entity_AttackInterval(const Entity *e) {
    if (!e) return 1.0f;
    float interval = e->attackInterval;
    if (Entity_HasHex(e, HEX_FALTERING)) interval *= 1.5f;
    return interval;
}

int Entity_ScaleOutgoingDamage(const Entity *e, int damage) {
    if (!e) return damage;
    // GW1's Weakness takes 66% off the WEAPON's damage - not the 25%
    // this used to apply, and not the bonus damage an attack skill adds
    // on top, which is why this only wraps basic attacks.
    if (Entity_HasCondition(e, COND_WEAKNESS)) {
        damage = (int)((float)damage * GW_WEAKNESS_DAMAGE_SCALE);
        if (damage < 1) damage = 1;
    }
    return damage;
}

void Entity_BreakStance(Entity *e) {
    if (!e) return;
    e->stanceTimer = 0.0f;
    e->blockChance = 0.0f;
    e->stanceArmorBonus = 0;
}

AttackOutcome Entity_ResolveAttack(const Entity *attacker, Entity *defender) {
    // Blind is checked first and on the ATTACKER: a blinded swing misses
    // before the defender's stance ever matters. GW1's number is a flat
    // 90% miss, unaffected by anything the defender does.
    if (attacker && Entity_HasCondition(attacker, COND_BLIND) &&
        GetRandomValue(1, 100) <= GW_BLIND_MISS_PERCENT) {
        if (defender) defender->blindMissFlashTimer = 1.0f;
        return ATTACK_MISS_BLIND;
    }
    // Block is the defender's: a stance that's still up rolls its chance
    // against this one attack. Spells don't reach here, so a block stance
    // stops swings and arrows but never a Fire Bolt - exactly GW1.
    if (defender && defender->stanceTimer > 0.0f && defender->blockChance > 0.0f &&
        GetRandomValue(1, 100) <= (int)(defender->blockChance * 100.0f)) {
        defender->blockFlashTimer = 1.0f;
        return ATTACK_BLOCKED;
    }
    return ATTACK_LANDS;
}

// Only adrenal skills charge, so a bar with none of them never
// accumulates anything - which is exactly right: adrenaline is a
// Warrior mechanic, not a universal resource.
static bool SlotIsAdrenal(const Entity *e, int slot) {
    if (slot < 0 || slot >= SKILL_BAR_SIZE) return false;
    int id = e->skillBar[slot];
    return id >= 0 && id < g_skillCount && g_skillDB[id].adrenalineCost > 0;
}

void Entity_GainAdrenalineStrike(Entity *e) {
    if (!e) return;
    for (int i = 0; i < SKILL_BAR_SIZE; i++) {
        if (!SlotIsAdrenal(e, i)) continue;
        int cap = g_skillDB[e->skillBar[i]].adrenalineCost;
        e->adrenaline[i] += GW_ADRENALINE_PER_STRIKE;
        // Charge caps at the skill's cost: GW1 doesn't bank surplus
        // adrenaline against the next use.
        if (e->adrenaline[i] > cap) e->adrenaline[i] = cap;
    }
}

void Entity_AddAdrenalinePoints(Entity *e, int points) {
    if (!e) return;
    for (int i = 0; i < SKILL_BAR_SIZE; i++) {
        if (!SlotIsAdrenal(e, i)) continue;
        int cap = g_skillDB[e->skillBar[i]].adrenalineCost;
        e->adrenaline[i] += points;
        if (e->adrenaline[i] > cap) e->adrenaline[i] = cap;
        if (e->adrenaline[i] < 0) e->adrenaline[i] = 0;
    }
}

void Entity_SpendAdrenaline(Entity *e, int slot) {
    if (!e || slot < 0 || slot >= SKILL_BAR_SIZE) return;
    e->adrenaline[slot] = 0;
    for (int i = 0; i < SKILL_BAR_SIZE; i++) {
        if (i == slot || !SlotIsAdrenal(e, i)) continue;
        e->adrenaline[i] -= GW_ADRENALINE_CROSS_DRAIN;
        if (e->adrenaline[i] < 0) e->adrenaline[i] = 0;
    }
}

int Entity_EffectiveRank(const Entity *e, AttributeKind attr) {
    if (!e || attr < 0 || attr >= ATTR_COUNT) return 0;
    int rank = e->attributeRank[attr];
    if (rank > 0 && Entity_HasCondition(e, COND_WEAKNESS)) rank--;
    return rank;
}

const char *Entity_EffectName(const ActiveEffect *fx) {
    if (!fx || !fx->active) return "";
    if (fx->category == EFFECT_HEX) {
        switch ((HexKind)fx->kind) {
            case HEX_FALTERING: return "Faltering";
            case HEX_BACKLASH:  return "Backlash";
            default: return "Hex";
        }
    }
    switch ((ConditionKind)fx->kind) {
        case COND_BLEEDING:   return "Bleeding";
        case COND_BURNING:    return "Burning";
        case COND_POISON:     return "Poison";
        case COND_CRIPPLED:   return "Crippled";
        case COND_WEAKNESS:   return "Weakness";
        case COND_BLIND:      return "Blind";
        case COND_DEEP_WOUND: return "Deep Wound";
        default: return "Condition";
    }
}

Color Entity_EffectColor(const ActiveEffect *fx) {
    if (!fx || !fx->active) return (Color){ 200, 200, 200, 255 };
    // Hexes read purple, conditions read by their own flavor - the same
    // language the party window has always used for its status arrows.
    if (fx->category == EFFECT_HEX) return (Color){ 178, 118, 220, 255 };
    switch ((ConditionKind)fx->kind) {
        case COND_BLEEDING:   return (Color){ 208, 70, 70, 255 };
        case COND_BURNING:    return (Color){ 240, 140, 50, 255 };
        case COND_POISON:     return (Color){ 96, 168, 78, 255 };
        case COND_CRIPPLED:   return (Color){ 190, 150, 90, 255 };
        case COND_WEAKNESS:   return (Color){ 150, 150, 160, 255 };
        case COND_BLIND:      return (Color){ 90, 90, 100, 255 };
        case COND_DEEP_WOUND: return (Color){ 150, 40, 40, 255 };
        default: return (Color){ 190, 150, 90, 255 };
    }
}
