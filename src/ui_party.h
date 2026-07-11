#ifndef UI_PARTY_H
#define UI_PARTY_H

#include "raylib.h"
#include <stdbool.h>

// GW1-style party window: a compact panel on the right edge listing
// every party member (player + heroes/henchmen) with a health bar,
// their current death penalty, GW1's condition/hex status arrows, and -
// in outposts only, GW1's rule for party editing - a dismiss button on
// hired henchmen.
void UI_DrawPartyPanel(int screenWidth, int screenHeight);

// True if the point is inside the party panel - input.c uses this so
// clicks on the panel (e.g. the dismiss button) don't fall through to
// click-to-move.
bool UI_PartyPanelContains(Vector2 point);

#endif
