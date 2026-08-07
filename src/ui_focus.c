#include "ui_focus.h"
#include "audio.h"
#include <math.h>

#define GAMEPAD_ID 0
#define STICK_THRESHOLD 0.55f

static int g_index = 0;
static int g_registeredThisFrame = 0;
static int g_countLastFrame = 0;
static int g_pendingStep = 0;
static bool g_stickLatched = false;

// Deliberately NOT gated on a gamepad being present. The same focus runs
// off the arrow keys, which means keyboard players get list navigation
// for free and - more usefully - the whole path can be exercised on a
// machine with no controller attached.
static int NavStep(void) {
    int nav = 0;
    if (IsKeyPressed(KEY_DOWN)) nav++;
    if (IsKeyPressed(KEY_UP)) nav--;

    if (IsGamepadAvailable(GAMEPAD_ID)) {
        if (IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) nav++;
        if (IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_LEFT_FACE_UP)) nav--;
        // The stick steps once per push and re-arms at centre, so holding
        // it doesn't rip through the list.
        float ly = GetGamepadAxisMovement(GAMEPAD_ID, GAMEPAD_AXIS_LEFT_Y);
        if (fabsf(ly) < STICK_THRESHOLD) {
            g_stickLatched = false;
        } else if (!g_stickLatched) {
            nav += (ly > 0.0f) ? 1 : -1;
            g_stickLatched = true;
        }
    }
    return nav;
}

void UIFocus_Begin(void) {
    // Navigation is applied against the PREVIOUS frame's count, because
    // in immediate mode this frame's items haven't been drawn yet. The
    // list is stable between frames, so the only cost is that a list
    // which just appeared takes one frame before it can be stepped.
    int step = NavStep() + g_pendingStep;
    g_pendingStep = 0;

    if (step != 0 && g_countLastFrame > 0) {
        int before = g_index;
        g_index = (g_index + step) % g_countLastFrame;
        if (g_index < 0) g_index += g_countLastFrame;
        if (g_index != before) Audio_Play(SFX_UI_MOVE);
    }

    g_registeredThisFrame = 0;
}

void UIFocus_End(void) {
    g_countLastFrame = g_registeredThisFrame;
    if (g_countLastFrame <= 0) {
        // Nothing focusable this frame - a trainer with nothing left to
        // teach, say. Reset rather than keeping the old index, or the
        // next list to appear opens with its highlight pointing at an
        // item that isn't there and nothing looks selected.
        g_index = 0;
    } else if (g_index >= g_countLastFrame) {
        // A list that shrank (a skill bought, an item sold) must not
        // leave the highlight past the end of it.
        g_index = g_countLastFrame - 1;
    }
}

bool UIFocus_Item(void) {
    int mine = g_registeredThisFrame++;
    return mine == g_index;
}

bool UIFocus_Confirm(void) {
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) return true;
    // RIGHT_FACE_DOWN is the bottom face button: X on a PlayStation pad,
    // A on an Xbox one. raylib names it by position, not by letter,
    // which is what makes one binding correct on both.
    return IsGamepadAvailable(GAMEPAD_ID) &&
           IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
}

bool UIFocus_Cancel(void) {
    if (IsKeyPressed(KEY_ESCAPE)) return true;
    return IsGamepadAvailable(GAMEPAD_ID) &&
           IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
}

void UIFocus_Clear(void) {
    g_index = 0;
    g_pendingStep = 0;
    g_stickLatched = false;
}
