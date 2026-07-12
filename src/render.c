#include "render.h"
#include "entity.h"
#include "items.h"
#include "projectile.h"
#include "world.h"
#include "quests.h"
#include "ui_font.h"
#include <math.h>
#include <stddef.h>

#define PLAYER_INDEX 0

// Ground loot: gold piles as coins, weapon/armor drops as diamonds with
// a small name label so you can tell whether walking over is worth it.
static void DrawDrops(void) {
    for (int i = 0; i < MAX_DROPS; i++) {
        const GroundDrop *d = &g_drops[i];
        if (!d->active) continue;

        if (d->gold > 0) {
            DrawCircleV(d->pos, 5.0f, GOLD);
            DrawCircleLines((int)d->pos.x, (int)d->pos.y, 5.0f, (Color){ 120, 90, 20, 255 });
        } else {
            Color c = (d->item.kind == ITEM_WEAPON) ? SKYBLUE : (Color){ 120, 220, 130, 255 };
            DrawPoly(d->pos, 4, 7.0f, 45.0f, c);
            DrawPolyLines(d->pos, 4, 7.0f, 45.0f, BLACK);
            int tw = UITextWidth(d->item.name, 10);
            UIText(d->item.name, (int)(d->pos.x - tw / 2), (int)(d->pos.y + 10), 10, c);
        }
    }
}

// Basic environmental art: a fixed, hand-placed scatter of trees/rocks/
// grass tufts built from primitive shapes, since the prototype has no
// sprite assets yet. Purely decorative - no collision or LoS blocking.
typedef enum { PROP_TREE, PROP_ROCK, PROP_GRASS, PROP_TENT, PROP_FIRE } PropType;
typedef struct { Vector2 pos; PropType type; float scale; } EnvProp;

// Ashford Camp: a small circle of tents around a fire, palisade-ish
// rocks, a couple of trees for shade.
static const EnvProp g_outpostProps[] = {
    { { 0, -140 }, PROP_FIRE, 1.0f },
    { { -150, -140 }, PROP_TENT, 1.0f },
    { { -190, -40 }, PROP_TENT, 0.9f },
    { { 150, -150 }, PROP_TENT, 1.1f },
    { { 190, -50 }, PROP_TENT, 0.9f },
    { { -120, 150 }, PROP_TENT, 1.0f },
    { { 140, 160 }, PROP_TENT, 0.95f },
    { { -260, -180 }, PROP_TREE, 1.0f },
    { { 280, -200 }, PROP_TREE, 1.1f },
    { { -280, 120 }, PROP_ROCK, 1.0f },
    { { 240, 140 }, PROP_ROCK, 0.9f },
};
#define OUTPOST_PROP_COUNT (int)(sizeof(g_outpostProps) / sizeof(g_outpostProps[0]))

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
    // Eastern reaches of the expanded plains.
    { { 620, -420 }, PROP_TREE, 1.1f },
    { { 980, -400 }, PROP_TREE, 0.9f },
    { { 1180, -160 }, PROP_TREE, 1.2f },
    { { 1240, 240 }, PROP_TREE, 1.0f },
    { { 760, 430 }, PROP_TREE, 1.1f },
    { { 1050, 60 }, PROP_ROCK, 1.1f },
    { { 640, 200 }, PROP_ROCK, 0.9f },
    { { 880, -60 }, PROP_ROCK, 0.8f },
    { { 1300, -60 }, PROP_ROCK, 1.0f },
    { { 720, -120 }, PROP_GRASS, 1.0f },
    { { 1000, 180 }, PROP_GRASS, 1.1f },
    { { 1150, -280 }, PROP_GRASS, 0.9f },
    { { 560, 60 }, PROP_GRASS, 1.0f },
    { { 1330, 150 }, PROP_GRASS, 1.0f },
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

static void DrawTent(Vector2 pos, float scale) {
    Vector2 top = { pos.x, pos.y - 26 * scale };
    Vector2 left = { pos.x - 22 * scale, pos.y + 8 * scale };
    Vector2 right = { pos.x + 22 * scale, pos.y + 8 * scale };
    DrawTriangle(top, left, right, (Color){ 150, 120, 80, 255 });
    DrawTriangleLines(top, left, right, (Color){ 90, 70, 45, 255 });
    // Entrance flap
    DrawTriangle((Vector2){ pos.x, pos.y - 8 * scale },
                 (Vector2){ pos.x - 7 * scale, pos.y + 8 * scale },
                 (Vector2){ pos.x + 7 * scale, pos.y + 8 * scale },
                 (Color){ 60, 48, 32, 255 });
}

static void DrawCampfire(Vector2 pos, float scale) {
    DrawCircleV(pos, 10 * scale, (Color){ 60, 50, 45, 255 });
    DrawCircleV(pos, 6 * scale, (Color){ 230, 130, 40, 255 });
    DrawCircleV((Vector2){ pos.x, pos.y - 3 * scale }, 3 * scale, (Color){ 255, 210, 90, 255 });
}

static void DrawProps(const EnvProp *props, int count) {
    for (int i = 0; i < count; i++) {
        const EnvProp *p = &props[i];
        switch (p->type) {
            case PROP_TREE: DrawTree(p->pos, p->scale); break;
            case PROP_ROCK: DrawRock(p->pos, p->scale); break;
            case PROP_GRASS: DrawGrassTuft(p->pos, p->scale); break;
            case PROP_TENT: DrawTent(p->pos, p->scale); break;
            case PROP_FIRE: DrawCampfire(p->pos, p->scale); break;
        }
    }
}

static void DrawEnvironment(void) {
    if (World_GetMode() == MODE_OUTPOST) {
        DrawProps(g_outpostProps, OUTPOST_PROP_COUNT);
    } else {
        DrawProps(g_envProps, ENV_PROP_COUNT);
    }

    // The zone portal: a swirl of blue, labeled with where it goes -
    // GW1's map-travel gates reduced to their essence.
    Vector2 portalPos;
    const char *portalLabel;
    World_GetPortal(&portalPos, &portalLabel);
    DrawCircleGradient((int)portalPos.x, (int)portalPos.y, 34.0f,
                       (Color){ 90, 150, 255, 200 }, (Color){ 30, 40, 90, 40 });
    DrawCircleLines((int)portalPos.x, (int)portalPos.y, 34.0f, (Color){ 120, 170, 255, 180 });
    int tw = UITextWidth(portalLabel, 11);
    UIText(portalLabel, (int)(portalPos.x - tw / 2), (int)(portalPos.y + 40), 11, (Color){ 150, 190, 255, 255 });

    // Resurrection shrine: a stone marker with a soft glow. On a party
    // wipe everyone respawns here with their death penalty, like GW1.
    Vector2 shrinePos;
    if (World_GetShrine(&shrinePos)) {
        DrawCircleGradient((int)shrinePos.x, (int)shrinePos.y, 26.0f,
                           (Color){ 200, 220, 255, 90 }, (Color){ 60, 70, 100, 0 });
        DrawRectangle((int)shrinePos.x - 6, (int)shrinePos.y - 22, 12, 26, (Color){ 130, 130, 140, 255 });
        DrawRectangle((int)shrinePos.x - 12, (int)shrinePos.y + 2, 24, 6, (Color){ 110, 110, 120, 255 });
        int sw = UITextWidth("Resurrection Shrine", 10);
        UIText("Resurrection Shrine", (int)(shrinePos.x - sw / 2), (int)(shrinePos.y + 14), 10, (Color){ 180, 200, 230, 255 });
    }

    // Reach-quest marker: a green flag planted at the objective while
    // the quest is active.
    for (int i = 0; i < QUEST_COUNT; i++) {
        const Quest *q = &g_quests[i];
        if (q->type != QTYPE_REACH || q->state != QUEST_ACTIVE) continue;
        if (World_GetMode() != MODE_EXPLORABLE) continue;
        Vector2 p = q->targetPos;
        DrawCircleLines((int)p.x, (int)p.y, q->reachRadius, (Color){ 90, 220, 90, 90 });
        DrawLineEx((Vector2){ p.x, p.y + 6 }, (Vector2){ p.x, p.y - 26 }, 3.0f, (Color){ 200, 200, 200, 255 });
        DrawTriangle((Vector2){ p.x, p.y - 26 }, (Vector2){ p.x, p.y - 12 },
                     (Vector2){ p.x + 18, p.y - 19 }, (Color){ 90, 220, 90, 255 });
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

    // Ground tint sells the zone: packed dirt in the camp, green grass
    // out in the plains.
    Color gridColor = (World_GetMode() == MODE_OUTPOST)
        ? (Color){ 80, 68, 52, 255 } : (Color){ 52, 76, 48, 255 };
    for (int x = startX; x <= endX; x += gridSpacing) {
        DrawLine(x, startY, x, endY, gridColor);
    }
    for (int y = startY; y <= endY; y += gridSpacing) {
        DrawLine(startX, y, endX, y, gridColor);
    }

    DrawEnvironment();
    DrawDrops();

    // Faint "danger bubble" around the player, like the aggro circle on
    // GW1's compass. Only meaningful (and only drawn) in combat zones -
    // outposts are safe.
    Entity *player = Entity_Get(PLAYER_INDEX);
    if (player && player->alive && World_GetMode() == MODE_EXPLORABLE) {
        DrawCircleLines((int)player->pos.x, (int)player->pos.y, 130.0f, (Color){ 220, 170, 60, 60 });
    }

    // Ring under the player's current target, so it's obvious at a glance
    // which entity the target panel/skill-bar actions apply to.
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
            int tw = UITextWidth(label, 10);
            UIText(label, (int)(e->pos.x - tw / 2), (int)(e->pos.y - e->radius - 34.0f), 10, GOLD);
        }

        if (e->dodgeFlashTimer > 0.0f) {
            const char *label = "DODGED";
            int tw = UITextWidth(label, 10);
            UIText(label, (int)(e->pos.x - tw / 2), (int)(e->pos.y - e->radius - 34.0f), 10, (Color){ 200, 200, 210, 255 });
        }

        // GW1's green exclamation point over quest givers with something
        // to offer (or a reward to hand out).
        if (e->kind == ENT_NPC && e->npcRole == NPC_QUEST_GIVER && Quests_GiverHasAttention()) {
            int mw = UITextWidth("!", 16);
            UIText("!", (int)(e->pos.x - mw / 2), (int)(e->pos.y - e->radius - 32.0f), 16, (Color){ 90, 230, 90, 255 });
        }

        Color nameColor = (e->kind == ENT_NPC) ? (Color){ 150, 230, 150, 255 } : RAYWHITE;
        int textWidth = UITextWidth(e->name, 10);
        UIText(e->name, (int)(e->pos.x - textWidth / 2), (int)(e->pos.y + e->radius + 4), 10, nameColor);
    }

    Projectile_Draw();

    EndMode2D();
}
