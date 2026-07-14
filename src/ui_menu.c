#include "ui_menu.h"
#include "ui_font.h"
#include "raylib.h"

static bool MenuButton(Rectangle r, const char *label, int font, bool isDefault) {
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, r);

    Color bg = hovered ? (Color){ 80, 90, 120, 255 }
             : isDefault ? (Color){ 58, 66, 92, 255 }
                         : (Color){ 45, 50, 70, 255 };
    DrawRectangleRec(r, bg);
    DrawRectangleLinesEx(r, 2, hovered ? (Color){ 200, 200, 215, 255 }
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

    int btnW = (int)(280 * scale);
    int btnH = (int)(48 * scale);
    int gap = (int)(16 * scale);
    int font = (int)(16 * scale);
    int x = (screenWidth - btnW) / 2;
    int y = (int)(screenHeight * 0.45f);

    MenuAction defaultAction = hasSave ? MENU_CONTINUE : MENU_NEW_GAME;
    MenuAction result = MENU_NONE;

    if (hasSave) {
        if (MenuButton((Rectangle){ (float)x, (float)y, (float)btnW, (float)btnH },
                       "Continue", font, defaultAction == MENU_CONTINUE)) {
            result = MENU_CONTINUE;
        }
        y += btnH + gap;
    }

    if (MenuButton((Rectangle){ (float)x, (float)y, (float)btnW, (float)btnH },
                   "New Game", font, defaultAction == MENU_NEW_GAME)) {
        result = MENU_NEW_GAME;
    }
    y += btnH + gap;

    if (MenuButton((Rectangle){ (float)x, (float)y, (float)btnW, (float)btnH },
                   "Quit", font, false)) {
        result = MENU_QUIT;
    }
    y += btnH + gap;

    // Enter / gamepad A takes the default path.
    if (result == MENU_NONE &&
        (IsKeyPressed(KEY_ENTER) ||
         (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)))) {
        result = defaultAction;
    }

    const char *hint = hasSave ? "Enter: continue your journey" : "Enter: begin";
    int hintFont = (int)(11 * scale);
    int hw = UITextWidth(hint, hintFont);
    UIText(hint, (screenWidth - hw) / 2, y + (int)(6 * scale), hintFont, GRAY);

    return result;
}
