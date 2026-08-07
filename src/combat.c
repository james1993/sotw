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
                                        caster->attributeRank[ATTR_EXPERTISE],
                                        skill->type == SKILLTYPE_ATTACK_SKILL);

    if (energyCost > 0 && caster->energy < energyCost) return false;
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
    caster->adrenaline -= skill->adrenalineCost;
    if (caster->adrenaline < 0) caster->adrenaline = 0;

    // Which skill the target panel shows as "current/recent" - set here so
    // it covers both branches below, not just cast-time skills.
    caster->lastCastSkillSlot = slot;
    Audio_PlaySkill(skill);

    // Fast Casting (Mesmer primary) multiplies SPELL activation by
    // 2^(-rank/15) - rank 15 halves it. Signets and attack skills are
    // untouched, which is why a Mesmer bar is spells almost end to end.
    float castTime = GW_SkillCastTime(skill->castTime,
                                      caster->attributeRank[ATTR_FAST_CASTING],
                                      skill->type == SKILLTYPE_SPELL);

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

    caster->skillRecharge[slot] = skill->recharge;
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
    float regenInterval = Entity_EnergyRegenInterval(e);
    e->energyRegenAccum += dt;
    while (e->energyRegenAccum >= regenInterval) {
        e->energyRegenAccum -= regenInterval;
        e->energy++;
        if (e->energy > e->maxEnergy) e->energy = e->maxEnergy;
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

    // The zone's playable area is a hard wall for everyone.
    {
        Rectangle b = World_GetBounds();
        if (e->pos.x < b.x) e->pos.x = b.x;
        if (e->pos.y < b.y) e->pos.y = b.y;
        if (e->pos.x > b.x + b.width) e->pos.x = b.x + b.width;
        if (e->pos.y > b.y + b.height) e->pos.y = b.y + b.height;
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
                    Entity_ApplyDamage(target, dmg, e);
                    Fx_Slash(target->pos, atan2f(target->pos.y - e->pos.y,
                                                 target->pos.x - e->pos.x));
                    if (target->kind == ENT_MONSTER && !target->aggroed) {
                        // Landing a hit wakes a sleeping monster up regardless
                        // of its aggro range - pulling with melee still works,
                        // it just means getting close enough to swing first.
                        // Group members join in, GW1-style.
                        Entity_WakeMonsterGroup(target, Entity_RefOf((int)(e - g_entities)));
                    }
                    e->adrenaline += 4; // basic attacks also build adrenaline in GW1
                    if (e->adrenaline > 100) e->adrenaline = 100;
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
