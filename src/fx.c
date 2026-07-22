#include "fx.h"
#include <math.h>
#include <stddef.h>

#define MAX_FX 96

typedef enum {
    FXK_NONE = 0,
    FXK_SLASH,
    FXK_BURST,
    FXK_HEAL,
    FXK_RING,
    FXK_STARS
} FxKind;

typedef struct {
    FxKind kind;
    Vector2 pos;
    float angle;   // slash orientation
    float radius;  // ring target radius
    float t, dur;  // life
    Color color;
} WorldFx;

static WorldFx g_fx[MAX_FX];

Color Fx_AttrColor(AttributeKind a) {
    switch (a) {
        case ATTR_FIRE_MAGIC:      return (Color){ 255, 140, 50, 255 };
        case ATTR_ENERGY_STORAGE:  return (Color){ 150, 120, 255, 255 };
        case ATTR_HEALING_PRAYERS: return (Color){ 120, 240, 140, 255 };
        case ATTR_SMITING_PRAYERS: return (Color){ 255, 220, 110, 255 };
        case ATTR_DIVINE_FAVOR:    return (Color){ 220, 240, 255, 255 };
        case ATTR_STRENGTH:
        case ATTR_TACTICS:         return (Color){ 210, 210, 220, 255 };
        default:                   return (Color){ 200, 200, 210, 255 };
    }
}

static WorldFx *Alloc(void) {
    for (int i = 0; i < MAX_FX; i++) {
        if (g_fx[i].kind == FXK_NONE) return &g_fx[i];
    }
    return NULL; // pool full: drop the effect, nobody will miss one spark
}

static void Spawn(FxKind kind, Vector2 pos, float angle, float radius, float dur, Color color) {
    WorldFx *f = Alloc();
    if (!f) return;
    f->kind = kind;
    f->pos = pos;
    f->angle = angle;
    f->radius = radius;
    f->t = 0.0f;
    f->dur = dur;
    f->color = color;
}

void Fx_Slash(Vector2 pos, float angle) { Spawn(FXK_SLASH, pos, angle, 0, 0.22f, (Color){ 240, 240, 250, 255 }); }
void Fx_Burst(Vector2 pos, Color color) { Spawn(FXK_BURST, pos, 0, 0, 0.32f, color); }
void Fx_Heal(Vector2 pos)               { Spawn(FXK_HEAL, pos, 0, 0, 0.8f, (Color){ 120, 240, 140, 255 }); }
void Fx_Ring(Vector2 pos, float radius, Color color) { Spawn(FXK_RING, pos, 0, radius, 0.45f, color); }
void Fx_Stars(Vector2 pos)              { Spawn(FXK_STARS, pos, 0, 0, 1.0f, (Color){ 255, 230, 130, 255 }); }

void Fx_Update(float dt) {
    for (int i = 0; i < MAX_FX; i++) {
        if (g_fx[i].kind == FXK_NONE) continue;
        g_fx[i].t += dt;
        if (g_fx[i].t >= g_fx[i].dur) g_fx[i].kind = FXK_NONE;
    }
}

void Fx_Clear(void) {
    for (int i = 0; i < MAX_FX; i++) g_fx[i].kind = FXK_NONE;
}

void Fx_Draw(void) {
    for (int i = 0; i < MAX_FX; i++) {
        WorldFx *f = &g_fx[i];
        if (f->kind == FXK_NONE) continue;
        float u = f->t / f->dur;       // 0..1 life
        float fade = 1.0f - u;

        switch (f->kind) {
            case FXK_SLASH: {
                // An arc sweeping through the swing direction.
                float sweep = -50.0f + 100.0f * u;
                float deg = f->angle * 57.2958f;
                Color c = Fade(f->color, fade);
                DrawRing(f->pos, 14.0f, 20.0f, deg + sweep - 30.0f, deg + sweep + 30.0f, 12, c);
                break;
            }
            case FXK_BURST: {
                // Radial shards flying outward.
                for (int s = 0; s < 8; s++) {
                    float a = s * 0.785f + 0.3f;
                    float d0 = 6.0f + 22.0f * u;
                    float d1 = d0 + 7.0f * fade;
                    Vector2 p0 = { f->pos.x + cosf(a) * d0, f->pos.y + sinf(a) * d0 };
                    Vector2 p1 = { f->pos.x + cosf(a) * d1, f->pos.y + sinf(a) * d1 };
                    DrawLineEx(p0, p1, 2.5f * fade + 0.5f, Fade(f->color, fade));
                }
                DrawCircleV(f->pos, 8.0f * fade, Fade(f->color, fade * 0.5f));
                break;
            }
            case FXK_HEAL: {
                // Sparkle crosses drifting upward.
                for (int s = 0; s < 5; s++) {
                    float phase = s * 1.7f;
                    float rise = u * 30.0f + s * 4.0f;
                    float sway = sinf(u * 6.0f + phase) * 7.0f;
                    Vector2 p = { f->pos.x + sway + (s - 2) * 6.0f, f->pos.y - 8.0f - rise };
                    float sz = 4.0f * fade;
                    Color c = Fade(f->color, fade);
                    DrawLineEx((Vector2){ p.x - sz, p.y }, (Vector2){ p.x + sz, p.y }, 1.6f, c);
                    DrawLineEx((Vector2){ p.x, p.y - sz }, (Vector2){ p.x, p.y + sz }, 1.6f, c);
                }
                break;
            }
            case FXK_RING: {
                float rr = f->radius * (0.25f + 0.75f * u);
                DrawCircleLines((int)f->pos.x, (int)f->pos.y, rr, Fade(f->color, fade));
                DrawCircleLines((int)f->pos.x, (int)f->pos.y, rr * 0.85f, Fade(f->color, fade * 0.5f));
                break;
            }
            case FXK_STARS: {
                // Little stars orbiting above a dazed head.
                for (int s = 0; s < 3; s++) {
                    float a = u * 7.0f + s * 2.09f;
                    Vector2 p = { f->pos.x + cosf(a) * 14.0f, f->pos.y - 26.0f + sinf(a) * 4.0f };
                    DrawPoly(p, 4, 3.5f * fade + 0.5f, u * 180.0f, Fade(f->color, fade));
                }
                break;
            }
            default: break;
        }
    }
}
