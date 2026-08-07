#ifndef GWMATH_H
#define GWMATH_H

#include <stdbool.h>

// Guild Wars' actual formulas, in one file, so the numbers that define
// how the game feels are stated once and can be checked against the
// source material rather than being scattered as magic constants.
//
// References: docs/research/gw1-mechanics.md.

// --- Armor -----------------------------------------------------------
// Damage scales as 2^((base - AL)/40): every 40 points of armor halves
// incoming damage, every 40 below the 60-AL baseline doubles it. This is
// the single most important number in GW1 combat and the reason a
// Warrior at AL 80 takes roughly 70% of what a caster at AL 60 takes.
#define GW_ARMOR_BASELINE 60.0f
#define GW_ARMOR_HALVING  40.0f

// --- Energy ----------------------------------------------------------
// Every character has the same 20 base energy regardless of profession;
// casters do not get a bigger pool for free. Only Energy Storage (the
// Elementalist primary) raises it, at +3 per rank, which is exactly why
// an Elementalist is the profession that can afford expensive spells.
#define GW_BASE_ENERGY 20
#define GW_ENERGY_STORAGE_PER_RANK 3

// One pip of energy regeneration restores 1 energy every 3 seconds.
// Characters have 3 pips naturally, so baseline regeneration is 1
// energy per second - the clock every energy cost in the game is
// balanced against.
#define GW_SECONDS_PER_PIP 3.0f
#define GW_BASE_ENERGY_PIPS 3

// One pip of HEALTH regeneration is 2 health per second - a different
// rate from energy, which is a detail that trips people up.
#define GW_HEALTH_PER_PIP 2.0f

// Degeneration is measured in the same pips, and GW1 caps the total at
// 10 in either direction: 20 health a second, no matter how many
// sources are stacked on you. Without the cap, three conditions and a
// hex simply delete a character, which is the reason the cap exists.
#define GW_MAX_DEGEN_PIPS 10.0f

// GW1's condition degeneration, in pips. Bleeding is -3, which is SIX
// health a second, not three - the pip is the unit, not the health.
#define GW_PIPS_BLEEDING 3.0f
#define GW_PIPS_BURNING  7.0f
#define GW_PIPS_POISON   4.0f

// Weakness cuts weapon damage by 66% (bonus damage from attack skills
// is untouched) and drops every non-zero attribute by 1.
#define GW_WEAKNESS_DAMAGE_SCALE 0.34f

// Deep Wound: 20% off maximum health and 20% off healing received. It
// is the reason a spike lands - the target's ceiling drops at the same
// moment their healer's numbers get smaller.
#define GW_DEEP_WOUND_HEALTH_LOSS 0.20f
#define GW_DEEP_WOUND_HEAL_LOSS   0.20f

// --- Adrenaline ------------------------------------------------------
// GW1 counts adrenaline in STRIKES, worth 25 points each, and every
// adrenal skill carries its OWN pool. Landing a hit gives one strike to
// every adrenal skill on the bar; losing 1% of your maximum health
// gives one more point.
//
// The part that matters for play is what happens when you SPEND: the
// skill you fired empties, and every other adrenal skill on the bar
// loses 25 points. That is precisely why you cannot charge four adrenal
// skills and dump them together, and modelling adrenaline as one shared
// pool - as this did - quietly removed the constraint the whole
// Warrior bar is built around.
#define GW_ADRENALINE_PER_STRIKE 25
#define GW_ADRENALINE_CROSS_DRAIN 25

// --- Level -----------------------------------------------------------
// +20 maximum health per level, on top of a 100 base at level 1.
#define GW_BASE_HEALTH 100
#define GW_HEALTH_PER_LEVEL 20

// --- Primary attribute effects --------------------------------------
// Strength: 1% armor penetration per rank, on attack skills only.
#define GW_STRENGTH_PENETRATION_PER_RANK 0.01f

// Expertise: reduces the energy cost of attack skills by 4% per rank.
#define GW_EXPERTISE_REDUCTION_PER_RANK 0.04f

// Divine Favor: a Monk spell cast on an ally heals for an extra
// 3.2 per rank, on top of whatever the spell itself does.
#define GW_DIVINE_FAVOR_PER_RANK 3.2f

// Soul Reaping: gain energy equal to your rank whenever a creature dies
// nearby. This is what lets a Necromancer spend far past a 20 pool.
#define GW_SOUL_REAPING_RADIUS 600.0f

// Fast Casting: casting time is multiplied by 2^(-rank/15), so rank 15
// halves it. Applies to spells, not to signets or attack skills.
#define GW_FAST_CASTING_DIVISOR 15.0f

// Soul Reaping fires at most this many times in a rolling window, GW1's
// own throttle. Without it a Necromancer standing in a wipe refills to
// full off a single fight, which is the exact abuse the cap was added
// to stop.
#define GW_SOUL_REAPING_MAX_TRIGGERS 3
#define GW_SOUL_REAPING_WINDOW 15.0f

// --- The formulas themselves ----------------------------------------
// Stated as functions rather than sprinkled at call sites so there is
// exactly one place to check any of them against the source material.

// Damage multiplier for an effective armor level. 1.0 at AL 60.
float GW_ArmorMultiplier(float effectiveArmor);

// Incoming damage after armor. `armorPenetration` is a 0..1 fraction of
// the target's armor that simply doesn't apply (Strength, penetration
// weapon mods). A hit that connects always does at least 1.
int GW_ArmorScaledDamage(int rawDamage, int armor, float armorPenetration);

// Maximum energy: 20 for everyone, plus 3 per rank of Energy Storage.
int GW_MaxEnergy(int energyStorageRank);

// Seconds to regain a single point of energy at this many pips.
float GW_EnergyRegenInterval(int pips);

// Energy cost after Expertise, which only discounts attack skills.
int GW_SkillEnergyCost(int baseCost, int expertiseRank, bool isAttackSkill);

// Cast time after Fast Casting, which only shortens spells - signets and
// attack skills keep their full activation.
float GW_SkillCastTime(float baseCastTime, int fastCastingRank, bool isSpell);

// Divine Favor's flat bonus healing at a given rank.
int GW_DivineFavorBonus(int rank);

#endif
