#ifndef BUILDS_H
#define BUILDS_H

#include <stdbool.h>
#include "attributes.h"
#include "entity.h"

// GW1's skill templates: a saved bar plus the attribute spread that
// makes it work. Saving one without the other is close to useless - a
// bar of Healing Prayers skills on a Fire Magic spread is not the build
// you saved - which is why the two live in one record here, and why the
// panel that edits them is one screen rather than two.
#define BUILD_SLOT_COUNT 4
#define BUILD_NAME_LEN 32

typedef struct {
    bool used;
    char name[BUILD_NAME_LEN];
    int skillBar[SKILL_BAR_SIZE];
    unsigned char attributeRank[ATTR_COUNT];
} BuildTemplate;

BuildTemplate *Builds_Slot(int slot);

// Captures the player's current bar and ranks. Names the slot from the
// professions and the deepest attribute if it has no name yet, so a
// saved build is identifiable without anyone having to type.
void Builds_SaveFrom(int slot, const Entity *player);

// Applies a saved template. Skills the character hasn't learned (or
// can't use with their current professions) are dropped rather than
// silently equipped, and the attribute spread is fitted to whatever
// point budget the character actually has - a template saved at level
// 20 loaded at level 12 comes back scaled down, not free.
bool Builds_LoadInto(int slot, Entity *player);

void Builds_Clear(int slot);
void Builds_Reset(void);

#endif
