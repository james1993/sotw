#include "ui_cursor.h"
#include "ui_font.h"
#include <math.h>

#define GAMEPAD_ID 0
#define CURSOR_STICK_DEADZONE 0.25f
// Pixels per second at the reference window height; scales with the UI.
#define CURSOR_SPEED 900.0f

static Vector2 g_pos;
static bool g_active = false;
static bool g_padDrives = false; // last pointer input was the stick, not the mouse
static Vector2 g_lastMouse;

void UICursor_Update(float dt, bool menuOpen) {
    bool shouldBeActive = menuOpen && IsGamepadAvailable(GAMEPAD_ID);

    if (shouldBeActive && !g_active) {
        // Spawn just above the dialog (bottom-center), like GW1's menu
        // cursor appearing when a conversation opens.
        g_pos = (Vector2){ GetScreenWidth() / 2.0f, GetScreenHeight() * 0.72f };
        g_lastMouse = GetMousePosition();
        g_padDrives = true;
    }
    g_active = shouldBeActive;
    if (!g_active) return;

    float lx = GetGamepadAxisMovement(GAMEPAD_ID, GAMEPAD_AXIS_LEFT_X);
    float ly = GetGamepadAxisMovement(GAMEPAD_ID, GAMEPAD_AXIS_LEFT_Y);
    if (fabsf(lx) < CURSOR_STICK_DEADZONE) lx = 0.0f;
    if (fabsf(ly) < CURSOR_STICK_DEADZONE) ly = 0.0f;
    if (lx != 0.0f || ly != 0.0f) {
        float speed = CURSOR_SPEED * UI_Scale(GetScreenHeight());
        g_pos.x += lx * speed * dt;
        g_pos.y += ly * speed * dt;
        g_padDrives = true;
    }

    // Any real mouse motion hands the pointer back to the mouse.
    Vector2 m = GetMousePosition();
    if (m.x != g_lastMouse.x || m.y != g_lastMouse.y) g_padDrives = false;
    g_lastMouse = m;

    if (g_pos.x < 0.0f) g_pos.x = 0.0f;
    if (g_pos.y < 0.0f) g_pos.y = 0.0f;
    if (g_pos.x > (float)GetScreenWidth()) g_pos.x = (float)GetScreenWidth();
    if (g_pos.y > (float)GetScreenHeight()) g_pos.y = (float)GetScreenHeight();
}

bool UICursor_Active(void) {
    return g_active;
}

Vector2 UI_PointerPos(void) {
    if (g_active && g_padDrives) return g_pos;
    return GetMousePosition();
}

bool UI_PointerClicked(void) {
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return true;
    // A is the cursor's mouse button while the pad drives the pointer.
    return g_active && g_padDrives &&
           IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
}

void UICursor_Draw(int screenHeight) {
    if (!g_active || !g_padDrives) return;
    float s = UI_Scale(screenHeight);
    // A pointer arrow: tip at the hotspot, tail down-right.
    Vector2 tip = g_pos;
    Vector2 flank = { g_pos.x + 12.0f * s, g_pos.y + 12.0f * s };
    Vector2 tail = { g_pos.x, g_pos.y + 17.0f * s };
    DrawTriangle(tip, flank, tail, RAYWHITE);
    DrawTriangleLines(tip, flank, tail, BLACK);
}
