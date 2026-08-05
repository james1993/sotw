#include "effect.h"
#include "fx.h"
#include <math.h>
#include <stddef.h>

static float RankScaledValue(const EffectStep *step, const Entity *caster, AttributeKind attr) {
    int rank = (attr >= 0 && attr < ATTR_COUNT) ? caster->attributeRank[attr] : 0;
    return step->baseValue + step->perAttributeRank * (float)rank;
}

// Per-second health loss for the afflictions that degenerate. GW1 calls
// these "degeneration pips"; keeping the numbers in one table means a
// hex and a condition that both tick are balanced against each other
// rather than against whatever the skill author happened to type.
static float AfflictionTickDamage(EffectCategory category, int kind) {
    if (category == EFFECT_HEX) {
        return ((HexKind)kind == HEX_SHROUD_OF_DOUBT) ? 1.0f : 0.0f;
    }
    switch ((ConditionKind)kind) {
        case COND_BLEEDING: return 2.0f;
        case COND_BURNING:  return 5.0f;
        default:            return 0.0f; // Crippled/Weakness impair, not damage
    }
}

// Applies a condition or hex, refreshing rather than stacking when the
// same affliction is already present - GW1's rule, and the reason
// spamming one skill doesn't multiply its degeneration.
static void ApplyAffliction(Entity *target, EffectCategory category, int kind, float duration) {
    ActiveEffect *free = NULL;
    for (int i = 0; i < MAX_ACTIVE_EFFECTS; i++) {
        ActiveEffect *fx = &target->effects[i];
        if (fx->active && fx->category == category && fx->kind == kind) {
            if (duration > fx->remaining) fx->remaining = duration;
            return;
        }
        if (!free && !fx->active) free = fx;
    }
    if (!free) return; // effect budget full; the weakest thing to do is nothing
    free->active = true;
    free->category = category;
    free->kind = kind;
    free->remaining = duration;
    free->tickAccum = 0.0f;
    free->tickDamage = AfflictionTickDamage(category, kind);
}

static void ApplyStepToEntity(Entity *caster, const Skill *skill, const EffectStep *step, Entity *target) {
    if (!target || !target->alive) return;

    switch (step->kind) {
        case FX_DAMAGE: {
            int dmg = (int)RankScaledValue(step, caster, skill->attribute);
            Entity_ApplyDamage(target, dmg, caster);
            Fx_Burst(target->pos, Fx_AttrColor(skill->attribute));
            Entity_MarkInCombat(caster);
            if (target->kind == ENT_MONSTER && !target->aggroed) {
                // A ranged pull: damaging a sleeping monster wakes it up
                // even from outside its passive aggro range, same as
                // GW1 - this is what lets a bow/spell pull work at all.
                // Its campmates come with it (group aggro).
                Entity_WakeMonsterGroup(target, Entity_RefOf((int)(caster - g_entities)));
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
            Fx_Heal(target->pos);
            target->hp += amount;
            if (target->hp > target->maxHp) target->hp = target->maxHp;
            break;
        }
        case FX_APPLY_CONDITION:
        case FX_APPLY_HEX: {
            // GW1 semantics: reapplying refreshes duration; the same
            // affliction never occupies two slots. Category is part of
            // the identity, so a hex and a condition that happen to
            // share a numeric kind are still distinct effects.
            EffectCategory category =
                (step->kind == FX_APPLY_HEX) ? EFFECT_HEX : EFFECT_CONDITION;
            ApplyAffliction(target, category, step->conditionKind, step->duration);
            break;
        }
        case FX_REMOVE_CONDITION:
        case FX_REMOVE_HEX: {
            // Cleanses land on an ally, and the count comes from the
            // step so one skill can strip a single affliction and
            // another can wipe the board.
            EffectCategory category =
                (step->kind == FX_REMOVE_HEX) ? EFFECT_HEX : EFFECT_CONDITION;
            int want = (step->conditionKind > 0) ? step->conditionKind : 1;
            int removed = Entity_RemoveEffects(target, category, want);
            if (removed > 0) {
                // A visible pop so a cleanse reads as having done
                // something even when nothing else about the target
                // changes on screen.
                Fx_Burst(target->pos, category == EFFECT_HEX
                         ? (Color){ 178, 118, 220, 255 }
                         : (Color){ 200, 230, 255, 255 });
                // Cleanses that also heal put the number in baseValue.
                int heal = (int)RankScaledValue(step, caster, skill->attribute);
                if (heal > 0) {
                    heal += 3 * caster->attributeRank[ATTR_DIVINE_FAVOR];
                    target->hp += heal;
                    if (target->hp > target->maxHp) target->hp = target->maxHp;
                    Fx_Heal(target->pos);
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
            Fx_Stars(target->pos);
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
            if (i == 0) Fx_Ring(origin, skill->aoeRadius, Fx_AttrColor(skill->attribute));
            for (int j = 0; j < g_entityCount; j++) {
                Entity *other = &g_entities[j];
                if (!other->alive || other->team == caster->team) continue;
                float dx = other->pos.x - origin.x;
                float dy = other->pos.y - origin.y;
                if (sqrtf(dx * dx + dy * dy) <= skill->aoeRadius) {
                    ApplyStepToEntity(caster, skill, step, other);
                }
            }
            continue;
        }

        // TARGET_SINGLE_FOE / TARGET_SINGLE_ALLY
        ApplyStepToEntity(caster, skill, step, target);
    }
}
