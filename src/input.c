#include "input.h"
#include "entity.h"
#include "combat.h"
#include <math.h>
#include <stdbool.h>

#define PLAYER_INDEX 0
#define MAX_CLICK_MOVE_DISTANCE 2000.0f
#define GAMEPAD_ID 0
#define STICK_DEADZONE 0.25f
#define TRIGGER_AXIS_THRESHOLD 0.3f

// True while the player's most recent input came from the gamepad. In
// gamepad mode the game auto-targets the nearest foe in attack range
// every frame (so auto-attack "just happens" when you walk up to
// something, per the stick-driven control scheme) - but that same
// auto-targeting would fight mouse click-targeting, so it only runs
// while the pad, not the mouse, was touched last.
static bool g_gamepadMode = false;

static bool TriggerDown(int axisId, int buttonId) {
    if (IsGamepadButtonDown(GAMEPAD_ID, buttonId)) return true;
    // Analog triggers rest at -1 and reach +1 fully pressed on most
    // backends; anything meaningfully past the midpoint counts as held.
    return GetGamepadAxisMovement(GAMEPAD_ID, axisId) > TRIGGER_AXIS_THRESHOLD;
}

static void UpdateGamepad(Entity *player, float dt) {
    if (!IsGamepadAvailable(GAMEPAD_ID)) return;

    // --- Left stick: direct movement (not click-to-move) ---
    float lx = GetGamepadAxisMovement(GAMEPAD_ID, GAMEPAD_AXIS_LEFT_X);
    float ly = GetGamepadAxisMovement(GAMEPAD_ID, GAMEPAD_AXIS_LEFT_Y);
    float mag = sqrtf(lx * lx + ly * ly);
    if (mag > STICK_DEADZONE) {
        if (mag > 1.0f) { lx /= mag; ly /= mag; }
        // Casting roots the caster, same as the click-to-move path.
        if (!Entity_IsCasting(player)) {
            player->pos.x += lx * player->moveSpeed * dt;
            player->pos.y += ly * player->moveSpeed * dt;
            player->hasMoveTarget = false; // stick overrides any pending click-move
        }
        g_gamepadMode = true;
    }

    // --- Skills: L2 + A/B/X/Y = slots 1-4, R2 + A/B/X/Y = slots 5-8 ---
    // Xbox-layout naming: A = face down, B = face right, X = face left,
    // Y = face up.
    int face = -1;
    if (IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) face = 0;  // A
    else if (IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) face = 1; // B
    else if (IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_RIGHT_FACE_LEFT)) face = 2;  // X
    else if (IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_RIGHT_FACE_UP)) face = 3;    // Y

    if (face >= 0) {
        bool l2 = TriggerDown(GAMEPAD_AXIS_LEFT_TRIGGER, GAMEPAD_BUTTON_LEFT_TRIGGER_2);
        bool r2 = TriggerDown(GAMEPAD_AXIS_RIGHT_TRIGGER, GAMEPAD_BUTTON_RIGHT_TRIGGER_2);
        if (l2) {
            Combat_ActivateSkill(PLAYER_INDEX, face, player->targetIndex);
            g_gamepadMode = true;
        } else if (r2) {
            Combat_ActivateSkill(PLAYER_INDEX, 4 + face, player->targetIndex);
            g_gamepadMode = true;
        }
    }

    // --- Auto-targeting: nearest foe within attack range ---
    // Runs every frame in gamepad mode so auto-attack engages the moment
    // you walk into range and drops the moment you leave it. Deliberately
    // NOT sticky beyond attack range: the chase-your-target logic in
    // combat.c would otherwise wrestle the stick for control of the
    // player's position, and stick-driven kiting only works if walking
    // away actually disengages.
    if (g_gamepadMode) {
        int best = -1;
        float bestDist = 1e9f;
        for (int i = 0; i < g_entityCount; i++) {
            Entity *e = &g_entities[i];
            if (!e->alive || e->team == player->team) continue;
            float dx = e->pos.x - player->pos.x, dy = e->pos.y - player->pos.y;
            float d = sqrtf(dx * dx + dy * dy);
            if (d <= player->attackRange && d < bestDist) {
                bestDist = d;
                best = i;
            }
        }
        player->targetIndex = best;
    }
}

void Input_Update(Camera2D *camera, float dt) {
    // Scroll wheel zoom, independent of window size - this is the direct
    // fix for "things are too small to see" on a high-res display.
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
        camera->zoom += wheel * 0.1f;
        if (camera->zoom < MIN_CAMERA_ZOOM) camera->zoom = MIN_CAMERA_ZOOM;
        if (camera->zoom > MAX_CAMERA_ZOOM) camera->zoom = MAX_CAMERA_ZOOM;
    }

    Entity *player = Entity_Get(PLAYER_INDEX);
    if (!player || !player->alive) return;

    // Explicit manual override: always clears the current target/chase
    // order, regardless of what a click resolves to. A guaranteed way to
    // stop auto-chasing an out-of-range target without needing to land a
    // click that's certain to miss every entity.
    if (IsKeyPressed(KEY_ESCAPE)) {
        player->targetIndex = -1;
        player->hasMoveTarget = false;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        g_gamepadMode = false; // mouse takes back targeting control

        Vector2 mouseScreen = GetMousePosition();
        Vector2 world = GetScreenToWorld2D(mouseScreen, *camera);

        int clickedEntity = -1;
        for (int i = 0; i < g_entityCount; i++) {
            Entity *e = &g_entities[i];
            if (!e->alive || i == PLAYER_INDEX) continue;
            if (CheckCollisionPointCircle(world, e->pos, e->radius + 4.0f)) {
                clickedEntity = i;
                break;
            }
        }

        if (clickedEntity >= 0 && g_entities[clickedEntity].team != player->team) {
            player->targetIndex = clickedEntity;
        } else {
            // Defensive cap: bound how far a single click can send the
            // player. If screen-to-world coordinates are ever wrong (e.g.
            // a screen-size/mouse-position mismatch on some display
            // configurations), this stops it from becoming an
            // effectively endless, unrecoverable walk - worst case is an
            // obviously-too-far move you can immediately click to correct.
            Vector2 delta = { world.x - player->pos.x, world.y - player->pos.y };
            float dist = sqrtf(delta.x * delta.x + delta.y * delta.y);
            if (dist > MAX_CLICK_MOVE_DISTANCE) {
                float scale = MAX_CLICK_MOVE_DISTANCE / dist;
                world.x = player->pos.x + delta.x * scale;
                world.y = player->pos.y + delta.y * scale;
            }

            player->targetIndex = -1;
            player->moveTarget = world;
            player->hasMoveTarget = true;
        }
    }

    int keys[8] = { KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR, KEY_FIVE, KEY_SIX, KEY_SEVEN, KEY_EIGHT };
    for (int i = 0; i < 8; i++) {
        if (IsKeyPressed(keys[i])) {
            Combat_ActivateSkill(PLAYER_INDEX, i, player->targetIndex);
        }
    }

    UpdateGamepad(player, dt);
}
