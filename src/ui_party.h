#ifndef UI_PARTY_H
#define UI_PARTY_H

// GW1-style party window: a compact panel on the right edge listing
// every party member (team 0) with a health bar, plus the same
// at-a-glance status arrows GW1 uses - a brown arrow when the member
// has a condition on them, a purple arrow when they're hexed.
void UI_DrawPartyPanel(int screenWidth, int screenHeight);

#endif
