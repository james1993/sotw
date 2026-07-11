#include "effect.h"
#include <math.h>

static float RankScaledValue(const EffectStep *step, const Entity *caster, AttributeKind attr) {
    int rank = (attr >= 0 && attr < ATTR_COUNT) ? caster->attributeRank[attr] : 0;
    return step->baseValue + step->perAttributeRank * (float)rank;
}

static void ApplyStepToEntity(Entity *caster, const Skill *skill, const EffectStep *step, Entity *target) {
    if (!target || !target->alive) return;

    switch (step->kind) {
        case FX_DAMAGE: {
            int dmg = (int)RankScaledValue(step, caster, skill->attribute);
            Entity_ApplyDamage(target, dmg, caster);
            Entity_MarkInCombat(caster);
            if (target->kind == ENT_MONSTER && !target->aggroed) {
                // A ranged pull: damaging a sleeping monster wakes it up
                // even from outside its passive aggro range, same as
                // GW1 - this is what lets a bow/spell pull work at all.
                target->aggroed = true;
                target->targetIndex = (int)(caster - g_entities);
            }
            break;
        }
        case FX_HEAL: {
            int amount = (int)RankScaledValue(step, caster, skill->attribute);
            // Divine Favor: GW1's Monk primary attribute adds bonus
            // healing (~3.2/rank there, 3/rank here) to every monk spell
            // that heals - the whole reason a primary Monk out-heals a
            // secondary one with identical Healing Prayers.
            amount += 3 * caster->attributeRank[ATTR_DIVINE_FAVOR];
            target->hp += amount;
            if (target->hp > target->maxHp) target->hp = target->maxHp;
            break;
        }
        case FX_APPLY_CONDITION: {
            for (int i = 0; i < MAX_ACTIVE_EFFECTS; i++) {
                if (!target->effects[i].active) {
                    target->effects[i].active = true;
                    target->effects[i].kind = (ConditionKind)step->conditionKind;
                    target->effects[i].remaining = step->duration;
                    target->effects[i].tickAccum = 0.0f;
                    target->effects[i].tickDamage =
                        (step->conditionKind == COND_BLEEDING) ? 2.0f :
                        (step->conditionKind == COND_BURNING) ? 5.0f : 0.0f;
                    break;
                }
            }
            break;
        }
        case FX_ENERGY_DELTA: {
            int amount = (int)RankScaledValue(step, caster, skill->attribute);
            target->energy += amount;
            if (target->energy > target->maxEnergy) target->energy = target->maxEnergy;
            if (target->energy < 0) target->energy = 0;
            break;
        }
        case FX_ADRENALINE_DELTA: {
            target->adrenaline += (int)step->baseValue;
            if (target->adrenaline > 100) target->adrenaline = 100;
            if (target->adrenaline < 0) target->adrenaline = 0;
            break;
        }
        case FX_KNOCKDOWN: {
            // Simplified: cancels the target's current action. A full
            // implementation would add a movement/cast lockout timer
            // driven by step->duration.
            target->castingSlot = -1;
            target->hasMoveTarget = false;
            break;
        }
        case FX_INTERRUPT: {
            if (target->castingSlot >= 0) {
                int slot = target->castingSlot;
                int interruptedSkillIdx = target->skillBar[slot];
                if (interruptedSkillIdx >= 0 && interruptedSkillIdx < g_skillCount) {
                    // GW1's interrupt penalty: the interrupted skill gets a
                    // longer recharge than if it had simply been used.
                    target->skillRecharge[slot] = g_skillDB[interruptedSkillIdx].recharge * 2.0f;
                }
                target->castingSlot = -1;
                target->castTimeRemaining = 0.0f;
                target->interruptFlashTimer = 1.0f;
                target->lastCastInterrupted = true;
                target->postCastDisplayTimer = 3.0f;
            }
            break;
        }
        default:
            break;
    }
}

void Effect_Execute(Entity *caster, const Skill *skill, Entity *target) {
    for (int i = 0; i < skill->stepCount; i++) {
        const EffectStep *step = &skill->steps[i];

        if (skill->targeting == TARGET_SELF) {
            ApplyStepToEntity(caster, skill, step, caster);
            continue;
        }

        if (skill->targeting == TARGET_AOE_FOES) {
            Vector2 origin = target ? target->pos : caster->pos;
            for (int j = 0; j < g_entityCount; j++) {
                Entity *other = &g_entities[j];
                if (!other->alive || other->team == caster->team) continue;
                float dx = other->pos.x - origin.x;
                float dy = other->pos.y - origin.y;
                if (sqrtf(dx * dx + dy * dy) <= 64.0f) { // fixed AoE footprint radius
                    ApplyStepToEntity(caster, skill, step, other);
                }
            }
            continue;
        }

        // TARGET_SINGLE_FOE / TARGET_SINGLE_ALLY
        ApplyStepToEntity(caster, skill, step, target);
    }
}
