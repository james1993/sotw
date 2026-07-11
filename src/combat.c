#include "combat.h"
#include "skill.h"
#include "effect.h"
#include "world.h"
#include "projectile.h"
#include "raylib.h"
#include <math.h>
#include <stddef.h>

// Auto-attacks from beyond this range are projectiles with travel time
// (dodgeable); anything closer is a melee swing and hits instantly.
#define RANGED_ATTACK_THRESHOLD 60.0f

static float Dist(Vector2 a, Vector2 b) {
    float dx = a.x - b.x, dy = a.y - b.y;
    return sqrtf(dx * dx + dy * dy);
}

// GW1 splits health regen into a slow in-combat trickle and a much
// faster regen once you haven't dealt or taken damage in a few seconds -
// the "sit down and rest" pattern most RPGs use. See
// docs/research/gw1-mechanics.md.
#define OUT_OF_COMBAT_DELAY 4.0f
#define OUT_OF_COMBAT_REGEN_PCT_PER_SEC 0.06f

bool Combat_ActivateSkill(int casterIndex, int slot, int targetIndex) {
    // GW1 disables the skill bar in towns and outposts entirely - even
    // self-targeted skills can't be cast in a safe zone.
    if (World_GetMode() == MODE_OUTPOST) return false;

    Entity *caster = Entity_Get(casterIndex);
    if (!caster || !caster->alive) return false;
    if (slot < 0 || slot >= SKILL_BAR_SIZE) return false;

    int skillIdx = caster->skillBar[slot];
    if (skillIdx < 0 || skillIdx >= g_skillCount) return false;
    if (Entity_IsCasting(caster)) return false;
    if (caster->skillRecharge[slot] > 0.0f) return false;

    Skill *skill = &g_skillDB[skillIdx];

    if (skill->energyCost > 0 && caster->energy < skill->energyCost) return false;
    if (skill->adrenalineCost > 0 && caster->adrenaline < skill->adrenalineCost) return false;

    Entity *target = Entity_Get(targetIndex);
    if (skill->targeting == TARGET_SINGLE_FOE || skill->targeting == TARGET_AOE_FOES) {
        if (!target || !target->alive || target->team == caster->team) return false;
        if (Dist(caster->pos, target->pos) > skill->range) return false;
    }
    if (skill->targeting == TARGET_SINGLE_ALLY) {
        // GW1: ally-targeted spells fall back to casting on yourself when
        // the current target isn't a valid ally (a foe, dead, or nothing).
        if (!target || !target->alive || target->team != caster->team) {
            targetIndex = casterIndex;
            target = caster;
        }
        if (Dist(caster->pos, target->pos) > skill->range) return false;
    }

    caster->energy -= skill->energyCost;
    caster->adrenaline -= skill->adrenalineCost;
    if (caster->adrenaline < 0) caster->adrenaline = 0;

    // Which skill the target panel shows as "current/recent" - set here so
    // it covers both branches below, not just cast-time skills.
    caster->lastCastSkillSlot = slot;

    if (skill->castTime > 0.0f) {
        caster->castingSlot = slot;
        caster->castTimeRemaining = skill->castTime;
        caster->castTimeTotal = skill->castTime;
        caster->castTargetIndex = targetIndex;
        // Chasing/attacking only follows foe targets, so only offensive
        // casts update the caster's current target.
        if (skill->targeting == TARGET_SINGLE_FOE || skill->targeting == TARGET_AOE_FOES) {
            caster->targetIndex = targetIndex;
        }
        caster->hasMoveTarget = false; // casting roots the caster, matches GW1 spellcasting
    } else {
        Effect_Execute(caster, skill, target);
        caster->skillRecharge[slot] = skill->recharge;
        caster->lastCastInterrupted = false;
        caster->postCastDisplayTimer = 3.0f;
    }
    return true;
}

static void ResolveCast(Entity *caster) {
    int slot = caster->castingSlot;
    int skillIdx = caster->skillBar[slot];
    Skill *skill = &g_skillDB[skillIdx];
    Entity *target = Entity_Get(caster->castTargetIndex);

    bool targetStillValid = true;
    if (skill->targeting == TARGET_SINGLE_FOE || skill->targeting == TARGET_AOE_FOES) {
        targetStillValid = target && target->alive;
    }
    if (skill->targeting == TARGET_SINGLE_ALLY) {
        targetStillValid = target && target->alive && target->team == caster->team;
    }
    if (targetStillValid) {
        Effect_Execute(caster, skill, target);
    }

    caster->skillRecharge[slot] = skill->recharge;
    caster->castingSlot = -1;
    caster->lastCastInterrupted = false;
    caster->postCastDisplayTimer = 3.0f;
}

void Combat_UpdateEntity(Entity *e, float dt) {
    if (!e->alive) return;

    // Energy regen: roughly one pip every 3 seconds at baseline.
    e->energyRegenAccum += dt;
    const float regenInterval = 3.0f;
    if (e->energyRegenAccum >= regenInterval) {
        e->energyRegenAccum -= regenInterval;
        e->energy++;
        if (e->energy > e->maxEnergy) e->energy = e->maxEnergy;
    }

    // Health regen: nothing while anyone's recently traded blows, then a
    // fast percentage-of-max regen once things have been quiet a moment.
    e->timeSinceCombat += dt;
    if (e->timeSinceCombat >= OUT_OF_COMBAT_DELAY && e->hp > 0 && e->hp < e->maxHp) {
        e->hpRegenAccum += (float)e->maxHp * OUT_OF_COMBAT_REGEN_PCT_PER_SEC * dt;
        while (e->hpRegenAccum >= 1.0f && e->hp < e->maxHp) {
            e->hpRegenAccum -= 1.0f;
            e->hp++;
        }
    } else {
        e->hpRegenAccum = 0.0f;
    }

    if (e->interruptFlashTimer > 0.0f) {
        e->interruptFlashTimer -= dt;
        if (e->interruptFlashTimer < 0.0f) e->interruptFlashTimer = 0.0f;
    }

    if (e->dodgeFlashTimer > 0.0f) {
        e->dodgeFlashTimer -= dt;
        if (e->dodgeFlashTimer < 0.0f) e->dodgeFlashTimer = 0.0f;
    }

    if (e->postCastDisplayTimer > 0.0f) {
        e->postCastDisplayTimer -= dt;
        if (e->postCastDisplayTimer < 0.0f) e->postCastDisplayTimer = 0.0f;
    }

    for (int i = 0; i < SKILL_BAR_SIZE; i++) {
        if (e->skillRecharge[i] > 0.0f) {
            e->skillRecharge[i] -= dt;
            if (e->skillRecharge[i] < 0.0f) e->skillRecharge[i] = 0.0f;
        }
    }

    for (int i = 0; i < MAX_ACTIVE_EFFECTS; i++) {
        ActiveEffect *fx = &e->effects[i];
        if (!fx->active) continue;
        fx->remaining -= dt;
        if (fx->tickDamage > 0.0f) {
            fx->tickAccum += dt;
            if (fx->tickAccum >= 1.0f) {
                fx->tickAccum -= 1.0f;
                Entity_ApplyDamage(e, (int)fx->tickDamage, NULL);
            }
        }
        if (fx->remaining <= 0.0f) fx->active = false;
    }

    if (Entity_IsCasting(e)) {
        e->castTimeRemaining -= dt;
        if (e->castTimeRemaining <= 0.0f) {
            ResolveCast(e);
        }
        return; // no movement/attacks while casting
    }

    if (e->hasMoveTarget) {
        float dx = e->moveTarget.x - e->pos.x;
        float dy = e->moveTarget.y - e->pos.y;
        float d = sqrtf(dx * dx + dy * dy);
        if (d > 2.0f) {
            e->pos.x += (dx / d) * e->moveSpeed * dt;
            e->pos.y += (dy / d) * e->moveSpeed * dt;
        } else {
            e->hasMoveTarget = false;
        }
    }

    Entity *target = Entity_Get(e->targetIndex);
    if (target && target->alive && target->team != e->team) {
        float d = Dist(e->pos, target->pos);
        if (d <= e->attackRange) {
            e->hasMoveTarget = false;
            e->attackTimer -= dt;
            if (e->attackTimer <= 0.0f) {
                int dmg = e->attackDamageMin + GetRandomValue(0, e->attackDamageMax - e->attackDamageMin);
                Entity_MarkInCombat(e);
                if (e->attackRange > RANGED_ATTACK_THRESHOLD) {
                    // Ranged: a visible bolt flies to where the target is
                    // standing NOW; adrenaline/aggro/damage resolve on
                    // impact - or not at all, if they dodge (projectile.c).
                    Projectile_Spawn((int)(e - g_entities), e->targetIndex, dmg);
                } else {
                    Entity_ApplyDamage(target, dmg, e);
                    if (target->kind == ENT_MONSTER && !target->aggroed) {
                        // Landing a hit wakes a sleeping monster up regardless
                        // of its aggro range - pulling with melee still works,
                        // it just means getting close enough to swing first.
                        target->aggroed = true;
                        target->targetIndex = (int)(e - g_entities);
                    }
                    e->adrenaline += 4; // basic attacks also build adrenaline in GW1
                    if (e->adrenaline > 100) e->adrenaline = 100;
                }
                e->attackTimer = e->attackInterval;
            }
        } else {
            e->moveTarget = target->pos;
            e->hasMoveTarget = true;
        }
    }
}

void Combat_TickTimers(float dt) {
    for (int i = 0; i < g_entityCount; i++) {
        Combat_UpdateEntity(&g_entities[i], dt);
    }
}
