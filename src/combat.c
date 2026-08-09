#include "combat.h"
#include "audio.h"
#include "gwmath.h"
#include "skill.h"
#include "effect.h"
#include "world.h"
#include "projectile.h"
#include "fx.h"
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
    if (caster->knockdownTimer > 0.0f) return false; // knocked down: no actions
    if (slot < 0 || slot >= SKILL_BAR_SIZE) return false;

    int skillIdx = caster->skillBar[slot];
    if (skillIdx < 0 || skillIdx >= g_skillCount) return false;
    if (Entity_IsCasting(caster)) return false;
    if (caster->skillRecharge[slot] > 0.0f) return false;

    Skill *skill = &g_skillDB[skillIdx];

    // Expertise (Ranger primary) discounts attack skills by 4% a rank,
    // which is the only reason a Ranger can afford to press one every
    // few seconds off a 20-energy pool.
    int energyCost = GW_SkillEnergyCost(skill->energyCost,
                                        Entity_EffectiveRank(caster, ATTR_EXPERTISE),
                                        skill->type == SKILLTYPE_ATTACK_SKILL);

    if (energyCost > 0 && caster->energy < energyCost) return false;
    if (skill->adrenalineCost > 0 && caster->adrenaline[slot] < skill->adrenalineCost) return false;

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
    EntityRef targetRef = Entity_RefOf(targetIndex);

    // Casting AT a foe is an attack order: it commits the caster (and,
    // through them, the party) to closing with that foe. Ally- and
    // self-targeted skills deliberately do not, so healing mid-retreat
    // doesn't turn the retreat into a charge.
    if (target && target->team != caster->team && skill->targeting != TARGET_SELF) {
        caster->targetRef = targetRef;
        caster->engaged = true;
    }

    caster->energy -= energyCost;
    if (skill->adrenalineCost > 0) {
        Entity_SpendAdrenaline(caster, slot);
        // Disciplined Stance ends the instant you throw an adrenal skill -
        // the trade GW1 makes you weigh between defending and swinging.
        Entity_BreakStance(caster);
    }
    // Exhaustion: an overcast skill eats into the energy ceiling until it
    // recovers (see Combat_UpdateEntity).
    if (skill->exhausting) {
        caster->exhaustion += GW_EXHAUSTION_PER_CAST;
        if (caster->exhaustion > (float)caster->maxEnergy) caster->exhaustion = (float)caster->maxEnergy;
    }
    // Diversion has been waiting for the target's next skill - this is it.
    // The penalty is carried on the caster and folded into this skill's
    // recharge when it is set (below, or in ResolveCast for a timed cast).
    caster->divPenalty = 0.0f;
    for (int i = 0; i < MAX_ACTIVE_EFFECTS; i++) {
        ActiveEffect *fx = &caster->effects[i];
        if (fx->active && fx->category == EFFECT_HEX && fx->kind == HEX_DIVERSION) {
            caster->divPenalty = fx->magnitude;
            fx->active = false;
            break;
        }
    }

    // Which skill the target panel shows as "current/recent" - set here so
    // it covers both branches below, not just cast-time skills.
    caster->lastCastSkillSlot = slot;
    Audio_PlaySkill(skill);

    // Fast Casting (Mesmer primary) multiplies SPELL activation by
    // 2^(-rank/15) - rank 15 halves it. Signets and attack skills are
    // untouched, which is why a Mesmer bar is spells almost end to end.
    float castTime = GW_SkillCastTime(skill->castTime,
                                      Entity_EffectiveRank(caster, ATTR_FAST_CASTING),
                                      skill->type == SKILLTYPE_SPELL);
    // Dazed doubles spell activation (its interrupt-vulnerability half is
    // not modelled). Attack skills and signets are untouched.
    if (skill->type == SKILLTYPE_SPELL && Entity_HasCondition(caster, COND_DAZED)) {
        castTime *= 2.0f;
    }

    if (castTime > 0.0f) {
        caster->castingSlot = slot;
        caster->castTimeRemaining = castTime;
        caster->castTimeTotal = castTime;
        caster->castTargetRef = targetRef;
        // Chasing/attacking only follows foe targets, so only offensive
        // casts update the caster's current target.
        if (skill->targeting == TARGET_SINGLE_FOE || skill->targeting == TARGET_AOE_FOES) {
            caster->targetRef = targetRef;
        }
        caster->hasMoveTarget = false; // casting roots the caster, matches GW1 spellcasting
    } else {
        Effect_Execute(caster, skill, target);
        caster->skillRecharge[slot] = skill->recharge + caster->divPenalty;
        caster->divPenalty = 0.0f;
        caster->lastCastInterrupted = false;
        caster->postCastDisplayTimer = 3.0f;
    }
    return true;
}

static void ResolveCast(Entity *caster) {
    int slot = caster->castingSlot;
    int skillIdx = caster->skillBar[slot];
    Skill *skill = &g_skillDB[skillIdx];
    Entity *target = Entity_Resolve(caster->castTargetRef);

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

    caster->skillRecharge[slot] = skill->recharge + caster->divPenalty;
    caster->divPenalty = 0.0f;
    caster->castingSlot = -1;
    caster->lastCastInterrupted = false;
    caster->postCastDisplayTimer = 3.0f;
}

void Combat_UpdateEntity(Entity *e, float dt) {
    if (!e->alive) return;

    // Energy regen on GW1's real clock: a pip is 1 energy every 3
    // seconds, everyone has 3 pips, so this is 1 energy per second. The
    // accumulator doubles as the fractional part the resource bars use
    // to fill smoothly between whole-point ticks.
    // Exhaustion recovers slowly and lowers the energy ceiling while it
    // lasts; maintained enchantments (upkeep) slow the regen itself. Both
    // are GW1 energy mechanics that the flat "1 per second" hid.
    if (e->exhaustion > 0.0f) {
        e->exhaustion -= GW_EXHAUSTION_RECOVER_PER_SEC * dt;
        if (e->exhaustion < 0.0f) e->exhaustion = 0.0f;
    }
    int energyCap = e->maxEnergy - (int)e->exhaustion;
    if (energyCap < 0) energyCap = 0;
    if (e->energy > energyCap) e->energy = energyCap; // exhaustion bites immediately

    int effPips = e->energyRegenPips - Entity_UpkeepPips(e) - e->weaponEnergyDrain;
    if (effPips > 0) {
        float regenInterval = GW_EnergyRegenInterval(effPips);
        e->energyRegenAccum += dt;
        while (e->energyRegenAccum >= regenInterval) {
            e->energyRegenAccum -= regenInterval;
            e->energy++;
            if (e->energy > energyCap) e->energy = energyCap;
        }
    } else {
        e->energyRegenAccum = 0.0f; // no net regen while upkeep cancels it
    }

    // Soul Reaping's rolling 15-second trigger window (entity.c).
    if (e->soulReapingWindow > 0.0f) {
        e->soulReapingWindow -= dt;
        if (e->soulReapingWindow <= 0.0f) e->soulReapingTriggers = 0;
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

    if (e->blindMissFlashTimer > 0.0f) {
        e->blindMissFlashTimer -= dt;
        if (e->blindMissFlashTimer < 0.0f) e->blindMissFlashTimer = 0.0f;
    }

    if (e->blockFlashTimer > 0.0f) {
        e->blockFlashTimer -= dt;
        if (e->blockFlashTimer < 0.0f) e->blockFlashTimer = 0.0f;
    }

    // A defensive stance runs on its own clock and simply lapses when it
    // runs out (it also ends early if the holder uses an adrenal skill -
    // see Combat_ActivateSkill).
    if (e->stanceTimer > 0.0f) {
        e->stanceTimer -= dt;
        if (e->stanceTimer <= 0.0f) Entity_BreakStance(e);
    }

    if (e->postCastDisplayTimer > 0.0f) {
        e->postCastDisplayTimer -= dt;
        if (e->postCastDisplayTimer < 0.0f) e->postCastDisplayTimer = 0.0f;
    }

    if (e->attackAnimTimer > 0.0f) {
        e->attackAnimTimer -= dt;
        if (e->attackAnimTimer < 0.0f) e->attackAnimTimer = 0.0f;
    }

    for (int i = 0; i < SKILL_BAR_SIZE; i++) {
        if (e->skillRecharge[i] > 0.0f) {
            e->skillRecharge[i] -= dt;
            if (e->skillRecharge[i] < 0.0f) e->skillRecharge[i] = 0.0f;
        }
    }

    // Degeneration, GW1's way: every source contributes PIPS, the pips
    // are summed, the total is capped at 10, and only then does it
    // become health. Ticking each condition separately (as this used to)
    // both got the rate wrong and made the cap impossible to express -
    // and the cap is the whole reason a stack of conditions wears you
    // down instead of deleting you.
    {
        float pips = 0.0f;
        for (int i = 0; i < MAX_ACTIVE_EFFECTS; i++) {
            ActiveEffect *fx = &e->effects[i];
            if (!fx->active) continue;
            fx->remaining -= dt;
            pips += fx->degenPips;
            if (fx->remaining <= 0.0f) fx->active = false;
        }
        pips += (float)e->weaponHealthDegen; // Vampiric's -1 health regen drawback
        // Degeneration (conditions/hexes, positive pips) and regeneration
        // (enchantments, negative pips) net against each other in this one
        // sum, and the result is capped at +/-10 pips - GW1's health-drift
        // arrows exactly. Positive drains, negative heals.
        if (pips > GW_MAX_DEGEN_PIPS) pips = GW_MAX_DEGEN_PIPS;
        if (pips < -GW_MAX_DEGEN_PIPS) pips = -GW_MAX_DEGEN_PIPS;
        if (pips != 0.0f) {
            // Continuous, not one-second lumps: GW1's health bar slides.
            e->degenAccum += pips * GW_HEALTH_PER_PIP * dt;
            while (e->degenAccum >= 1.0f && e->alive) {
                e->degenAccum -= 1.0f;
                Entity_ApplyDamage(e, 1, NULL);
            }
            while (e->degenAccum <= -1.0f && e->alive && e->hp < e->maxHp) {
                e->degenAccum += 1.0f;
                e->hp++;
            }
            // Don't bank regen once the bar is full - GW1 wastes the overflow.
            if (e->hp >= e->maxHp && e->degenAccum < 0.0f) e->degenAccum = 0.0f;
        } else {
            e->degenAccum = 0.0f;
        }
    }

    // Knockdown is a real timed lockout: recharge and degeneration above
    // keep running (GW1 doesn't pause them), but nothing below - no
    // casting, moving or attacking - happens until you get up.
    if (e->knockdownTimer > 0.0f) {
        e->knockdownTimer -= dt;
        if (e->knockdownTimer < 0.0f) e->knockdownTimer = 0.0f;
        return;
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
            // Clamp to the remaining distance so a long frame (window
            // drag, zone hiccup) can't overshoot the target and oscillate.
            float step = Entity_MoveSpeed(e) * dt;
            if (step > d) step = d;
            e->pos.x += (dx / d) * step;
            e->pos.y += (dy / d) * step;
        } else {
            e->hasMoveTarget = false;
        }
    }

    // The zone's edge. The bounds rectangle is an invisible backstop;
    // what actually stops you is the ridge of rock generated inside it,
    // which is why the walkable region isn't a rectangle.
    {
        Rectangle b = World_GetBounds();
        if (e->pos.x < b.x) e->pos.x = b.x;
        if (e->pos.y < b.y) e->pos.y = b.y;
        if (e->pos.x > b.x + b.width) e->pos.x = b.x + b.width;
        if (e->pos.y > b.y + b.height) e->pos.y = b.y + b.height;
        World_ResolveBarriers(&e->pos, e->radius);
    }

    Entity *target = Entity_Resolve(e->targetRef);
    if (target && target->alive && target->team != e->team && e->engaged) {
        float d = Dist(e->pos, target->pos);
        if (d <= e->attackRange) {
            e->hasMoveTarget = false;
            e->attackTimer -= dt;
            if (e->attackTimer <= 0.0f) {
                int dmg = e->attackDamageMin + GetRandomValue(0, e->attackDamageMax - e->attackDamageMin);
                dmg = Entity_ScaleOutgoingDamage(e, dmg); // Weakness
                Entity_MarkInCombat(e);

                // Price of Faith: the hex punishes the act of attacking,
                // so it resolves on the swing itself rather than on the
                // hit - it costs you even when the blow misses or the
                // projectile is dodged.
                if (Entity_HasHex(e, HEX_BACKLASH)) {
                    Entity_ApplyDamage(e, 12, NULL);
                    Fx_Burst(e->pos, (Color){ 178, 118, 220, 255 });
                }
                e->attackAnimTimer = ATTACK_ANIM_DURATION; // the visible swing (sprite.c)
                {
                    // Ranged does NOT mean bow: an Elementalist's staff
                    // and a Charr shaman's ranged attack are bolts of
                    // magic, and a bowstring on either sounds wrong. Only
                    // an actual archer gets the string.
                    Entity *player = Entity_Get(0);
                    float dx = player ? e->pos.x - player->pos.x : 0.0f;
                    SoundId id = SFX_SWING;
                    if (e->attackRange > RANGED_ATTACK_THRESHOLD) {
                        id = (e->primaryProfession == PROF_RANGER) ? SFX_BOW
                                                                   : SFX_CAST_ARCANE;
                    }
                    Audio_PlayAt(id, dx);
                }
                if (e->attackRange > RANGED_ATTACK_THRESHOLD) {
                    // Ranged: a visible bolt flies to where the target is
                    // standing NOW; adrenaline/aggro/damage resolve on
                    // impact - or not at all, if they dodge (projectile.c).
                    Projectile_Spawn((int)(e - g_entities), Entity_RefIndex(e->targetRef), dmg);
                } else {
                    // Melee still swings - the arc plays and a sleeping
                    // monster wakes to it - but Blind (attacker) and a
                    // block stance (target) can rob it of its damage. A
                    // missed or blocked swing earns no adrenaline in GW1.
                    AttackOutcome outcome = Entity_ResolveAttack(e, target);
                    Fx_Slash(target->pos, atan2f(target->pos.y - e->pos.y,
                                                 target->pos.x - e->pos.x));
                    if (target->kind == ENT_MONSTER && !target->aggroed) {
                        // Landing a hit wakes a sleeping monster up regardless
                        // of its aggro range - pulling with melee still works,
                        // it just means getting close enough to swing first.
                        // Group members join in, GW1-style.
                        Entity_WakeMonsterGroup(target, Entity_RefOf((int)(e - g_entities)));
                    }
                    if (outcome == ATTACK_LANDS) {
                        // Weapon mods ride on the basic swing: Sundering
                        // penetrates armour, Vampiric steals health.
                        Entity_ApplyDamagePen(target, dmg, e, e->weaponArmorPen / 100.0f);
                        if (e->weaponLifesteal > 0) {
                            e->hp += e->weaponLifesteal;
                            if (e->hp > e->maxHp) e->hp = e->maxHp;
                        }
                        if (e->weaponEnergyGain > 0) { // Zealous
                            e->energy += e->weaponEnergyGain;
                            if (e->energy > e->maxEnergy) e->energy = e->maxEnergy;
                        }
                        Entity_GainAdrenalineStrike(e); // one strike, GW1's unit
                    }
                }
                e->attackTimer = Entity_AttackInterval(e); // slowed while hexed
            }
        } else {
            e->moveTarget = target->pos;
            e->hasMoveTarget = true;
        }
    }
}

void Combat_TickTimers(float dt) {
    for (int i = 0; i < g_entityCount; i++) {
        Entity *e = &g_entities[i];
        Combat_UpdateEntity(e, dt);

        // Sprite animation bookkeeping, from actual movement this frame
        // (covers click-pathing, AI chasing, and stick movement alike).
        float mdx = e->pos.x - e->prevPos.x;
        float mdy = e->pos.y - e->prevPos.y;
        float md = sqrtf(mdx * mdx + mdy * mdy);
        if (md > 0.01f) {
            e->facing = (Vector2){ mdx / md, mdy / md };
            e->animTime += md * 0.055f; // stride frequency tied to speed
            e->moveBlend += dt * 8.0f;
        } else {
            e->moveBlend -= dt * 8.0f;
        }
        if (e->moveBlend < 0.0f) e->moveBlend = 0.0f;
        if (e->moveBlend > 1.0f) e->moveBlend = 1.0f;
        e->prevPos = e->pos;
    }
}
