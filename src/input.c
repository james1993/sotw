#include "input.h"
#include "entity.h"
#include "combat.h"
#include "ui_panels.h"
#include "ui_party.h"
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

// Set when a target was explicitly chosen on the D-pad (GW1's official
// gamepad scheme: up/down walks the party list, left/right cycles
// enemies). While a manual selection is live, the proximity auto-target
// leaves it alone.
static bool g_manualGamepadTarget = false;

static bool TriggerDown(int axisId, int buttonId) {
    if (IsGamepadButtonDown(GAMEPAD_ID, buttonId)) return true;
    // Analog triggers rest at -1 and reach +1 fully pressed on most
    // backends; anything meaningfully past the midpoint counts as held.
    return GetGamepadAxisMovement(GAMEPAD_ID, axisId) > TRIGGER_AXIS_THRESHOLD;
}

// D-pad up/down: walk the party list (player + heroes, entity order).
static void CyclePartyTarget(Entity *player, int direction) {
    int members[MAX_ENTITIES];
    int count = 0, currentPos = -1;
    for (int i = 0; i < g_entityCount; i++) {
        Entity *e = &g_entities[i];
        if (e->team != 0 || !e->alive) continue;
        if (e->kind != ENT_PLAYER && e->kind != ENT_HERO) continue;
        if (i == player->targetIndex) currentPos = count;
        members[count++] = i;
    }
    if (count == 0) return;
    int pos = (currentPos < 0) ? (direction > 0 ? 0 : count - 1)
                               : (currentPos + direction + count) % count;
    player->targetIndex = members[pos];
    g_manualGamepadTarget = true;
}

// D-pad left/right: cycle living foes, nearest first.
static void CycleFoeTarget(Entity *player, int direction) {
    int foes[MAX_ENTITIES];
    float dists[MAX_ENTITIES];
    int count = 0;
    for (int i = 0; i < g_entityCount; i++) {
        Entity *e = &g_entities[i];
        if (!e->alive || e->team == player->team) continue;
        float dx = e->pos.x - player->pos.x, dy = e->pos.y - player->pos.y;
        foes[count] = i;
        dists[count] = dx * dx + dy * dy;
        count++;
    }
    if (count == 0) return;
    // insertion sort by distance - the foe list is tiny
    for (int i = 1; i < count; i++) {
        int fi = foes[i]; float di = dists[i]; int j = i - 1;
        while (j >= 0 && dists[j] > di) { foes[j + 1] = foes[j]; dists[j + 1] = dists[j]; j--; }
        foes[j + 1] = fi; dists[j + 1] = di;
    }
    int currentPos = -1;
    for (int i = 0; i < count; i++) {
        if (foes[i] == player->targetIndex) currentPos = i;
    }
    int pos = (currentPos < 0) ? 0 : (currentPos + direction + count) % count;
    player->targetIndex = foes[pos];
    g_manualGamepadTarget = true;
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
        } else if (face == 1) {
            // Bare B (no trigger): drop the current target, GW1-gamepad's
            // escape hatch.
            player->targetIndex = -1;
            g_manualGamepadTarget = false;
            g_gamepadMode = true;
        }
    }

    // --- D-pad targeting, matching GW1's official gamepad scheme:
    // up/down selects party members, left/right cycles enemies. ---
    if (IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_LEFT_FACE_UP)) {
        CyclePartyTarget(player, -1);
        g_gamepadMode = true;
    }
    if (IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) {
        CyclePartyTarget(player, +1);
        g_gamepadMode = true;
    }
    if (IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_LEFT_FACE_LEFT)) {
        CycleFoeTarget(player, -1);
        g_gamepadMode = true;
    }
    if (IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) {
        CycleFoeTarget(player, +1);
        g_gamepadMode = true;
    }

    // A manual selection expires when the target dies.
    if (g_manualGamepadTarget) {
        Entity *t = Entity_Get(player->targetIndex);
        if (!t || !t->alive) {
            g_manualGamepadTarget = false;
            player->targetIndex = -1;
        }
    }

    // --- Auto-targeting: nearest foe within attack range ---
    // Runs every frame in gamepad mode so auto-attack engages the moment
    // you walk into range and drops the moment you leave it. Deliberately
    // NOT sticky beyond attack range: the chase-your-target logic in
    // combat.c would otherwise wrestle the stick for control of the
    // player's position, and stick-driven kiting only works if walking
    // away actually disengages. Suspended while a D-pad selection is
    // live so cycling to a specific target isn't instantly overwritten.
    if (g_gamepadMode && !g_manualGamepadTarget) {
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

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !UI_PointerOverPanels(GetMousePosition())
        && !UI_PartyPanelContains(GetMousePosition())) {
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
        } else if (clickedEntity >= 0 && g_entities[clickedEntity].kind == ENT_HERO) {
            // Clicking a party member selects them, so ally-targeted
            // spells (Orison) land on them instead of self-falling back.
            player->targetIndex = clickedEntity;
        } else if (clickedEntity >= 0 && g_entities[clickedEntity].kind == ENT_NPC) {
            // Outpost NPC: talk if close enough, otherwise walk over to
            // them (click again on arrival to open the conversation).
            Entity *npc = &g_entities[clickedEntity];
            float dx = npc->pos.x - player->pos.x, dy = npc->pos.y - player->pos.y;
            if (sqrtf(dx * dx + dy * dy) <= 90.0f) {
                UI_OpenNpcDialog(clickedEntity);
            } else {
                player->moveTarget = npc->pos;
                player->hasMoveTarget = true;
            }
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

    // GW1's classic keyboard targeting: C = nearest foe, Tab = cycle foes.
    if (IsKeyPressed(KEY_C)) {
        g_manualGamepadTarget = false;
        int save = player->targetIndex;
        player->targetIndex = -1; // force "nearest" rather than "next"
        CycleFoeTarget(player, +1);
        if (player->targetIndex < 0) player->targetIndex = save;
        g_manualGamepadTarget = false;
    }
    if (IsKeyPressed(KEY_TAB)) {
        CycleFoeTarget(player, +1);
        g_manualGamepadTarget = false;
    }

    int keys[8] = { KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR, KEY_FIVE, KEY_SIX, KEY_SEVEN, KEY_EIGHT };
    for (int i = 0; i < 8; i++) {
        if (IsKeyPressed(keys[i])) {
            Combat_ActivateSkill(PLAYER_INDEX, i, player->targetIndex);
        }
    }

    UpdateGamepad(player, dt);
}
