#include "ui_compass.h"
#include "entity.h"
#include "world.h"
#include "quests.h"
#include "ui_font.h"
#include "ui_hit.h"
#include <math.h>

#define PLAYER_INDEX 0
// How much world the compass shows: everything within this many units
// of the player maps onto the disc.
#define COMPASS_WORLD_RANGE 560.0f

static void Geometry(int screenWidth, int screenHeight, Vector2 *center, float *radius) {
    float scale = UI_Scale(screenHeight);
    *radius = 92.0f * scale;
    center->x = (float)screenWidth - *radius - 16.0f * scale;
    center->y = *radius + 38.0f * scale;
}

float UI_CompassBottom(int screenHeight) {
    float scale = UI_Scale(screenHeight);
    float radius = 92.0f * scale;
    return radius * 2.0f + 38.0f * scale;
}

// Maps a world position onto the compass. Returns false when out of range.
static bool WorldToCompass(Vector2 worldPos, Vector2 playerPos, Vector2 center,
                           float radius, float margin, Vector2 *out) {
    float k = radius / COMPASS_WORLD_RANGE;
    float dx = (worldPos.x - playerPos.x) * k;
    float dy = (worldPos.y - playerPos.y) * k;
    if (sqrtf(dx * dx + dy * dy) > radius - margin) return false;
    out->x = center.x + dx;
    out->y = center.y + dy;
    return true;
}

void UI_DrawCompass(int screenWidth, int screenHeight) {
    Entity *player = Entity_Get(PLAYER_INDEX);
    if (!player) return;

    Vector2 center;
    float radius;
    Geometry(screenWidth, screenHeight, &center, &radius);
    float k = radius / COMPASS_WORLD_RANGE;

    // Clicks on the compass are UI, not click-to-move.
    UIHit_Claim((Rectangle){ center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f });

    DrawCircleV(center, radius, (Color){ 14, 17, 23, 215 });
    DrawCircleLines((int)center.x, (int)center.y, radius, (Color){ 130, 130, 150, 255 });
    DrawCircleLines((int)center.x, (int)center.y, radius - 1.0f, (Color){ 80, 80, 95, 255 });

    // The aggro bubble, on the compass where GW1 keeps it.
    if (World_GetMode() == MODE_EXPLORABLE) {
        DrawCircleLines((int)center.x, (int)center.y, AGGRO_RING_RADIUS * k, (Color){ 220, 170, 60, 150 });
    }

    Vector2 p;

    // The instance boundary, dashed - sampled along each edge so only
    // the stretch inside compass range shows, like a radar wall.
    {
        Rectangle b = World_GetBounds();
        const float step = 30.0f;
        Color wall = { 190, 90, 70, 230 };
        for (float x = b.x; x <= b.x + b.width; x += step) {
            if (WorldToCompass((Vector2){ x, b.y }, player->pos, center, radius, 3.0f, &p))
                DrawRectangle((int)p.x - 1, (int)p.y - 1, 3, 3, wall);
            if (WorldToCompass((Vector2){ x, b.y + b.height }, player->pos, center, radius, 3.0f, &p))
                DrawRectangle((int)p.x - 1, (int)p.y - 1, 3, 3, wall);
        }
        for (float y = b.y; y <= b.y + b.height; y += step) {
            if (WorldToCompass((Vector2){ b.x, y }, player->pos, center, radius, 3.0f, &p))
                DrawRectangle((int)p.x - 1, (int)p.y - 1, 3, 3, wall);
            if (WorldToCompass((Vector2){ b.x + b.width, y }, player->pos, center, radius, 3.0f, &p))
                DrawRectangle((int)p.x - 1, (int)p.y - 1, 3, 3, wall);
        }
    }

    // Portal and shrine landmarks.
    for (int i = 0; i < World_GetPortalCount(); i++) {
        const ZonePortal *portal = World_GetPortal(i);
        if (WorldToCompass(portal->pos, player->pos, center, radius, 5.0f, &p)) {
            DrawRectangle((int)p.x - 3, (int)p.y - 3, 6, 6, (Color){ 110, 160, 255, 255 });
        }
    }
    Vector2 shrinePos;
    if (World_GetShrine(&shrinePos) &&
        WorldToCompass(shrinePos, player->pos, center, radius, 5.0f, &p)) {
        DrawRectangle((int)p.x - 3, (int)p.y - 3, 6, 6, (Color){ 200, 210, 235, 255 });
    }

    // Active reach-quest marker.
    for (int i = 0; i < QUEST_COUNT; i++) {
        const Quest *q = &g_quests[i];
        if (q->type != QTYPE_REACH || q->state != QUEST_ACTIVE) continue;
        if (q->targetZone != World_GetZoneId()) continue;
        if (WorldToCompass(q->targetPos, player->pos, center, radius, 5.0f, &p)) {
            DrawTriangle((Vector2){ p.x, p.y - 5 }, (Vector2){ p.x - 4, p.y + 3 },
                         (Vector2){ p.x + 4, p.y + 3 }, (Color){ 90, 220, 90, 255 });
        }
    }

    // Entities: allies green, NPCs pale green, monsters red (brighter
    // when awake and hunting). The player's current target gets a gold
    // ring so it reads on the compass too.
    int targetIdx = Entity_RefIndex(player->targetRef);
    for (int i = 0; i < g_entityCount; i++) {
        Entity *e = &g_entities[i];
        if (!e->alive || i == PLAYER_INDEX) continue;
        if (!WorldToCompass(e->pos, player->pos, center, radius, 4.0f, &p)) continue;

        Color c;
        if (e->kind == ENT_MONSTER) {
            c = e->aggroed ? (Color){ 255, 70, 60, 255 } : (Color){ 190, 60, 55, 255 };
        } else if (e->kind == ENT_NPC) {
            c = (Color){ 150, 220, 150, 255 };
        } else {
            c = (Color){ 80, 210, 90, 255 };
        }
        DrawCircleV(p, 3.5f, c);
        if (i == targetIdx) {
            DrawCircleLines((int)p.x, (int)p.y, 5.5f, GOLD);
        }
    }

    // The player, dead center.
    DrawCircleV(center, 4.0f, RAYWHITE);
    DrawCircleLines((int)center.x, (int)center.y, 4.0f, BLACK);
}
