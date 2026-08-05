#ifndef UI_CREATE_H
#define UI_CREATE_H

#include "character.h"

// The character creation screen: profession, appearance, name.
//
// GW1 opens on this, not on a world, because the profession you pick is
// the single most consequential decision in the game and it deserves a
// screen of its own rather than a line in a config file. It shows a live
// preview of the character you're building - the same procedural sprite
// the world draws - so the choices are visible rather than abstract.
//
// Secondary profession is deliberately absent: GW1 makes you earn that
// in-game once you've actually played the primary, and so does this.
typedef enum {
    CREATE_NONE = 0,
    CREATE_CONFIRM,  // g_character now holds the finished character
    CREATE_CANCEL    // back to the main menu
} CreateAction;

// Call once when entering the screen to reset it to defaults.
void UI_CreateReset(void);

// Immediate-mode: draws and returns what the player did this frame.
CreateAction UI_DrawCreateScreen(int screenWidth, int screenHeight, float dt);

#endif
