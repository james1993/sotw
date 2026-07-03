#ifndef INPUT_H
#define INPUT_H

#include "raylib.h"

// Click-to-move + click-to-target + 1-8 skill bar hotkeys for the
// player entity (index 0). See docs/design/demake-design.md #1.
void Input_Update(Camera2D camera);

#endif
