#ifndef UI_HINTS_H
#define UI_HINTS_H

// The persistent control legend along the bottom-left: which key or pad
// button opens each panel. Nothing about this game tells you that K is
// attributes or that Select opens the map, and a player shouldn't have
// to read a README to find their own inventory - so the bindings live
// on screen, swapping to pad glyphs the moment a controller is seen.
void UI_DrawControlHints(int screenWidth, int screenHeight);

// The zone name card that fades in on arrival and then fades out, the
// way GW1 announces a new area. Call once per frame while playing.
void UI_DrawZoneTitle(int screenWidth, int screenHeight, float dt);

// Restarts the zone-title animation - called on a zone change.
void UI_ResetZoneTitle(void);

#endif
