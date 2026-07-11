#include "attributes.h"

const char *g_attributeNames[ATTR_COUNT] = {
    "Strength",
    "Tactics",
    "Fire Magic",
    "Energy Storage",
    "Healing Prayers",
    "Smiting Prayers",
    "Divine Favor",
};

const Profession g_attributeProfession[ATTR_COUNT] = {
    PROF_WARRIOR,      // Strength
    PROF_WARRIOR,      // Tactics
    PROF_ELEMENTALIST, // Fire Magic
    PROF_ELEMENTALIST, // Energy Storage
    PROF_MONK,         // Healing Prayers
    PROF_MONK,         // Smiting Prayers
    PROF_MONK,         // Divine Favor
};

const bool g_attributeIsPrimary[ATTR_COUNT] = {
    true,  // Strength
    false, // Tactics
    false, // Fire Magic
    true,  // Energy Storage
    false, // Healing Prayers
    false, // Smiting Prayers
    true,  // Divine Favor
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
