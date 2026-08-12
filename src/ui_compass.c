#include "ui_compass.h"
#include "entity.h"
#include "world.h"
#include "quests.h"
#include "ai_hero.h"
#include "mapdraw.h"
#include "ui_font.h"
#include "ui_hit.h"
#include "ui_theme.h"
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
    float scale = UI_Scale(screenHeight);

    // Clicks on the compass are UI, not click-to-move.
    UIHit_Claim((Rectangle){ center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f });

    // Dished glass under a gilt bezel. A flat disc with a hairline
    // outline read as a placeholder; the falloff and ring give the
    // compass the weight of an actual instrument, matching the gold
    // trim the panels use.
    DrawCircleV(center, radius, (Color){ 10, 12, 17, 225 });
    DrawCircleGradient((int)center.x, (int)center.y, radius,
                       (Color){ 40, 48, 62, 90 }, (Color){ 6, 8, 12, 30 });

    // Faint bearing lines, so movement across the disc has a reference.
    DrawLine((int)(center.x - radius + 6), (int)center.y,
             (int)(center.x + radius - 6), (int)center.y, (Color){ 90, 96, 112, 45 });
    DrawLine((int)center.x, (int)(center.y - radius + 6),
             (int)center.x, (int)(center.y + radius - 6), (Color){ 90, 96, 112, 45 });

    DrawRing(center, radius, radius + 4.0f * scale, 0, 360, 64, (Color){ 26, 24, 20, 240 });
    DrawCircleLines((int)center.x, (int)center.y, radius, UI_GOLD);
    DrawCircleLines((int)center.x, (int)center.y, radius + 4.0f * scale, UI_GOLD_DIM);

    // Four cardinal ticks on the bezel.
    for (int i = 0; i < 4; i++) {
        float a = i * (PI / 2.0f);
        Vector2 in = { center.x + cosf(a) * radius, center.y + sinf(a) * radius };
        Vector2 out = { center.x + cosf(a) * (radius + 4.0f * scale),
                        center.y + sinf(a) * (radius + 4.0f * scale) };
        DrawLineEx(in, out, 2.0f, UI_GOLD);
    }

    // The aggro bubble, on the compass where GW1 keeps it.
    if (World_GetMode() == MODE_EXPLORABLE) {
        DrawCircleLines((int)center.x, (int)center.y, AGGRO_RING_RADIUS * k, (Color){ 220, 170, 60, 150 });
    }

    Vector2 p;

    // The zone edge, plotted as the terrain that actually forms it: one
    // stone dot per rock mass in the ridge. It used to be a red dashed
    // RECTANGLE, which both looked like a debug overlay and described a
    // shape the zone no longer has.
    {
        Color stone = { 128, 130, 140, 225 };
        for (int i = 0; i < World_GetBarrierCount(); i++) {
            const ZoneBarrier *b = World_GetBarrier(i);
            if (WorldToCompass(b->pos, player->pos, center, radius, 3.0f, &p)) {
                DrawRectangle((int)p.x - 1, (int)p.y - 1, 3, 3, stone);
            }
        }
    }

    // Freehand drawings you scribble on the compass. The breadcrumb trail
    // of where you've walked is deliberately NOT drawn here - like GW1, it
    // lives only on the full map (U). Both are world-space, so
    // WorldToCompass clips them to the disc for free.
    {
        Vector2 a, b;
        float ttl = MapDraw_StrokeTTL();
        for (int i = 1; i < MapDraw_StrokeCount(); i++) {
            const MapStrokePt *s0 = MapDraw_StrokeAt(i - 1), *s1 = MapDraw_StrokeAt(i);
            if (s0->stroke != s1->stroke) continue;
            if (WorldToCompass(s0->pos, player->pos, center, radius, 2.0f, &a) &&
                WorldToCompass(s1->pos, player->pos, center, radius, 2.0f, &b)) {
                float f = 1.0f - s0->age / ttl; if (f < 0.0f) f = 0.0f;
                DrawLineEx(a, b, 2.0f, (Color){ 240, 210, 90, (unsigned char)(220 * f) });
            }
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

    // Active reach-quest marker. In range it sits on the target; out of
    // range it becomes an arrow at the rim pointing the way, so the
    // compass always answers "which way is my next step?".
    for (int i = 0; i < QUEST_COUNT; i++) {
        const Quest *q = &g_quests[i];
        if (q->type != QTYPE_REACH || q->state != QUEST_ACTIVE) continue;
        if (q->targetZone != World_GetZoneId()) continue;
        if (WorldToCompass(q->targetPos, player->pos, center, radius, 5.0f, &p)) {
            DrawTriangle((Vector2){ p.x, p.y - 5 }, (Vector2){ p.x - 4, p.y + 3 },
                         (Vector2){ p.x + 4, p.y + 3 }, (Color){ 90, 220, 90, 255 });
        } else {
            float a = atan2f(q->targetPos.y - player->pos.y, q->targetPos.x - player->pos.x);
            Vector2 tip = { center.x + cosf(a) * (radius - 3.0f), center.y + sinf(a) * (radius - 3.0f) };
            Vector2 bl  = { center.x + cosf(a + 2.5f) * (radius - 11.0f),
                            center.y + sinf(a + 2.5f) * (radius - 11.0f) };
            Vector2 br  = { center.x + cosf(a - 2.5f) * (radius - 11.0f),
                            center.y + sinf(a - 2.5f) * (radius - 11.0f) };
            DrawTriangle(tip, bl, br, (Color){ 90, 220, 90, 255 });
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

    // Party flag (GW1): click anywhere on the compass to send the party to
    // that spot and hold; click your own dot at the centre to recall them.
    // Only in the field - in town the party stands put regardless.
    if (World_GetMode() == MODE_EXPLORABLE) {
        Vector2 mp = GetMousePosition();
        Vector2 md = { mp.x - center.x, mp.y - center.y };
        float mdist = sqrtf(md.x * md.x + md.y * md.y);
        if (mdist <= radius && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (mdist < 10.0f) {
                AI_ClearPartyFlag();
            } else {
                AI_SetPartyFlag((Vector2){ player->pos.x + md.x / k, player->pos.y + md.y / k });
            }
        }
        Vector2 flag;
        if (AI_PartyFlagActive(&flag) &&
            WorldToCompass(flag, player->pos, center, radius, 4.0f, &p)) {
            DrawLineEx((Vector2){ p.x, p.y + 5 }, (Vector2){ p.x, p.y - 7 }, 1.5f,
                       (Color){ 235, 225, 120, 255 });
            DrawRectangle((int)p.x, (int)p.y - 7, 7, 5, (Color){ 235, 205, 60, 255 });
        }
    }

    // Draw on the compass with the RIGHT button (left is the party flag):
    // hold and drag to scribble a route or a warning, GW1's map drawing.
    {
        Vector2 mp = GetMousePosition();
        Vector2 md = { mp.x - center.x, mp.y - center.y };
        if (sqrtf(md.x * md.x + md.y * md.y) <= radius) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) MapDraw_BeginStroke();
            if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
                MapDraw_AddPoint((Vector2){ player->pos.x + md.x / k, player->pos.y + md.y / k });
        }
    }

    // The player, dead center.
    DrawCircleV(center, 4.0f, RAYWHITE);
    DrawCircleLines((int)center.x, (int)center.y, 4.0f, BLACK);
}
