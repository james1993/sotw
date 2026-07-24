#ifndef UI_WORLD_H
#define UI_WORLD_H

#include "raylib.h"

// Everything that floats above an entity - nameplates, health and cast
// bars, damage flashes, quest markers, the interact prompt - drawn in
// SCREEN space after the world pass.
//
// These used to be drawn inside BeginMode2D, which meant the camera
// zoom scaled them: a 10px nameplate rendered at 23px on a zoomed-in
// view, blurry and inconsistent, and bars grew or shrank with the
// camera. Projecting each entity's position to screen coordinates and
// drawing at a fixed UI scale keeps text crisp and every plate the same
// size no matter how far in you are - the single biggest readability
// difference in the whole HUD.
//
// It also declutters: monsters only get a plate when they matter
// (targeted, hovered, awake, or hurt), so a quiet field isn't a wall of
// floating text.
void UIWorld_Draw(Camera2D camera, int screenWidth, int screenHeight);

#endif
