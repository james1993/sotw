#ifndef UI_TARGET_H
#define UI_TARGET_H

// Draws a target info panel (name, HP bar, cast bar, and their equipped
// skill bar) when the player currently has a target selected. This is
// what makes cast time/interrupts actually playable - GW1 always shows
// your target's health and skill bar for the same reason, otherwise you
// can't see a cast coming in time to punish it.
void UI_DrawTargetPanel(int screenWidth, int screenHeight, int startY);

#endif
