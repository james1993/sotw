#ifndef CHARACTER_H
#define CHARACTER_H

#include "attributes.h"
#include "raylib.h"
#include <stdbool.h>

// Everything chosen at character creation, in one place.
//
// GW1 asks for a primary profession and a look at creation and then
// makes you earn your SECOND profession in-game from an NPC - the
// choice that defines a build is deliberately not front-loaded onto a
// player who has never fought anything yet. This mirrors that: the
// creator sets primary + appearance + name, and secondaryProfession
// stays PROF_NONE until a profession trainer grants it.

#define CHARACTER_NAME_MAX 20

// Sentinel for "no secondary yet". Kept outside the Profession enum so
// every existing profession loop stays exactly as it was.
#define PROF_NONE (-1)

typedef struct {
    char name[CHARACTER_NAME_MAX + 1];
    Profession primary;
    int secondary;      // Profession, or PROF_NONE until earned in-game

    // Appearance. The sprite renderer is procedural, so these are read
    // straight into the shapes it draws rather than picking a texture.
    int sex;            // 0 = female, 1 = male; cosmetic only
    int skinTone;       // index into the palettes below
    int hairColor;
    int hairStyle;
} CharacterDef;

#define SKIN_TONE_COUNT 5
#define HAIR_COLOR_COUNT 6
#define HAIR_STYLE_COUNT 3

extern const Color g_skinTones[SKIN_TONE_COUNT];
extern const Color g_hairColors[HAIR_COLOR_COUNT];

// Display helpers shared by the creator, the HUD and the save file.
const char *Character_ProfessionName(int profession);      // "Warrior"
const char *Character_ProfessionAbbrev(int profession);    // "W"
const char *Character_ProfessionBlurb(int profession);     // one-line pitch

// "Ilsa (Mo/E)", or "Ilsa (Mo)" before a secondary is earned - the
// nameplate format GW1 uses everywhere.
void Character_FormatTitle(const CharacterDef *def, char *out, int outSize);

// A sane default so nothing is uninitialised if creation is skipped.
CharacterDef Character_Default(void);

// The character the current run is playing. World_Init reads this.
extern CharacterDef g_character;

#endif
