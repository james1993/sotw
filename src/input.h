#ifndef INPUT_H
#define INPUT_H

#include "raylib.h"

// Shared zoom bounds - also used by main.c to clamp the window-size-based
// default zoom, so the initial view and scroll-wheel zoom agree on range.
#define MIN_CAMERA_ZOOM 0.5f
#define MAX_CAMERA_ZOOM 6.0f

// Click-to-move + click-to-target + 1-8 skill bar hotkeys for the
// player entity (index 0), plus scroll-wheel camera zoom and gamepad
// support (left stick moves, nearest foe in attack range is
// auto-targeted for auto-attack, L2 + A/B/X/Y = skills 1-4 and
// R2 + A/B/X/Y = skills 5-8). Takes the camera by pointer since zoom
// needs to persist back to the caller; dt drives stick movement.
// See docs/design/demake-design.md #1.
void Input_Update(Camera2D *camera, float dt);

// True once the player has touched the scroll wheel. Until then main.c
// keeps re-deriving the default zoom from the window size, so a window
// manager that maximizes the window a few frames after startup still
// ends up with the right default view.
bool Input_UserAdjustedZoom(void);

// The NPC the interact button would talk to right now (nearest one in
// range), or -1. The world overlay highlights exactly this entity and
// draws its "Talk" prompt, so the thing you see lit up is always the
// thing a press acts on.
int Input_InteractNpcIndex(void);

// The entity under the mouse pointer this frame, or -1 - drives hover
// highlighting in the world overlay.
int Input_HoverEntityIndex(void);

#endif
