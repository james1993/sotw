#include "input.h"
#include "entity.h"
#include "combat.h"

#define PLAYER_INDEX 0

void Input_Update(Camera2D camera) {
    Entity *player = Entity_Get(PLAYER_INDEX);
    if (!player || !player->alive) return;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mouseScreen = GetMousePosition();
        Vector2 world = GetScreenToWorld2D(mouseScreen, camera);

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
