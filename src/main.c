#include "raylib.h"
#include "entity.h"
#include "attributes.h"
#include "skill.h"
#include "combat.h"
#include "ai_hero.h"
#include "input.h"
#include "ui_skillbar.h"
#include "ui_target.h"
#include "ui_party.h"
#include "render.h"

#define PLAYER_INDEX 0

int main(void) {
    // Deliberately NOT using FLAG_WINDOW_HIGHDPI: on displays with OS-level
    // scaling it can make GetScreenWidth()/GetScreenHeight() disagree in
    // scale with what GetMousePosition() actually reports, which is a
    // very likely explanation for click-to-move sending the player to a
    // wildly wrong, consistently-offset world position - every click
    // landing "way off in one direction" rather than where the cursor
    // actually was. Rendering may be a little softer on Retina/4K
    // displays without it, but coordinates staying consistent matters
    // far more than crispness right now.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1600, 900, "Guild Wars 1 2D Demake - Prototype");

    // Ask the OS/window manager to maximize the window - this is the only
    // sizing mechanism used, because computing a size ourselves from
    // GetMonitorWidth/Height is unreliable: that call returns *physical*
    // pixels, while SetWindowSize expects *logical* points on any display
    // using OS-level scaling (Windows DPI scaling, macOS Retina). Mixing
    // the two previously produced a window sized far larger than the
    // real desktop, which is what made everything look zoomed out and
    // pushed the skill bar off the bottom of the visible window.
    // Maximizing delegates all of that unit conversion to the OS, so
    // there's no manual pixel math to get wrong. If no window manager is
    // present to honor the request (e.g. a bare X server), the window
    // simply stays at the 1600x900 fallback above - resizable, so it can
    // still be dragged bigger by hand.
    SetWindowState(FLAG_WINDOW_MAXIMIZED);

    SetWindowMinSize(960, 600);
    SetTargetFPS(60);

    SkillDB_Init();

    // Player: Warrior primary / Elementalist secondary, matching the
    // dual-profession rule from docs/research/gw1-mechanics.md #4 -
    // Strength/Tactics are full-strength primary attributes here.
    int playerIdx = Entity_Spawn(ENT_PLAYER, "Player (War/Ele)", 0, (Vector2){ 0, 0 }, (Color){ 200, 60, 60, 255 });
    Entity *player = Entity_Get(playerIdx);
    player->primaryProfession = 0;
    player->attributeRank[ATTR_STRENGTH] = 10;
    player->attributeRank[ATTR_TACTICS] = 6;
    player->maxHp = player->hp = 150;
    player->maxEnergy = player->energy = 20;
    player->skillBar[0] = 0; // Gash
    player->skillBar[1] = 1; // Rush Strike
    player->skillBar[2] = 2; // Battle Cry
    player->skillBar[3] = 3; // Deathblow (elite)
    player->skillBar[4] = 9; // Distracting Blow (interrupt)

    // Hero: player-configured AI companion, Elementalist primary -
    // demonstrating the hero system from docs/research/gw1-mechanics.md #10.
    int heroIdx = Entity_Spawn(ENT_HERO, "Hero (Ele)", 0, (Vector2){ -40, 40 }, (Color){ 60, 120, 220, 255 });
    Entity *hero = Entity_Get(heroIdx);
    hero->primaryProfession = 2;
    hero->attributeRank[ATTR_FIRE_MAGIC] = 11;
    hero->attributeRank[ATTR_ENERGY_STORAGE] = 9;
    hero->maxEnergy = hero->energy = 50;
    hero->attackRange = 220.0f; // caster keeps distance
    hero->skillBar[0] = 4; // Fire Bolt
    hero->skillBar[1] = 5; // Cinder Storm
    hero->skillBar[2] = 6; // Mind Sear (energy management)

    // A loose group of three, spaced further apart than any one monster's
    // aggro range - approaching one doesn't automatically wake the others,
    // so pulling them one at a time (rather than fighting all three at
    // once) is a real, spatially-legible option. See
    // docs/research/gw1-mechanics.md #7 and ai_hero.c.
    int monsterIdx = Entity_Spawn(ENT_MONSTER, "Charr Brute", 1, (Vector2){ 260, 20 }, (Color){ 90, 90, 90, 255 });
    Entity *monster = Entity_Get(monsterIdx);
    monster->attributeRank[ATTR_STRENGTH] = 8;
    monster->maxHp = monster->hp = 220;
    monster->maxEnergy = monster->energy = 20;
    monster->aggroRange = 130.0f;
    monster->leashRange = 320.0f;
    // Feral Howl first in priority so the AI actually casts it whenever
    // it's up, rather than Claw Swipe (always available once adrenaline
    // is full) crowding it out - see ai_hero.c's priority-order scan.
    monster->skillBar[0] = 10; // Feral Howl (self-heal - interrupt it!)
    monster->skillBar[1] = 8;  // Claw Swipe

    int gruntAIdx = Entity_Spawn(ENT_MONSTER, "Charr Grunt", 1, (Vector2){ 440, 110 }, (Color){ 100, 90, 80, 255 });
    Entity *gruntA = Entity_Get(gruntAIdx);
    gruntA->attributeRank[ATTR_STRENGTH] = 6;
    gruntA->maxHp = gruntA->hp = 140;
    gruntA->aggroRange = 120.0f;
    gruntA->leashRange = 300.0f;
    gruntA->skillBar[0] = 8; // Claw Swipe

    int gruntBIdx = Entity_Spawn(ENT_MONSTER, "Charr Grunt", 1, (Vector2){ 400, -130 }, (Color){ 100, 90, 80, 255 });
    Entity *gruntB = Entity_Get(gruntBIdx);
    gruntB->attributeRank[ATTR_STRENGTH] = 6;
    gruntB->maxHp = gruntB->hp = 140;
    gruntB->aggroRange = 120.0f;
    gruntB->leashRange = 300.0f;
    gruntB->skillBar[0] = 8; // Claw Swipe

    Camera2D camera = { 0 };
    camera.offset = (Vector2){ GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f };
    camera.target = player->pos;
    // Default zoom scales with window width instead of a flat 1.0, so the
    // starting skirmish fills a consistent, comfortable fraction of the
    // window on any resolution - a flat zoom looked fine on the 1280x800
    // window it was tuned on, but left everything looking tiny and
    // distant on a large/high-res window.
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
        if (playerNow) camera.target = playerNow->pos;

        BeginDrawing();
        ClearBackground((Color){ 15, 15, 20, 255 });

        Render_World(camera);
        UI_DrawSkillBar(screenWidth, screenHeight);
        UI_DrawResourceBars(screenWidth, screenHeight);
        UI_DrawPartyPanel(screenWidth, screenHeight);

        int uiFontSize = UI_ScaledFontSize(screenHeight, 16);
        DrawText("Left-click ground to move, left-click an enemy to target/auto-attack.", 20, 20, uiFontSize, LIGHTGRAY);
        DrawText("Keys 1-5: your skill bar (5 = interrupt). Scroll wheel to zoom. Escape clears target.", 20, 20 + uiFontSize + 4, uiFontSize, LIGHTGRAY);
        DrawText("The circle around you is your aggro bubble - sleeping monsters inside it wake up. Pull one at a time.", 20, 20 + 2 * (uiFontSize + 4), uiFontSize, LIGHTGRAY);
        DrawFPS(screenWidth - 90, 10);

        // Target panel starts below the help text so the two never overlap.
        UI_DrawTargetPanel(screenWidth, screenHeight, 20 + 3 * (uiFontSize + 4) + 16);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
