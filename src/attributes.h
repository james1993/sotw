#ifndef ATTRIBUTES_H
#define ATTRIBUTES_H

#include <stdbool.h>

// The six professions of Guild Wars: Prophecies. Assassin and Ritualist
// are Factions; Paragon and Dervish are Nightfall - neither campaign is
// in scope, so neither is here.
typedef enum {
    PROF_WARRIOR = 0,
    PROF_RANGER,
    PROF_MONK,
    PROF_NECROMANCER,
    PROF_MESMER,
    PROF_ELEMENTALIST,
    PROF_COUNT
} Profession;

// GW1's attribute list for those six professions. Each attribute belongs
// to one profession; the PRIMARY attribute of each is usable only by
// characters with that profession as their PRIMARY - see
// docs/research/gw1-mechanics.md #4. The primary attributes are the ones
// carrying a unique mechanical effect, which is what makes the primary
// choice permanent and consequential rather than cosmetic.
typedef enum {
    // Warrior
    ATTR_STRENGTH = 0,       // primary: armor penetration on attack skills
    ATTR_TACTICS,
    ATTR_SWORDSMANSHIP,
    // Ranger
    ATTR_EXPERTISE,          // primary: cheaper attack skills
    ATTR_MARKSMANSHIP,
    ATTR_WILDERNESS_SURVIVAL,
    ATTR_BEAST_MASTERY,
    // Monk
    ATTR_DIVINE_FAVOR,       // primary: bonus healing on Monk spells
    ATTR_HEALING_PRAYERS,
    ATTR_SMITING_PRAYERS,
    ATTR_PROTECTION_PRAYERS,
    // Necromancer
    ATTR_SOUL_REAPING,       // primary: energy back when something dies
    ATTR_BLOOD_MAGIC,
    ATTR_DEATH_MAGIC,
    ATTR_CURSES,
    // Mesmer
    ATTR_FAST_CASTING,       // primary: shorter cast times
    ATTR_DOMINATION_MAGIC,
    ATTR_ILLUSION_MAGIC,
    ATTR_INSPIRATION_MAGIC,
    // Elementalist
    ATTR_ENERGY_STORAGE,     // primary: +3 max energy per rank
    ATTR_FIRE_MAGIC,
    ATTR_WATER_MAGIC,
    ATTR_AIR_MAGIC,
    ATTR_EARTH_MAGIC,
    // Not a GW1 attribute. Monster skills live here so they belong to no
    // profession, which is what keeps Claw Swipe and friends out of
    // every trainer list and attribute panel automatically - the check
    // is ownership, so there is nothing to remember to filter.
    ATTR_MONSTROUS,
    ATTR_COUNT
} AttributeKind;

extern const char *g_attributeNames[ATTR_COUNT];
extern const Profession g_attributeProfession[ATTR_COUNT];
extern const bool g_attributeIsPrimary[ATTR_COUNT];

// GW1's actual cumulative attribute-point cost to reach each rank 0-12:
// rank 1 costs 1 point total, rank 12 costs 97. This steep curve is why
// spreading points thin is cheap but maxing a line is a real commitment.
extern const int g_attrCumulativeCost[13];

#define ATTRIBUTE_RANK_CAP 12

// True if a character with the given primary/secondary professions can
// put points into the attribute at all.
bool Attribute_Accessible(AttributeKind attr, Profession primary, Profession secondary);

// The primary attribute of a profession - the one only its primaries may
// spend points in, and the one carrying that profession's mechanic.
AttributeKind Attribute_PrimaryFor(Profession profession);

#endif
