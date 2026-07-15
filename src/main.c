#include "raylib.h"
#include "entity.h"
#include "skill.h"
#include "combat.h"
#include "ai_hero.h"
#include "input.h"
#include "items.h"
#include "projectile.h"
#include "world.h"
#include "quests.h"
#include "save.h"
#include "ui_skillbar.h"
#include "ui_target.h"
#include "ui_party.h"
#include "ui_compass.h"
#include "ui_panels.h"
#include "ui_hit.h"
#include "ui_cursor.h"
#include "ui_map.h"
#include "ui_menu.h"
#include "ui_font.h"
#include "render.h"

#define PLAYER_INDEX 0

typedef enum {
    APP_MENU,
    APP_PLAYING
} AppState;

// Fresh-start (or save-restored) world. Reset order matters: globals
// first, then World_Init hands out the starting kit, then a save (if
// requested) replaces that kit and logs in at the last outpost.
// Autosaving only turns on once the state on disk can't be clobbered
// by defaults.
static void StartGame(bool loadSave, Camera2D *camera) {
    Items_Reset();
    Quests_Reset();
    g_entityCount = 0; // generation counters keep climbing, killing stale refs

    World_Init();
    if (loadSave) Save_LoadAndApply();
    Save_Enable();

    Entity *player = Entity_Get(PLAYER_INDEX);
    if (player) camera->target = player->pos;
}

int main(void) {
    // Deliberately NOT using FLAG_WINDOW_HIGHDPI: on displays with OS-level
    // scaling it can make GetScreenWidth()/GetScreenHeight() disagree in
    // scale with what GetMousePosition() actually reports, which sent
    // click-to-move to wildly wrong positions. Coordinates staying
    // consistent matters more than crispness.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1600, 900, "Guild Wars 1 2D Demake - Prototype");

    // Maximize via the OS/window manager rather than computing a size
    // from monitor dimensions ourselves - GetMonitorWidth/Height returns
    // physical pixels while SetWindowSize expects logical points on
    // scaled displays, and mixing them previously produced a window far
    // larger than the desktop. If no WM honors the request (bare X
    // server), the 1600x900 fallback above stays, still resizable.
    SetWindowState(FLAG_WINDOW_MAXIMIZED);

    SetWindowMinSize(960, 600);
    SetTargetFPS(60);

    // raylib quits on Escape by default; we use Escape to clear the
    // current target (input.c) and close dialogs (ui_panels.c), so the
    // default would exit the game on the first target-drop.
    SetExitKey(KEY_NULL);

    UIFont_Init();
    SkillDB_Init();

    AppState app = APP_MENU;

    Camera2D camera = { 0 };
    camera.offset = (Vector2){ GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f };
    // Default zoom scales with window width so the starting area fills a
    // comfortable fraction of the window on any resolution.
    camera.zoom = GetScreenWidth() / 700.0f;
    if (camera.zoom < MIN_CAMERA_ZOOM) camera.zoom = MIN_CAMERA_ZOOM;
    if (camera.zoom > MAX_CAMERA_ZOOM) camera.zoom = MAX_CAMERA_ZOOM;
    int lastScreenWidth = GetScreenWidth();

    bool quitRequested = false;
    bool paused = false;

    while (!WindowShouldClose() && !quitRequested) {
        float dt = GetFrameTime();
        int screenWidth = GetScreenWidth();
        int screenHeight = GetScreenHeight();

        // The maximize request above lands asynchronously on some window
        // managers, so the width the startup zoom was computed from may
        // have been the pre-maximize fallback. Until the player takes
        // manual control of zoom, re-derive the default whenever the
        // window size actually changes (maximize landing, resize, monitor
        // move).
        if (screenWidth != lastScreenWidth && !Input_UserAdjustedZoom()) {
            camera.zoom = screenWidth / 700.0f;
            if (camera.zoom < MIN_CAMERA_ZOOM) camera.zoom = MIN_CAMERA_ZOOM;
            if (camera.zoom > MAX_CAMERA_ZOOM) camera.zoom = MAX_CAMERA_ZOOM;
        }
        lastScreenWidth = screenWidth;

        // Re-centered every frame so resizing the window (or moving it to
        // a different monitor) doesn't leave the camera offset stale.
        camera.offset = (Vector2){ screenWidth / 2.0f, screenHeight / 2.0f };

        // --- Main menu: the world doesn't exist until a choice is made ---
        if (app == APP_MENU) {
            BeginDrawing();
            ClearBackground((Color){ 16, 15, 18, 255 });
            MenuAction action = UI_DrawMainMenu(screenWidth, screenHeight, Save_Exists());
            EndDrawing();

            if (action == MENU_QUIT) {
                quitRequested = true;
            } else if (action == MENU_NEW_GAME) {
                StartGame(false, &camera);
                app = APP_PLAYING;
            } else if (action == MENU_CONTINUE) {
                StartGame(true, &camera);
                app = APP_PLAYING;
            }
            continue;
        }

        // --- In-game frame ---
        // P or the gamepad's Start button toggles the pause menu; while
        // paused the world is frozen (single-player privilege GW1 never
        // had) and only the pause menu takes input.
        if (IsKeyPressed(KEY_P) ||
            (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT))) {
            paused = !paused;
        }

        Entity *playerNow = Entity_Get(PLAYER_INDEX);
        if (!paused) {
            Input_Update(&camera, dt);
            AI_Update(dt);
            Combat_TickTimers(dt);
            Projectile_Update(dt);

            playerNow = Entity_Get(PLAYER_INDEX);
            World_Update(playerNow, dt);
            playerNow = Entity_Get(PLAYER_INDEX); // zone loads rebuild the array
            Items_UpdatePickup(playerNow);
            Quests_Update(playerNow);
        }
        if (playerNow) camera.target = playerNow->pos;

        BeginDrawing();
        // Warm dirt tones in camp, cool grass tones in the plains -
        // per-zone data (world.c).
        ClearBackground(World_GetClearColor());

        Render_World(camera);

        // Screen-space UI from here down; every widget claims its rect
        // with UIHit so next frame's clicks stop at the UI instead of
        // falling through into the world.
        UIHit_NewFrame();

        // Zone name, top center - how GW1 tells you where you are.
        {
            const char *zone = World_GetZoneName();
            int font = UI_ScaledFontSize(screenHeight, 18);
            int tw = UITextWidth(zone, font);
            UIText(zone, (screenWidth - tw) / 2, 14, font, (Color){ 220, 210, 180, 255 });
        }

        UI_DrawSkillBar(screenWidth, screenHeight);
        UI_DrawResourceBars(screenWidth, screenHeight);
        UI_DrawCompass(screenWidth, screenHeight);
        UI_DrawPartyPanel(screenWidth, screenHeight);
        UI_DrawTargetPanel(screenWidth, screenHeight, 50);
        Quests_DrawTracker(screenWidth, screenHeight);
        if (!paused) {
            // Interactive UI is skipped under the pause overlay so its
            // buttons can't be clicked through the menu.
            UI_PanelsUpdateAndDraw(screenWidth, screenHeight);
            UI_MapUpdateAndDraw(screenWidth, screenHeight); // region map (M/Select)
            UICursor_Draw(screenHeight); // menu pointer, above everything it clicks
        } else {
            PauseAction pa = UI_DrawPauseMenu(screenWidth, screenHeight);
            if (pa == PAUSE_RESUME) {
                paused = false;
            } else if (pa == PAUSE_QUIT_TO_MENU) {
                Save_Write();
                paused = false;
                app = APP_MENU;
            } else if (pa == PAUSE_QUIT_GAME) {
                paused = false;
                quitRequested = true; // final autosave runs after the loop
            }
        }

        // Death overlay: GW1 dims the world and tells you plainly.
        if (playerNow && !playerNow->alive) {
            DrawRectangle(0, 0, screenWidth, screenHeight, (Color){ 0, 0, 0, 110 });
            int bigFont = UI_ScaledFontSize(screenHeight, 34);
            const char *msg = "You have died.";
            int tw = UITextWidth(msg, bigFont);
            UIText(msg, (screenWidth - tw) / 2, screenHeight / 2 - bigFont, bigFont, (Color){ 220, 80, 80, 255 });

            bool anyAlive = false;
            for (int i = 0; i < g_entityCount; i++) {
                Entity *e = &g_entities[i];
                if (e->team == 0 && e->alive && (e->kind == ENT_PLAYER || e->kind == ENT_HERO)) anyAlive = true;
            }
            int smallFont = UI_ScaledFontSize(screenHeight, 15);
            const char *sub = anyAlive ? "Your party fights on..."
                                       : "Your party has fallen. Returning to the shrine...";
            int sw = UITextWidth(sub, smallFont);
            UIText(sub, (screenWidth - sw) / 2, screenHeight / 2 + 8, smallFont, LIGHTGRAY);
        }

        EndDrawing();
    }

    // Final autosave on the way out (no-op if the menu never started a
    // game) - quitting from the X button loses nothing, like GW1.
    Save_Write();

    CloseWindow();
    return 0;
}
