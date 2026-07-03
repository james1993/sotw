#ifndef RENDER_H
#define RENDER_H

#include "raylib.h"

// Draws the ground grid and every entity (health bar, cast bar, name)
// inside the given 2D camera. UI (skill bar, resource bars) is drawn
// separately in screen space - see ui_skillbar.h.
void Render_World(Camera2D camera);

#endif
