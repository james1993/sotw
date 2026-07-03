#ifndef ATTRIBUTES_H
#define ATTRIBUTES_H

// A small vertical slice of GW1's attribute list: enough to exercise the
// primary/secondary profession rule (Strength/Tactics = Warrior-like,
// Fire Magic/Energy Storage = Elementalist-like) without needing all ten
// professions' worth of data. See docs/design/demake-design.md #4.
typedef enum {
    ATTR_STRENGTH = 0,
    ATTR_TACTICS,
    ATTR_FIRE_MAGIC,
    ATTR_ENERGY_STORAGE,
    ATTR_COUNT
} AttributeKind;

extern const char *g_attributeNames[ATTR_COUNT];

#define ATTRIBUTE_POINT_CAP 200
#define ATTRIBUTE_RANK_CAP 12

#endif
