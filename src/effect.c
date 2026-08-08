#include "effect.h"
#include "fx.h"
#include "gwmath.h"
#include <math.h>
#include <stddef.h>

// Strength's armor penetration, which GW1 grants only on attack skills -
// a Warrior's spells and shouts penetrate nothing. Returns a 0..1
// fraction of the target's armor to ignore.
static float AttackPenetration(const Entity *caster, const Skill *skill) {
    if (skill->type != SKILLTYPE_ATTACK_SKILL) return 0.0f;
    // A skill's own penetration (Penetrating Blow) plus Strength's, which
    // GW1 grants only on attack skills. Capped at 1 so nothing goes past
    // ignoring all armor.
    float pen = skill->armorPen +
                GW_STRENGTH_PENETRATION_PER_RANK * (float)Entity_EffectiveRank(caster, ATTR_STRENGTH);
    return pen > 1.0f ? 1.0f : pen;
}

// Divine Favor fires on Monk SPELLS cast on an ally - not on signets,
// not on a secondary's borrowed Healing Prayers used by someone whose
// primary is something else (they simply can't have the rank).
static int DivineFavorBonus(const Entity *caster, const Skill *skill) {
    if (skill->type != SKILLTYPE_SPELL) return 0;
    if (g_attributeProfession[skill->attribute] != PROF_MONK) return 0;
    return GW_DivineFavorBonus(Entity_EffectiveRank(caster, ATTR_DIVINE_FAVOR));
}

static float RankScaledValue(const EffectStep *step, const Entity *caster, AttributeKind attr) {
    int rank = Entity_EffectiveRank(caster, attr);
    return step->baseValue + step->perAttributeRank * (float)rank;
}

// Health degeneration in GW1's PIPS. This used to return health per
// second and hand back the pip NUMBER - so Bleeding ticked 2 a second
// instead of its real 6, and Burning 5 instead of 14. Conditions were
// roughly a third as dangerous as GW1's, which is most of the reason
// they read as ignorable. One pip is two health a second; that
// conversion lives in Combat_UpdateEntity, once.
static float AfflictionDegenPips(EffectCategory category, int kind) {
    if (category == EFFECT_HEX) {
        switch ((HexKind)kind) {
            case HEX_FALTERING: return 1.0f;
            case HEX_PHANTASM:  return GW_PIPS_PHANTASM;
            default:            return 0.0f;
        }
    }
    switch ((ConditionKind)kind) {
        case COND_BLEEDING: return GW_PIPS_BLEEDING;
        case COND_BURNING:  return GW_PIPS_BURNING;
        case COND_POISON:   return GW_PIPS_POISON;
        default:            return 0.0f; // Crippled/Weakness impair, not degenerate
    }
}

// Applies a condition or hex, refreshing rather than stacking when the
// same affliction is already present - GW1's rule, and the reason
// spamming one skill doesn't multiply its degeneration.
static void ApplyAffliction(Entity *target, EffectCategory category, int kind,
                            float duration, float magnitude) {
    ActiveEffect *free = NULL;
    for (int i = 0; i < MAX_ACTIVE_EFFECTS; i++) {
        ActiveEffect *fx = &target->effects[i];
        if (fx->active && fx->category == category && fx->kind == kind) {
            if (duration > fx->remaining) fx->remaining = duration;
            // A recast refreshes the magnitude too (a stronger Mending
            // replaces a weaker one rather than being ignored).
            fx->magnitude = magnitude;
            if (category == EFFECT_ENCHANTMENT && kind == ENCH_REGEN) {
                fx->degenPips = -magnitude;
            }
            return;
        }
        if (!free && !fx->active) free = fx;
    }
    if (!free) return; // effect budget full; the weakest thing to do is nothing
    free->active = true;
    free->category = category;
    free->kind = kind;
    free->remaining = duration;
    free->magnitude = magnitude;
    // Regenerating enchantments feed the same health-drift sum as
    // degeneration, but with the opposite sign.
    if (category == EFFECT_ENCHANTMENT) {
        free->degenPips = (kind == ENCH_REGEN) ? -magnitude : 0.0f;
    } else {
        free->degenPips = AfflictionDegenPips(category, kind);
    }
    // Deep Wound changes the health CEILING, so the derived stats have
    // to move with it rather than at the next thing that happens to
    // recompute them.
    if (category == EFFECT_CONDITION && (ConditionKind)kind == COND_DEEP_WOUND) {
        Entity_RecomputePenalizedStats(target);
    }
}

static void ApplyStepToEntity(Entity *caster, const Skill *skill, const EffectStep *step, Entity *target) {
    if (!target || !target->alive) return;

    switch (step->kind) {
        case FX_DAMAGE: {
            int dmg = (int)RankScaledValue(step, caster, skill->attribute);
            Entity_ApplyDamagePen(target, dmg, caster, AttackPenetration(caster, skill));
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
            amount = Entity_ScaleIncomingHeal(target, amount); // Deep Wound
            // Divine Favor: GW1's Monk primary adds 3.2 healing per rank
            // to every Monk spell that heals - the whole reason a primary
            // Monk out-heals a secondary one with identical Healing
            // Prayers, and the reason it's worth being one.
            amount += DivineFavorBonus(caster, skill);
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
            ApplyAffliction(target, category, step->conditionKind, step->duration, 0.0f);
            break;
        }
        case FX_APPLY_ENCHANTMENT: {
            // Lands on an ally or the caster (enchantments never target a
            // foe). The magnitude is rank-scaled, so a higher Healing
            // Prayers gives more regeneration.
            float mag = RankScaledValue(step, caster, skill->attribute);
            ApplyAffliction(target, EFFECT_ENCHANTMENT, step->conditionKind,
                            step->duration, mag);
            Fx_Heal(target->pos);
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
                    heal += DivineFavorBonus(caster, skill);
                    target->hp += heal;
                    if (target->hp > target->maxHp) target->hp = target->maxHp;
                    Fx_Heal(target->pos);
                }
            }
            break;
        }
        case FX_REMOVE_ENCHANTMENT: {
            // Enchantment stripping - the counter to the whole system, and
            // aimed at a FOE. Shatter Enchantment's damage-on-removal lives
            // in baseValue, so it fires only when something was actually
            // torn off.
            int want = (step->conditionKind > 0) ? step->conditionKind : 1;
            int removed = Entity_RemoveEffects(target, EFFECT_ENCHANTMENT, want);
            if (removed > 0) {
                Fx_Burst(target->pos, (Color){ 120, 210, 130, 255 });
                int dmg = (int)RankScaledValue(step, caster, skill->attribute);
                if (dmg > 0) Entity_ApplyDamage(target, dmg, caster);
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
            // Points, spread across every adrenal skill on the bar - the
            // same way a landed strike charges them all.
            Entity_AddAdrenalinePoints(target, (int)step->baseValue);
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
        case FX_STANCE_BLOCK: {
            // A self-buff, always landing on the caster (stances are
            // TARGET_SELF). Duration scales with the skill's attribute -
            // Disciplined Stance holds longer the more Tactics you have.
            int rank = Entity_EffectiveRank(caster, skill->attribute);
            target->blockChance = step->baseValue;
            target->stanceArmorBonus = step->conditionKind;
            target->stanceTimer = step->duration + step->perAttributeRank * (float)rank;
            Fx_Ring(target->pos, target->radius + 10.0f, (Color){ 150, 200, 240, 255 });
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
    // An attack skill is still an ATTACK: it can be missed because the
    // caster is Blind, or blocked by the target's stance, and when it is,
    // NONE of its steps land - a blocked Gash deals no damage and inflicts
    // no Deep Wound. Spells and shouts skip this entirely, exactly as in
    // GW1, which is why a Blinded Elementalist still casts fine.
    if (skill->type == SKILLTYPE_ATTACK_SKILL &&
        (skill->targeting == TARGET_SINGLE_FOE || skill->targeting == TARGET_AOE_FOES) &&
        target && target->alive && target->team != caster->team) {
        AttackOutcome outcome = Entity_ResolveAttack(caster, target);
        if (outcome != ATTACK_LANDS) {
            Fx_Burst(target->pos, (Color){ 200, 200, 210, 200 });
            return;
        }
    }

    for (int i = 0; i < skill->stepCount; i++) {
        const EffectStep *step = &skill->steps[i];

        // A self step lands on the caster no matter what the skill is
        // aimed at, which is how one skill can drain a foe and feed you
        // in the same breath.
        if (step->selfTarget) {
            ApplyStepToEntity(caster, skill, step, caster);
            continue;
        }

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
