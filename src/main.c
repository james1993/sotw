#include "raylib.h"
#include "entity.h"
#include "skill.h"
#include "combat.h"
#include "ai_hero.h"
#include "input.h"
#include "items.h"
#include "world.h"
#include "quests.h"
#include "ui_skillbar.h"
#include "ui_target.h"
#include "ui_party.h"
#include "ui_panels.h"
#include "ui_font.h"
#include "render.h"

#define PLAYER_INDEX 0

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

    UIFont_Init();
    SkillDB_Init();
    World_Init(); // creates the player and loads Ashford Camp

    Camera2D camera = { 0 };
    camera.offset = (Vector2){ GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f };
    camera.target = Entity_Get(PLAYER_INDEX)->pos;
    // Default zoom scales with window width so the starting area fills a
    // comfortable fraction of the window on any resolution.
    camera.zoom = GetScreenWidth() / 700.0f;
    if (camera.zoom < MIN_CAMERA_ZOOM) camera.zoom = MIN_CAMERA_ZOOM;
    if (camera.zoom > MAX_CAMERA_ZOOM) camera.zoom = MAX_CAMERA_ZOOM;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        int screenWidth = GetScreenWidth();
        int screenHeight = GetScreenHeight();

        // Re-centered every frame so resizing the window (or moving it to
        // a different monitor) doesn't leave the camera offset stale.
        camera.offset = (Vector2){ screenWidth / 2.0f, screenHeight / 2.0f };

        Input_Update(&camera, dt);
        AI_Update(dt);
        Combat_TickTimers(dt);

        Entity *playerNow = Entity_Get(PLAYER_INDEX);
        World_Update(playerNow, dt);
        playerNow = Entity_Get(PLAYER_INDEX); // zone loads rebuild the array
        Items_UpdatePickup(playerNow);
        if (playerNow) camera.target = playerNow->pos;

        BeginDrawing();
        // Warm dirt tones in camp, cool grass tones in the plains.
        ClearBackground(World_GetMode() == MODE_OUTPOST
            ? (Color){ 24, 20, 14, 255 } : (Color){ 14, 22, 13, 255 });

        Render_World(camera);

        // Zone name, top center - how GW1 tells you where you are.
        {
            const char *zone = World_GetZoneName();
            int font = UI_ScaledFontSize(screenHeight, 18);
            int tw = UITextWidth(zone, font);
            UIText(zone, (screenWidth - tw) / 2, 14, font, (Color){ 220, 210, 180, 255 });
        }

        UI_DrawSkillBar(screenWidth, screenHeight);
        UI_DrawResourceBars(screenWidth, screenHeight);
        UI_DrawPartyPanel(screenWidth, screenHeight);
        UI_DrawTargetPanel(screenWidth, screenHeight, 50);
        Quests_DrawTracker(screenWidth, screenHeight);
        UI_PanelsUpdateAndDraw(screenWidth, screenHeight);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
