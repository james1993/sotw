#ifndef UI_EFFECTS_H
#define UI_EFFECTS_H

// GW1's effects monitor: the row of icons in the top-left that shows
// what is currently on your character - a morale boost or death penalty,
// then every enchantment, hex and condition. Conditions run red, hexes
// purple and enchantments gold, the same language GW1 uses, so a glance
// says which kind of thing a removal skill would have to touch.
//
// Drawn before the quest tracker so the tracker can tuck in beneath it;
// UI_EffectsMonitorBottom reports how far down the row reached (0 when
// nothing is showing) so the two never overlap.
void UI_DrawEffectsMonitor(int screenWidth, int screenHeight);
float UI_EffectsMonitorBottom(void);

#endif
