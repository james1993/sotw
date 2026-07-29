#ifndef UI_MAP_H
#define UI_MAP_H

#include <stdbool.h>

// Fullscreen region map, GW1's M map reduced to its essence: the whole
// zone at once - boundary, portals, shrine, quest markers, terrain
// props, and every entity in the compass color language - so you can
// orient yourself when the compass's local slice isn't enough.
// Toggled by M or the gamepad's Select/Back button; Escape/B also
// close it. Handles its own toggle input; call every frame after the
// rest of the UI so it draws on top.
void UI_MapUpdateAndDraw(int screenWidth, int screenHeight);

// True while the map overlay is up (input.c: B closes it first).
bool UI_IsMapOpen(void);
void UI_CloseMapOverlay(void);

// Opens the map directly - the pause hub's route to it, so the map is
// reachable without knowing the M / Select binding.
void UI_OpenMapOverlay(void);

#endif
