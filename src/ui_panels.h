#ifndef UI_PANELS_H
#define UI_PANELS_H

#include "raylib.h"
#include <stdbool.h>

// Inventory panel ('I'), attributes panel ('K'), and the NPC
// dialog/merchant windows, immediate-mode: this call handles toggle
// keys, clicks on rows/buttons, and drawing, all at once. Call during
// the draw phase.
void UI_PanelsUpdateAndDraw(int screenWidth, int screenHeight);

// Opens the interaction dialog for an outpost NPC (quest giver,
// merchant, henchman). Closed by Escape or walking away.
void UI_OpenNpcDialog(int entityIndex);

// True while an NPC dialog is on screen. input.c uses this so the
// gamepad talk button (X/Square) opens conversations when none is up
// but advances the current one instead of re-opening it.
bool UI_IsNpcDialogOpen(void);

// Closes the dialog (and the shop under it). Escape and the gamepad's
// B both back out of a conversation through this.
void UI_CloseNpcDialog(void);

#endif
