#include "gwmath.h"
#include <math.h>

float GW_ArmorMultiplier(float effectiveArmor) {
    return powf(2.0f, (GW_ARMOR_BASELINE - effectiveArmor) / GW_ARMOR_HALVING);
}

int GW_ArmorScaledDamage(int rawDamage, int armor, float armorPenetration) {
    if (rawDamage <= 0) return 0;
    if (armorPenetration < 0.0f) armorPenetration = 0.0f;
    if (armorPenetration > 1.0f) armorPenetration = 1.0f;
    // Penetration removes a fraction of the armor before the curve is
    // applied, which is why 20% penetration is worth far more against a
    // Warrior than against a caster - it scales with what's there.
    float effective = (float)armor * (1.0f - armorPenetration);
    int dmg = (int)((float)rawDamage * GW_ArmorMultiplier(effective));
    return (dmg < 1) ? 1 : dmg; // a connecting hit is never a no-op
}

int GW_MaxEnergy(int energyStorageRank) {
    if (energyStorageRank < 0) energyStorageRank = 0;
    return GW_BASE_ENERGY + GW_ENERGY_STORAGE_PER_RANK * energyStorageRank;
}

float GW_EnergyRegenInterval(int pips) {
    if (pips < 1) pips = 1; // no negative-regen sources yet
    return GW_SECONDS_PER_PIP / (float)pips;
}

int GW_SkillEnergyCost(int baseCost, int expertiseRank, bool isAttackSkill) {
    if (baseCost <= 0 || !isAttackSkill || expertiseRank <= 0) return baseCost;
    float factor = 1.0f - GW_EXPERTISE_REDUCTION_PER_RANK * (float)expertiseRank;
    if (factor < 0.0f) factor = 0.0f;
    int cost = (int)((float)baseCost * factor + 0.5f);
    return (cost < 0) ? 0 : cost;
}

float GW_SkillCastTime(float baseCastTime, int fastCastingRank, bool isSpell) {
    if (baseCastTime <= 0.0f || !isSpell || fastCastingRank <= 0) return baseCastTime;
    return baseCastTime * powf(2.0f, -(float)fastCastingRank / GW_FAST_CASTING_DIVISOR);
}

int GW_DivineFavorBonus(int rank) {
    if (rank <= 0) return 0;
    return (int)(GW_DIVINE_FAVOR_PER_RANK * (float)rank + 0.5f);
}
