#include "render.h"
#include "entity.h"
#include <math.h>
#include <stddef.h>

#define PLAYER_INDEX 0

// Basic environmental art: a fixed, hand-placed scatter of trees/rocks/
// grass tufts built from primitive shapes, since the prototype has no
// sprite assets yet. Purely decorative - no collision or LoS blocking.
typedef enum { PROP_TREE, PROP_ROCK, PROP_GRASS } PropType;
typedef struct { Vector2 pos; PropType type; float scale; } EnvProp;

static const EnvProp g_envProps[] = {
    { { -320, -160 }, PROP_TREE, 1.1f },
    { { -260, -230 }, PROP_TREE, 0.85f },
    { { -140, -260 }, PROP_TREE, 1.0f },
    { { 120, -260 }, PROP_TREE, 0.9f },
    { { 340, -180 }, PROP_TREE, 1.2f },
    { { 420, -60 }, PROP_TREE, 0.8f },
    { { 400, 160 }, PROP_TREE, 1.0f },
    { { 300, 260 }, PROP_TREE, 0.9f },
    { { -80, 260 }, PROP_TREE, 1.1f },
    { { -300, 200 }, PROP_TREE, 0.85f },
    { { -420, 40 }, PROP_TREE, 1.0f },
    { { -220, -60 }, PROP_ROCK, 1.0f },
    { { -160, 120 }, PROP_ROCK, 0.8f },
    { { 140, -80 }, PROP_ROCK, 1.1f },
    { { 380, 40 }, PROP_ROCK, 0.9f },
    { { 60, 180 }, PROP_ROCK, 0.7f },
    { { -60, -160 }, PROP_ROCK, 0.9f },
    { { 200, 140 }, PROP_GRASS, 1.0f },
    { { -180, 20 }, PROP_GRASS, 0.9f },
    { { 100, -40 }, PROP_GRASS, 1.1f },
    { { -100, 140 }, PROP_GRASS, 0.8f },
    { { 320, -20 }, PROP_GRASS, 1.0f },
    { { -20, 220 }, PROP_GRASS, 0.9f },
    { { 220, -160 }, PROP_GRASS, 1.0f },
};
#define ENV_PROP_COUNT (int)(sizeof(g_envProps) / sizeof(g_envProps[0]))

static void DrawTree(Vector2 pos, float scale) {
    DrawRectangle((int)(pos.x - 3 * scale), (int)(pos.y - 2 * scale), (int)(6 * scale), (int)(14 * scale),
        (Color){ 90, 60, 40, 255 });
    DrawCircleV((Vector2){ pos.x - 8 * scale, pos.y - 12 * scale }, 11 * scale, (Color){ 42, 95, 52, 255 });
    DrawCircleV((Vector2){ pos.x + 9 * scale, pos.y - 14 * scale }, 12 * scale, (Color){ 36, 82, 46, 255 });
    DrawCircleV((Vector2){ pos.x, pos.y - 20 * scale }, 15 * scale, (Color){ 46, 100, 56, 255 });
}

static void DrawRock(Vector2 pos, float scale) {
    DrawCircleV((Vector2){ pos.x + 5 * scale, pos.y + 3 * scale }, 7 * scale, (Color){ 62, 62, 67, 255 });
    DrawCircleV(pos, 10 * scale, (Color){ 82, 82, 88, 255 });
    DrawCircleLines((int)pos.x, (int)pos.y, 10 * scale, (Color){ 40, 40, 45, 255 });
}

static void DrawGrassTuft(Vector2 pos, float scale) {
    for (int i = 0; i < 4; i++) {
        float angle = -0.5f + i * 0.33f;
        Vector2 tip = { pos.x + sinf(angle) * 6 * scale, pos.y - 10 * scale };
        DrawLineEx(pos, tip, 2.0f, (Color){ 60, 115, 62, 255 });
    }
}

static void DrawEnvironment(void) {
    for (int i = 0; i < ENV_PROP_COUNT; i++) {
        const EnvProp *p = &g_envProps[i];
        switch (p->type) {
            case PROP_TREE: DrawTree(p->pos, p->scale); break;
            case PROP_ROCK: DrawRock(p->pos, p->scale); break;
            case PROP_GRASS: DrawGrassTuft(p->pos, p->scale); break;
        }
    }
}

static void DrawHealthBar(const Entity *e) {
    float w = 30.0f, h = 4.0f;
    Vector2 pos = { e->pos.x - w / 2, e->pos.y - e->radius - 14.0f };
    float pct = (e->maxHp > 0) ? (float)e->hp / (float)e->maxHp : 0.0f;
    DrawRectangle((int)pos.x, (int)pos.y, (int)w, (int)h, DARKGRAY);
    DrawRectangle((int)pos.x, (int)pos.y, (int)(w * pct), (int)h, (e->team == 0) ? GREEN : RED);
    DrawRectangleLines((int)pos.x, (int)pos.y, (int)w, (int)h, BLACK);
}

static void DrawCastBar(const Entity *e) {
    if (!Entity_IsCasting(e)) return;
    float w = 30.0f, h = 4.0f;
    Vector2 pos = { e->pos.x - w / 2, e->pos.y - e->radius - 20.0f };
    float pct = (e->castTimeTotal > 0.0f) ? 1.0f - (e->castTimeRemaining / e->castTimeTotal) : 0.0f;
    DrawRectangle((int)pos.x, (int)pos.y, (int)w, (int)h, DARKGRAY);
    DrawRectangle((int)pos.x, (int)pos.y, (int)(w * pct), (int)h, SKYBLUE);
    DrawRectangleLines((int)pos.x, (int)pos.y, (int)w, (int)h, BLACK);
}

void Render_World(Camera2D camera) {
    BeginMode2D(camera);

    // Grid covers whatever is actually visible, computed from the
    // camera/screen instead of a fixed world-unit range. A hardcoded
    // range only fills the window at the specific zoom/resolution it was
    // tuned for - on a bigger window or when zoomed out, a fixed range
    // left the game world as a small square surrounded by blank space.
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();
    Vector2 topLeft = GetScreenToWorld2D((Vector2){ 0, 0 }, camera);
    Vector2 bottomRight = GetScreenToWorld2D((Vector2){ (float)screenWidth, (float)screenHeight }, camera);

    const int gridSpacing = 40;
    int startX = ((int)floorf(topLeft.x / gridSpacing) - 1) * gridSpacing;
    int endX = ((int)ceilf(bottomRight.x / gridSpacing) + 1) * gridSpacing;
    int startY = ((int)floorf(topLeft.y / gridSpacing) - 1) * gridSpacing;
    int endY = ((int)ceilf(bottomRight.y / gridSpacing) + 1) * gridSpacing;

    for (int x = startX; x <= endX; x += gridSpacing) {
        DrawLine(x, startY, x, endY, (Color){ 60, 60, 60, 255 });
    }
    for (int y = startY; y <= endY; y += gridSpacing) {
        DrawLine(startX, y, endX, y, (Color){ 60, 60, 60, 255 });
    }

    DrawEnvironment();

    // Faint aggro-range circle around idle monsters, so the new pull
    // mechanic is legible while learning it (real GW1 famously doesn't
    // show this, which is exactly why pulling is a "feel" skill there -
    // but for a demake introducing the mechanic, showing the boundary is
    // worth the trade-off).
    for (int i = 0; i < g_entityCount; i++) {
        Entity *e = &g_entities[i];
        if (!e->alive || e->kind != ENT_MONSTER || e->aggroed) continue;
        DrawCircleLines((int)e->spawnPos.x, (int)e->spawnPos.y, e->aggroRange, (Color){ 220, 170, 60, 70 });
    }

    // Ring under the player's current target, so it's obvious at a glance
    // which entity the target panel/skill-bar actions apply to.
    Entity *player = Entity_Get(PLAYER_INDEX);
    Entity *currentTarget = player ? Entity_Get(player->targetIndex) : NULL;
    if (currentTarget && currentTarget->alive) {
        DrawCircleLines((int)currentTarget->pos.x, (int)currentTarget->pos.y, currentTarget->radius + 6.0f, GOLD);
        DrawCircleLines((int)currentTarget->pos.x, (int)currentTarget->pos.y, currentTarget->radius + 7.5f, GOLD);
    }

    for (int i = 0; i < g_entityCount; i++) {
        Entity *e = &g_entities[i];
        if (!e->alive) continue;

        DrawCircleV(e->pos, e->radius, e->color);
        DrawCircleLines((int)e->pos.x, (int)e->pos.y, e->radius, BLACK);
        DrawHealthBar(e);
        DrawCastBar(e);

        if (e->interruptFlashTimer > 0.0f) {
            const char *label = "INTERRUPTED";
            int tw = MeasureText(label, 10);
            DrawText(label, (int)(e->pos.x - tw / 2), (int)(e->pos.y - e->radius - 34.0f), 10, GOLD);
        }

        int textWidth = MeasureText(e->name, 10);
        DrawText(e->name, (int)(e->pos.x - textWidth / 2), (int)(e->pos.y + e->radius + 4), 10, RAYWHITE);
    }

    EndMode2D();
}
