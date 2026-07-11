#ifndef UI_PANELS_H
#define UI_PANELS_H

#include "raylib.h"
#include <stdbool.h>

// Inventory panel ('I'), attributes panel ('K'), and the NPC
// dialog/merchant windows, immediate-mode: this call handles toggle
// keys, clicks on rows/buttons, and drawing, all at once. Call during
// the draw phase.
void UI_PanelsUpdateAndDraw(int screenWidth, int screenHeight);

// True if the point (screen space) is over an open panel - input.c uses
// this so clicking a panel doesn't also click-to-move the player.
bool UI_PointerOverPanels(Vector2 point);

// Opens the interaction dialog for an outpost NPC (quest giver,
// merchant, henchman). Closed by Escape or walking away.
void UI_OpenNpcDialog(int entityIndex);

#endif
