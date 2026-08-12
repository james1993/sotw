#include "ui_menu.h"
#include "audio.h"
#include <stdio.h>
#include "ui_font.h"
#include "ui_theme.h"
#include "raylib.h"
#include <stddef.h>
#include <math.h>

#define GAMEPAD_ID 0
#define MENU_STICK_THRESHOLD 0.5f

// Which entry is highlighted (one selection per menu). Driven by
// D-pad/left stick/arrow keys; mouse hover moves it too (only on
// actual mouse motion, so a parked pointer doesn't pin the highlight
// and fight the pad).
static int g_selected = 0;
static int g_pauseSelected = 0;
static bool g_stickLatched = false;
static Vector2 g_lastMouse;

// One step of up/down navigation from keyboard + pad, shared by both
// menus. Returns -1/0/+1.
static int MenuNavStep(void) {
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
    if (nav != 0) Audio_Play(SFX_UI_MOVE);
    return nav;
}

static bool MenuConfirm(void) {
    bool confirmed = IsKeyPressed(KEY_ENTER) ||
                     (IsGamepadAvailable(GAMEPAD_ID) &&
                      IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_RIGHT_FACE_DOWN));
    if (confirmed) Audio_Play(SFX_UI_CLICK);
    return confirmed;
}

static bool MenuButton(Rectangle r, const char *label, int font, bool selected) {
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, r);

    Color bg = (hovered || selected) ? (Color){ 80, 90, 120, 255 }
                                     : (Color){ 45, 50, 70, 255 };
    DrawRectangleGradientV((int)r.x, (int)r.y, (int)r.width, (int)r.height,
                           bg, (Color){ bg.r / 2, bg.g / 2, bg.b / 2, 255 });
    DrawRectangleLinesEx(r, 2, selected ? UI_GOLD : UI_GOLD_DIM);

    int tw = UITextWidth(label, font);
    UIText(label, (int)(r.x + (r.width - tw) / 2),
           (int)(r.y + (r.height - font) / 2), font, RAYWHITE);

    bool clicked = hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (clicked) Audio_Play(SFX_UI_CLICK);
    return clicked;
}

MenuAction UI_DrawMainMenu(int screenWidth, int screenHeight, bool hasSave) {
    float scale = UI_Scale(screenHeight);

    // Title block, upper third.
    {
        const char *title = "GUILD WARS - 2D DEMAKE";
        int titleFont = (int)(36 * scale);
        int tw = UITextDisplayWidth(title, titleFont);
        UITextDisplay(title, (screenWidth - tw) / 2, (int)(screenHeight * 0.22f),
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
    // One entry, not "Continue" plus "New Game": with six character
    // slots the roster is where both of those decisions are made, and
    // duplicating them here would only ask the same question twice.
    (void)hasSave;
    labels[count] = "Play";  actions[count++] = MENU_NEW_GAME;
    labels[count] = "Quit";  actions[count++] = MENU_QUIT;

    // --- Selection movement: D-pad / left stick / arrow keys / W-S ---
    int nav = MenuNavStep();
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
    if (result == MENU_NONE && MenuConfirm()) {
        result = actions[g_selected];
    }

    const char *hint = "Up/Down or D-pad selects - Enter or (A) confirms";
    int hintFont = (int)(11 * scale);
    int hw = UITextWidth(hint, hintFont);
    UIText(hint, (screenWidth - hw) / 2, y + (int)(6 * scale), hintFont, GRAY);

    // Asset attribution, on the title screen because that is exactly what
    // CC BY asks a video game for: a credit the player can reach. The
    // fonts are OFL and the ground is CC0 (no attribution required), but
    // naming them here costs a line and is the decent thing to do. The
    // full breakdown, per icon, is in assets/CREDITS.md.
    {
        const char *credits[] = {
            "Skill icons by Lorc and Delapouite - game-icons.net - CC BY 3.0",
            "Cinzel and Alegreya Sans under the SIL Open Font License",
            "Ground textures by Kenney - kenney.nl - CC0",
        };
        int creditFont = (int)(10 * scale);
        int cy = screenHeight - (int)(14 * scale) - creditFont * 3 - (int)(8 * scale);
        for (int i = 0; i < 3; i++) {
            int cw = UITextWidth(credits[i], creditFont);
            UIText(credits[i], (screenWidth - cw) / 2, cy, creditFont,
                   (Color){ 120, 116, 108, 255 });
            cy += creditFont + (int)(4 * scale);
        }
    }

    return result;
}

// One entry in the pause hub. Separators carry a NULL label and group
// the destructive exits away from the everyday screens, so "Quit Game"
// is never the neighbour of "Resume".
typedef struct {
    const char *label;
    const char *shortcut; // key that also opens it, NULL for none
    PauseAction action;
} PauseEntry;

static const PauseEntry g_pauseEntries[] = {
    { "Resume",            NULL,  PAUSE_RESUME },
    { "Skills & Attributes", "L", PAUSE_OPEN_SKILLS },
    { "Equipment",         "E",   PAUSE_OPEN_EQUIPMENT },
    { "Inventory",         "I",   PAUSE_OPEN_INVENTORY },
    { "Hero",              "T",   PAUSE_OPEN_TITLES },
    { "Region Map",        "M",   PAUSE_OPEN_MAP },
    { NULL,                NULL,  PAUSE_NONE }, // separator
    // The label is rewritten each frame with the live value, so the row
    // shows the current volume rather than just offering to change it.
    { "Sound",             NULL,  PAUSE_VOLUME },
    { NULL,                NULL,  PAUSE_NONE }, // separator
    { "Change Character"  , NULL,  PAUSE_QUIT_TO_MENU },
    { "Quit Game",         NULL,  PAUSE_QUIT_GAME },
};
#define PAUSE_ENTRY_COUNT (int)(sizeof(g_pauseEntries) / sizeof(g_pauseEntries[0]))

// Steps the highlight, skipping separators in whichever direction we're
// heading so navigation never lands on a blank line.
static int PauseStep(int from, int dir) {
    int i = from;
    for (int guard = 0; guard < PAUSE_ENTRY_COUNT; guard++) {
        i = (i + dir + PAUSE_ENTRY_COUNT) % PAUSE_ENTRY_COUNT;
        if (g_pauseEntries[i].label) return i;
    }
    return from;
}

PauseAction UI_DrawPauseMenu(int screenWidth, int screenHeight) {
    float scale = UI_Scale(screenHeight);

    DrawRectangle(0, 0, screenWidth, screenHeight, (Color){ 0, 0, 0, 178 });

    int font = UI_FontSize(scale, UI_TEXT_MD);
    int titleFont = UI_FontSize(scale, UI_TEXT_XL);
    int hintFont = UI_FontSize(scale, UI_TEXT_XS);

    int btnW = (int)(320 * scale);
    int btnH = (int)(40 * scale);
    int gap = UI_SP(scale, 2);
    int sepH = UI_SP(scale, 3);

    // Measure the stack so the whole panel can be centered as one block
    // rather than starting at a guessed fraction of the screen.
    int listH = 0;
    for (int i = 0; i < PAUSE_ENTRY_COUNT; i++) {
        listH += g_pauseEntries[i].label ? btnH + gap : sepH;
    }
    int padY = UI_SP(scale, 6);
    int panelH = padY * 2 + titleFont + UI_SP(scale, 4) + listH + hintFont + UI_SP(scale, 3);
    int panelW = btnW + UI_SP(scale, 10);
    Rectangle panel = { (screenWidth - panelW) / 2.0f, (screenHeight - panelH) / 2.0f,
                        (float)panelW, (float)panelH };
    UI_ThemePanel(panel, scale, 0);

    int y = (int)panel.y + padY;
    UI_TextShadowCentered("MENU", screenWidth / 2, y, titleFont, UI_GOLD);
    y += titleFont + UI_SP(scale, 4);

    int nav = MenuNavStep();
    if (nav != 0) g_pauseSelected = PauseStep(g_pauseSelected, nav);
    if (!g_pauseEntries[g_pauseSelected].label) g_pauseSelected = PauseStep(g_pauseSelected, 1);

    int x = (screenWidth - btnW) / 2;

    Vector2 mouse = GetMousePosition();
    bool mouseMoved = (mouse.x != g_lastMouse.x || mouse.y != g_lastMouse.y);
    g_lastMouse = mouse;

    PauseAction result = PAUSE_NONE;
    for (int i = 0; i < PAUSE_ENTRY_COUNT; i++) {
        const PauseEntry *entry = &g_pauseEntries[i];
        if (!entry->label) {
            // Separator: a hairline rule, inset so it reads as a divider
            // rather than a border.
            DrawRectangle(x + btnW / 6, y + sepH / 2, btnW - btnW / 3, 1,
                          (Color){ 92, 84, 64, 160 });
            y += sepH;
            continue;
        }

        Rectangle r = { (float)x, (float)y, (float)btnW, (float)btnH };
        if (mouseMoved && CheckCollisionPointRec(mouse, r)) g_pauseSelected = i;
        bool selected = (i == g_pauseSelected);

        const char *label = entry->label;
        char volumeLabel[48];
        if (entry->action == PAUSE_VOLUME) {
            int v = Audio_GetVolume();
            if (v <= 0) snprintf(volumeLabel, sizeof(volumeLabel), "Sound:  off");
            else        snprintf(volumeLabel, sizeof(volumeLabel), "Sound:  %d%%", v);
            label = volumeLabel;
        }
        if (UI_Button(r, label, font, true, selected)) result = entry->action;

        // The keyboard shortcut sits right-aligned inside the row, so the
        // menu teaches its own bindings instead of hiding them.
        if (entry->shortcut) {
            int bw = UI_KeyBadge(entry->shortcut, 0, 0, hintFont, false);
            UI_KeyBadge(entry->shortcut, (int)(r.x + r.width) - bw - UI_SP(scale, 3),
                        (int)(r.y + (r.height - hintFont * 1.5f) / 2), hintFont, true);
        }
        y += btnH + gap;
    }

    if (result == PAUSE_NONE && MenuConfirm()) {
        result = g_pauseEntries[g_pauseSelected].action;
    }

    UI_TextShadowCentered(
        IsGamepadAvailable(GAMEPAD_ID) ? "D-pad selects  -  (A) confirms  -  (B) resumes"
                                       : "Up/Down selects  -  Enter confirms  -  Esc resumes",
        screenWidth / 2, y + UI_SP(scale, 1), hintFont, UI_TEXT_MUTED);
    // P/Start (handled by main.c as the toggle), Escape, and B all resume.
    if (result == PAUSE_NONE &&
        (IsKeyPressed(KEY_ESCAPE) ||
         (IsGamepadAvailable(GAMEPAD_ID) &&
          IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)))) {
        result = PAUSE_RESUME;
    }

    return result;
}
