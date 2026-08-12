#include "ui_map.h"
#include "entity.h"
#include "world.h"
#include "quests.h"
#include "mapdraw.h"
#include "ui_font.h"
#include "ui_hit.h"
#include "ui_theme.h"
#include "raylib.h"
#include <math.h>

#define PLAYER_INDEX 0
#define GAMEPAD_ID 0

static bool g_open = false;

bool UI_IsMapOpen(void) {
    return g_open;
}

void UI_CloseMapOverlay(void) {
    g_open = false;
}

void UI_OpenMapOverlay(void) {
    g_open = true;
}

// World -> map-panel transform, shared by everything drawn below.
typedef struct {
    Rectangle bounds; // world-space playable area
    Rectangle panel;  // screen-space drawing area
    float k;          // world units -> pixels
} MapXform;

static Vector2 W2M(const MapXform *m, Vector2 world) {
    return (Vector2){
        m->panel.x + (world.x - m->bounds.x) * m->k,
        m->panel.y + (world.y - m->bounds.y) * m->k
    };
}

// Map panel -> world, for turning a pointer drag into a drawing.
static Vector2 M2W(const MapXform *m, Vector2 screen) {
    return (Vector2){
        m->bounds.x + (screen.x - m->panel.x) / m->k,
        m->bounds.y + (screen.y - m->panel.y) / m->k
    };
}

void UI_MapUpdateAndDraw(int screenWidth, int screenHeight) {
    if (IsKeyPressed(KEY_M) ||
        (IsGamepadAvailable(GAMEPAD_ID) &&
         IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_MIDDLE_LEFT))) {
        g_open = !g_open;
    }
    if (g_open && IsKeyPressed(KEY_ESCAPE)) g_open = false;
    if (!g_open) return;

    float scale = UI_Scale(screenHeight);
    int margin = (int)(60 * scale);

    MapXform m;
    m.bounds = World_GetBounds();
    // Fit the zone into the window preserving aspect, centered.
    float availW = (float)(screenWidth - margin * 2);
    float availH = (float)(screenHeight - margin * 2);
    float kx = availW / m.bounds.width;
    float ky = availH / m.bounds.height;
    m.k = (kx < ky) ? kx : ky;
    float mapW = m.bounds.width * m.k;
    float mapH = m.bounds.height * m.k;
    m.panel = (Rectangle){ (screenWidth - mapW) / 2.0f, (screenHeight - mapH) / 2.0f, mapW, mapH };

    // Dim the world, then the parchment... well, flat panel. Claim the
    // whole screen: while the map is up, clicks belong to the map.
    UIHit_Claim((Rectangle){ 0, 0, (float)screenWidth, (float)screenHeight });
    DrawRectangle(0, 0, screenWidth, screenHeight, (Color){ 0, 0, 0, 150 });
    DrawRectangleRec(m.panel, (Color){ 22, 26, 22, 245 });

    // The instance boundary. The panel edge is the outer backstop; the
    // ridge drawn inside it is the shape the zone ACTUALLY has, which is
    // the whole reason the edge isn't a rectangle any more.
    DrawRectangleLinesEx(m.panel, 4, (Color){ 96, 92, 84, 220 }); // stone, not alarm-red
    // Scissored to the panel: a mass sitting near the edge has a radius
    // that reaches past the bounds, and unclipped it spilled the ridge
    // out over the dimmed world outside the map.
    BeginScissorMode((int)m.panel.x, (int)m.panel.y, (int)m.panel.width, (int)m.panel.height);
    for (int i = 0; i < World_GetBarrierCount(); i++) {
        const ZoneBarrier *b = World_GetBarrier(i);
        Vector2 p = W2M(&m, b->pos);
        DrawCircleV(p, b->radius * m.k, (Color){ 74, 74, 82, 255 });
    }
    EndScissorMode();

    // Gilt frame around the boundary wall, matching every other window.
    DrawRectangleLinesEx((Rectangle){ m.panel.x - 3, m.panel.y - 3,
                                      m.panel.width + 6, m.panel.height + 6 }, 2,
                         (Color){ 8, 8, 12, 255 });
    DrawRectangleLinesEx((Rectangle){ m.panel.x - 5, m.panel.y - 5,
                                      m.panel.width + 10, m.panel.height + 10 }, 1, UI_GOLD_DIM);

    // Terrain props, faint - enough to recognize the lay of the land.
    int propCount = 0;
    const EnvProp *props = World_GetProps(&propCount);
    for (int i = 0; i < propCount; i++) {
        Vector2 p = W2M(&m, props[i].pos);
        Color c;
        switch (props[i].type) {
            case PROP_TREE: c = (Color){ 60, 110, 65, 255 }; break;
            case PROP_ROCK: c = (Color){ 105, 105, 110, 255 }; break;
            case PROP_TENT: c = (Color){ 150, 120, 80, 255 }; break;
            case PROP_FIRE: c = (Color){ 230, 130, 40, 255 }; break;
            default: c = (Color){ 70, 120, 70, 200 }; break; // grass
        }
        DrawCircleV(p, (props[i].type == PROP_TREE) ? 4.0f : 2.5f, c);
    }

    // Portals + labels, shrine, active reach-quest markers.
    for (int i = 0; i < World_GetPortalCount(); i++) {
        const ZonePortal *portal = World_GetPortal(i);
        Vector2 p = W2M(&m, portal->pos);
        DrawRectangle((int)p.x - 5, (int)p.y - 5, 10, 10, (Color){ 110, 160, 255, 255 });
        int font = (int)(11 * scale);
        UIText(portal->label, (int)p.x + 8, (int)p.y - font / 2, font, (Color){ 150, 190, 255, 255 });
    }
    Vector2 shrinePos;
    if (World_GetShrine(&shrinePos)) {
        Vector2 p = W2M(&m, shrinePos);
        DrawRectangle((int)p.x - 4, (int)p.y - 4, 8, 8, (Color){ 200, 210, 235, 255 });
    }
    for (int i = 0; i < QUEST_COUNT; i++) {
        const Quest *q = &g_quests[i];
        if (q->type != QTYPE_REACH || q->state != QUEST_ACTIVE) continue;
        if (q->targetZone != World_GetZoneId()) continue;
        Vector2 p = W2M(&m, q->targetPos);
        DrawTriangle((Vector2){ p.x, p.y - 7 }, (Vector2){ p.x - 6, p.y + 5 },
                     (Vector2){ p.x + 6, p.y + 5 }, (Color){ 90, 220, 90, 255 });
    }

    // Entities, compass color language; targeted foe ringed gold.
    Entity *player = Entity_Get(PLAYER_INDEX);
    int targetIdx = player ? Entity_RefIndex(player->targetRef) : -1;
    for (int i = 0; i < g_entityCount; i++) {
        Entity *e = &g_entities[i];
        if (!e->alive || i == PLAYER_INDEX) continue;
        Vector2 p = W2M(&m, e->pos);
        Color c;
        if (e->kind == ENT_MONSTER) {
            c = e->aggroed ? (Color){ 255, 70, 60, 255 } : (Color){ 190, 60, 55, 255 };
        } else if (e->kind == ENT_NPC) {
            c = (Color){ 150, 220, 150, 255 };
        } else {
            c = (Color){ 80, 210, 90, 255 };
        }
        DrawCircleV(p, 4.0f, c);
        if (i == targetIdx) {
            DrawCircleLines((int)p.x, (int)p.y, 6.5f, GOLD);
        }
    }
    if (player && player->alive) {
        Vector2 p = W2M(&m, player->pos);
        DrawCircleV(p, 5.0f, RAYWHITE);
        DrawCircleLines((int)p.x, (int)p.y, 5.0f, BLACK);
    }

    // Breadcrumb trail and freehand drawings, clipped to the panel. The
    // trail shows where you've been; the drawings are yours to add.
    BeginScissorMode((int)m.panel.x, (int)m.panel.y, (int)m.panel.width, (int)m.panel.height);
    for (int i = 1; i < MapDraw_TrailCount(); i++) {
        DrawLineEx(W2M(&m, MapDraw_TrailAt(i - 1)), W2M(&m, MapDraw_TrailAt(i)), 2.0f,
                   (Color){ 120, 150, 210, 130 });
    }
    {
        float ttl = MapDraw_StrokeTTL();
        for (int i = 1; i < MapDraw_StrokeCount(); i++) {
            const MapStrokePt *s0 = MapDraw_StrokeAt(i - 1), *s1 = MapDraw_StrokeAt(i);
            if (s0->stroke != s1->stroke) continue;
            float f = 1.0f - s0->age / ttl; if (f < 0.0f) f = 0.0f;
            DrawLineEx(W2M(&m, s0->pos), W2M(&m, s1->pos), 3.0f,
                       (Color){ 240, 210, 90, (unsigned char)(230 * f) });
        }
    }
    EndScissorMode();

    // Left-drag inside the map draws a stroke, the way GW1 lets you mark
    // up the map. The travel buttons sit in the margin, clear of the panel.
    {
        Vector2 mp = GetMousePosition();
        if (CheckCollisionPointRec(mp, m.panel)) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) MapDraw_BeginStroke();
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) MapDraw_AddPoint(M2W(&m, mp));
        }
    }

    // Map travel: a column of visited outposts you can jump to, GW1-style.
    // Only works from within an outpost - never mid-explorable - so the
    // buttons grey out and explain themselves elsewhere.
    {
        bool inOutpost = World_IsOutpost(World_GetZoneId());
        int font = (int)(12 * scale);
        int btnH = (int)(26 * scale);
        int btnW = (int)(150 * scale);
        int bx = (int)(margin / 2);
        int by = (int)(margin);
        Vector2 mp = GetMousePosition();
        bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

        UIText("Map Travel", bx, by, font, (Color){ 220, 210, 180, 255 });
        by += font + (int)(8 * scale);

        for (int z = 0; z < ZONE_COUNT; z++) {
            if (!World_IsOutpost((ZoneId)z) || !World_ZoneUnlocked((ZoneId)z)) continue;
            if (!World_OutpostVisited((ZoneId)z)) continue;
            bool here = (z == (int)World_GetZoneId());
            Rectangle r = { (float)bx, (float)by, (float)btnW, (float)btnH };
            bool hover = inOutpost && !here && CheckCollisionPointRec(mp, r);
            Color bg = here ? (Color){ 60, 70, 60, 230 }
                            : hover ? (Color){ 70, 90, 120, 240 }
                                    : (Color){ 40, 46, 54, 230 };
            DrawRectangleRec(r, bg);
            DrawRectangleLinesEx(r, 1, here ? UI_GOLD_DIM : (Color){ 90, 92, 84, 200 });
            Color tc = (inOutpost || here) ? RAYWHITE : (Color){ 130, 130, 130, 255 };
            UIText(World_ZoneName((ZoneId)z), bx + (int)(8 * scale),
                   by + (btnH - font) / 2, font, tc);
            if (here) {
                int sw = UITextWidth("(here)", (int)(10 * scale));
                UIText("(here)", bx + btnW - sw - (int)(8 * scale),
                       by + (btnH - (int)(10 * scale)) / 2, (int)(10 * scale), UI_GOLD_DIM);
            }
            if (hover && click) {
                if (World_TravelToOutpost((ZoneId)z)) { g_open = false; return; }
            }
            by += btnH + (int)(6 * scale);
        }
        if (!inOutpost) {
            UIText("Travel from an outpost.", bx, by + (int)(4 * scale),
                   (int)(10 * scale), (Color){ 180, 150, 120, 255 });
        }
    }

    // Title + close hint.
    {
        int font = (int)(16 * scale);
        const char *name = World_GetZoneName();
        int tw = UITextWidth(name, font);
        UIText(name, (int)(m.panel.x + (m.panel.width - tw) / 2),
               (int)(m.panel.y - font - 8), font, (Color){ 220, 210, 180, 255 });

        int hintFont = (int)(11 * scale);
        const char *hint = "M / Select / Esc closes";
        int hw = UITextWidth(hint, hintFont);
        UIText(hint, (int)(m.panel.x + (m.panel.width - hw) / 2),
               (int)(m.panel.y + m.panel.height + 8), hintFont, GRAY);
    }
}
