#ifndef UI_NAV_H
#define UI_NAV_H

#include "raylib.h"
#include <stdbool.h>

// Spatial D-pad focus for screens laid out in two dimensions.
//
// ui_focus.c walks a LIST by index, which is right for a column of
// dialogue replies and wrong for the character creator - three columns,
// a grid of profession buttons, rows of colour swatches. There is no
// sensible linear order through that, and inventing one means the
// D-pad wanders somewhere the eye doesn't expect.
//
// So this navigates by GEOMETRY instead: every control registers its
// rectangle, and a direction press moves to the nearest control that
// actually lies that way. Nothing has to declare an order, and the
// focus goes where the layout says it should.
//
// Immediate mode, same shape as ui_focus: Begin, register each control
// with Item() while drawing it, End.

#define UI_NAV_MAX_ITEMS 64

void UINav_Begin(void);

// Registers a control. Returns true when it currently has focus.
// Register unconditionally and in a stable order - identity is the
// registration index, so a control that skips some frames renumbers
// everything after it.
bool UINav_Item(Rectangle rect);

void UINav_End(void);

// A focused stepper or swatch row calls this to take left/right for
// itself - returns -1, 0 or +1 and, when non-zero, consumes the press
// so focus doesn't also jump sideways. Only meaningful for the control
// that currently has focus.
int UINav_ClaimHorizontal(void);

// Enter, or the pad's bottom face button (A on Xbox, Cross on
// PlayStation). True at most once per press.
bool UINav_Confirm(void);

// Drops focus entirely - the state a screen opens in, so nothing is
// highlighted until the player actually reaches for the D-pad.
void UINav_Clear(void);

// True once the player has moved focus at all. Screens use this to stay
// out of the way of a mouse until a pad is actually being used.
bool UINav_Active(void);

#endif
