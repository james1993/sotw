#include "ui_nav.h"
#include "audio.h"
#include <math.h>

#define GAMEPAD_ID 0
#define STICK_THRESHOLD 0.6f

typedef enum { DIR_NONE = 0, DIR_LEFT, DIR_RIGHT, DIR_UP, DIR_DOWN } NavDir;

static Rectangle g_items[UI_NAV_MAX_ITEMS];
static Rectangle g_itemsLast[UI_NAV_MAX_ITEMS];
static int g_count = 0;
static int g_countLast = 0;
static int g_index = -1;
static NavDir g_pending = DIR_NONE;
static bool g_stickLatched = false;
static bool g_active = false;

static NavDir ReadDirection(void) {
    if (IsKeyPressed(KEY_LEFT))  return DIR_LEFT;
    if (IsKeyPressed(KEY_RIGHT)) return DIR_RIGHT;
    if (IsKeyPressed(KEY_UP))    return DIR_UP;
    if (IsKeyPressed(KEY_DOWN))  return DIR_DOWN;

    if (IsGamepadAvailable(GAMEPAD_ID)) {
        if (IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_LEFT_FACE_LEFT))  return DIR_LEFT;
        if (IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) return DIR_RIGHT;
        if (IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_LEFT_FACE_UP))    return DIR_UP;
        if (IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_LEFT_FACE_DOWN))  return DIR_DOWN;

        // The stick steps once per push and re-arms at centre, so holding
        // it doesn't rip across the screen.
        float lx = GetGamepadAxisMovement(GAMEPAD_ID, GAMEPAD_AXIS_LEFT_X);
        float ly = GetGamepadAxisMovement(GAMEPAD_ID, GAMEPAD_AXIS_LEFT_Y);
        if (fabsf(lx) < STICK_THRESHOLD && fabsf(ly) < STICK_THRESHOLD) {
            g_stickLatched = false;
        } else if (!g_stickLatched) {
            g_stickLatched = true;
            if (fabsf(lx) > fabsf(ly)) return (lx > 0.0f) ? DIR_RIGHT : DIR_LEFT;
            return (ly > 0.0f) ? DIR_DOWN : DIR_UP;
        }
    }
    return DIR_NONE;
}

static Vector2 Centre(Rectangle r) {
    return (Vector2){ r.x + r.width / 2.0f, r.y + r.height / 2.0f };
}

// The nearest control that genuinely lies in `dir`. Distance along the
// direction is what ranks candidates; distance across it is penalised
// heavily, so pressing Down from a button lands on the thing under it
// rather than on something further along the same row.
static int BestInDirection(int from, NavDir dir) {
    if (from < 0 || from >= g_countLast) return from;
    Vector2 a = Centre(g_itemsLast[from]);

    int best = -1;
    float bestScore = 1e18f;
    for (int i = 0; i < g_countLast; i++) {
        if (i == from) continue;
        Vector2 b = Centre(g_itemsLast[i]);
        float dx = b.x - a.x, dy = b.y - a.y;

        float along, across;
        switch (dir) {
            case DIR_LEFT:  along = -dx; across = fabsf(dy); break;
            case DIR_RIGHT: along =  dx; across = fabsf(dy); break;
            case DIR_UP:    along = -dy; across = fabsf(dx); break;
            default:        along =  dy; across = fabsf(dx); break;
        }
        // A tolerance rather than "> 0": controls in the same row are
        // never exactly aligned, and a strict test makes a row of
        // swatches unreachable from the label above it.
        if (along <= 2.0f) continue;

        float score = along + across * 2.5f;
        if (score < bestScore) { bestScore = score; best = i; }
    }
    return (best >= 0) ? best : from;
}

void UINav_Begin(void) {
    g_pending = ReadDirection();
    g_count = 0;
}

bool UINav_Item(Rectangle rect) {
    if (g_count >= UI_NAV_MAX_ITEMS) return false;
    int mine = g_count++;
    g_items[mine] = rect;
    return mine == g_index;
}

int UINav_ClaimHorizontal(void) {
    if (g_pending == DIR_LEFT)  { g_pending = DIR_NONE; return -1; }
    if (g_pending == DIR_RIGHT) { g_pending = DIR_NONE; return +1; }
    return 0;
}

void UINav_End(void) {
    // The move is applied against the PREVIOUS frame's rectangles,
    // because in immediate mode this frame's controls have already been
    // drawn by the time we get here. Layout is stable between frames, so
    // the only cost is that a screen which just appeared takes one frame
    // before it can be stepped.
    if (g_pending != DIR_NONE && g_countLast > 0) {
        int before = g_index;
        if (g_index < 0) {
            g_index = 0; // first press wakes focus up rather than moving it
        } else {
            g_index = BestInDirection(g_index, g_pending);
        }
        g_active = true;
        if (g_index != before) Audio_Play(SFX_UI_MOVE);
    }
    g_pending = DIR_NONE;

    for (int i = 0; i < g_count; i++) g_itemsLast[i] = g_items[i];
    g_countLast = g_count;
    if (g_index >= g_countLast) g_index = g_countLast - 1;
}

bool UINav_Confirm(void) {
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) return true;
    return IsGamepadAvailable(GAMEPAD_ID) &&
           IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
}

void UINav_Clear(void) {
    g_index = -1;
    g_pending = DIR_NONE;
    g_stickLatched = false;
    g_active = false;
}

bool UINav_Active(void) {
    return g_active;
}
