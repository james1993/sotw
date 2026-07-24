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

// A transient banner for things the player must not miss: a skill
// learned, an elite captured, a level gained. Earning a skill is the
// core progression beat now, so it gets an announcement rather than
// quietly appearing in a list.
void UI_Notify(const char *message);

// Draws and ages the notification stack. Call once per frame.
void UI_DrawNotifications(int screenWidth, int screenHeight, float dt);

// Clears pending banners (new game / load).
void UI_ClearNotifications(void);

#endif
