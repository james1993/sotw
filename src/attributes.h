#ifndef ATTRIBUTES_H
#define ATTRIBUTES_H

#include <stdbool.h>

typedef enum {
    PROF_WARRIOR = 0,
    PROF_ELEMENTALIST,
    PROF_MONK,
    PROF_COUNT
} Profession;

// A vertical slice of GW1's attribute list, enough to exercise the
// primary/secondary profession rule across three professions. Each
// attribute belongs to one profession; primary attributes are only
// usable by characters with that profession as their PRIMARY - see
// docs/research/gw1-mechanics.md #4.
typedef enum {
    ATTR_STRENGTH = 0,     // Warrior primary
    ATTR_TACTICS,          // Warrior
    ATTR_FIRE_MAGIC,       // Elementalist
    ATTR_ENERGY_STORAGE,   // Elementalist primary
    ATTR_HEALING_PRAYERS,  // Monk
    ATTR_SMITING_PRAYERS,  // Monk
    ATTR_DIVINE_FAVOR,     // Monk primary
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

#endif
