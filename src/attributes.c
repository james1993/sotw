#include "attributes.h"

const char *g_attributeNames[ATTR_COUNT] = {
    "Strength",
    "Tactics",
    "Swordsmanship",
    "Expertise",
    "Marksmanship",
    "Wilderness Survival",
    "Beast Mastery",
    "Divine Favor",
    "Healing Prayers",
    "Smiting Prayers",
    "Protection Prayers",
    "Soul Reaping",
    "Blood Magic",
    "Death Magic",
    "Curses",
    "Fast Casting",
    "Domination Magic",
    "Illusion Magic",
    "Inspiration Magic",
    "Energy Storage",
    "Fire Magic",
    "Water Magic",
    "Air Magic",
    "Earth Magic",
    "Monstrous",
};

const Profession g_attributeProfession[ATTR_COUNT] = {
    PROF_WARRIOR,      // Strength
    PROF_WARRIOR,      // Tactics
    PROF_WARRIOR,      // Swordsmanship
    PROF_RANGER,       // Expertise
    PROF_RANGER,       // Marksmanship
    PROF_RANGER,       // Wilderness Survival
    PROF_RANGER,       // Beast Mastery
    PROF_MONK,         // Divine Favor
    PROF_MONK,         // Healing Prayers
    PROF_MONK,         // Smiting Prayers
    PROF_MONK,         // Protection Prayers
    PROF_NECROMANCER,  // Soul Reaping
    PROF_NECROMANCER,  // Blood Magic
    PROF_NECROMANCER,  // Death Magic
    PROF_NECROMANCER,  // Curses
    PROF_MESMER,       // Fast Casting
    PROF_MESMER,       // Domination Magic
    PROF_MESMER,       // Illusion Magic
    PROF_MESMER,       // Inspiration Magic
    PROF_ELEMENTALIST, // Energy Storage
    PROF_ELEMENTALIST, // Fire Magic
    PROF_ELEMENTALIST, // Water Magic
    PROF_ELEMENTALIST, // Air Magic
    PROF_ELEMENTALIST, // Earth Magic
    PROF_COUNT,        // Monstrous - owned by no profession, on purpose
};

const bool g_attributeIsPrimary[ATTR_COUNT] = {
    true,  // Strength
    false, // Tactics
    false, // Swordsmanship
    true,  // Expertise
    false, // Marksmanship
    false, // Wilderness Survival
    false, // Beast Mastery
    true,  // Divine Favor
    false, // Healing Prayers
    false, // Smiting Prayers
    false, // Protection Prayers
    true,  // Soul Reaping
    false, // Blood Magic
    false, // Death Magic
    false, // Curses
    true,  // Fast Casting
    false, // Domination Magic
    false, // Illusion Magic
    false, // Inspiration Magic
    true,  // Energy Storage
    false, // Fire Magic
    false, // Water Magic
    false, // Air Magic
    false, // Earth Magic
    false, // Monstrous
};

const int g_attrCumulativeCost[13] = {
    0, 1, 3, 6, 10, 15, 21, 28, 37, 48, 61, 77, 97
};

bool Attribute_Accessible(AttributeKind attr, Profession primary, Profession secondary) {
    Profession owner = g_attributeProfession[attr];
    if (owner == primary) return true;
    if (owner == secondary && !g_attributeIsPrimary[attr]) return true;
    return false;
}

AttributeKind Attribute_PrimaryFor(Profession profession) {
    for (int a = 0; a < ATTR_COUNT; a++) {
        if (g_attributeProfession[a] == profession && g_attributeIsPrimary[a]) {
            return (AttributeKind)a;
        }
    }
    return ATTR_STRENGTH; // unreachable: every profession has a primary
}
