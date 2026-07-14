#ifndef UI_CURSOR_H
#define UI_CURSOR_H

#include "raylib.h"
#include <stdbool.h>

// GW1-style virtual menu cursor: while an NPC dialog or the merchant
// window is open and a gamepad is present, the left stick steers an
// on-screen pointer and A clicks whatever it hovers - how GW1 lets a
// pad navigate quest and merchant menus. Touching the real mouse hands
// the pointer back to it immediately; the cursor disappears when the
// dialog closes and the stick goes back to moving the character.

// Advance the cursor (input phase, once per frame). menuOpen is
// "is an NPC dialog up" - the shop can only exist under one.
void UICursor_Update(float dt, bool menuOpen);

// True while the cursor exists (menu open + pad present). input.c uses
// this to route the left stick to the cursor instead of movement.
bool UICursor_Active(void);

// Pointer arrow, drawn on top of all menu UI (only visible while the
// pad is actually driving the pointer).
void UICursor_Draw(int screenHeight);

// THE pointer, for menu widgets: the virtual cursor while the pad
// drives it, the real mouse otherwise. Menu hover/click goes through
// these instead of GetMousePosition/IsMouseButtonPressed so both input
// devices work everywhere.
Vector2 UI_PointerPos(void);
bool UI_PointerClicked(void);

#endif
