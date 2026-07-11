#ifndef UI_PANELS_H
#define UI_PANELS_H

#include "raylib.h"
#include <stdbool.h>

// Inventory panel ('I') and attributes panel ('K'), immediate-mode:
// this call handles the toggle keys, any clicks on rows/buttons, and
// drawing, all at once. Call during the draw phase.
void UI_PanelsUpdateAndDraw(int screenWidth, int screenHeight);

// True if the point (screen space) is over an open panel - input.c uses
// this so clicking a panel doesn't also click-to-move the player.
bool UI_PointerOverPanels(Vector2 point);

#endif
