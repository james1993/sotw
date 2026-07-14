#include "ui_menu.h"
#include "ui_font.h"
#include "raylib.h"
#include <math.h>

#define GAMEPAD_ID 0
#define MENU_STICK_THRESHOLD 0.5f

// Which entry is highlighted. Driven by D-pad/left stick/arrow keys;
// mouse hover moves it too (only on actual mouse motion, so a parked
// pointer doesn't pin the highlight and fight the pad).
static int g_selected = 0;
static bool g_stickLatched = false;
static Vector2 g_lastMouse;

static bool MenuButton(Rectangle r, const char *label, int font, bool selected) {
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, r);

    Color bg = (hovered || selected) ? (Color){ 80, 90, 120, 255 }
                                     : (Color){ 45, 50, 70, 255 };
    DrawRectangleRec(r, bg);
    DrawRectangleLinesEx(r, 2, selected ? (Color){ 210, 195, 150, 255 }
                                        : (Color){ 130, 130, 150, 255 });

    int tw = UITextWidth(label, font);
    UIText(label, (int)(r.x + (r.width - tw) / 2),
           (int)(r.y + (r.height - font) / 2), font, RAYWHITE);

    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

MenuAction UI_DrawMainMenu(int screenWidth, int screenHeight, bool hasSave) {
    float scale = UI_Scale(screenHeight);

    // Title block, upper third.
    {
        const char *title = "GUILD WARS - 2D DEMAKE";
        int titleFont = (int)(36 * scale);
        int tw = UITextWidth(title, titleFont);
        UIText(title, (screenWidth - tw) / 2, (int)(screenHeight * 0.22f),
               titleFont, (Color){ 220, 200, 140, 255 });

        const char *sub = "a raylib prototype";
        int subFont = (int)(14 * scale);
        int sw = UITextWidth(sub, subFont);
        UIText(sub, (screenWidth - sw) / 2,
               (int)(screenHeight * 0.22f) + titleFont + (int)(8 * scale),
               subFont, LIGHTGRAY);
    }

    // The entry list depends on whether a save exists.
    const char *labels[3];
    MenuAction actions[3];
    int count = 0;
    if (hasSave) { labels[count] = "Continue"; actions[count++] = MENU_CONTINUE; }
    labels[count] = "New Game"; actions[count++] = MENU_NEW_GAME;
    labels[count] = "Quit";     actions[count++] = MENU_QUIT;

    // --- Selection movement: D-pad / left stick / arrow keys / W-S ---
    int nav = 0;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) nav++;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) nav--;
    if (IsGamepadAvailable(GAMEPAD_ID)) {
        if (IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) nav++;
        if (IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_LEFT_FACE_UP)) nav--;
        // Stick with a latch: one step per push, re-armed at center.
        float ly = GetGamepadAxisMovement(GAMEPAD_ID, GAMEPAD_AXIS_LEFT_Y);
        if (fabsf(ly) < MENU_STICK_THRESHOLD) {
            g_stickLatched = false;
        } else if (!g_stickLatched) {
            nav += (ly > 0.0f) ? 1 : -1;
            g_stickLatched = true;
        }
    }
    if (g_selected >= count) g_selected = count - 1;
    g_selected = (g_selected + nav + count) % count;

    // --- Buttons ---
    int btnW = (int)(280 * scale);
    int btnH = (int)(48 * scale);
    int gap = (int)(16 * scale);
    int font = (int)(16 * scale);
    int x = (screenWidth - btnW) / 2;
    int y = (int)(screenHeight * 0.45f);

    Vector2 mouse = GetMousePosition();
    bool mouseMoved = (mouse.x != g_lastMouse.x || mouse.y != g_lastMouse.y);
    g_lastMouse = mouse;

    MenuAction result = MENU_NONE;
    for (int i = 0; i < count; i++) {
        Rectangle r = { (float)x, (float)y, (float)btnW, (float)btnH };
        if (mouseMoved && CheckCollisionPointRec(mouse, r)) g_selected = i;
        if (MenuButton(r, labels[i], font, i == g_selected)) result = actions[i];
        y += btnH + gap;
    }

    // Enter / gamepad A confirm the highlighted entry.
    if (result == MENU_NONE &&
        (IsKeyPressed(KEY_ENTER) ||
         (IsGamepadAvailable(GAMEPAD_ID) &&
          IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)))) {
        result = actions[g_selected];
    }

    const char *hint = "Up/Down or D-pad selects - Enter or (A) confirms";
    int hintFont = (int)(11 * scale);
    int hw = UITextWidth(hint, hintFont);
    UIText(hint, (screenWidth - hw) / 2, y + (int)(6 * scale), hintFont, GRAY);

    return result;
}
