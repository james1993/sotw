#ifndef INPUT_H
#define INPUT_H

#include "raylib.h"

// Shared zoom bounds - also used by main.c to clamp the window-size-based
// default zoom, so the initial view and scroll-wheel zoom agree on range.
#define MIN_CAMERA_ZOOM 0.5f
#define MAX_CAMERA_ZOOM 6.0f

// Click-to-move + click-to-target + 1-8 skill bar hotkeys for the
// player entity (index 0), plus scroll-wheel camera zoom. Takes the
// camera by pointer since zoom needs to persist back to the caller.
// See docs/design/demake-design.md #1.
void Input_Update(Camera2D *camera);

#endif
