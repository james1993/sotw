#ifndef WORLD_H
#define WORLD_H

#include "raylib.h"
#include <stdbool.h>

struct Entity;

// GW1's fundamental world split: outposts are safe social hubs
// (merchants, henchmen, quest givers, no combat), explorable areas are
// per-party combat instances loaded fresh on every entry. See
// docs/design/raylib-architecture.md #7.
typedef enum {
    MODE_OUTPOST,
    MODE_EXPLORABLE
} GameMode;

GameMode World_GetMode(void);
const char *World_GetZoneName(void);

// Creates the player + loads the starting outpost. Call once at startup.
void World_Init(void);

// Portal proximity check + zone transitions. Call every frame.
void World_Update(struct Entity *player, float dt);

// Portal location + destination label for rendering.
void World_GetPortal(Vector2 *pos, const char **label);

// Whether Little Thom has been hired into the party (persists across
// zone loads; GW1 henchmen stay in the party until dismissed).
bool World_IsThomHired(void);
void World_SetThomHired(bool hired);

// Dismiss a hired henchman entity: outposts only, GW1's rule for party
// editing. Converts the party member back into the standing NPC.
void World_DismissHenchman(struct Entity *henchman);

// Resurrection shrine (explorable zones only). On a full party wipe the
// party respawns here, each member carrying their stacked death penalty
// - straight from GW1.
bool World_GetShrine(Vector2 *pos);

#endif
