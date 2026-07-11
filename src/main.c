#include "raylib.h"
#include "entity.h"
#include "attributes.h"
#include "skill.h"
#include "combat.h"
#include "ai_hero.h"
#include "input.h"
#include "items.h"
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

    // Player: Monk primary / Elementalist secondary. Monk skills run on
    // energy only - no adrenaline mechanic anywhere on this bar - and
    // Divine Favor (the Monk primary attribute) adds bonus healing per
    // rank, which only a primary Monk gets. Fire Bolt comes from the
    // Elementalist secondary; Energy Storage stays inaccessible because
    // it's the Ele PRIMARY attribute (docs/research/gw1-mechanics.md #4).
    int playerIdx = Entity_Spawn(ENT_PLAYER, "Player (Mo/E)", 0, (Vector2){ 0, 0 }, (Color){ 220, 200, 120, 255 });
    Entity *player = Entity_Get(playerIdx);
    player->primaryProfession = PROF_MONK;
    player->secondaryProfession = PROF_ELEMENTALIST;
    player->level = 5;
    player->maxHp = player->hp = 100 + 20 * (player->level - 1); // GW1: +20 HP per level
    player->maxEnergy = player->energy = 30;
    // Level 5 grants 20 attribute points (5 per level-up); these ranks
    // spend 17 of them per the GW1 cost table, leaving 3 free to place
    // in the attributes panel (K).
    player->attributeRank[ATTR_HEALING_PRAYERS] = 4;
    player->attributeRank[ATTR_SMITING_PRAYERS] = 3;
    player->attributeRank[ATTR_DIVINE_FAVOR] = 1;
    player->attributePoints = 3;
    player->skillBar[0] = 11; // Orison of Healing
    player->skillBar[1] = 12; // Banish
    player->skillBar[2] = 13; // Smite
    player->skillBar[3] = 14; // Bane Signet
    player->skillBar[4] = 4;  // Fire Bolt (Elementalist secondary)

    // Starting equipment, GW1-style fixed-power items: a max-damage wand
    // and low-tier armor. Both live in the inventory (I) and are
    // equipped, driving attack stats and damage mitigation.
    Item startRod = { ITEM_WEAPON, "Smiting Rod", 11, 22, 160.0f, 1.75f, 0 };
    Item startRaiment = { ITEM_ARMOR, "Monk Raiment (AL 30)", 0, 0, 0, 0, 30 };
    Items_AddToInventory(startRod);
    Items_AddToInventory(startRaiment);
    Items_EquipWeapon(player, 0);
    Items_EquipArmor(player, 1);

    // Hero: Vekk, the Asuran Elementalist - a proper GW1 hero name for a
    // proper GW1 hero (docs/research/gw1-mechanics.md #10).
    int heroIdx = Entity_Spawn(ENT_HERO, "Vekk", 0, (Vector2){ -40, 40 }, (Color){ 60, 120, 220, 255 });
    Entity *hero = Entity_Get(heroIdx);
    hero->primaryProfession = PROF_ELEMENTALIST;
    hero->secondaryProfession = PROF_MONK;
    hero->level = 5;
    hero->maxHp = hero->hp = 100 + 20 * (hero->level - 1);
    hero->maxEnergy = hero->energy = 50;
    hero->attackRange = 220.0f; // caster keeps distance
    hero->attributeRank[ATTR_FIRE_MAGIC] = 4;
    hero->attributeRank[ATTR_ENERGY_STORAGE] = 3;
    hero->skillBar[0] = 4; // Fire Bolt
    hero->skillBar[1] = 5; // Cinder Storm
    hero->skillBar[2] = 6; // Mind Sear (energy management)

    // A loose group of three, spaced further apart than any one monster's
    // aggro range, so pulling them one at a time is a real option.
    int monsterIdx = Entity_Spawn(ENT_MONSTER, "Charr Brute", 1, (Vector2){ 260, 20 }, (Color){ 90, 90, 90, 255 });
    Entity *monster = Entity_Get(monsterIdx);
    monster->attributeRank[ATTR_STRENGTH] = 8;
    monster->level = 5;
    monster->maxHp = monster->hp = 220;
    monster->maxEnergy = monster->energy = 20;
    monster->armor = 60;
    monster->aggroRange = 130.0f;
    monster->leashRange = 320.0f;
    // Feral Howl first in priority so the AI actually casts it whenever
    // it's up - see ai_hero.c's priority-order scan.
    monster->skillBar[0] = 10; // Feral Howl (self-heal - interrupt it!)
    monster->skillBar[1] = 8;  // Claw Swipe

    int gruntAIdx = Entity_Spawn(ENT_MONSTER, "Charr Grunt", 1, (Vector2){ 440, 110 }, (Color){ 100, 90, 80, 255 });
    Entity *gruntA = Entity_Get(gruntAIdx);
    gruntA->attributeRank[ATTR_STRENGTH] = 6;
    gruntA->level = 2;
    gruntA->maxHp = gruntA->hp = 140;
    gruntA->armor = 40;
    gruntA->aggroRange = 120.0f;
    gruntA->leashRange = 300.0f;
    gruntA->skillBar[0] = 8; // Claw Swipe

    int gruntBIdx = Entity_Spawn(ENT_MONSTER, "Charr Grunt", 1, (Vector2){ 400, -130 }, (Color){ 100, 90, 80, 255 });
    Entity *gruntB = Entity_Get(gruntBIdx);
    gruntB->attributeRank[ATTR_STRENGTH] = 6;
    gruntB->level = 2;
    gruntB->maxHp = gruntB->hp = 140;
    gruntB->armor = 40;
    gruntB->aggroRange = 120.0f;
    gruntB->leashRange = 300.0f;
    gruntB->skillBar[0] = 8; // Claw Swipe

    Camera2D camera = { 0 };
    camera.offset = (Vector2){ GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f };
    camera.target = player->pos;
    // Default zoom scales with window width so the starting skirmish
    // fills a comfortable fraction of the window on any resolution.
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
        Items_UpdatePickup(playerNow);
        if (playerNow) camera.target = playerNow->pos;

        BeginDrawing();
        ClearBackground((Color){ 15, 15, 20, 255 });

        Render_World(camera);
        UI_DrawSkillBar(screenWidth, screenHeight);
        UI_DrawResourceBars(screenWidth, screenHeight);
        UI_DrawPartyPanel(screenWidth, screenHeight);
        UI_DrawTargetPanel(screenWidth, screenHeight, 20);
        UI_PanelsUpdateAndDraw(screenWidth, screenHeight);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
