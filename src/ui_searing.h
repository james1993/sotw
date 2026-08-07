#ifndef UI_SEARING_H
#define UI_SEARING_H

#include <stdbool.h>

// The Searing: the end of pre-Searing, and the end of this prototype.
//
// In GW1 you take Sir Tydus' trial at the Ascalon Academy, and the
// Charr crystals fall while you're inside. That is the point of no
// return - everything this demake covers happens before it - so rather
// than pretend there's more, the prototype stops here and says so.
//
// A character who has been through it stays finished: the roster marks
// them, and choosing them replays this rather than dropping them back
// into a county that no longer exists.

void UI_SearingReset(void);

// Immediate-mode. Returns true once the player has asked to leave.
bool UI_DrawSearing(int screenWidth, int screenHeight, float dt);

#endif
