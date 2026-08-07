#ifndef UI_FOCUS_H
#define UI_FOCUS_H

#include "raylib.h"
#include <stdbool.h>

// Keyboard and D-pad focus for menus that are LISTS of choices - the NPC
// dialog and the windows it opens (shop, armorer, trainer, profession).
//
// A free-floating cursor is right for a spatial screen like the
// character creator, and wrong here: picking a reply out of three is a
// list, and steering a pointer onto it is work the player shouldn't have
// to do. So these screens get a focus index instead - up/down steps it,
// confirm activates it, and it lands on X (PlayStation) / A (Xbox).
//
// Immediate mode, so the ordering matters: UIFocus_Begin consumes this
// frame's navigation using LAST frame's item count, then each widget
// asks UIFocus_Item in draw order whether it is the focused one. The
// mouse still works exactly as before; the two coexist.

// Once per frame, before any focusable widget is drawn.
void UIFocus_Begin(void);

// Once per frame, after them.
void UIFocus_End(void);

// Registers the next focusable item in draw order. Returns true when it
// is the focused one - pass that straight to UI_Button's `highlighted`.
bool UIFocus_Item(void);

// True when the player pressed confirm this frame. Pair with the result
// of UIFocus_Item: `if (clicked || (focused && UIFocus_Confirm()))`.
bool UIFocus_Confirm(void);

// True when the player pressed cancel/back (B / Circle / Escape).
bool UIFocus_Cancel(void);

// Puts focus back on the first item. Call when the list a player is
// looking at changes out from under them - a new dialog, a window
// opening - or the highlight is left pointing at something else.
void UIFocus_Clear(void);

#endif
