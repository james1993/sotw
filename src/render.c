#include "render.h"
#include "entity.h"
#include "items.h"
#include "input.h"
#include "projectile.h"
#include "world.h"
#include "quests.h"
#include "ui_font.h"
#include "ui_theme.h"
#include "sprite.h"
#include "fx.h"
#include <math.h>
#include <stddef.h>

#define PLAYER_INDEX 0

// Ground loot. Only the world-space art lives here - the name labels
// are drawn by the screen-space overlay (ui_world.c) so they stay crisp
// and consistently sized at any zoom.
static void DrawDrops(double now) {
    for (int i = 0; i < MAX_DROPS; i++) {
        const GroundDrop *d = &g_drops[i];
        if (!d->active) continue;

        // A soft pool of light under every drop: loot should catch the
        // eye across a field, the way GW1's sparkle does.
        float pulse = 0.75f + 0.25f * sinf((float)now * 2.4f + i);
        Color glow = (d->gold > 0) ? (Color){ 255, 205, 90, 70 }
                                   : (Color){ 150, 205, 255, 60 };
        DrawCircleGradient((int)d->pos.x, (int)d->pos.y, 16.0f * pulse,
                           glow, (Color){ 0, 0, 0, 0 });

        if (d->gold > 0) {
            // A little stack of coins rather than one flat dot.
            for (int c = 0; c < 3; c++) {
                Vector2 p = { d->pos.x + (c - 1) * 3.5f, d->pos.y - c * 1.6f };
                DrawEllipse((int)p.x, (int)p.y, 5.0f, 3.0f, (Color){ 214, 168, 52, 255 });
                DrawEllipse((int)p.x, (int)(p.y - 1.2f), 4.0f, 2.2f, (Color){ 255, 216, 110, 255 });
            }
        } else {
            Sprite_DrawItemDrop(&d->item, d->pos);
        }
    }
}

// Environmental art drawers: primitive-shape trees/rocks/grass/tents,
// since the prototype has no sprite assets yet. WHAT to draw and WHERE
// comes from the current zone's prop table (world.c) - render.c only
// knows how each prop type looks.
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
    int propCount = 0;
    const EnvProp *props = World_GetProps(&propCount);
    DrawProps(props, propCount);

    // Zone portals: swirls of blue, labeled with where they go - GW1's
    // map-travel gates reduced to their essence. Zones may have several.
    for (int i = 0; i < World_GetPortalCount(); i++) {
        const ZonePortal *portal = World_GetPortal(i);
        Vector2 portalPos = portal->pos;
        DrawCircleGradient((int)portalPos.x, (int)portalPos.y, 34.0f,
                           (Color){ 90, 150, 255, 200 }, (Color){ 30, 40, 90, 40 });
        DrawCircleLines((int)portalPos.x, (int)portalPos.y, 34.0f, (Color){ 120, 170, 255, 180 });
        int tw = UITextWidth(portal->label, 11);
        UIText(portal->label, (int)(portalPos.x - tw / 2), (int)(portalPos.y + 40), 11, (Color){ 150, 190, 255, 255 });
    }

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
        if (q->targetZone != World_GetZoneId()) continue;
        Vector2 p = q->targetPos;
        DrawCircleLines((int)p.x, (int)p.y, q->reachRadius, (Color){ 90, 220, 90, 90 });
        DrawLineEx((Vector2){ p.x, p.y + 6 }, (Vector2){ p.x, p.y - 26 }, 3.0f, (Color){ 200, 200, 200, 255 });
        DrawTriangle((Vector2){ p.x, p.y - 26 }, (Vector2){ p.x, p.y - 12 },
                     (Vector2){ p.x + 18, p.y - 19 }, (Color){ 90, 220, 90, 255 });
    }
}

// Selection reticle: a flat ring on the ground under the target plus
// four corner ticks, GW1's way of saying "this one". Drawn as an
// ellipse rather than a circle so it reads as lying on the ground
// instead of hovering vertically in front of the sprite.
static void DrawTargetReticle(const Entity *e, Color color, double now) {
    float rx = e->radius + 9.0f;
    float ry = rx * 0.45f;
    Vector2 c = { e->pos.x, e->pos.y + e->radius * 0.35f };

    DrawEllipseLines(c.x, c.y, rx, ry, Fade(color, 0.85f));
    DrawEllipseLines(c.x, c.y, rx - 1.5f, ry - 0.7f, Fade(color, 0.35f));

    // Ticks rotate slowly - just enough motion to catch the eye in a
    // busy fight without becoming a distraction.
    float spin = (float)now * 0.8f;
    for (int i = 0; i < 4; i++) {
        float a = spin + i * (PI / 2.0f);
        float ox = cosf(a), oy = sinf(a) * 0.45f;
        Vector2 inner = { c.x + ox * (rx - 3.0f), c.y + oy * (rx - 3.0f) };
        Vector2 outer = { c.x + ox * (rx + 4.0f), c.y + oy * (rx + 4.0f) };
        DrawLineEx(inner, outer, 2.0f, color);
    }
}

// A softer version of the same ring for whatever the mouse is over:
// hover should hint, not shout.
static void DrawHoverRing(const Entity *e, Color color) {
    float rx = e->radius + 7.0f;
    Vector2 c = { e->pos.x, e->pos.y + e->radius * 0.35f };
    DrawEllipseLines(c.x, c.y, rx, rx * 0.45f, Fade(color, 0.5f));
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
    // out in the plains. Per-zone data (world.c).
    //
    // The grid is deliberately faint and every fourth line is a touch
    // stronger. A uniform bright lattice read as a debug overlay; at
    // this weight it's ground texture you stop noticing, which is the
    // point - it should suggest terrain, not label it.
    Color gridColor = World_GetGridColor();
    Color minor = { gridColor.r, gridColor.g, gridColor.b, 70 };
    Color major = { gridColor.r, gridColor.g, gridColor.b, 130 };
    for (int x = startX; x <= endX; x += gridSpacing) {
        bool isMajor = (((x / gridSpacing) % 4) == 0);
        DrawLine(x, startY, x, endY, isMajor ? major : minor);
    }
    for (int y = startY; y <= endY; y += gridSpacing) {
        bool isMajor = (((y / gridSpacing) % 4) == 0);
        DrawLine(startX, y, endX, y, isMajor ? major : minor);
    }

    // Mottled ground patches: a scattering of large, barely-there
    // blotches keyed to a fixed lattice, so the floor has some variation
    // instead of reading as graph paper. Deterministic from position, so
    // they don't crawl as the camera moves.
    {
        const int blotchSpacing = 160;
        int bx0 = ((int)floorf(topLeft.x / blotchSpacing) - 1) * blotchSpacing;
        int bx1 = ((int)ceilf(bottomRight.x / blotchSpacing) + 1) * blotchSpacing;
        int by0 = ((int)floorf(topLeft.y / blotchSpacing) - 1) * blotchSpacing;
        int by1 = ((int)ceilf(bottomRight.y / blotchSpacing) + 1) * blotchSpacing;
        for (int x = bx0; x <= bx1; x += blotchSpacing) {
            for (int y = by0; y <= by1; y += blotchSpacing) {
                unsigned h = (unsigned)(x * 73856093) ^ (unsigned)(y * 19349663);
                float ox = (float)(h % 97) - 48.0f;
                float oy = (float)((h >> 8) % 97) - 48.0f;
                float r = 60.0f + (float)((h >> 16) % 60);
                unsigned char a = (unsigned char)(10 + (h >> 24) % 9);
                // Gradient, not a flat disc: a hard-edged circle reads as
                // a bubble sitting on the floor, a falloff reads as the
                // floor itself being uneven.
                DrawCircleGradient((int)(x + ox), (int)(y + oy), r,
                                   (Color){ gridColor.r, gridColor.g, gridColor.b, a },
                                   (Color){ gridColor.r, gridColor.g, gridColor.b, 0 });
            }
        }
    }

    // The instance boundary: a visible wall line at the playable edge,
    // with a soft inner falloff so it reads as the edge of the world
    // rather than a stray rectangle someone left on screen.
    {
        // One crisp edge line with a single soft band just inside it.
        // Stacking several thin outlines instead produced visible
        // stripes that read as a rendering artifact rather than a wall.
        Rectangle b = World_GetBounds();
        DrawRectangleLinesEx((Rectangle){ b.x + 11, b.y + 11, b.width - 22, b.height - 22 },
                             22.0f, (Color){ 150, 70, 55, 28 });
        DrawRectangleLinesEx(b, 4.0f, (Color){ 172, 86, 64, 225 });
    }

    DrawEnvironment();
    DrawDrops(GetTime());

    // Faint "danger bubble" around the player, like the aggro circle on
    // GW1's compass. Only meaningful (and only drawn) in combat zones -
    // outposts are safe.
    Entity *player = Entity_Get(PLAYER_INDEX);
    if (player && player->alive && World_GetMode() == MODE_EXPLORABLE) {
        DrawCircleLines((int)player->pos.x, (int)player->pos.y, AGGRO_RING_RADIUS, (Color){ 220, 170, 60, 60 });
    }

    double now = GetTime();

    // Ground markers under entities, drawn before any sprite so nobody
    // stands "behind" their own reticle.
    int targetIdx = player ? Entity_RefIndex(player->targetRef) : -1;
    int hoverIdx = Input_HoverEntityIndex();
    int interactIdx = Input_InteractNpcIndex();

    if (hoverIdx >= 0 && hoverIdx != targetIdx) {
        Entity *h = Entity_Get(hoverIdx);
        if (h && h->alive) {
            DrawHoverRing(h, h->kind == ENT_MONSTER ? (Color){ 240, 130, 120, 255 }
                                                    : (Color){ 170, 215, 255, 255 });
        }
    }
    if (interactIdx >= 0) {
        // The NPC the interact button is aimed at gets the same ring
        // treatment as a combat target - in green, and only ever one at
        // a time, so "who am I talking to" is answered before you press
        // anything.
        Entity *n = Entity_Get(interactIdx);
        if (n && n->alive) DrawTargetReticle(n, (Color){ 120, 226, 130, 255 }, now);
    }
    if (targetIdx >= 0) {
        Entity *t = Entity_Get(targetIdx);
        if (t && t->alive) {
            DrawTargetReticle(t, t->kind == ENT_MONSTER ? (Color){ 248, 168, 96, 255 }
                                                        : (Color){ 150, 210, 255, 255 }, now);
        }
    }

    // Depth sort: entities lower on the screen are nearer the camera, so
    // they draw last and overlap the ones behind them. Without this,
    // characters standing on the same ground pop in front of each other
    // by array order, which reads as a bug the moment two sprites touch.
    int order[MAX_ENTITIES];
    int drawCount = 0;
    for (int i = 0; i < g_entityCount; i++) {
        if (g_entities[i].alive) order[drawCount++] = i;
    }
    for (int i = 1; i < drawCount; i++) {
        int v = order[i];
        float vy = g_entities[v].pos.y;
        int j = i - 1;
        while (j >= 0 && g_entities[order[j]].pos.y > vy) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = v;
    }
    for (int i = 0; i < drawCount; i++) {
        Sprite_DrawEntity(&g_entities[order[i]], now);
    }

    Projectile_Draw();
    Fx_Draw();

    EndMode2D();

    // Screen-space vignette: darkens the corners a little so the eye
    // settles on the middle of the field where the party is, and the
    // HUD panels sitting in those corners have something to sit against
    // instead of floating on flat background.
    {
        int steps = 6;
        for (int i = 0; i < steps; i++) {
            int inset = i * (screenHeight / 26);
            unsigned char a = (unsigned char)(22 - i * 3);
            if (a == 0) break;
            DrawRectangleLinesEx((Rectangle){ (float)inset, (float)inset,
                                              (float)(screenWidth - inset * 2),
                                              (float)(screenHeight - inset * 2) },
                                 (float)(screenHeight / 26), (Color){ 0, 0, 0, a });
        }
    }
}
