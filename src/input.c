#include "input.h"
#include "entity.h"
#include "combat.h"
#include <math.h>

#define PLAYER_INDEX 0
#define MIN_ZOOM 0.5f
#define MAX_ZOOM 3.0f
#define MAX_CLICK_MOVE_DISTANCE 800.0f

void Input_Update(Camera2D *camera) {
    // Scroll wheel zoom, independent of window size - this is the direct
    // fix for "things are too small to see" on a high-res display.
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
        camera->zoom += wheel * 0.1f;
        if (camera->zoom < MIN_ZOOM) camera->zoom = MIN_ZOOM;
        if (camera->zoom > MAX_ZOOM) camera->zoom = MAX_ZOOM;
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
}
