#include "render.h"
#include "entity.h"
#include <math.h>

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

    for (int i = 0; i < g_entityCount; i++) {
        Entity *e = &g_entities[i];
        if (!e->alive) continue;

        DrawCircleV(e->pos, e->radius, e->color);
        DrawCircleLines((int)e->pos.x, (int)e->pos.y, e->radius, BLACK);
        DrawHealthBar(e);
        DrawCastBar(e);

        int textWidth = MeasureText(e->name, 10);
        DrawText(e->name, (int)(e->pos.x - textWidth / 2), (int)(e->pos.y + e->radius + 4), 10, RAYWHITE);
    }

    EndMode2D();
}
