#ifndef AREA_H
#define AREA_H

#include "raylib.h"
#include <stdbool.h>

struct Entity;

// GW1's positional area auras - the third way (after enchantments and
// conditions) a skill changes a fight, by claiming a patch of ground
// rather than touching a single target. Wards (Elementalist Earth) buff
// allies standing in them; wells (Necromancer, raised on a corpse) heal
// allies or rot foes; nature-ritual spirits (Ranger) blanket everything
// in range, friend and foe alike. Modelled by MECHANIC, like the effect
// system: one AreaKind per behaviour, shared by every skill that makes it.
typedef enum {
    AREA_WARD_HARM,        // allies inside gain armor (Ward Against Harm)
    AREA_WELL_BLOOD,       // allies inside gain health regen (on a corpse)
    AREA_WELL_SUFFERING,   // foes inside suffer health degen (on a corpse)
    AREA_SPIRIT_WINNOWING  // every creature inside takes extra attack damage
} AreaKind;

// Clears every area - called on zone load (auras don't survive rezoning,
// like projectiles and minions).
void Area_Reset(void);

// Creates an area at a position. Recasting the same kind by the same owner
// replaces the old one (GW1: a second ward/well/spirit of the same name
// supersedes the first). magnitude carries the effect strength - armor,
// regen/degen pips, or bonus damage - by kind.
void Area_Spawn(AreaKind kind, Vector2 pos, float radius, float duration,
                float magnitude, int ownerTeam);

// Expires areas and applies the wells' periodic regen (allies) and degen
// (foes) to whoever stands in them.
void Area_Update(float dt);

// Ground runes for wards/wells and a totem for a spirit, under the living.
void Area_Draw(void);

// Bonus armor for an entity standing in a friendly Ward Against Harm.
int Area_BonusArmor(const struct Entity *e);

// Extra per-attack damage an entity takes from any Winnowing spirit it is
// standing in - GW1's ritual hits friend and foe the same, so no team test.
int Area_BonusAttackDamage(const struct Entity *e);

#endif
