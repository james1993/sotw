#include "ui_mapmarks.h"
#include <math.h>

static Color Fade8(Color c, unsigned char a) {
    // Scale an existing colour's alpha by a/255, so callers pass a single
    // fade for the whole marker.
    c.a = (unsigned char)((int)c.a * a / 255);
    return c;
}

void UIMap_DrawProp(Vector2 p, PropType type, float unit, unsigned char alpha) {
    switch (type) {
        case PROP_HOUSE: {
            // A building footprint - the strongest landmark on any map.
            float s = unit * 2.6f;
            DrawRectangle((int)(p.x - s), (int)(p.y - s * 0.8f), (int)(s * 2), (int)(s * 1.6f),
                          Fade8((Color){ 156, 126, 88, 255 }, alpha));
            DrawRectangleLines((int)(p.x - s), (int)(p.y - s * 0.8f), (int)(s * 2), (int)(s * 1.6f),
                               Fade8((Color){ 92, 70, 46, 255 }, alpha));
        } break;
        case PROP_STALL: {
            float s = unit * 1.4f;
            DrawRectangle((int)(p.x - s), (int)(p.y - s * 0.7f), (int)(s * 2), (int)(s * 1.4f),
                          Fade8((Color){ 150, 108, 64, 255 }, alpha));
        } break;
        case PROP_FOUNTAIN: {
            // Water reads blue - the other unmistakable landmark.
            DrawCircleV(p, unit * 1.7f, Fade8((Color){ 66, 118, 176, 255 }, alpha));
            DrawCircleV(p, unit * 0.9f, Fade8((Color){ 120, 178, 216, 255 }, alpha));
        } break;
        case PROP_STATUE: {
            DrawCircleV(p, unit * 1.1f, Fade8((Color){ 176, 178, 186, 255 }, alpha));
        } break;
        case PROP_TENT: {
            float s = unit * 1.5f;
            DrawTriangle((Vector2){ p.x, p.y - s }, (Vector2){ p.x - s, p.y + s * 0.7f },
                         (Vector2){ p.x + s, p.y + s * 0.7f }, Fade8((Color){ 168, 134, 90, 255 }, alpha));
        } break;
        case PROP_FIRE: case PROP_BRAZIER: {
            DrawCircleV(p, unit * 0.9f, Fade8((Color){ 232, 132, 44, 255 }, alpha));
        } break;
        case PROP_TREE: {
            DrawCircleV(p, unit * 1.3f, Fade8((Color){ 58, 112, 66, 255 }, alpha));
        } break;
        case PROP_ROCK: {
            DrawCircleV(p, unit * 1.0f, Fade8((Color){ 108, 108, 114, 255 }, alpha));
        } break;
        case PROP_BANNER: {
            DrawRectangle((int)(p.x - unit * 0.4f), (int)(p.y - unit), (int)(unit * 0.8f), (int)(unit * 2),
                          Fade8((Color){ 150, 80, 70, 255 }, alpha));
        } break;
        case PROP_GRASS: default:
            // Ground cover: a faint speck so the terrain has texture without
            // crowding the landmarks.
            DrawCircleV(p, unit * 0.5f, Fade8((Color){ 70, 118, 72, 160 }, alpha));
            break;
    }
}

void UIMap_DrawBlip(Vector2 p, int kind, bool aggroed, bool targeted, float unit) {
    if (kind == 2) {
        // Monster: a red diamond, bigger than the friendly dots, with a
        // dark rim so it holds up over any terrain colour. Awake foes glow
        // hotter and gain an outer ring.
        float s = unit * (aggroed ? 1.9f : 1.6f);
        Color fill = aggroed ? (Color){ 255, 74, 62, 255 } : (Color){ 196, 62, 56, 255 };
        DrawPoly(p, 4, s, 0.0f, fill);      // rotation 0 => a point-up diamond
        DrawPolyLinesEx(p, 4, s, 0.0f, 1.2f, (Color){ 30, 12, 12, 235 });
        if (aggroed) DrawPolyLinesEx(p, 4, s + unit * 0.8f, 0.0f, 1.0f, (Color){ 255, 120, 90, 150 });
    } else if (kind == 1) {
        // NPC: a soft cyan dot - clearly neither party green nor foe red.
        DrawCircleV(p, unit * 1.05f, (Color){ 120, 205, 200, 255 });
        DrawCircleLines((int)p.x, (int)p.y, unit * 1.05f, (Color){ 20, 60, 58, 200 });
    } else {
        // Ally: a bright green dot.
        DrawCircleV(p, unit, (Color){ 86, 214, 96, 255 });
        DrawCircleLines((int)p.x, (int)p.y, unit, (Color){ 16, 54, 22, 200 });
    }
    if (targeted) DrawCircleLines((int)p.x, (int)p.y, unit * 2.0f, GOLD);
}
