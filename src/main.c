#include "raylib.h"
#include "entity.h"
#include "attributes.h"
#include "skill.h"
#include "combat.h"
#include "ai_hero.h"
#include "input.h"
#include "ui_skillbar.h"
#include "render.h"

#define PLAYER_INDEX 0

int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI | FLAG_VSYNC_HINT);
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

    int monsterIdx = Entity_Spawn(ENT_MONSTER, "Charr Brute", 1, (Vector2){ 260, 20 }, (Color){ 90, 90, 90, 255 });
    Entity *monster = Entity_Get(monsterIdx);
    monster->attributeRank[ATTR_STRENGTH] = 8;
    monster->maxHp = monster->hp = 220;
    monster->skillBar[0] = 8; // Claw Swipe

    Camera2D camera = { 0 };
    camera.offset = (Vector2){ GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f };
    camera.target = player->pos;
    camera.zoom = 1.0f;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        int screenWidth = GetScreenWidth();
        int screenHeight = GetScreenHeight();

        // Re-centered every frame so resizing the window (or moving it to
        // a different monitor) doesn't leave the camera offset stale.
        camera.offset = (Vector2){ screenWidth / 2.0f, screenHeight / 2.0f };

        Input_Update(&camera);
        AI_Update(dt);
        Combat_TickTimers(dt);

        Entity *playerNow = Entity_Get(PLAYER_INDEX);
        if (playerNow) camera.target = playerNow->pos;

        BeginDrawing();
        ClearBackground((Color){ 15, 15, 20, 255 });

        Render_World(camera);
        UI_DrawSkillBar(screenWidth, screenHeight);
        UI_DrawResourceBars(screenWidth, screenHeight);

        int uiFontSize = UI_ScaledFontSize(screenHeight, 16);
        DrawText("Left-click ground to move, left-click an enemy to target/auto-attack.", 20, 20, uiFontSize, LIGHTGRAY);
        DrawText("Keys 1-4: your skill bar. Scroll wheel to zoom. Escape clears your target.", 20, 20 + uiFontSize + 4, uiFontSize, LIGHTGRAY);
        DrawFPS(screenWidth - 90, 10);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
