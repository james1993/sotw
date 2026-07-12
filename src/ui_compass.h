#ifndef UI_COMPASS_H
#define UI_COMPASS_H

// GW1-style compass: a round minimap in the top-right corner, centered
// on the player, showing nearby allies/enemies/NPCs as dots, the portal
// and shrine, the reach-quest marker, and the player's aggro bubble as
// a ring - the compass being where GW1 actually displays the bubble.
// Watching patrol dots sweep across it is how you time a safe pull.
void UI_DrawCompass(int screenWidth, int screenHeight);

// Bottom edge (screen y) of the compass, so right-side UI (party panel
// in outposts, quest tracker) can stack below it.
float UI_CompassBottom(int screenHeight);

#endif
