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
#include "ground.h"
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
// Props all sit on a soft contact shadow and are lit from the upper
// left, so they feel planted on the ground rather than pasted over it -
// the same trick the character sprites already use.
static void DrawPropShadow(Vector2 pos, float rx, float ry) {
    DrawEllipse((int)pos.x, (int)(pos.y + ry * 0.5f), rx, ry, (Color){ 0, 0, 0, 70 });
}

static void DrawTree(Vector2 pos, float scale) {
    DrawPropShadow((Vector2){ pos.x, pos.y + 10 * scale }, 15 * scale, 5 * scale);
    DrawRectangle((int)(pos.x - 3 * scale), (int)(pos.y - 2 * scale), (int)(6 * scale), (int)(14 * scale),
        (Color){ 78, 52, 34, 255 });
    DrawRectangle((int)(pos.x - 3 * scale), (int)(pos.y - 2 * scale), (int)(2 * scale), (int)(14 * scale),
        (Color){ 104, 72, 46, 255 }); // lit side of the trunk
    // Canopy: shaded lobes first, then a lit crown and a highlight.
    DrawCircleV((Vector2){ pos.x - 8 * scale, pos.y - 12 * scale }, 11 * scale, (Color){ 34, 76, 42, 255 });
    DrawCircleV((Vector2){ pos.x + 9 * scale, pos.y - 14 * scale }, 12 * scale, (Color){ 30, 68, 38, 255 });
    DrawCircleV((Vector2){ pos.x, pos.y - 20 * scale }, 15 * scale, (Color){ 46, 100, 56, 255 });
    DrawCircleV((Vector2){ pos.x - 5 * scale, pos.y - 24 * scale }, 6 * scale, (Color){ 62, 122, 68, 255 });
}

static void DrawRock(Vector2 pos, float scale) {
    DrawPropShadow(pos, 12 * scale, 4.5f * scale);
    DrawCircleV((Vector2){ pos.x + 5 * scale, pos.y + 3 * scale }, 7 * scale, (Color){ 56, 56, 61, 255 });
    DrawCircleV(pos, 10 * scale, (Color){ 76, 76, 83, 255 });
    // A crescent highlight on the upper left reads as a lit facet.
    DrawCircleV((Vector2){ pos.x - 3 * scale, pos.y - 3 * scale }, 5 * scale, (Color){ 104, 104, 112, 255 });
    DrawCircleLines((int)pos.x, (int)pos.y, 10 * scale, (Color){ 34, 34, 39, 255 });
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
    DrawPropShadow((Vector2){ pos.x, pos.y + 8 * scale }, 24 * scale, 6 * scale);

    // Two canvas panels at different values give the tent a ridge and a
    // shaded side instead of reading as one flat triangle.
    DrawTriangle(top, left, (Vector2){ pos.x, pos.y + 8 * scale }, (Color){ 162, 131, 88, 255 });
    DrawTriangle(top, (Vector2){ pos.x, pos.y + 8 * scale }, right, (Color){ 124, 98, 64, 255 });
    DrawTriangleLines(top, left, right, (Color){ 78, 60, 38, 255 });

    // Guy lines and pegs.
    DrawLineEx(top, (Vector2){ pos.x - 30 * scale, pos.y + 9 * scale }, 1.2f, (Color){ 92, 74, 50, 200 });
    DrawLineEx(top, (Vector2){ pos.x + 30 * scale, pos.y + 9 * scale }, 1.2f, (Color){ 92, 74, 50, 200 });

    // Entrance flap, darker at the bottom where it falls into shadow.
    DrawTriangle((Vector2){ pos.x, pos.y - 8 * scale },
                 (Vector2){ pos.x - 7 * scale, pos.y + 8 * scale },
                 (Vector2){ pos.x + 7 * scale, pos.y + 8 * scale },
                 (Color){ 44, 34, 24, 255 });
}

static void DrawCampfire(Vector2 pos, float scale) {
    // Firelight pooling on the ground, animated so camps feel alive.
    float flicker = 0.85f + 0.15f * sinf((float)GetTime() * 7.3f + pos.x);
    DrawCircleGradient((int)pos.x, (int)pos.y, 46 * scale * flicker,
                       (Color){ 255, 150, 60, 46 }, (Color){ 255, 120, 40, 0 });

    // Stone ring.
    for (int i = 0; i < 7; i++) {
        float a = i * (2.0f * PI / 7.0f);
        DrawCircleV((Vector2){ pos.x + cosf(a) * 11 * scale, pos.y + sinf(a) * 6 * scale },
                    3.2f * scale, (Color){ 74, 70, 66, 255 });
    }
    // Logs, then flame.
    DrawLineEx((Vector2){ pos.x - 7 * scale, pos.y + 2 * scale },
               (Vector2){ pos.x + 7 * scale, pos.y - 2 * scale }, 3.0f * scale, (Color){ 78, 54, 36, 255 });
    DrawLineEx((Vector2){ pos.x - 6 * scale, pos.y - 2 * scale },
               (Vector2){ pos.x + 6 * scale, pos.y + 2 * scale }, 3.0f * scale, (Color){ 64, 44, 30, 255 });
    float h = (9.0f + 2.5f * sinf((float)GetTime() * 9.1f + pos.y)) * scale;
    DrawTriangle((Vector2){ pos.x, pos.y - h - 5 * scale },
                 (Vector2){ pos.x - 6 * scale, pos.y + 2 * scale },
                 (Vector2){ pos.x + 6 * scale, pos.y + 2 * scale },
                 (Color){ 232, 128, 42, 255 });
    DrawTriangle((Vector2){ pos.x, pos.y - h * 0.55f - 3 * scale },
                 (Vector2){ pos.x - 3 * scale, pos.y + 1 * scale },
                 (Vector2){ pos.x + 3 * scale, pos.y + 1 * scale },
                 (Color){ 255, 214, 96, 255 });
}


// The zone edge, drawn as terrain. Three passes rather than one loop of
// complete mountains: drawn whole, each mass would be outlined against
// its neighbours and the ridge would read as a row of separate lumps.
// Laying down every skirt, then every body, then every lit face merges
// the chain into one landform.
//
// The foot is a flattened ellipse, not a circle. A circle the size of
// the collision radius reads as a boulder with a cone balanced on it;
// a skirt reads as the base of a slope seen from above.
static void DrawZoneRidge(void) {
    int count = World_GetBarrierCount();
    if (count <= 0) return;

    const Color kStoneDark = { 44, 45, 52, 255 };
    const Color kStoneBody = { 66, 67, 76, 255 };
    const Color kStoneLit  = { 92, 94, 104, 255 };
    const Color kCap       = { 132, 134, 145, 255 };

    // Pass 1: shadow and skirt, so the ridge sits ON the terrain.
    for (int i = 0; i < count; i++) {
        const ZoneBarrier *b = World_GetBarrier(i);
        DrawEllipse((int)b->pos.x, (int)(b->pos.y + b->radius * 0.30f),
                    b->radius * 1.15f, b->radius * 0.44f, (Color){ 0, 0, 0, 80 });
        DrawEllipse((int)b->pos.x, (int)(b->pos.y + b->radius * 0.16f),
                    b->radius * 1.06f, b->radius * 0.52f, kStoneDark);
    }

    // Pass 2: the slope itself - a broad low cone per mass, all in one
    // value so overlapping neighbours fuse into a continuous wall.
    for (int i = 0; i < count; i++) {
        const ZoneBarrier *b = World_GetBarrier(i);
        float peak = b->radius * (0.80f + 0.45f * b->height);
        DrawTriangle((Vector2){ b->pos.x, b->pos.y - peak },
                     (Vector2){ b->pos.x - b->radius * 1.05f, b->pos.y + b->radius * 0.36f },
                     (Vector2){ b->pos.x + b->radius * 1.05f, b->pos.y + b->radius * 0.36f },
                     kStoneBody);
    }

    // Pass 3: the lit west face and a scree cap. Every mass is lit from
    // the same side, which is what makes flat grey read as rock.
    for (int i = 0; i < count; i++) {
        const ZoneBarrier *b = World_GetBarrier(i);
        float peak = b->radius * (0.80f + 0.45f * b->height);
        Vector2 top = { b->pos.x, b->pos.y - peak };
        DrawTriangle(top,
                     (Vector2){ b->pos.x - b->radius * 1.05f, b->pos.y + b->radius * 0.36f },
                     (Vector2){ b->pos.x - b->radius * 0.06f, b->pos.y + b->radius * 0.36f },
                     kStoneLit);
        float capH = peak * 0.30f;
        DrawTriangle(top,
                     (Vector2){ b->pos.x - b->radius * 0.26f, b->pos.y - peak + capH },
                     (Vector2){ b->pos.x + b->radius * 0.26f, b->pos.y - peak + capH },
                     kCap);
    }
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
    Color gridColor = World_GetGridColor();

    // The surface itself: laid stone inside a settlement, broken ground
    // outside it, tinted with the zone's own colour (ground.c). Drawn
    // first, so everything below sits on top of it.
    Ground_Draw((Rectangle){ (float)startX, (float)startY,
                             (float)(endX - startX), (float)(endY - startY) },
                gridColor, World_GetMode() == MODE_OUTPOST);

    // The grid is deliberately faint and every fourth line is a touch
    // stronger. It survived the ground texture because it still does a
    // job the texture can't: it gives distance a scale you can count.
    // Now that the surface carries the detail, though, it's pulled back
    // to roughly half its old weight - two textures competing read as
    // noise, and the lattice is the one that should yield.
    Color minor = { gridColor.r, gridColor.g, gridColor.b, 34 };
    Color major = { gridColor.r, gridColor.g, gridColor.b, 66 };
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

    DrawZoneRidge();

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
