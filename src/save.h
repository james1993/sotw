#ifndef SAVE_H
#define SAVE_H

#include <stdbool.h>

// GW1-style persistence: the CHARACTER is saved - level, XP, attribute
// ranks and free points, skill bar, gold, inventory, equipment, quest
// log, hired henchmen - while world instances never are. Loading "logs
// back in" at the last outpost visited, party fully restored, exactly
// like GW1. There is no manual save button, also like GW1: the game
// autosaves on the events that matter (zone transitions, quest changes,
// hiring/dismissing, quitting).
//
// Save location: $XDG_DATA_HOME/sotw-demake/save.txt (Linux, falling
// back to ~/.local/share), %APPDATA%/sotw-demake/save.txt (Windows),
// or next to the executable as a last resort. One slot, plain text,
// versioned.

// True when a save file exists on disk (drives the menu's Continue).
bool Save_Exists(void);

// Applies the save on disk to the freshly-initialized world: overwrites
// the player's progression/inventory/quests, then reloads the saved
// outpost so the party (including a hired Little Thom) respawns
// correctly. Returns false when there is no save or it can't be read.
// Call after World_Init(), before Save_Enable().
bool Save_LoadAndApply(void);

// Turns autosaving on. Until this is called Save_Write is a no-op, so
// the zone load inside World_Init can never clobber an existing save
// with default state.
void Save_Enable(void);

// Autosave. No-op until Save_Enable. Returns true on a successful write.
bool Save_Write(void);

// Where the save lives, for logs/debugging.
const char *Save_GetPath(void);

#endif
