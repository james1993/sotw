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

// Zone indices into the zone table in world.c. Adding a zone means
// adding an entry there (name, portals, spawns, props, colors) - no
// code changes elsewhere.
typedef enum {
    ZONE_ASHFORD_CAMP = 0,
    ZONE_ASHFORD_PLAINS,
    ZONE_COUNT
} ZoneId;

#define MAX_ZONE_PORTALS 4

// A gate to another zone. Zones can have several (hub outposts with
// multiple exits), each with its own destination and arrival point.
typedef struct {
    Vector2 pos;
    const char *label;   // "To Ashford Plains" - drawn under the swirl
    ZoneId destZone;
    Vector2 destEntry;   // where the player appears on the other side
} ZonePortal;

// Decorative environment props, hand-placed per zone. Drawn by render.c;
// purely visual, no collision or LoS.
typedef enum { PROP_TREE, PROP_ROCK, PROP_GRASS, PROP_TENT, PROP_FIRE } PropType;
typedef struct { Vector2 pos; PropType type; float scale; } EnvProp;

GameMode World_GetMode(void);
const char *World_GetZoneName(void);

// Zone look: window clear color and ground-grid color (warm dirt in
// camp, cool grass in the plains).
Color World_GetClearColor(void);
Color World_GetGridColor(void);

// Creates the player + loads the starting outpost. Call once at startup.
void World_Init(void);

// Portal proximity checks + zone transitions. Call every frame.
void World_Update(struct Entity *player, float dt);

// The current zone's portals, for render.c (world gates) and
// ui_compass.c (blue squares).
int World_GetPortalCount(void);
const ZonePortal *World_GetPortal(int index);

// The current zone's decorative props, for render.c.
const EnvProp *World_GetProps(int *count);

// The last OUTPOST the player was in (the current zone when it's an
// outpost). This is what the save system stores: loading a save "logs
// back in" there, GW1-style - explorable instances are never saved.
ZoneId World_GetLastOutpostId(void);

// Loads straight into an outpost (save-game login). Falls back to the
// starting camp if the id isn't an outpost.
void World_RestoreToOutpost(ZoneId zone);

// Whether Little Thom has been hired into the party (persists across
// zone loads; GW1 henchmen stay in the party until dismissed).
bool World_IsThomHired(void);
void World_SetThomHired(bool hired);

// Dismiss a hired henchman entity: outposts only, GW1's rule for party
// editing. Converts the party member back into the standing NPC.
void World_DismissHenchman(struct Entity *henchman);

// Fills in Little Thom's Warrior stat block and skill bar - the single
// source of truth shared by zone loads and the hire dialog.
void World_SetupThomStats(struct Entity *thom);

// Resurrection shrine (explorable zones only). On a full party wipe the
// party respawns here, each member carrying their stacked death penalty
// - straight from GW1.
bool World_GetShrine(Vector2 *pos);

#endif
