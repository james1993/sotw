#ifndef UI_ICONS_H
#define UI_ICONS_H

#include "raylib.h"
#include <stdbool.h>

// The skill-icon atlas (assets/icons/skills.png).
//
// Icons are stored as white silhouettes on transparency and TINTED when
// drawn, so one greyscale sheet serves every school colour instead of
// shipping the same picture in eight palettes. The atlas is indexed by
// SkillId - cell n is skill n - which is checked at build time by
// tools/build_icon_atlas.py against the enum in skill.h.

void UIIcons_Init(void);
void UIIcons_Unload(void);

// False when the sheet couldn't be found; callers fall back to drawing
// something plain rather than nothing (same policy as the bundled font).
bool UIIcons_Ready(void);

// Draws icon `index` fitted to `dst` in `tint`. Out-of-range indices
// draw nothing, so a skill added without an icon is a blank tile rather
// than a garbled one.
void UIIcons_Draw(int index, Rectangle dst, Color tint);

#endif
