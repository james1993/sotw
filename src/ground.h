#ifndef GROUND_H
#define GROUND_H

#include "raylib.h"
#include <stdbool.h>

// The tiled ground surface under every zone.
//
// The world used to be a flat fill plus a lattice of grid lines, which
// read as a level editor rather than terrain. These are seamless
// patterns (Kenney, CC0) stored as alpha masks and TINTED with the
// zone's own ground colour, so one greyscale tile serves the packed
// dirt of a camp and the grass of the plains alike - the zone data
// stays the single source of what a place looks like.

void Ground_Init(void);
void Ground_Unload(void);

// Fills `view` (world-space) with the zone's surface. `outpost` picks
// the laid stone of a settlement over the broken ground of the wild.
// Silently draws nothing if the textures are missing, leaving the flat
// fill that was there before.
void Ground_Draw(Rectangle view, Color tint, bool outpost);

#endif
