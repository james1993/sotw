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
// Save location: $XDG_DATA_HOME/sotw-demake/charN.txt (Linux, falling
// back to ~/.local/share), %APPDATA%/sotw-demake/charN.txt (Windows),
// or next to the executable as a last resort. Plain text, versioned.
//
// SIX SLOTS, like GW1's character select. A slot is a whole character,
// not a snapshot of one - which is why the selection screen can show
// you a roster and why deleting one is deleting a person rather than
// rewinding.

#define SAVE_SLOT_COUNT 6

// What the selection screen needs to draw a slot without loading the
// whole game: enough to recognise a character, and nothing else.
typedef struct {
    bool used;
    char name[32];
    int primary;      // Profession
    int secondary;    // Profession; equal to primary means "none yet"
    int level;
    bool reforged;
    bool searingSurvived; // finished the campaign - the run is over

    // Enough of the character's look to draw them in the roster exactly
    // as they appear in the world: appearance, armour tier, dye, weapon.
    int sex, skinTone, hairColor, hairStyle;
    int armor;          // equipped armour rating -> robe tier
    bool dyed;
    int dyePacked;      // 0xRRGGBB, meaningful when dyed
    int weaponVisual;   // WeaponVisual of the equipped weapon
} SaveSlotInfo;

// Which slot everything else reads and writes. Set before World_Init /
// Save_LoadAndApply / Save_Enable; -1 means "no slot", and saving is a
// no-op until one is chosen.
void Save_SelectSlot(int slot);
int Save_SelectedSlot(void);

// Reads just the header fields of a slot's file. Cheap enough to call
// every frame while the selection screen is up.
bool Save_ReadSlotInfo(int slot, SaveSlotInfo *out);

// Deletes a slot's file. There is no undo, which is why the screen asks.
bool Save_DeleteSlot(int slot);

// True when the SELECTED slot has a file on disk.
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

// Turns it back off. Essential now that a session can start more than
// one game: without this, the SECOND character you pick inherits the
// first one's enabled flag, and World_Init's zone load autosaves
// default state over their file before it has been read.
void Save_Disable(void);

// Autosave. No-op until Save_Enable. Returns true on a successful write.
bool Save_Write(void);

// Where the save lives, for logs/debugging.
const char *Save_GetPath(void);

#endif
