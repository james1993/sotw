#ifndef UI_TOOLTIP_H
#define UI_TOOLTIP_H

#include "raylib.h"

struct Entity;

// GW1's skill tooltip: the name, what kind of skill it is and which
// attribute drives it, what it actually does at the viewer's CURRENT
// rank, and the energy / activation / recharge footer.
//
// The body text is generated from the skill's effect steps rather than
// hand-written, for the same reason the skills themselves are data: a
// written description drifts the moment a number is retuned, and a
// tooltip that lies about what a skill does is worse than none. What
// you read here is what the effect VM will run.
//
// `viewer` supplies the attribute ranks the numbers are computed at, and
// may be NULL (the numbers then read at rank 0). `anchor` is the widget
// being hovered - the panel is placed beside it and nudged to stay on
// screen. Call during drawing, after the panel underneath it.
void UITooltip_Skill(int skillIndex, const struct Entity *viewer,
                     Rectangle anchor, int screenWidth, int screenHeight);

// Deferred form, for the common case: a widget asks for a tooltip while
// it draws, and the tooltip is painted after everything else so it can't
// end up underneath a panel drawn later. Request as often as you like -
// the last request of the frame wins, which is the one the player is
// actually pointing at. Flush draws it and clears the request.
void UITooltip_Request(int skillIndex, Rectangle anchor);
void UITooltip_Flush(const struct Entity *viewer, int screenWidth, int screenHeight);

#endif
