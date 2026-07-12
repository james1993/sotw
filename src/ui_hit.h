#ifndef UI_HIT_H
#define UI_HIT_H

#include "raylib.h"
#include <stdbool.h>

// Immediate-mode UI hit registry. Every screen-space widget claims the
// rect it drew, every frame; input checks the pointer against those
// claims before treating a click as a world click. This replaces the
// old pattern where each new widget had to remember to add itself to an
// ever-growing check in input.c - forgetting meant clicks fell through
// the widget and walked the player (the compass had exactly this bug).
//
// Input runs before drawing, so UIHit_Contains tests against the
// PREVIOUS frame's claims - the standard immediate-mode tradeoff, one
// frame of staleness on a 60fps UI nobody can perceive.
void UIHit_NewFrame(void);          // once per frame, before UI draws
void UIHit_Claim(Rectangle rect);   // "this rect is UI - clicks stop here"
bool UIHit_Contains(Vector2 point); // against last frame's claims

#endif
