#include "raylib.h"
#include "entity.h"
#include "skill.h"
#include "combat.h"
#include "ai_hero.h"
#include "input.h"
#include "items.h"
#include "projectile.h"
#include "fx.h"
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
#include "ui_create.h"
#include "character.h"
#include "ui_font.h"
#include "ui_icons.h"
#include "ground.h"
#include "audio.h"
#include "ui_world.h"
#include "ui_hints.h"
#include "ui_tooltip.h"
#include "titles.h"
#include "render.h"

#define PLAYER_INDEX 0

typedef enum {
    APP_MENU,
    APP_CREATE,   // character creation, between the menu and the world
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
    Titles_Reset();
    g_entityCount = 0; // generation counters keep climbing, killing stale refs

    World_Init();
    if (loadSave) Save_LoadAndApply();
    Save_Enable();
    UI_ResetZoneTitle(); // a new game announces its starting zone again
    UI_ClearNotifications();

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
    UIIcons_Init();
    Ground_Init();
    Audio_Init();
    SkillDB_Init();
    // A sane character exists from the first frame, so Continue on a
    // save written before creation existed still has something valid.
    g_character = Character_Default();

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
        Audio_Update(dt);
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
                // A new game goes through creation first - the world is
                // built FROM the character, so it can't exist yet.
                UI_CreateReset();
                app = APP_CREATE;
            } else if (action == MENU_CONTINUE) {
                StartGame(true, &camera);
                app = APP_PLAYING;
            }
            continue;
        }

        // --- Character creation ---
        if (app == APP_CREATE) {
            // The creator is a SPATIAL screen - three columns, a grid of
            // colour swatches - so it gets the virtual cursor rather
            // than list focus: steering a pointer is the right verb for
            // picking a swatch, and stepping an index is not. This is
            // the only place the cursor is driven from outside
            // Input_Update, because APP_CREATE never reaches it.
            UICursor_Update(dt, true);
            BeginDrawing();
            CreateAction ca = UI_DrawCreateScreen(screenWidth, screenHeight, dt);
            UICursor_Draw(screenHeight);
            EndDrawing();
            if (ca == CREATE_CONFIRM) {
                StartGame(false, &camera);
                app = APP_PLAYING;
            } else if (ca == CREATE_CANCEL) {
                app = APP_MENU;
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
            Fx_Update(dt);

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

        // Nameplates, floating bars and the interact prompt: projected
        // from world positions but drawn at UI scale, so they stay crisp
        // and same-sized however far the camera is zoomed in.
        UIWorld_Draw(camera, screenWidth, screenHeight);

        // Screen-space UI from here down; every widget claims its rect
        // with UIHit so next frame's clicks stop at the UI instead of
        // falling through into the world.
        UIHit_NewFrame();

        // Zone name, top center - how GW1 tells you where you are - plus
        // the arrival card that announces a new area.
        UI_DrawZoneTitle(screenWidth, screenHeight, paused ? 0.0f : dt);

        UI_DrawControlHints(screenWidth, screenHeight);
        UI_DrawNotifications(screenWidth, screenHeight, paused ? 0.0f : dt);
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
            // Skill tooltips are requested by whichever widget the
            // pointer is over - the HUD bar, the build editor, the
            // trainer's list - and painted here so they land above all
            // of them rather than under the next window drawn.
            UITooltip_Flush(Entity_Get(PLAYER_INDEX), screenWidth, screenHeight);
            UICursor_Draw(screenHeight); // menu pointer, above everything it clicks
        } else {
            PauseAction pa = UI_DrawPauseMenu(screenWidth, screenHeight);
            // Every screen-opening entry unpauses on the way out: the
            // panel you asked for is only usable in the live game, so
            // the menu hands you straight to it instead of leaving you
            // to dismiss an overlay first.
            switch (pa) {
                case PAUSE_RESUME:
                    paused = false;
                    break;
                case PAUSE_OPEN_SKILLS:
                    UI_OpenPanel(PANEL_SKILLS);
                    paused = false;
                    break;
                case PAUSE_OPEN_EQUIPMENT:
                    UI_OpenPanel(PANEL_EQUIPMENT);
                    paused = false;
                    break;
                case PAUSE_OPEN_INVENTORY:
                    UI_OpenPanel(PANEL_INVENTORY);
                    paused = false;
                    break;
                case PAUSE_OPEN_ATTRIBUTES:
                    UI_OpenPanel(PANEL_ATTRIBUTES);
                    paused = false;
                    break;
                case PAUSE_OPEN_TITLES:
                    UI_OpenPanel(PANEL_TITLES);
                    paused = false;
                    break;
                case PAUSE_OPEN_MAP:
                    UI_OpenMapOverlay();
                    paused = false;
                    break;
                case PAUSE_VOLUME: {
                    // Steps 0 -> 25 -> 50 -> 75 -> 100 -> 0, snapping to
                    // the grid rather than adding 25 to whatever was
                    // loaded - otherwise a saved 70 walks off to 95. A
                    // single control that reaches silence is worth more
                    // here than a slider nobody can drag with a gamepad.
                    int v = (Audio_GetVolume() / 25 + 1) * 25;
                    Audio_SetVolume(v > 100 ? 0 : v);
                    Audio_Play(SFX_UI_CLICK); // audition the new level
                    Save_Write();
                    break; // stays paused, unlike every other entry
                }
                case PAUSE_QUIT_TO_MENU:
                    Save_Write();
                    paused = false;
                    app = APP_MENU;
                    break;
                case PAUSE_QUIT_GAME:
                    paused = false;
                    quitRequested = true; // final autosave runs after the loop
                    break;
                default:
                    break;
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

    UIIcons_Unload();
    Ground_Unload();
    Audio_Unload();
    CloseWindow();
    return 0;
}
