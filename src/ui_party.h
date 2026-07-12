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

// Bottom edge (screen y) of the last-drawn party panel, so other
// right-side UI (the quest tracker) can stack below it in outposts.
// 0 when no panel was drawn.
float UI_PartyPanelBottom(void);

#endif
