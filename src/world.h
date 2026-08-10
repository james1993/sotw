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
// New zones append at the end: the save file stores these values as
// ints, so reordering would send old saves to the wrong outpost.
// Pre-Searing Ascalon (docs/research/pre-searing.md). Two outposts, six
// explorable areas each themed on a profession, and Piken Square - the
// outpost Reforged Mode adds, which you have to fight north to reach.
typedef enum {
    ZONE_ASHFORD_ABBEY = 0,   // outpost: where you start
    ZONE_LAKESIDE_COUNTY,     // Mesmer/Monk country; the gentle first field
    ZONE_ASCALON_CITY,        // outpost: the capital, and the hub
    ZONE_GREEN_HILLS,         // Warrior country, and the theatre
    ZONE_REGENT_VALLEY,       // Ranger country: bandits and spiders
    ZONE_WIZARDS_FOLLY,       // Elementalist country
    ZONE_CATACOMBS,           // Necromancer country: the undead below Ashford
    ZONE_NORTHLANDS,          // the Charr frontier
    ZONE_PIKEN_SQUARE,        // outpost, Reforged Mode only
    // Appended so existing ids (and every save that stores one) stay put.
    ZONE_FOIBLES_FAIR,        // outpost: the fairground off Ashford
    ZONE_BARRADIN_ESTATE,     // explorable: Duke Barradin's devourer-plagued lands
    ZONE_FORT_RANIK,          // outpost, Reforged Mode only: the frontier fort
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

// --- Zone edges ------------------------------------------------------
//
// A zone's walkable area used to be its bounds rectangle, drawn as a red
// line at the edge. That reads as a debug overlay, and it made every
// zone the same shape. Instead the edge is now TERRAIN: a ridge of
// overlapping rock masses generated around the perimeter, each one
// pushed inward by a different amount, so the playable region is an
// irregular blob and what stops you is a mountain you can see.
//
// The bounds rectangle survives as an invisible outer backstop - nothing
// should ever reach it, because the ridge sits inside it.
typedef struct {
    Vector2 pos;
    float radius;   // collision radius, and roughly the drawn footprint
    float height;   // how tall the mass is drawn; varies along the ridge
    unsigned seed;  // per-mass, so the silhouette is stable frame to frame
} ZoneBarrier;

#define MAX_ZONE_BARRIERS 256

int World_GetBarrierCount(void);
const ZoneBarrier *World_GetBarrier(int index);

// Pushes `pos` out of every barrier it overlaps, given the mover's own
// radius. Call after any movement; it is the only thing keeping anyone
// inside the ridge.
void World_ResolveBarriers(Vector2 *pos, float moverRadius);

GameMode World_GetMode(void);
const char *World_GetZoneName(void);

// The zone the party is currently in - lets reach-quest markers draw
// only in the zone their target actually lives in.
ZoneId World_GetZoneId(void);

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

// The zone's playable area (world-space). Movement is clamped to it,
// render.c draws it as a wall line, and the compass/region map show it
// so you can always tell where the instance ends.
Rectangle World_GetBounds(void);

// The last OUTPOST the player was in (the current zone when it's an
// outpost). This is what the save system stores: loading a save "logs
// back in" there, GW1-style - explorable instances are never saved.
ZoneId World_GetLastOutpostId(void);

// Loads straight into an outpost (save-game login). Falls back to the
// starting camp if the id isn't an outpost.
void World_RestoreToOutpost(ZoneId zone);

// --- Map travel (GW1) ------------------------------------------------
// True if the zone is an outpost / has been visited. A character can
// map-travel only to outposts they have already set foot in.
bool World_IsOutpost(ZoneId zone);
bool World_OutpostVisited(ZoneId zone);
const char *World_ZoneName(ZoneId zone);

// Instantly travels to a visited outpost. Only works from within an
// outpost (not mid-explorable) and to a different, unlocked, visited
// outpost. Returns true if the travel happened.
bool World_TravelToOutpost(ZoneId zone);

// The visited-outposts bitmask, for the save system to persist/restore.
unsigned World_VisitedMask(void);
void World_SetVisitedMask(unsigned mask);

// True when this zone is reachable by the current character. Piken
// Square exists only for Reforged characters, so its portal is hidden
// and its label explains itself rather than silently failing.
bool World_ZoneUnlocked(ZoneId zone);

// Whether Little Thom has been hired into the party (persists across
// zone loads; GW1 henchmen stay in the party until dismissed).
// The Searing. Pre-Searing ends when Sir Tydus' Academy trial is taken,
// exactly as GW1 ends it - the Charr burn Ascalon and the campaign this
// prototype covers is over. Once set, the character is finished: the
// roster says so and the world stops accepting them.
bool World_SearingHappened(void);
void World_SetSearingHappened(bool happened);

bool World_IsThomHired(void);
void World_SetThomHired(bool hired);

// Whether the player has a charmed animal companion (Beast Mastery). Set
// by Charm Animal, saved with the character, and read on zone load to
// respawn the pet in each new instance.
bool World_IsPetCharmed(void);
void World_SetPetCharmed(bool charmed);

// The pet's own level and experience (GW1: a pet levels independently, up
// to 20). Persisted with the character and restored on load.
int World_GetPetLevel(void);
int World_GetPetXp(void);
int World_GetPetSpecies(void);       // Species of the charmed pet
void World_SetPetProgress(int level, int xp);
void World_SetPetSpecies(int species);

// Charms a wild animal into the player's pet: records its starting level,
// scales its stats to that level and the given Beast Mastery rank, and
// marks the party as owning a pet.
void World_CharmPet(struct Entity *animal, int beastRank);

// Grants the living pet experience for a kill of the given level, handling
// level-ups (and rescaling the live pet). No-op without a charmed, living
// pet. GW1's pet-XP-from-fighting, condensed.
void World_AwardPetXp(int monsterLevel);

// Turns an entity into the player's pet at a given level, scaled to the
// given Beast Mastery rank - shared by Charm Animal (converting a wild
// Moa) and the zone loader (respawning the companion).
void World_SetupPetStats(struct Entity *pet, int level, int beastRank);

// Raises an undead minion (team 0) at a position, scaled to the given
// Death Magic rank - health, damage and decay rate all ride on it.
// Returns the new entity index, or -1 if the array is full. Minions are
// transient: they decay and die, and don't survive a zone change.
int World_RaiseMinion(Vector2 pos, int deathRank);

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
