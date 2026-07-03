#ifndef UI_SKILLBAR_H
#define UI_SKILLBAR_H

// Screen-space UI for the player's 8-slot skill bar and resource bars
// (HP/Energy/Adrenaline). All sizes scale with screenHeight relative to
// an 800px reference, so the UI stays legible on both small and
// high-res windows. See docs/design/raylib-architecture.md #5.
void UI_DrawSkillBar(int screenWidth, int screenHeight);
void UI_DrawResourceBars(int screenWidth, int screenHeight);

// Scales a font size the same way the skill bar/resource bars scale,
// for use by any other screen-space text (e.g. main.c's help text).
int UI_ScaledFontSize(int screenHeight, int baseSize);

#endif
