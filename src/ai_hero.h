#ifndef AI_HERO_H
#define AI_HERO_H

#include "raylib.h"
#include <stdbool.h>

// Drives both hero and monster entities through the same
// Combat_ActivateSkill() path the player's input uses - see
// docs/design/raylib-architecture.md #6.
void AI_Update(float dt);

// Party flag (GW1): a ground point party members move to and hold instead
// of following the player. Set/cleared from the compass, drawn there and
// in the world, and cleared on zone change.
void AI_SetPartyFlag(Vector2 pos);
void AI_ClearPartyFlag(void);
bool AI_PartyFlagActive(Vector2 *out);

#endif
