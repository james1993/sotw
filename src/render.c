#include "render.h"
#include "entity.h"

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

    const int gridSpacing = 40;
    const int range = 800;
    for (int x = -range; x <= range; x += gridSpacing) {
        DrawLine(x, -range, x, range, (Color){ 60, 60, 60, 255 });
    }
    for (int y = -range; y <= range; y += gridSpacing) {
        DrawLine(-range, y, range, y, (Color){ 60, 60, 60, 255 });
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
