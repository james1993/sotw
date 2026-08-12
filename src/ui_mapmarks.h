#ifndef UI_MAPMARKS_H
#define UI_MAPMARKS_H

#include "raylib.h"
#include "world.h" // PropType

// Shared cartography + blip language for the compass and the full map, so
// a house, a fountain, or a foe reads the same on both.

// Draws a terrain prop as a small map marker at screen position p. `unit`
// is the base marker size in pixels (the map uses a larger unit than the
// compass); `alpha` (0..255) fades the whole marker so the compass can
// keep terrain faint under the blips.
void UIMap_DrawProp(Vector2 p, PropType type, float unit, unsigned char alpha);

// Draws an entity blip. kind: 0 ally, 1 NPC, 2 monster. Monsters are a
// red diamond - a different SHAPE, not just a colour - so they separate
// from the round friendly dots at a glance; aggroed ones brighten and get
// a ring. `targeted` rings the blip gold.
void UIMap_DrawBlip(Vector2 p, int kind, bool aggroed, bool targeted, float unit);

#endif
