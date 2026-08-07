#ifndef UI_SELECT_H
#define UI_SELECT_H

// GW1's character selection: the roster you meet after logging in,
// before any world exists. Six slots, each a whole character rather
// than a snapshot of one - which is the point. A save slot you
// overwrite is a rewind; a character slot is a person you made, and
// keeping six of them is what lets you have a Monk and a Warrior
// without one costing you the other.

typedef enum {
    SELECT_NONE = 0,
    SELECT_PLAY,    // an existing character was chosen; Save_SelectedSlot() has it
    SELECT_CREATE,  // an empty slot was chosen; the creator writes into it
    SELECT_BACK     // back to the title screen
} SelectAction;

// Call when entering the screen: re-reads the roster from disk and
// clears any pending delete confirmation.
void UI_SelectReset(void);

// Immediate-mode. Draws the roster and returns what the player did.
SelectAction UI_DrawSelectScreen(int screenWidth, int screenHeight, float dt);

#endif
